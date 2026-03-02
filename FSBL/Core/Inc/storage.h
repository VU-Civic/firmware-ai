#ifndef __STORAGE_HEADER_H__
#define __STORAGE_HEADER_H__

#include "common.h"

void storage_init(void);
void storage_handle_sd_card_state_change(void);
void storage_open_audio_file(volatile audio_packet_t *audio_data, const ai_data_t *ai_results, uint8_t clip_length_seconds);
void storage_write_audio_file(volatile audio_packet_t *audio_data);
void storage_write_device_metadata_file(const char *fw_revision, volatile audio_packet_t *audio_data);

#ifdef PERIPHERAL_TESTS
uint8_t storage_test_peripheral(void);
#endif

#endif  // #ifndef __STORAGE_HEADER_H__
