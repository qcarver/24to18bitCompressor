/*
 * compressor.cpp
 *
 *  Created on: May 7, 2019
 *  Author: qcarver@gmail.com for compressing an 18 bit sound type
 */

#include "compressor.h"
#include "string.h"

#define COMPRESSED_UNIT_SIZE 9
#define EXPANDED_UNIT_SIZE 12

/*
 * The following graphic depicts where expanded bits in the 12 byte space are
 * placed in the compressed 9 byte space. For example Byte[0]:lloooooo will not
 * change, and the least 2 bits of Byte[11] end up in the least two bits of
 * Byte[8]. Note that bits Byte[0,3 and 6]:oolloooo,oooolloo and ooooooll are
 * absent in the compressed form. This is because the MSB of the expanded byte
 * only has 4 possible values where [x%3]:lloooooo maps as follows:
 * {11 => 0xFF, 10 => 0x80, 01 => 0x7F and 00 => 0x0} this is the trick to the
 * lossless compression, every mod3 byte we can map 2 bits to a whole byte
 *
+-------+------+----------------+----------------+----------------+-----------------+
| Byte# | Mask | lloooooo (xC0) | oolloooo (x30) | oooolloo (x0C) | ooooooll (0x03) |
+-------+------+----------------+----------------+----------------+-----------------+
| 0     | C0   | 0:lloooooo     | 1:lloooooo     | 1:oolloooo     | 1:oooolloo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 1     | FF   | 1:ooooooll     | 2:lloooooo     | 2:oolloooo     | 2:oooolloo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 2     | FF   | 2:ooooooll     | 3:lloooooo     | 4:lloooooo     | 4:oolloooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 3     | C0   | 4:oooolloo     | 4:ooooooll     | 5:lloooooo     | 5:oolloooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 4     | FF   | 5:oooolloo     | 5:ooooooll     | 6:lloooooo     | 7:lloooooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 5     | FF   | 7:oolloooo     | 7:oooolloo     | 7:ooooooll     | 8:lloooooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 6     | C0   | 8:oolloooo     | 8:oooolloo     | 8:ooooooll     | 9:lloooooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 7     | FF   | 10:lloooooo    | 10:oolloooo    | 10:oooolloo    | 10:ooooooll     |
+-------+------+----------------+----------------+----------------+-----------------+
| 8     | FF   | 11:lloooooo    | 11:oolloooo    | 11:oooolloo    | 11:ooooooll     |
+-------+------+----------------+----------------+----------------+-----------------+
| 9     | C0   |                |                |                |                 |
+-------+------+----------------+----------------+----------------+-----------------+
| 10    | FF   |                |                |                |                 |
+-------+------+----------------+----------------+----------------+-----------------+
| 11    | FF   |                |                |                |                 |
+-------+------+----------------+----------------+----------------+-----------------+
chart made with: https://www.tablesgenerator.com/text_tables#
 */

template<int unit_size> union Unit {
	//ensures order ... silly, but ounce of prevention
	uint8_t asBytes[unit_size];
	struct __attribute__((__packed__)){
		//ensure ooooooll are the lsbits and lloooooo are the msbits
#ifdef ORDER_BIG_ENDIAN
		//WARNING:DELETE THIS LINE IF YOU HAVE BUILT AND TESTED W BIG ENDIAN (I HAVE NOT)
		uint8_t lloooooo :2, oolloooo :2, oooolloo :2, ooooooll :2;
#else
		uint8_t ooooooll :2, oooolloo :2, oolloooo :2, lloooooo :2;
#endif
	} asBitFields[unit_size];
};

//We never instantiate a CompressionBuffer. Instead cast the buffer to one.
//the union allows us access to locations relevant addresses in both use cases
union __attribute__((__packed__)) CompressionBuffer{
	uint8_t asDma[DMA_SIZE];
	Unit<COMPRESSED_UNIT_SIZE> asCompressed[DMA_SIZE/EXPANDED_UNIT_SIZE];
	Unit<EXPANDED_UNIT_SIZE> asExpanded[DMA_SIZE/EXPANDED_UNIT_SIZE];
};

CompressionBuffer & asCompressionBuffer(uint8_t (&dmaBuffer)[DMA_SIZE]){
	static_assert((sizeof(CompressionBuffer)==sizeof(uint8_t[DMA_SIZE])),
			"reinterpret_cast is casting to a type of a different size");
	return reinterpret_cast<CompressionBuffer (&)>(dmaBuffer);
}

