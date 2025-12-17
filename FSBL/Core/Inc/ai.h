#ifndef __AI_HEADER_H__
#define __AI_HEADER_H__

#include "common.h"

void ai_init(void);
void ai_process(volatile audio_packet_t *packet, uint8_t *output);

#endif  // #ifndef __AI_HEADER_H__
