// Header Inclusions ---------------------------------------------------------------------------------------------------

#include <arm_math.h>
#include "ai.h"
#include "ll_aton_runtime.h"
#include "mcu_cache.h"
#include "npu_cache.h"
#include "system.h"


// AI Network and Inference Definitions --------------------------------------------------------------------------------

#define AI_INPUT_NUM_COLUMNS                   224
#define AI_INPUT_NUM_ROWS                      224

#define AI_FFT_FILTER_SIZE                     4096   // Must be 256, 512, 1024, 2048, or 4096
#define AI_FFT_STEP_SIZE                       250    // Must be 100, 125, 160, 200, 250, 320, 400, 500, 800, or 1000 and <= AI_FFT_FILTER_SIZE
#define AI_FFT_WINDOW_SIZE                     864    // Must be <= AI_FFT_FILTER_SIZE and >= AI_FFT_STEP_SIZE

#define AI_SPECTROGRAM_MIN_FREQUENCY_HZ        125
#define AI_SPECTROGRAM_MAX_FREQUENCY_HZ        10000
#define AI_SPECTROGRAM_NUM_MELS                AI_INPUT_NUM_ROWS   // If num_mels != num_rows, spectrogram output must be upscaled
#define AI_SPECTROGRAM_NUM_TIME_STEPS          AI_INPUT_NUM_COLUMNS   // If num_time_steps != num_columns, tspectrogram output must be upscaled

#define MEL_SPECTROGRAM_NO_SCALING             0
#define MEL_SPECTROGRAM_DB_SCALING             1
#define MEL_SPECTROGRAM_POWER_SCALING          2

#define SCALING_TYPE_NONE                      0
#define SCALING_TYPE_MIN_MAX                   1
#define SCALING_TYPE_ABS_MAX                   2
#define SCALING_TYPE_Z_SCORE                   3

#define AI_USE_POWER_SPECTROGRAM               1
#define AI_USE_MEL_SLANEY_FORMULA              0
#define AI_USE_MEL_ENERGY_NORMALIZATION        0
#define AI_MEL_SPECTROGRAM_SCALING_TYPE        MEL_SPECTROGRAM_DB_SCALING
#define AI_INPUT_SCALING_TYPE                  SCALING_TYPE_MIN_MAX

#define AI_FFT_INPUT_PADDING_END_INDEX         ((AI_FFT_FILTER_SIZE - AI_FFT_WINDOW_SIZE) / 2)
#define AI_NUM_TIME_STEPS_PER_PACKET           (AUDIO_PACKET_NUM_SAMPLES / AI_FFT_STEP_SIZE)
#define AI_AVAILABLE_WINDOWS_PER_PACKET        (1 + ((AUDIO_PACKET_NUM_SAMPLES - AI_FFT_WINDOW_SIZE) / AI_FFT_STEP_SIZE))
#define AI_MISSING_WINDOWS_PER_PACKET          (AI_NUM_TIME_STEPS_PER_PACKET - AI_AVAILABLE_WINDOWS_PER_PACKET)
#define AI_NUM_PACKETS_FOR_INPUT               ((AI_SPECTROGRAM_NUM_TIME_STEPS + AI_NUM_TIME_STEPS_PER_PACKET - 1) / AI_NUM_TIME_STEPS_PER_PACKET)

#define arm_rfft_init                          ARM_EXPAND(AI_FFT_FILTER_SIZE)
#define ARM_EXPAND(x)                          ARM_STRINGIFY(x)
#define ARM_STRINGIFY(x)                       arm_rfft_fast_init_ ## x ## _f32


// AI-Specific Type Definitions ----------------------------------------------------------------------------------------

typedef struct {
   int32_t filter_start_index[AI_SPECTROGRAM_NUM_MELS];
   int32_t filter_length[AI_SPECTROGRAM_NUM_MELS];
   float filter_weights[2*AI_FFT_FILTER_SIZE];
} mel_filterbank_t;


