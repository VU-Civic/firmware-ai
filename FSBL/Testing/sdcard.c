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
   ai_init();
   comms_init();

   // Finalize the system configuration
   ai_data_t ai_results = { .ai_firmware_version = FIRMWARE_REVISION, .class_probabilities = { 0 } };
   const uint32_t reception_timeout = AUDIO_PACKET_RECEPTION_TIMEOUT_SECONDS * SystemCoreClock;
   volatile audio_packet_t *audio_data = 0;
   uint32_t last_reception_time = 0;
   system_finalize();

   // Wait until a valid data packet has been received
   while (!audio_data)
   {
      // Check if a new valid packet has been received
      audio_data = comms_incoming_data();
      if (audio_data && ((audio_data->imei[0] != 0) || (audio_data->imei[1] != 0) || (audio_data->imei[2] != 0)))
      {
         storage_write_device_metadata_file(FIRMWARE_REVISION, audio_data);
         last_reception_time = DWT->CYCCNT;
         comms_acknowledge_host();
      }
      else
      {
         audio_data = 0;
         comms_init();
      }
   }

   // Loop forever
   while (1)
   {
      // Check whether an SD card has been inserted or removed
      storage_handle_sd_card_state_change();

      // Check for any new audio data
      audio_data = comms_incoming_data();
      if (audio_data)
      {
         // Acknowledge the packet and store the reception timestamp
         comms_acknowledge_host();
         last_reception_time = DWT->CYCCNT;

         // Transmit fake results back to the host
         comms_transmit((uint8_t*)&ai_results, sizeof(ai_results));

         // Always try to create a new SD card audio file (will only succeed if previous file was closed)
         storage_open_audio_file(audio_data, &ai_results, audio_data->ai_config.audio_clip_length_seconds);
         storage_write_audio_file(audio_data);
      }
      else if ((DWT->CYCCNT - last_reception_time) >= reception_timeout)
      {
         // Signal an error to the host and attempt to re-initialize communications
         last_reception_time = DWT->CYCCNT;
         comms_unacknowledge_host();
         comms_init();
      }

      // Put the CPU to sleep if nothing left to process
      __disable_irq();
      if (!comms_data_available())
         system_sleep();
      __enable_irq();
   }
}
