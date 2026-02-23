#include "ai.h"
#include "comms.h"
#include "flash.h"
#include "storage.h"
#include "system.h"

#define CLIP_LENGTH_SECONDS   60

int main(void)
{
   // Initialize the system and peripherals
   system_init();
   flash_init();
   storage_init();
   comms_init();
   ai_init();

   // Finalize the system configuration
   ai_data_t ai_results = { .ai_firmware_version = FIRMWARE_REVISION, .class_probabilities = { 0 } };
   volatile audio_packet_t *audio_data = 0;
   system_finalize();

   // Wait until a valid data packet has been received
   uint8_t awaiting_data = 1;
   while (awaiting_data)
   {
      // Check if a new valid packet has been received
      audio_data = comms_incoming_data();
      if (audio_data && ((audio_data->imei[0] != 0) || (audio_data->imei[1] != 0) || (audio_data->imei[2] != 0)))
      {
         storage_write_device_metadata_file(FIRMWARE_REVISION, audio_data);
         awaiting_data = 0;
      }

      // Put the CPU to sleep if nothing left to process
      __disable_irq();
      if (!comms_data_available())
         system_sleep();
      __enable_irq();
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
         // Always try to create a new SD card audio file (will only succeed if previous file was closed)
         storage_open_audio_file(audio_data, &ai_results, CLIP_LENGTH_SECONDS);
         storage_write_audio_file(audio_data->audio);
      }

      // Put the CPU to sleep if nothing left to process
      __disable_irq();
      if (!comms_data_available())
         system_sleep();
      __enable_irq();
   }
}
