/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#include "unittest_wavfmt.inl" // <- pcm_data & files[]

#include "wavreader.h"
#include "wavwriter.h"

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <new>

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
static char testPref[512] = {'\0', };
#define TRACE_ERR(cond, fmt, ...) if (cond) { fprintf(stderr, "%s: (%u): " fmt "\n", testPref, __LINE__, ##__VA_ARGS__); return 1; }

#define NUM_CHANNLES 2
#define SAMPLE_RATE 8000
static const struct {
	const char *name;
	const unsigned char *data;
	unsigned len;
} files[] = {
	{ "sample_16b.wav", __sample_16b_wav, __sample_16b_wav_len },
	{ "sample_16b.wav", __sample_16b_wav, __sample_16b_wav_len },
	{ "sample_24b_16_8_obsolete.wav", __sample_24b_16_8_obsolete_wav, __sample_24b_16_8_obsolete_wav_len },
	{ "sample_24b_packed.wav", __sample_24b_packed_wav, __sample_24b_packed_wav_len },
	{ "sample_24b_packed_20.wav", __sample_24b_packed_20_wav, __sample_24b_packed_20_wav_len },
	{ "sample_32b_24float_non_standard.wav", __sample_32b_24float_non_standard_wav, __sample_32b_24float_non_standard_wav_len },
	{ "sample_32b_normalized_float.wav", __sample_32b_normalized_float_wav, __sample_32b_normalized_float_wav_len },
	{ "sample_32b_pcm.wav", __sample_32b_pcm_wav, __sample_32b_pcm_wav_len },
};

static int dump(const char* filename, const unsigned char *data, unsigned len)
{
	FILE *fp;
	fp = fopen(filename, "wb");
	TRACE_ERR(fp == NULL, "Can't open for writing %s", filename)
	TRACE_ERR(len != fwrite(data, 1, len, fp), "Error writing data to %s", filename)
	fclose(fp);

	return 0;
}

static int test_readSmpl(const char* filename)
{
	sprintf(testPref, "%s(%s)", "test_readSmpl", filename);

	WavReader* wr = WR_open(filename);
	TRACE_ERR(NULL == wr,  "Can't open for reading %s", filename)
	TRACE_ERR(wr->channels != 2, "Wrong channels number: %d", wr->channels)
	TRACE_ERR(wr->samples_per_channel*wr->channels < COUNT_OF(pcm_data), "Wrong samples number: %u", wr->samples_per_channel)

	unsigned bytesTotal = wr->samples_per_channel*wr->block_align;
	unsigned char *data = new (std::nothrow) unsigned char [bytesTotal];
	TRACE_ERR(wr->samples_per_channel != WR_readRaw(wr, data, wr->samples_per_channel), "Error reading data: %s", filename)

	unsigned channels = wr->channels;
	unsigned bits_per_sample = wr->bits_per_sample;
	unsigned block_align = wr->block_align;

	for(unsigned i = 0; i < wr->samples_per_channel; i++) {
		for(unsigned ch = 0; ch < (unsigned)wr->channels; ch++) {
			int x, y, idx = i*wr->channels + ch;
			if(idx < COUNT_OF(pcm_data)) {
				x = pcm_data[idx];
			} else {
				x = 0; // audition inserts zeros to the end of file
			}
			const unsigned char *pcm = data + block_align*i + ch*block_align/channels;
			if(wr->format == WAVE_FORMAT_PCM) {
				if(bits_per_sample == 16) {
					assert(block_align == 2*channels);
					y = *((short*) pcm);
				} else if(bits_per_sample == 24 || bits_per_sample == 20) {
					y =          pcm[2]; // little endian
					y = (y<<8) | pcm[1];
					y = (y<<8) | pcm[0];
				} else if(bits_per_sample == 32) {
					y = *(int32_t*)pcm;
				} else {
					TRACE_ERR(true, "Unsupported bits_per_sample: %d", bits_per_sample)
				}
			} else if (wr->format == WAVE_FORMAT_IEEE_FLOAT) {
				if(bits_per_sample == 32) {
					float z = *(float*)pcm;
					y = int(z*((1UL<<31) - 1));
				} else {
					TRACE_ERR(true, "Unsupported bits_per_sample: %d", bits_per_sample)
				}
			} else if (wr->format == WAVE_FORMAT_FLOAT_AUDITION) {
				if(bits_per_sample == 24) { // 24.8 unscaled
					float z = *(float*)pcm;
					y = int(z);
				} else  if(bits_per_sample == 32) { // 16.8 obsolete
					float z = *(float*)pcm;
					y = int(z*(1UL<<16));
				} else {
					TRACE_ERR(true, "Unsupported bits_per_sample: %d", bits_per_sample)
				}
			} else {
				TRACE_ERR(true, "Unsupported PCM format: %d", wr->format)
			}
			unsigned sample_bytes = (bits_per_sample+7)/8;
			if(sample_bytes > 2) {
				y <<= 8*(4 - sample_bytes); // extend sign
				y >>= 8*(4 - sample_bytes);
				y >>= 8*(sample_bytes - 2); // ignore LSB due to test data
			}
			TRACE_ERR(x != y, "Wrong samples value at position %u: %d(orig) != %d(read)", i*channels + ch, x, y)
		}
	}
	delete [] data;

	WR_close(wr);

	printf("ok: %s\n", testPref);

	return 0;
}

