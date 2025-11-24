#ifndef __APP_FILEX_HEADER_H__
#define __APP_FILEX_HEADER_H__

#include "fx_api.h"
#include "fx_stm32_sd_driver.h"

UINT FileX_Init(void);

void audio_open_file(uint32_t audio_timestamp);
uint8_t audio_write_file(int16_t *audio_data);
void audio_close_file(void);

#endif  // #ifndef __APP_FILEX_HEADER_H__
