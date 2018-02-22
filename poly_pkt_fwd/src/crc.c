#include "crc.h"
#include <stddef.h>

uint16_t crc_ccit(const uint8_t * data, unsigned size) {
	const uint16_t crc_poly = 0x1021; /* CCITT */
	const uint16_t init_val = 0xFFFF; /* CCITT */
	uint16_t x = init_val;
	unsigned i, j;

	if (data == NULL)  {
		return 0;
	}

	for (i=0; i<size; ++i) {
		x ^= (uint16_t)data[i] << 8;
		for (j=0; j<8; ++j) {
			x = (x & 0x8000) ? (x<<1) ^ crc_poly : (x<<1);
		}
	}

	return x;
}

uint8_t crc8_ccit(const uint8_t * data, unsigned size) {
	const uint8_t crc_poly = 0x87; /* CCITT */
	const uint8_t init_val = 0xFF; /* CCITT */
	uint8_t x = init_val;
	unsigned i, j;

	if (data == NULL)  {
		return 0;
	}

	for (i=0; i<size; ++i) {
		x ^= data[i];
		for (j=0; j<8; ++j) {
			x = (x & 0x80) ? (x<<1) ^ crc_poly : (x<<1);
		}
	}

	return x;
}