// Static AI Network Variables -----------------------------------------------------------------------------------------

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(CivicAlert)
static float *data_in, *data_out, *data_in_end, *data_out_end;
static float hanning_window[AI_FFT_WINDOW_SIZE];
static arm_rfft_fast_instance_f32 fft;
static mel_filterbank_t mel;


// Private Helper Functions --------------------------------------------------------------------------------------------

static float mel_scale(float freq)
{
#if AI_USE_MEL_SLANEY_FORMULA != 0

   const float f_min = 0.0f, f_sp = 200.0f / 3.0f;
   const float min_log_hz = 1000.0f, min_log_mel = (min_log_hz - f_min) / f_sp, logstep = logf(6.4f) / 27.0f;
   return (freq < min_log_hz) ? ((freq - f_min) / f_sp) : (min_log_mel + (logf(freq / min_log_hz) / logstep));

#else

   return 1127.0f * logf(1.0f + freq / 700.0f);

#endif  // #if AI_USE_MEL_SLANEY_FORMULA != 0
}

static void create_mel_filter_bank(void)
{
   // Calculate the Mel frequency band characteristics
   const float fft_bin_width = (float)AUDIO_PACKET_SAMPLE_RATE / AI_FFT_FILTER_SIZE;
   const float mel_low_freq = mel_scale(AI_SPECTROGRAM_MIN_FREQUENCY_HZ), mel_high_freq = mel_scale(AI_SPECTROGRAM_MAX_FREQUENCY_HZ);
   const float mel_freq_delta = (mel_high_freq - mel_low_freq) / (AI_SPECTROGRAM_NUM_MELS + 1);

   // Create a filter bank for each Mel band
   for (int32_t band = 0, filter_end_index, idx = 0; band < AI_SPECTROGRAM_NUM_MELS; ++band)
   {
      // Determine the left, right, and center frequencies for this band
      mel.filter_start_index[band] = filter_end_index = -1;
      const float left_mel = mel_low_freq + band * mel_freq_delta;
      const float center_mel = mel_low_freq + (band + 1) * mel_freq_delta;
      const float right_mel = mel_low_freq + (band + 2) * mel_freq_delta;

      // Store the FFT bin percentages to attribute to this Mel band
      for (int32_t i = 0; i < (AI_FFT_FILTER_SIZE / 2); ++i)
      {
         const float mel_freq = mel_scale(fft_bin_width * i);
         if (mel_freq > left_mel && mel_freq < right_mel)
         {
            mel.filter_weights[idx] = (mel_freq <= center_mel) ?
                  ((mel_freq - left_mel) / (center_mel - left_mel)) :
                  ((right_mel - mel_freq) / (right_mel - center_mel));
#if AI_USE_MEL_ENERGY_NORMALIZATION != 0
            const float energy_normalizer = 2.0f / (right_mel - left_mel);
            mel.filter_weights[idx] *= energy_normalizer;
#endif
            if (mel.filter_start_index[band] == -1)
               mel.filter_start_index[band] = i;
            filter_end_index = i;
            ++idx;
         }
      }

      // Handle the case of no weights for this band (means the FFT has insufficient resolution)
      if (mel.filter_start_index[band] == -1)
      {
         mel.filter_weights[idx++] = 0.0f;
         mel.filter_start_index[band] = 0;
         filter_end_index = 0;
      }
      mel.filter_length[band] = filter_end_index - mel.filter_start_index[band] + 1;
   }
}

