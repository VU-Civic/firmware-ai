#include "ai.h"
#include "comms.h"
#include "flash.h"
#include "storage.h"
#include "system.h"

__attribute__ ((section(".noncacheable")))
static ai_data_t ai_results = { .ai_firmware_version = FIRMWARE_BUILD_TIMESTAMP };

int main(void)
{
   // Initialize the system and peripherals
   system_init();
   flash_init();
   storage_init();
   comms_init();
   ai_init();

   // Illuminate the MCU status LED
   WRITE_REG(RCC->AHB4ENSR, RCC_AHB4ENR_GPIOBEN);
   (void)READ_BIT(RCC->AHB4ENR, RCC_AHB4ENR_GPIOBEN);
   WRITE_REG(LED_MCU_STATUS_GPIO_Port->BSRR, LED_MCU_STATUS_Pin);

   // Finalize the system configuration
   volatile audio_packet_t *audio_data = 0;
   uint32_t count = 0;
   system_finalize();

   // Loop forever
   uint32_t ts = FIRMWARE_BUILD_TIMESTAMP;
   while (1)
   {
      storage_handle_sd_card_state_change();
      audio_data = comms_incoming_data();
      if (audio_data)
      {
         storage_open_audio_file(ts);
         storage_write_audio_file((int16_t*)audio_data->audio);
         ai_process(audio_data, ai_results.class_probabilities);
         if (++count == 18)
         {
            ts += 3;
            count = 0;
            const uint32_t odr = LED_MCU_STATUS_GPIO_Port->ODR;
            LED_MCU_STATUS_GPIO_Port->BSRR = ((odr & LED_MCU_STATUS_Pin) << 16U) | (~odr & LED_MCU_STATUS_Pin);
            //comms_transmit((uint8_t*)&ai_results, sizeof(ai_results));
         }
      }
      //else
      //   system_sleep();
   }
}
