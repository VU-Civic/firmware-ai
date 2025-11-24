#define TFLAC_IMPLEMENTATION
#define TFLAC_DISABLE_COUNTERS

#include <stdio.h>
#include <time.h>
#include "app_filex.h"
#include "tflac.h"

#define AUDIO_SAMPLES_PER_CHANNEL             8000
#define AUDIO_SAMPLE_RATE                     48000
#define AUDIO_NUM_CHANNELS                    1
#define AUDIO_BITS_PER_SAMPLE                 16
#define AUDIO_CLIP_NUM_SECONDS                3
#define NUM_HOURS_PER_AUDIO_DIRECTORY         24

#define FLAC_ENCODER_BLOCK_SIZE               4096
#define FLAC_ENCODER_PARTITION_ORDER          3

#define AUDIO_CLIP_NUM_SAMPLES                (AUDIO_CLIP_NUM_SECONDS * AUDIO_SAMPLE_RATE)
#define NUM_SECONDS_PER_AUDIO_DIRECTORY       (NUM_HOURS_PER_AUDIO_DIRECTORY * 3600)

static uint8_t media_memory[8 * FX_STM32_SD_DEFAULT_SECTOR_SIZE] __NON_CACHEABLE;
static uint8_t flac_encoder_mem[16400] __NON_CACHEABLE;
static uint8_t output_buffer[8219] __NON_CACHEABLE;
static int16_t pcm[AUDIO_NUM_CHANNELS][FLAC_ENCODER_BLOCK_SIZE] __NON_CACHEABLE;
static int16_t* pcm_channels[AUDIO_NUM_CHANNELS] __NON_CACHEABLE;
static uint32_t audio_directory_timestamp, samples_written, bytes_written;
static uint32_t pcm_write_index, output_buffer_len;
static uint8_t audio_file_open;
static tflac flac_encoder;
static FX_MEDIA sdio_disk;
static FX_FILE fx_file;

UINT FileX_Init(void)
{
  // Initialize all static variables
  audio_directory_timestamp = audio_file_open = 0;
  for (uint32_t ch = 0; ch < AUDIO_NUM_CHANNELS; ++ch)
    pcm_channels[ch] = pcm[ch];

  // Initialize the FLAC encoder and FileX file system
  tflac_init(&flac_encoder);
  flac_encoder.samplerate = AUDIO_SAMPLE_RATE;
  flac_encoder.channels = AUDIO_NUM_CHANNELS;
  flac_encoder.bitdepth = AUDIO_BITS_PER_SAMPLE;
  flac_encoder.blocksize = FLAC_ENCODER_BLOCK_SIZE;
  flac_encoder.max_partition_order = FLAC_ENCODER_PARTITION_ORDER;
  flac_encoder.enable_md5 = 0;
  flac_encoder.channel_mode = TFLAC_CHANNEL_INDEPENDENT;
  tflac_set_constant_subframe(&flac_encoder, 1);
  tflac_set_fixed_subframe(&flac_encoder, 1);
  if (tflac_validate(&flac_encoder, flac_encoder_mem, tflac_size_memory(FLAC_ENCODER_BLOCK_SIZE)))
    return FX_INVALID_STATE;
  output_buffer_len = tflac_size_frame(FLAC_ENCODER_BLOCK_SIZE, AUDIO_NUM_CHANNELS, AUDIO_BITS_PER_SAMPLE);
  fx_system_initialize();

  // Open the file system on the SD card
  return fx_media_open(&sdio_disk, "CivicAlert", fx_stm32_sd_driver, 0, media_memory, sizeof(media_memory));
}

