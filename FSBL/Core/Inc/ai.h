#ifndef __AI_HEADER_H__
#define __AI_HEADER_H__

#include "common.h"

void ai_init(void);
void ai_build_spectrogram(const int16_t* audio_packet);
float ai_process(volatile audio_packet_t *packet);

#endif  // #ifndef __AI_HEADER_H__