static void compute_feature_column(const int16_t *audio_data, float *feature_column)
{
   // Statically define all feature extraction buffers
   static float input_signal[AI_FFT_WINDOW_SIZE], windowed_signal[AI_FFT_FILTER_SIZE] = { 0 };
   static float fft_output[AI_FFT_FILTER_SIZE+2] = { 0 }, fft_magnitudes[(AI_FFT_FILTER_SIZE / 2) + 1];

   // Convert the audio signal to a normalized floating point representation
   arm_q15_to_float(audio_data, input_signal, AI_FFT_WINDOW_SIZE);

   // Apply a Hanning window to the input signal (with implicit zero-padding)
   memset(windowed_signal, 0, sizeof(windowed_signal));
   arm_mult_f32(input_signal, hanning_window, &windowed_signal[AI_FFT_INPUT_PADDING_END_INDEX], AI_FFT_WINDOW_SIZE);

   // Compute the FFT of the signal followed by its complex magnitude (optionally squared for power)
   arm_rfft_fast_f32(&fft, windowed_signal, fft_output, 0);
   fft_output[AI_FFT_FILTER_SIZE] = fft_output[1];
   fft_output[1] = 0.0f;
#if AI_USE_POWER_SPECTROGRAM != 0
   arm_cmplx_mag_squared_f32(fft_output, fft_magnitudes, (AI_FFT_FILTER_SIZE / 2) + 1);
#else
   arm_cmplx_mag_f32(fft_output, fft_magnitudes, (AI_FFT_FILTER_SIZE / 2) + 1);
#endif  // #if AI_USE_POWER_SPECTROGRAM != 0

   // Apply a Mel filter to the magnitude/power spectrum
   const float *weights = mel.filter_weights;
   for (uint32_t bin = 0; bin < AI_SPECTROGRAM_NUM_MELS; ++bin)
   {
      arm_dot_prod_f32(&fft_magnitudes[mel.filter_start_index[bin]], weights, mel.filter_length[bin], &feature_column[bin]);
      weights += mel.filter_length[bin];
   }

   // Apply log scaling to the Mel spectrogram
   arm_offset_f32(feature_column, 1.0e-10f, feature_column, AI_SPECTROGRAM_NUM_MELS);
#if AI_MEL_SPECTROGRAM_SCALING_TYPE == MEL_SPECTROGRAM_DB_SCALING
   for (uint32_t bin = 0; bin < AI_SPECTROGRAM_NUM_MELS; ++bin)
      feature_column[bin] = 10.0f * log10f(feature_column[bin]);
#elif AI_MEL_SPECTROGRAM_SCALING_TYPE == MEL_SPECTROGRAM_POWER_SCALING
   arm_vlog_f32(feature_column, feature_column, AI_SPECTROGRAM_NUM_MELS);
#endif
}


// Public API Functions ------------------------------------------------------------------------------------------------