//upper 2 bits are same as (byte passed in & 0x3), lower 6 bits are same as (byte passed in & 0x1)
inline uint8_t decryptByte(uint8_t byte){
	//printf("decrypting field: %#02x ", byte);
	switch(byte){
	case 0: byte = 0; break;
	case 1: byte = 0x7F;  break;
	case 2: byte = 0x80; break;
	default: byte = 0xFF;
	}
	//printf("to: %#02x\n", byte);
	return byte;
}

/**
 * compressBuffer
 * losslessly compresses arrays of 3-byte samples (msb only 4 unique values) into 18 bits
 * @param dmaBuffer reference to a DMA sized element where src data is and where artifact is left
 * @return constant size of compressed data placed in buffer (constant which is DMA_SIZE *.75)
 */
uint16_t compressBuffer(uint8_t (&dmaBuffer)[DMA_SIZE]) {
	CompressionBuffer & buffer = asCompressionBuffer(dmaBuffer);
	for (int unit = 0; unit < DMA_SIZE/EXPANDED_UNIT_SIZE; unit++){
		buffer.asCompressed[unit].asBitFields[0].lloooooo = buffer.asExpanded[unit].asBitFields[0].lloooooo;
		buffer.asCompressed[unit].asBitFields[0].oolloooo = buffer.asExpanded[unit].asBitFields[1].lloooooo;
		buffer.asCompressed[unit].asBitFields[0].oooolloo = buffer.asExpanded[unit].asBitFields[1].oolloooo;
		buffer.asCompressed[unit].asBitFields[0].ooooooll = buffer.asExpanded[unit].asBitFields[1].oooolloo;
		buffer.asCompressed[unit].asBitFields[1].lloooooo = buffer.asExpanded[unit].asBitFields[1].ooooooll;
		buffer.asCompressed[unit].asBitFields[1].oolloooo = buffer.asExpanded[unit].asBitFields[2].lloooooo;
		buffer.asCompressed[unit].asBitFields[1].oooolloo = buffer.asExpanded[unit].asBitFields[2].oolloooo;
		buffer.asCompressed[unit].asBitFields[1].ooooooll = buffer.asExpanded[unit].asBitFields[2].oooolloo;
		buffer.asCompressed[unit].asBitFields[2].lloooooo = buffer.asExpanded[unit].asBitFields[2].ooooooll;
		buffer.asCompressed[unit].asBitFields[2].oolloooo = buffer.asExpanded[unit].asBitFields[3].lloooooo;
		buffer.asCompressed[unit].asBitFields[2].oooolloo = buffer.asExpanded[unit].asBitFields[4].lloooooo;
		buffer.asCompressed[unit].asBitFields[2].ooooooll = buffer.asExpanded[unit].asBitFields[4].oolloooo;
		buffer.asCompressed[unit].asBitFields[3].lloooooo = buffer.asExpanded[unit].asBitFields[4].oooolloo;
		buffer.asCompressed[unit].asBitFields[3].oolloooo = buffer.asExpanded[unit].asBitFields[4].ooooooll;
		buffer.asCompressed[unit].asBitFields[3].oooolloo = buffer.asExpanded[unit].asBitFields[5].lloooooo;
		buffer.asCompressed[unit].asBitFields[3].ooooooll = buffer.asExpanded[unit].asBitFields[5].oolloooo;
		buffer.asCompressed[unit].asBitFields[4].lloooooo = buffer.asExpanded[unit].asBitFields[5].oooolloo;
		buffer.asCompressed[unit].asBitFields[4].oolloooo = buffer.asExpanded[unit].asBitFields[5].ooooooll;
		buffer.asCompressed[unit].asBitFields[4].oooolloo = buffer.asExpanded[unit].asBitFields[6].lloooooo;
		buffer.asCompressed[unit].asBitFields[4].ooooooll = buffer.asExpanded[unit].asBitFields[7].lloooooo;
		buffer.asCompressed[unit].asBitFields[5].lloooooo = buffer.asExpanded[unit].asBitFields[7].oolloooo;
		buffer.asCompressed[unit].asBitFields[5].oolloooo = buffer.asExpanded[unit].asBitFields[7].oooolloo;
		buffer.asCompressed[unit].asBitFields[5].oooolloo = buffer.asExpanded[unit].asBitFields[7].ooooooll;
		buffer.asCompressed[unit].asBitFields[5].ooooooll = buffer.asExpanded[unit].asBitFields[8].lloooooo;
		buffer.asCompressed[unit].asBitFields[6].lloooooo = buffer.asExpanded[unit].asBitFields[8].oolloooo;
		buffer.asCompressed[unit].asBitFields[6].oolloooo = buffer.asExpanded[unit].asBitFields[8].oooolloo;
		buffer.asCompressed[unit].asBitFields[6].oooolloo = buffer.asExpanded[unit].asBitFields[8].ooooooll;
		buffer.asCompressed[unit].asBitFields[6].ooooooll = buffer.asExpanded[unit].asBitFields[9].lloooooo;
		//we have phased back into byte alignment at this tuple, so just byte copy (faster)
		buffer.asCompressed[unit].asBytes[7] = buffer.asExpanded[unit].asBytes[10];
		buffer.asCompressed[unit].asBytes[8] = buffer.asExpanded[unit].asBytes[11];
	}
	return (3*DMA_SIZE)/4;
}