typedef enum {
	SMPL_FMT_16,
	SMPL_FMT_24,
	SMPL_FMT_32,
	SMPL_FMT_FLOAT,
	SMPL_FMT_DOUBLE,
} SmplFmt;
static int test_readBuf(const char* filename, SmplFmt smpl_fmt, unsigned char *_data, unsigned *_samples_channel)
{
	sprintf(testPref, "%s(%s, %d)", "test_readBuf", filename, smpl_fmt);

	WavReader* wr = WR_open(filename);
	TRACE_ERR(NULL == wr,  "Can't open for reading %s", filename)

	TRACE_ERR(wr->channels != NUM_CHANNLES, "Wrong channels number: %u", wr->channels)
	TRACE_ERR(wr->sample_rate != SAMPLE_RATE, "Wrong sample rate: %u", wr->sample_rate)

	TRACE_ERR((wr->samples_per_channel-2)*wr->channels == COUNT_OF(pcm_data), "Wrong samples number: %u", wr->samples_per_channel)

	unsigned char *data = _data;
	signed nRead = 0;
	switch(smpl_fmt) {
		case SMPL_FMT_16:
			nRead = WR_readInt16(wr, (short*)data, wr->samples_per_channel);
			break;
		case SMPL_FMT_24:
			nRead = WR_readInt24p(wr, (unsigned char*)data, wr->samples_per_channel);
			break;	
		case SMPL_FMT_32:
			nRead = WR_readInt32(wr, (int32_t*)data, wr->samples_per_channel);
			break;
		case SMPL_FMT_FLOAT:
			nRead = WR_readFloat(wr, (float*)data, wr->samples_per_channel);
			break;
		case SMPL_FMT_DOUBLE:
			nRead = WR_readDouble(wr, (double*)data, wr->samples_per_channel);
			break;
	}
	TRACE_ERR(nRead != wr->samples_per_channel, "Error reading data: %s", filename)

	for(unsigned i = 0; i < wr->samples_per_channel; i++) {
		for(unsigned ch = 0; ch < wr->channels; ch++) {
			int x, y, idx = i*wr->channels + ch;
			if(idx < COUNT_OF(pcm_data)) {
				x = pcm_data[idx];
			} else {
				x = 0; // audition inserts zeros to the end of file
			}
			double z;
			float scale_to_i16;
			switch(smpl_fmt) {
				case SMPL_FMT_16:
					scale_to_i16 = 1;
					z = *(short*)data;
					data += 2;
					break;
				case SMPL_FMT_24:
					scale_to_i16 = 1.f/(1UL<<16);
					z = (((int32_t)data[0])<<8) + (((int32_t)data[1])<<16) + (((int32_t)data[2])<<24);
					data += 3;
					break;
				case SMPL_FMT_32:
					scale_to_i16 = 1.f/(1UL<<16);
					z = *(int32_t*)data;
					data += 4;
					break;
				case SMPL_FMT_FLOAT:
					scale_to_i16 = (1UL<<15);
					z = *(float*)data;
					data += sizeof(float);
					break;
				case SMPL_FMT_DOUBLE:
					scale_to_i16 = (1UL<<15);
					z = *(double*)data;
					data += sizeof(double);
					break;
			}
			y = (short)round(z*scale_to_i16);
			TRACE_ERR(x != y, "Wrong samples value at position %u: %d(orig) != %d(read)", i*wr->channels + ch, x, y)
		}
	}
	*_samples_channel = wr->samples_per_channel;
	WR_close(wr);
	printf("ok: %s\n", testPref);

	return 0;
}