void ai_init(void)
{
   // Power on and enable all AXISRAM memory banks and the NPU cache  // TODO: CAN ANY AXISRAM BANKS NOT BE ENABLED (NOT USED IN AI)
   //WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_CRCEN);  // TODO: DO I NEED TO ENABLE THE CRC CLOCK
   //(void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_CRCEN);
   WRITE_REG(RCC->AHB2ENSR, RCC_AHB2ENR_RAMCFGEN);
   (void)READ_BIT(RCC->AHB2ENR, RCC_AHB2ENR_RAMCFGEN);
   CLEAR_BIT(RAMCFG_SRAM2_AXI->CR, (RAMCFG_AXISRAM_POWERDOWN | RAMCFG_CR_ALE));
   CLEAR_BIT(RAMCFG_SRAM3_AXI->CR, (RAMCFG_AXISRAM_POWERDOWN | RAMCFG_CR_ALE));
   CLEAR_BIT(RAMCFG_SRAM4_AXI->CR, (RAMCFG_AXISRAM_POWERDOWN | RAMCFG_CR_ALE));
   CLEAR_BIT(RAMCFG_SRAM5_AXI->CR, (RAMCFG_AXISRAM_POWERDOWN | RAMCFG_CR_ALE));
   CLEAR_BIT(RAMCFG_SRAM6_AXI->CR, (RAMCFG_AXISRAM_POWERDOWN | RAMCFG_CR_ALE));
   CLEAR_BIT(RAMCFG_SRAM2_AXI->IER, RAMCFG_IT_ALL);
   CLEAR_BIT(RAMCFG_SRAM3_AXI->IER, RAMCFG_IT_ALL);
   CLEAR_BIT(RAMCFG_SRAM4_AXI->IER, RAMCFG_IT_ALL);
   CLEAR_BIT(RAMCFG_SRAM5_AXI->IER, RAMCFG_IT_ALL);
   CLEAR_BIT(RAMCFG_SRAM6_AXI->IER, RAMCFG_IT_ALL);
   SET_BIT(RAMCFG_SRAM2_AXI->ICR, RAMCFG_FLAGS_ALL);
   SET_BIT(RAMCFG_SRAM3_AXI->ICR, RAMCFG_FLAGS_ALL);
   SET_BIT(RAMCFG_SRAM4_AXI->ICR, RAMCFG_FLAGS_ALL);
   SET_BIT(RAMCFG_SRAM5_AXI->ICR, RAMCFG_FLAGS_ALL);
   SET_BIT(RAMCFG_SRAM6_AXI->ICR, RAMCFG_FLAGS_ALL);
   WRITE_REG(RCC->MEMENSR, (RCC_MEMENR_CACHEAXIRAMEN | RCC_MEMENR_AXISRAM2EN | RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN | RCC_MEMENR_AXISRAM5EN | RCC_MEMENR_AXISRAM6EN));
   (void)READ_BIT(RCC->MEMENR, RCC_MEMENR_CACHEAXIRAMEN);

   // Enable the NPU clock and reset the NPU peripheral
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_NPUEN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_NPUEN);
   WRITE_REG(RCC->AHB5RSTSR, RCC_AHB5ENR_NPUEN);
   WRITE_REG(RCC->AHB5RSTCR, RCC_AHB5ENR_NPUEN);

   // Enable the NPU cache clock and reset the NPU cache
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_CACHEAXIEN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_CACHEAXIEN);
   WRITE_REG(RCC->AHB5RSTSR, RCC_AHB5ENR_CACHEAXIEN);
   WRITE_REG(RCC->AHB5RSTCR, RCC_AHB5ENR_CACHEAXIEN);
   npu_cache_init();

   // Keep all IPs enabled during WFE so they can wake up the CPU (TODO: DO WE NEED THIS)
   LL_BUS_EnableClockLowPower(~0);
   LL_MEM_EnableClockLowPower(~0);
   LL_AHB1_GRP1_EnableClockLowPower(~0);
   LL_AHB2_GRP1_EnableClockLowPower(~0);
   LL_AHB3_GRP1_EnableClockLowPower(~0);
   LL_AHB4_GRP1_EnableClockLowPower(~0);
   LL_AHB5_GRP1_EnableClockLowPower(~0);
   LL_APB1_GRP1_EnableClockLowPower(~0);
   LL_APB1_GRP2_EnableClockLowPower(~0);
   LL_APB2_GRP1_EnableClockLowPower(~0);
   LL_APB4_GRP1_EnableClockLowPower(~0);
   LL_APB4_GRP2_EnableClockLowPower(~0);
   LL_APB5_GRP1_EnableClockLowPower(~0);
   LL_MISC_EnableClockLowPower(~0);

   // Initialize the FFT, Mel Filterbank, and windowing structures
   arm_rfft_init(&fft);
   create_mel_filter_bank();
   arm_hanning_f32(hanning_window, AI_FFT_WINDOW_SIZE);

   // Retrieve pointers to the AI input and output buffers
   const LL_Buffer_InfoTypeDef* input_buffers = NN_Interface_CivicAlert.input_buffers_info();
   const LL_Buffer_InfoTypeDef* output_buffers = NN_Interface_CivicAlert.output_buffers_info();
   data_in = (float*)LL_Buffer_addr_start(&input_buffers[0]);
   data_in_end = (float*)LL_Buffer_addr_end(&input_buffers[0]);
   data_out = (float*)LL_Buffer_addr_start(&output_buffers[0]);
   data_out_end = (float*)LL_Buffer_addr_end(&output_buffers[0]);

   // Initialize the AI runtime
   LL_ATON_RT_RuntimeInit();
   LL_ATON_RT_Init_Network(&NN_Instance_CivicAlert);

   // Disable the NPU and its cache
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_CACHEAXIEN);
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_NPUEN);
}

