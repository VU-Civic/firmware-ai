#include "ai.h"
#include "comms.h"
#include "flash.h"
#include "storage.h"
#include "system.h"

#define FLASH_MEMORY_BASE 0x70000000

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
   volatile audio_packet_t *audio_data = 0;
   system_finalize();

   // Test the flash memory
   uint8_t success = 1;
   const uint8_t flash_contents[] = { 0xA3, 0x01, 0x00, 0xF2, 0xFE, 0x25, 0x63, 0x28 };
   for (size_t i = 0; i < sizeof(flash_contents); ++i)
      *(volatile uint8_t*)(FLASH_MEMORY_BASE + i) = flash_contents[i];
   for (size_t i = 0; i < sizeof(flash_contents); ++i)
      success = success && (*(volatile uint8_t*)(FLASH_MEMORY_BASE + i) == flash_contents[i]);

   // Test the SD card storage
   success = success && storage_test_peripheral();

   // Loop forever testing SPI and I2C data communications
   while (1)
   {
      __disable_irq();
      audio_data = comms_incoming_data();
      if (audio_data)
      {
         __enable_irq();
         comms_acknowledge_host();
         ai_results.class_probabilities[0] = ((audio_data->imei[0] != 0) || (audio_data->imei[1] != 0) || (audio_data->imei[2] != 0));
         comms_transmit((uint8_t*)&ai_results, sizeof(ai_results));
         system_feed_watchdog();
      }
      else
         system_sleep();
   }
}
