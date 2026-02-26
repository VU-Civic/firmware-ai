#ifndef __COMMS_HEADER_H__
#define __COMMS_HEADER_H__

#include "common.h"

void comms_init(void);
void comms_acknowledge_host(void);
uint8_t comms_data_available(void);
void comms_transmit(uint8_t *data, uint8_t data_len);
volatile audio_packet_t* comms_incoming_data(void);
uint32_t comms_cycles_since_data_received(void);

#endif  // #ifndef __COMMS_HEADER_H__