void ai_process(volatile audio_packet_t *packet, uint8_t *output)
{
   // Set up all necessary AI processing buffers
   static int16_t pending_audio_data[AI_FFT_WINDOW_SIZE] = { 0 };
   static float spectrogram_buffer[AI_NUM_PACKETS_FOR_INPUT * AI_NUM_TIME_STEPS_PER_PACKET * AI_SPECTROGRAM_NUM_MELS] = { 0 };

   // Enable the NPU and its cache
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_NPUEN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_NPUEN);
   WRITE_REG(RCC->AHB5ENSR, RCC_AHB5ENR_CACHEAXIEN);
   (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_CACHEAXIEN);

   // Fill in previously missing AI features using the incoming data
   for (uint32_t audio_idx = 0, time_step = AI_AVAILABLE_WINDOWS_PER_PACKET; time_step < AI_NUM_TIME_STEPS_PER_PACKET; audio_idx += AI_FFT_STEP_SIZE, ++time_step)
   {
      const uint32_t copy_offset = audio_idx ? (AI_FFT_WINDOW_SIZE - AI_FFT_STEP_SIZE) : (AUDIO_PACKET_NUM_SAMPLES - (AI_AVAILABLE_WINDOWS_PER_PACKET * AI_FFT_STEP_SIZE));
      const uint32_t copy_size = audio_idx ? AI_FFT_STEP_SIZE : (AI_FFT_WINDOW_SIZE + (AI_AVAILABLE_WINDOWS_PER_PACKET * AI_FFT_STEP_SIZE) - AUDIO_PACKET_NUM_SAMPLES);
      arm_copy_q15((int16_t*)&packet->audio[audio_idx], &pending_audio_data[copy_offset], copy_size);
      compute_feature_column(pending_audio_data, &spectrogram_buffer[(((AI_NUM_PACKETS_FOR_INPUT - 1) * AI_NUM_TIME_STEPS_PER_PACKET) + time_step) * AI_SPECTROGRAM_NUM_MELS]);
      arm_copy_q15(&pending_audio_data[AI_FFT_STEP_SIZE], &pending_audio_data[0], AI_FFT_WINDOW_SIZE - AI_FFT_STEP_SIZE);
   }

   // Shift previous spectrogram windows to make room for the next batch
   arm_copy_f32(&spectrogram_buffer[AI_NUM_TIME_STEPS_PER_PACKET * AI_SPECTROGRAM_NUM_MELS], &spectrogram_buffer[0], (AI_NUM_PACKETS_FOR_INPUT - 1) * AI_NUM_TIME_STEPS_PER_PACKET * AI_SPECTROGRAM_NUM_MELS);

   // Compute new AI features from the incoming data
   for (uint32_t audio_idx = 0, time_step = 0; time_step < AI_AVAILABLE_WINDOWS_PER_PACKET; audio_idx += AI_FFT_STEP_SIZE, ++time_step)
      compute_feature_column((int16_t*)&packet->audio[audio_idx], &spectrogram_buffer[(((AI_NUM_PACKETS_FOR_INPUT - 1) * AI_NUM_TIME_STEPS_PER_PACKET) + time_step) * AI_SPECTROGRAM_NUM_MELS]);

   // Copy final spectrogram window into missing slots so there are no discontinuities in the last few ms of AI network input
   for (uint32_t time_step = AI_AVAILABLE_WINDOWS_PER_PACKET; time_step < AI_NUM_TIME_STEPS_PER_PACKET; ++time_step)
      arm_copy_f32(&spectrogram_buffer[(((AI_NUM_PACKETS_FOR_INPUT - 1) * AI_NUM_TIME_STEPS_PER_PACKET) + AI_AVAILABLE_WINDOWS_PER_PACKET - 1) * AI_SPECTROGRAM_NUM_MELS],
                   &spectrogram_buffer[(((AI_NUM_PACKETS_FOR_INPUT - 1) * AI_NUM_TIME_STEPS_PER_PACKET) + time_step) * AI_SPECTROGRAM_NUM_MELS], AI_SPECTROGRAM_NUM_MELS);

   // Store any pending audio data for the next packet
   arm_copy_q15((int16_t*)&packet->audio[AI_AVAILABLE_WINDOWS_PER_PACKET * AI_FFT_STEP_SIZE], pending_audio_data, AUDIO_PACKET_NUM_SAMPLES - (AI_AVAILABLE_WINDOWS_PER_PACKET * AI_FFT_STEP_SIZE));

   // Copy relevant part of the spectrogram into the AI network input tensor
   arm_copy_f32(spectrogram_buffer, data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS);

   // Apply input normalization/scaling
