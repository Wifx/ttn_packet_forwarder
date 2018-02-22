#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>

uint16_t crc_ccit(const uint8_t * data, unsigned size);
uint8_t crc8_ccit(const uint8_t * data, unsigned size);

#endif /* _CRC_H_ */