/**
 * expandBuffer
 * expands 18 bit sound samples with a special meaning in the first two bits to 3-byte samples
 * @param dmaBuffer pointer to a DMA sized element to expand into
 * @return size of expanded buffer, which is a constant (DMA_SIZE)
 */
uint16_t expandBuffer(uint8_t (&dmaBuffer)[DMA_SIZE]) {
	CompressionBuffer & buffer = asCompressionBuffer(dmaBuffer);
	//expand loops runs decrementally so as not to overwrite data we will decompress from
	for (int unit = (DMA_SIZE/EXPANDED_UNIT_SIZE)-1; unit > -1; unit--){
			buffer.asExpanded[unit].asBytes[11] = buffer.asCompressed[unit].asBytes[8];
			buffer.asExpanded[unit].asBytes[10] = buffer.asCompressed[unit].asBytes[7];
			//2 bits from src, are key to value of decryptedByte
			//printf("Decrypting asCompressed[%d].byte[%d]:\t %#02x\t",unit,6,buffer.asCompressed[unit].asBytes[6]);
			buffer.asExpanded[unit].asBytes[9] = decryptByte(buffer.asCompressed[unit].asBitFields[6].ooooooll);
			buffer.asExpanded[unit].asBitFields[8].ooooooll = buffer.asCompressed[unit].asBitFields[6].oooolloo;
			buffer.asExpanded[unit].asBitFields[8].oooolloo = buffer.asCompressed[unit].asBitFields[6].oolloooo;
			buffer.asExpanded[unit].asBitFields[8].oolloooo = buffer.asCompressed[unit].asBitFields[6].lloooooo;
			buffer.asExpanded[unit].asBitFields[8].lloooooo = buffer.asCompressed[unit].asBitFields[5].ooooooll;
			buffer.asExpanded[unit].asBitFields[7].ooooooll = buffer.asCompressed[unit].asBitFields[5].oooolloo;
			buffer.asExpanded[unit].asBitFields[7].oooolloo = buffer.asCompressed[unit].asBitFields[5].oolloooo;
			buffer.asExpanded[unit].asBitFields[7].oolloooo = buffer.asCompressed[unit].asBitFields[5].lloooooo;
			buffer.asExpanded[unit].asBitFields[7].lloooooo = buffer.asCompressed[unit].asBitFields[4].ooooooll;
			//2 bits from src, are key to value of decryptedByte
			//printf("Decrypting asCompressed[%d].byte[%d]:\t %#02x\t",unit,4,buffer.asCompressed[unit].asBytes[4]);
			buffer.asExpanded[unit].asBytes[6] = decryptByte(buffer.asCompressed[unit].asBitFields[4].oooolloo );
			buffer.asExpanded[unit].asBitFields[5].ooooooll = buffer.asCompressed[unit].asBitFields[4].oolloooo;
			buffer.asExpanded[unit].asBitFields[5].oooolloo = buffer.asCompressed[unit].asBitFields[4].lloooooo;
			buffer.asExpanded[unit].asBitFields[5].oolloooo = buffer.asCompressed[unit].asBitFields[3].ooooooll;
			buffer.asExpanded[unit].asBitFields[5].lloooooo = buffer.asCompressed[unit].asBitFields[3].oooolloo;
			buffer.asExpanded[unit].asBitFields[4].ooooooll = buffer.asCompressed[unit].asBitFields[3].oolloooo;
			buffer.asExpanded[unit].asBitFields[4].oooolloo = buffer.asCompressed[unit].asBitFields[3].lloooooo;
			buffer.asExpanded[unit].asBitFields[4].oolloooo = buffer.asCompressed[unit].asBitFields[2].ooooooll;
			buffer.asExpanded[unit].asBitFields[4].lloooooo = buffer.asCompressed[unit].asBitFields[2].oooolloo;
			//2 bits from src, are key to value of decryptedByte
			//printf("Decrypting asCompressed[%d].byte[%d]:\t %#02x\t",unit,2,buffer.asCompressed[unit].asBytes[2]);
			buffer.asExpanded[unit].asBytes[3] = decryptByte(buffer.asCompressed[unit].asBitFields[2].oolloooo);
			buffer.asExpanded[unit].asBitFields[2].ooooooll = buffer.asCompressed[unit].asBitFields[2].lloooooo;
			buffer.asExpanded[unit].asBitFields[2].oooolloo = buffer.asCompressed[unit].asBitFields[1].ooooooll;
			buffer.asExpanded[unit].asBitFields[2].oolloooo = buffer.asCompressed[unit].asBitFields[1].oooolloo;
			buffer.asExpanded[unit].asBitFields[2].lloooooo = buffer.asCompressed[unit].asBitFields[1].oolloooo;
			buffer.asExpanded[unit].asBitFields[1].ooooooll = buffer.asCompressed[unit].asBitFields[1].lloooooo;
			buffer.asExpanded[unit].asBitFields[1].oooolloo = buffer.asCompressed[unit].asBitFields[0].ooooooll;
			buffer.asExpanded[unit].asBitFields[1].oolloooo = buffer.asCompressed[unit].asBitFields[0].oooolloo;
			buffer.asExpanded[unit].asBitFields[1].lloooooo = buffer.asCompressed[unit].asBitFields[0].oolloooo;
			//2 bits from src, are key to value of decryptedByte
			//printf("Decrypting asCompressed[%d].byte[%d]:\t %#02x\t",unit,0,buffer.asCompressed[unit].asBytes[0]);
			buffer.asExpanded[unit].asBytes[0] = decryptByte(buffer.asCompressed[unit].asBitFields[0].lloooooo);
			//printf("//////////////////////////////////////////////////////////////\n");

	}
	return DMA_SIZE;
}