#if AI_INPUT_SCALING_TYPE == SCALING_TYPE_ABS_MAX   // Normalize the spectrogram using Abs-Max scaling
   float abs_max_value;
   arm_absmax_no_idx_f32(data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS, &abs_max_value);
   if (abs_max_value > 0.0f)
      arm_scale_f32(data_in, 1.0f / abs_max_value, data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS);
#elif AI_INPUT_SCALING_TYPE == SCALING_TYPE_MIN_MAX   // Normalize the spectrogram using Min-Max scaling
   float min_value, max_value;
   arm_max_no_idx_f32(data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS, &max_value);
   arm_min_no_idx_f32(data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS, &min_value);
   if (min_value != max_value)
   {
      const float scaler = 2.0f / (max_value - min_value);
      const float offset = -1.0f - (scaler * min_value);
      arm_scale_f32(data_in, scaler, data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS);
      arm_offset_f32(data_in, offset, data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS);
   }
#elif AI_INPUT_SCALING_TYPE == SCALING_TYPE_Z_SCORE   // Normalize the spectrogram using Z-Score scaling
   float mean_value, std_dev;
   arm_mean_f32(data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS, &mean_value);
   arm_std_f32(data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS, &std_dev);
   if (std_dev > 0.0f)
   {
      arm_offset_f32(data_in, -mean_value, data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS);
      arm_scale_f32(data_in, 1.0f / std_dev, data_in, AI_INPUT_NUM_COLUMNS * AI_INPUT_NUM_ROWS);
   }
#endif  // #if AI_INPUT_SCALING_TYPE == SCALING_TYPE_ABS_MAX

   // Reset the AI runtime and invalidate all caches
   LL_ATON_RT_Reset_Network(&NN_Instance_CivicAlert);
   LL_ATON_Cache_MCU_Clean_Invalidate_Range((uintptr_t)data_in, data_in_end - data_in);
   LL_ATON_Cache_NPU_Invalidate();

   // Run the inference loop
   LL_ATON_RT_RetValues_t ll_aton_rt_ret = LL_ATON_RT_DONE;
   do
   {
      ll_aton_rt_ret = LL_ATON_RT_RunEpochBlock(&NN_Instance_CivicAlert);
      if (ll_aton_rt_ret == LL_ATON_RT_WFE)
         LL_ATON_OSAL_WFE();
   } while (ll_aton_rt_ret != LL_ATON_RT_DONE);

   // Invalidate the output cache and process the classification output
   LL_ATON_Cache_MCU_Clean_Invalidate_Range((uintptr_t)data_out, data_out_end - data_out);
   for (uint32_t i = 0; i < AI_NUM_CLASSES; ++i)
   {
      const float out_float = 100.0f * data_out[i];
      output[i] = (uint8_t)((out_float > 100.0f) ? 100.0f : ((out_float < 0.0f) ? 0.0f : out_float));
   }

   // Disable the NPU and its cache and feed the watchdog
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_CACHEAXIEN);
   WRITE_REG(RCC->AHB5ENCR, RCC_AHB5ENR_NPUEN);
   system_feed_watchdog();
}
