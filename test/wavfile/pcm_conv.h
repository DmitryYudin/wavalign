/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#pragma once

//
// wavreader/wavwriter internal use
//
__inline static int32_t pcmconv_to_int32(const unsigned char *pcm, int format, unsigned bits_per_sample, int* err_sticky)
{
	int32_t x;
	if(format == WAVE_FORMAT_PCM) {
		if(bits_per_sample == 16) {
			x = *((short*)pcm);
			x <<= 16;
		} else if(bits_per_sample == 24 || bits_per_sample == 20) {
			x =          pcm[2]; // little endian
			x = (x<<8) | pcm[1];
			x = (x<<8) | pcm[0];
			x <<= 8;
		} else if(bits_per_sample == 32) {
			x = *(int32_t*)pcm;
		} else {
			assert(!"Unsupported WAVE_FORMAT_PCM::bits_per_sample");
			*err_sticky |= 1;
		}
	} else if (format == WAVE_FORMAT_IEEE_FLOAT) {
		if(bits_per_sample == 32) {
			float y = *(float*)pcm;
			float y_max = 1.f;
			float y_min = -1.f;
			uint32_t scale = (1UL<<31) - 1;
			if(y >= y_max) {
				x = (1UL<<31) - 1;
			} else if (y < y_min) {
				x = (1UL<<31);
			} else {
				x = (int32_t)(y*scale);
			}
		} else {
			assert(!"Unsupported WAVE_FORMAT_IEEE_FLOAT::bits_per_sample");
			*err_sticky |= 1;
		}
	} else if (format == WAVE_FORMAT_FLOAT_AUDITION) {
		if(bits_per_sample == 24) { // 24.8 unscaled
			float y = *(float*)pcm;
			float y_max = (float)(1UL<<23);
			float y_min = -(float)(1UL<<23);
			uint32_t scale = (1UL<<8);
			if(y >= y_max) {
				x = (1UL<<31) - 1;
			} else if (y < y_min) {
				x = (1UL<<31);
			} else {
				x = (int32_t)(y*scale);
			}			
		} else if(bits_per_sample == 32) { // 16.8 obsolete
			float y = *(float*)pcm;
			float y_max = (float)(1UL<<15);
			float y_min = -(float)(1UL<<15);
			uint32_t scale = (1UL<<16);
			if(y >= y_max) {
				x = (1UL<<31) - 1;
			} else if (y < y_min) {
				x = (1UL<<31);
			} else {
				x = (int32_t)(y*scale);
			}						
		} else {
			assert(!"Unsupported WAVE_FORMAT_FLOAT_AUDITION::bits_per_sample");
			*err_sticky |= 1;
		}
	} else {
		assert(!"Unsupported format");
		*err_sticky |= 1;
	}
	return x;
}
__inline static double pcmconv_to_double(const unsigned char *pcm, int format, unsigned bits_per_sample, int* err_sticky)
{
	double x;
	if(format == WAVE_FORMAT_PCM) {
		int32_t y;
		if(bits_per_sample == 16) {
			y = *((short*)pcm);
			y <<= 16;
		} else if(bits_per_sample == 24 || bits_per_sample == 20) {			
			y =          pcm[2]; // little endian
			y = (y<<8) | pcm[1];
			y = (y<<8) | pcm[0];
			y <<= 8;
		} else if(bits_per_sample == 32) {
			y = *(int32_t*)pcm;
		} else {
			assert(!"Unsupported WAVE_FORMAT_PCM::bits_per_sample");
			*err_sticky |= 1;
		}
		x = ((double)(y))/((1UL<<31) - 1);
	} else if (format == WAVE_FORMAT_IEEE_FLOAT) {
		if(bits_per_sample == 32) {
			x = *(float*)pcm;
		} else {
			assert(!"Unsupported WAVE_FORMAT_IEEE_FLOAT::bits_per_sample");
			*err_sticky |= 1;
		}
	} else if (format == WAVE_FORMAT_FLOAT_AUDITION) {
		if(bits_per_sample == 24) { // 24.8 unscaled
			x = *(float*)pcm;
			x /= 1UL<<23;
		} else if(bits_per_sample == 32) { // 16.8 obsolete
			x = *(float*)pcm;
			x /= 1UL<<15;
		} else {
			assert(!"Unsupported WAVE_FORMAT_FLOAT_AUDITION::bits_per_sample");
			*err_sticky |= 1;
		}
	} else {
		assert(!"Unsupported format");
		*err_sticky |= 1;
	}
	return x;
}