static int test_writeBuf(const char* filename_ref, SmplFmt smpl_fmt, unsigned char *data_ref, unsigned samples_channel)
{
	sprintf(testPref, "%s(%s, %d)", "test_writeBuf", filename_ref, smpl_fmt);

	const char* filename = "tmp.wav";
	int bits_per_sample;
	switch(smpl_fmt) {
		case SMPL_FMT_16:
			bits_per_sample = 16;
			break;
		case SMPL_FMT_24:
			bits_per_sample = 24;
			break;
		case SMPL_FMT_32:
			bits_per_sample = 32;
			break;
		case SMPL_FMT_FLOAT:
			bits_per_sample = 32;
			break;
		case SMPL_FMT_DOUBLE: // skip
			bits_per_sample = 64;
			break;
	}
	WavWriter* ww = WW_open(filename, WAVE_FORMAT_PCM, NUM_CHANNLES, SAMPLE_RATE, bits_per_sample);
	TRACE_ERR(NULL == ww,  "Can't open for writing %s", filename)

	signed nWritten = 0;
	switch(smpl_fmt) {
		case SMPL_FMT_16:
			nWritten = WW_writeInt16(ww, (short*)data_ref, samples_channel);
			break;
		case SMPL_FMT_24:
			nWritten = WW_writeInt24(ww, (unsigned char *)data_ref, samples_channel);
			break;
		case SMPL_FMT_32:
			nWritten = WW_writeInt32(ww, (int32_t *)data_ref, samples_channel);
			break;
		case SMPL_FMT_FLOAT:
			nWritten = WW_writeFloat(ww, (float *)data_ref, samples_channel);
			break;
		case SMPL_FMT_DOUBLE: // skip
			return 1;
	}
	if(smpl_fmt != SMPL_FMT_DOUBLE) {
		TRACE_ERR(nWritten != samples_channel, "Error writing data: %s", filename)
	}
	WW_close(ww);

	WavReader* wr = WR_open(filename);
	TRACE_ERR(NULL == wr,  "Can't open for reading %s", filename)

	TRACE_ERR(wr->channels != NUM_CHANNLES, "Wrong channels number: %u", wr->channels)
	TRACE_ERR(wr->sample_rate != SAMPLE_RATE, "Wrong sample rate: %u", wr->sample_rate)
	TRACE_ERR(wr->samples_per_channel != samples_channel, "Wrong samples number: %u", wr->samples_per_channel)

	unsigned char *data = new (std::nothrow) unsigned char [samples_channel*wr->channels*sizeof(double)];
	signed nRead = 0;
	switch(smpl_fmt) {
		case SMPL_FMT_16:
			nRead = WR_readInt16(wr, (short*)data, samples_channel);
			break;
		case SMPL_FMT_24:
			nRead = WR_readInt24p(wr, (unsigned char*)data, samples_channel);
			break;	
		case SMPL_FMT_32:
			nRead = WR_readInt32(wr, (int32_t*)data, samples_channel);
			break;
		case SMPL_FMT_FLOAT:
			nRead = WR_readFloat(wr, (float*)data, samples_channel);
			break;
		case SMPL_FMT_DOUBLE:
			nRead = WR_readDouble(wr, (double*)data, samples_channel);
			break;
	}
	TRACE_ERR(nRead != samples_channel, "Error reading data: %s", filename)	

	TRACE_ERR(0 != memcmp(data_ref, data, samples_channel*wr->channels*(wr->bits_per_sample>>3)), \
		"Different written/read data")

	WR_close(wr);
	printf("ok: %s\n", testPref);

	delete [] data;

	return 0;
}

int main()
{
	for(unsigned i = 0; i < COUNT_OF(files); i++) {
		TRACE_ERR(0 != dump(files[i].name, files[i].data, files[i].len), "dump error")
	}

	for(unsigned i = 0; i < COUNT_OF(files); i++) {
		TRACE_ERR(0 != test_readSmpl(files[i].name), "Test failed")
	}

	unsigned char *data = new (std::nothrow) unsigned char [(COUNT_OF(pcm_data)+4)*sizeof(double)];
	for(unsigned j = 0; j < 5; j++) {
		SmplFmt smpl_fmt = (SmplFmt)j;
		for(unsigned i = 0; i < COUNT_OF(files); i++) {
			unsigned samples_channel;
			TRACE_ERR(0 != test_readBuf(files[i].name, smpl_fmt, data, &samples_channel), "Test failed")
			if(smpl_fmt == SMPL_FMT_DOUBLE) {
				continue;
			}
			TRACE_ERR(0 != test_writeBuf(files[i].name, smpl_fmt, data, samples_channel), "Test failed")
		}
	}
	delete [] data;

	return 0;
}
