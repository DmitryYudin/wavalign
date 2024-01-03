/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

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

WavReader* WR_open(const char* filename);
void WR_close(WavReader*);

int WR_readInt16(WavReader*, int16_t* data, unsigned spc);
int WR_readInt24p(WavReader*, uint8_t* data, unsigned spc);
int WR_readInt32(WavReader*, int32_t* data, unsigned spc);
int WR_readFloat(WavReader*, float* data, unsigned spc);
int WR_readDouble(WavReader*, double* data, unsigned spc);
int WR_readRaw(WavReader*, uint8_t* data, unsigned spc);

#ifdef __cplusplus
}
#endif
