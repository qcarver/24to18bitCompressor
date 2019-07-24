#ifndef MAIN_COMPRESSOR_H_
#define MAIN_COMPRESSOR_H_
#include <stdint.h>

//come up with some defintions that would otw be defined in the app using it
#define DMA_SIZE (512*3)
typedef struct { uint8_t byte[3]; } sound24_t;
#include <cstdio>

uint16_t compressBuffer(uint8_t (&dmaBuffer)[DMA_SIZE]);
uint16_t expandBuffer(uint8_t (&dmaBuffer)[DMA_SIZE]);
#endif /* MAIN_COMPRESSOR_H_ */
