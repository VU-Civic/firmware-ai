#include "ai.h"
#include "comms.h"
#include "flash.h"
#include "storage.h"
#include "system.h"

int main(void)
{
   // Initialize the system and peripherals
   system_init();
   flash_init();
   storage_init();
   comms_init();
   ai_init();

   // Finalize the system configuration
   ai_data_t ai_results = { .ai_firmware_version = FIRMWARE_BUILD_TIMESTAMP };
   volatile audio_packet_t *audio_data = 0;
   system_finalize();

   // Loop forever // TODO: DELETE THIS
   uint32_t ts = FIRMWARE_BUILD_TIMESTAMP, count = 0, max = 0;
   storage_open_audio_file(ts);
   while (1)
   {
      // Check whether an SD card has been inserted or removed
      storage_handle_sd_card_state_change();

      // Check for any new audio data
      __disable_irq();
      audio_data = comms_incoming_data();
      if (audio_data)
      {
         // Re-enable interrupts and attempt to classify the audio
         __enable_irq();
         ai_process(audio_data, ai_results.class_probabilities);

         // Transmit the results back to the host
         comms_transmit((uint8_t*)&ai_results, sizeof(ai_results));

         // Start a new SD card audio file if the gunshot probability was above a threshold
         if (ai_results.class_probabilities[AI_GUNSHOT_CLASS_INDEX] >= AI_STORAGE_THRESHOLD)
            storage_open_audio_file((uint32_t)audio_data->timestamp);
         storage_write_audio_file(audio_data->audio);





         static volatile uint32_t execution_times[100] = { 0 }, exec_count = 0;
         execution_times[exec_count] = (uint32_t)((uint64_t)comms_cycles_since_data_received() * 1000 / SystemCoreClock);
         max = (execution_times[exec_count] > max) ? execution_times[exec_count] : max;
         exec_count = (exec_count + 1) % 100;
         if (++count == 24)
         {
            ts += 4;
            count = 0;
            storage_open_audio_file(ts);
            const uint32_t odr = LED_MCU_STATUS_GPIO_Port->ODR;
            LED_MCU_STATUS_GPIO_Port->BSRR = ((odr & LED_MCU_STATUS_Pin) << 16U) | (~odr & LED_MCU_STATUS_Pin);
         }
      }
      else
      {
         // Put the CPU to sleep if nothing left to process
         system_sleep();
         __enable_irq();
      }
   }
}