VOID audio_open_file(uint32_t audio_timestamp)
{
  // Close an already-opened audio file
  if (audio_file_open)
    audio_close_file();

  // Determine if time to create a new storage directory
  const time_t timestamp = (time_t)audio_timestamp;
  struct tm *curr_time = gmtime(&timestamp);
  static char time_string[24] = { 0 }, audio_directory[16] = { 0 };
  strftime(time_string, sizeof(time_string), "%F %H-%M-%S", curr_time);
  if ((audio_timestamp - audio_directory_timestamp) >= NUM_SECONDS_PER_AUDIO_DIRECTORY)
  {
    // Generate a new directory name from the current date
    curr_time->tm_hour = curr_time->tm_min = curr_time->tm_sec = 0;
    memset(audio_directory, 0, sizeof(audio_directory));
    strftime(audio_directory, sizeof(audio_directory), "%F", curr_time);
    fx_directory_create(&sdio_disk, audio_directory);
    audio_directory_timestamp = (uint32_t)mktime(curr_time);
  }

  // Open the requested file
  static char file_name[64] = { 0 };
  snprintf(file_name, sizeof(file_name), "%s/%s.flac", audio_directory, time_string);
  audio_file_open = (fx_file_create(&sdio_disk, file_name) == FX_SUCCESS) &&
                    (fx_file_open(&sdio_disk, &fx_file, file_name, FX_OPEN_FOR_WRITE) == FX_SUCCESS) &&
                    (fx_file_seek(&fx_file, 0) == FX_SUCCESS);

  // Initialize a new FLAC encoder (TODO: SIMPLIFY tflac.h, ADD OPTIMIZED VERSIONS FOR ARM, DOES IT NEED 64-BIT SUPPORT)
  if (audio_file_open)
  {
    // Reset the FLAC encoder
    pcm_write_index = samples_written = 0;
    flac_encoder.samplecount = TFLAC_U64_ZERO;
    flac_encoder.frameno = flac_encoder.verbatim_subframe_bits = 0;
    flac_encoder.wasted_bits = flac_encoder.subframe_bitdepth = 0;

    // Write a FLAC header and STREAMINFO structure (TODO: UNCOMMENT ONCE FIXED TO WORK WITHIN TIME)
    //fx_file_write(&fx_file, "fLaC", 4);
    //tflac_encode_streaminfo(&flac_encoder, 1, output_buffer, output_buffer_len, &bytes_written);
    //fx_file_write(&fx_file, output_buffer, bytes_written);
  }
}

uint8_t audio_write_file(int16_t *audio_data)
{
  // Only proceed if there is an open file and outstanding audio samples
  uint32_t samples_remaining = AUDIO_SAMPLES_PER_CHANNEL, read_index = 0;
  while (audio_file_open && samples_remaining)
  {
    // Cast the audio data into the type expected by the FLAC encoder
    const uint32_t samples_to_copy = ((FLAC_ENCODER_BLOCK_SIZE <= samples_remaining) ? FLAC_ENCODER_BLOCK_SIZE : samples_remaining) - pcm_write_index;
    for (uint32_t ch = 0; ch < AUDIO_NUM_CHANNELS; ++ch)
      memcpy(pcm_channels[ch] + pcm_write_index, audio_data + (ch * AUDIO_SAMPLES_PER_CHANNEL) + read_index, samples_to_copy * sizeof(audio_data[0]));
    pcm_write_index = (pcm_write_index + samples_to_copy) % FLAC_ENCODER_BLOCK_SIZE;
    samples_remaining -= samples_to_copy;
    samples_written += samples_to_copy;
    read_index += samples_to_copy;

    // Check if the PCM buffer has been completely filled
    if (!pcm_write_index)
    {
      // Format the raw audio data as FLAC
      //tflac_encode_s16p(&flac_encoder, FLAC_ENCODER_BLOCK_SIZE, pcm_channels, output_buffer, output_buffer_len, &bytes_written);
      //fx_file_write(&fx_file, output_buffer, bytes_written);
      fx_file_write(&fx_file, pcm_channels, FLAC_ENCODER_BLOCK_SIZE * 2);

      // Determine whether the full audio clip has been written
      if (samples_written >= AUDIO_CLIP_NUM_SAMPLES)
        audio_close_file();
    }
  }

  // TODO: DELETE THIS AND CHANGE AUDIO_NUM_CHANNELS BACK TO 4
  /*if (audio_file_open)
  {
    fx_file_write(&fx_file, audio_data, AUDIO_SAMPLES_PER_CHANNEL * 2);
    samples_written += AUDIO_SAMPLES_PER_CHANNEL;
    if (samples_written >= AUDIO_CLIP_NUM_SAMPLES)
      audio_close_file();
  }*/

  // Return whether the audio file is still open
  return audio_file_open;
}

VOID audio_close_file(void)
{
  // Finalize and close the currently open audio file
  if (audio_file_open)
  {
    // Finalize the FLAC stream
    //tflac_finalize(&flac_encoder);
    //fx_file_seek(&fx_file, 4);
    //tflac_encode_streaminfo(&flac_encoder, 1, output_buffer, output_buffer_len, &bytes_written);
    //fx_file_write(&fx_file, output_buffer, bytes_written);

    // Close the audio file
    fx_file_close(&fx_file);
    fx_media_flush(&sdio_disk);
    audio_file_open = 0;
  }
}
