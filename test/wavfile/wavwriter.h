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
} WavWriter;

WavWriter* WW_open(const char* filename, unsigned format, unsigned channels, uint32_t sample_rate,
                   unsigned bits_per_sample);
void WW_close(WavWriter*);

int WW_writeInt16(WavWriter*, const int16_t* data, unsigned spc);
int WW_writeInt24(WavWriter*, const uint8_t* data, unsigned spc);
int WW_writeInt32(WavWriter*, const int32_t* data, unsigned spc);
int WW_writeFloat(WavWriter*, const float* data, unsigned spc);
int WW_writeRaw(WavWriter*, const uint8_t* data, unsigned spc);

#ifdef __cplusplus
}
#endif
