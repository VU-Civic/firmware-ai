#ifndef __AI_HEADER_H__
#define __AI_HEADER_H__

#include "common.h"

ai_state_t* ai_create(void);
void ai_free(ai_state_t* state);
void ai_init(ai_state_t* state);
void ai_build_spectrogram(ai_state_t *state, const int16_t* audio_packet);
float ai_process(ai_state_t *state, volatile audio_packet_t *packet);

#endif  // #ifndef __AI_HEADER_H__