#define TEST_OBJECT
#ifdef TEST_OBJECT
//TEST CODE BELOW HERE/////////////////////////////////////////////////////////

/*To run test for this object in the desktop environment
1) Set your tool chain to your compiler: (eg)
set path= %path%C:\Program Files\mingw-w64\x86_64-8.1.0-win32-sjlj-rt_v6-rev0\mingw64\bin
2) Build with the TEST_OBJECT flag:
g++ -DTEST_OBJECT -o test compressor.cpp
3) Run the test by typing
test
*/
void printAllBuffers(uint8_t (&original)[DMA_SIZE], uint8_t (&midway)[DMA_SIZE], uint8_t (&result)[DMA_SIZE]){
	for (int i = 0; i < DMA_SIZE; i+=3){
		printf("%#02x %#02x %#02x  => %#02x %#02x %#02x  => %#02x %#02x %#02x \n",
				original[i],original[i+1],original[i+2],
				midway[i],midway[i+1],midway[i+2],
				result[i],result[i+1],result[i+2]
		);
	}
}

//the test...
int main(int argc, char **argv) {
	//Make a buffer to put data in
	uint8_t originalBuffer[DMA_SIZE];
	const uint16_t originalBufferSize = sizeof(originalBuffer);
	//tokenize the compressable bytes
	uint8_t compressableByte[]={0x0,0x7F,0x80,0xFF};
	const uint8_t num2BitValues = sizeof(compressableByte);
	//Stuff buffer w/ arb data. can try diff values in 'else' clause
	for (uint16_t i = 0; i < originalBufferSize; i++){
		if (i%3 == 0){
			//rotate compressable byte values everytime we stuff a compressable Byte
			originalBuffer[i] = compressableByte[(i/sizeof(sound24_t))%num2BitValues];
		} else {
			originalBuffer[i] = 0xFF;
		}
	}
	//save our progress for a print at the end
	uint8_t compressedBuffer[DMA_SIZE];
	memcpy(compressedBuffer,originalBuffer,DMA_SIZE);
	//squish it
	compressBuffer(compressedBuffer);
	//save our progress for a print at the end
	uint8_t compressedThenExpandedBuffer[DMA_SIZE];
	memcpy(compressedThenExpandedBuffer,compressedBuffer,DMA_SIZE);
	//unsquish it
	expandBuffer(compressedThenExpandedBuffer);
	//if buffer matches compress(expand(buffer))) then pass
	if (memcmp(compressedThenExpandedBuffer,originalBuffer,DMA_SIZE)==0){
		printf("Test passed.\n");
	} else {
		printf("Buffers differ:\n");
		printf("Original buffers,  compressedBuffer,  expanded-compressed buffer as follows:\n");
		printf("============================================================================\n");
		printAllBuffers(originalBuffer, compressedBuffer, compressedThenExpandedBuffer);
		printf("Test failed.");
	}
}
#endif



