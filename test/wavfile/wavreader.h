#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 3
#define WAVE_FORMAT_FLOAT_AUDITION 0xfffd
typedef struct {
    const unsigned format;
	const unsigned channels;
	const uint32_t sample_rate;
	const unsigned bits_per_sample;
	const unsigned block_align;
    const uint32_t samples_per_channel;
} WavReader;

WavReader* WR_open(const char *filename);
void WR_close(WavReader* wavReader);

int WR_readInt16 (WavReader* wavReader, int16_t* data, unsigned spc);
int WR_readInt24p(WavReader* wavReader, uint8_t* data, unsigned spc);
int WR_readInt32 (WavReader* wavReader, int32_t* data, unsigned spc);
int WR_readFloat (WavReader* wavReader, float* data, unsigned spc);
int WR_readDouble(WavReader* wavReader, double* data, unsigned spc);
int WR_readRaw   (WavReader* wavReader, uint8_t* data, unsigned spc);

#ifdef __cplusplus
}
#endif
