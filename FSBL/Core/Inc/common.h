#ifndef __COMMON_HEADER_H__
#define __COMMON_HEADER_H__

// Common Header Inclusions --------------------------------------------------------------------------------------------

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "revisions.h"
#include "stm32n6xx_hal.h"


// Custom Application Definitions --------------------------------------------------------------------------------------

#define STRINGIZE_HELPER(x)                           #x
#define STRINGIZE(x)                                  STRINGIZE_HELPER(x)

#define FIRMWARE_BUILD_TIMESTAMP                      _DATETIME
#define FIRMWARE_REVISION                             STRINGIZE(_FW_REVISION)

#ifdef PACKET_FULL_AUDIO
#define AUDIO_PACKET_NUM_CHANNELS                     4
#else
#define AUDIO_PACKET_NUM_CHANNELS                     1
#endif

#define AUDIO_PACKET_SAMPLE_RATE                      48000
#define AUDIO_PACKET_NUM_SAMPLES                      8000
#define AUDIO_PACKET_TOTAL_SAMPLES                    (AUDIO_PACKET_NUM_CHANNELS * AUDIO_PACKET_NUM_SAMPLES)
#define AUDIO_PACKET_START_DELIMITER                  { 0xAE, 0xA0, 0xA2, 0xF5 }
#define AUDIO_PACKET_END_DELIMITER                    { 0xFE, 0xF0, 0xF2, 0x25 }

#define AI_FIRMWARE_VERSION_LENGTH                    8
#define AI_NUM_CLASSES                                2
#define AI_GUNSHOT_CLASS_INDEX                        0

#define CELL_IMEI_LENGTH                              15
#define CELL_IMSI_LENGTH                              15

#define STORAGE_AUDIO_CLIP_HISTORY_SECONDS            1

#define USE_SETJMP_FOR_SD_STORAGE                     0

#ifndef MIN
   #define MIN(a, b)                                  (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
   #define MAX(a, b)                                  (((a) > (b)) ? (a) : (b))
#endif


// Global Type Definitions ---------------------------------------------------------------------------------------------

typedef struct
{
   GPIO_TypeDef *port;
   uint16_t pin;
} gpio_pin_t;

typedef struct __attribute__ ((__packed__))
{
   uint8_t storage_classification_threshold;
   uint8_t audio_clip_length_seconds;
} ai_config_t;

typedef struct __attribute__ ((__packed__, aligned(4)))
{
   uint8_t start_delimiter[4];
   int16_t audio[AUDIO_PACKET_TOTAL_SAMPLES];
   double timestamp;
   float lat, lon, ht;
   int32_t q1, q2, q3;
   char imei[CELL_IMEI_LENGTH+1], imsi[CELL_IMSI_LENGTH+1];
   ai_config_t ai_config;
   uint8_t reserved[6];
   uint8_t end_delimiter[4];
} audio_packet_t;

typedef struct __attribute__ ((__packed__))
{
   uint8_t ai_firmware_version[AI_FIRMWARE_VERSION_LENGTH];
   uint8_t class_probabilities[AI_NUM_CLASSES];
} ai_data_t;


// Global Function Prototypes ------------------------------------------------------------------------------------------

void sd_card_detection_isr(uint8_t sd_card_detected);
void comms_spi_cs_isr(void);


#endif  // #ifndef __COMMON_HEADER_H__
