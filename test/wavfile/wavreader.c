/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#include "wavreader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "pcm_conv.h"

#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

typedef struct {
	unsigned format;
	unsigned channels;
	uint32_t sample_rate;
	unsigned bits_per_sample;
	unsigned block_align;
    uint32_t samples_per_channel;

	uint32_t data_length;
	FILE *fp;
	unsigned char *buf; // internal buffer to perform data transformation into format requested
	unsigned bufSize;
} WR;
static int check_consistency()
{
	WavReader *wavReader = NULL;
	WR *wr = NULL;
	return \
		&wavReader->format == &wr->format && \
		&wavReader->channels == &wr->channels && \
		&wavReader->sample_rate == &wr->sample_rate && \
		&wavReader->bits_per_sample == &wr->bits_per_sample && \
		&wavReader->block_align == &wr->block_align && \
		&wavReader->samples_per_channel == &wr->samples_per_channel;
}

#define TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
static uint32_t read_tag32(WR* wr)
{
	uint32_t tag = 0;
	tag = (tag << 8) | fgetc(wr->fp);
	tag = (tag << 8) | fgetc(wr->fp);
	tag = (tag << 8) | fgetc(wr->fp);
	tag = (tag << 8) | fgetc(wr->fp);
	return tag;
}
static uint32_t read_int32(WR* wr)
{
	uint32_t value = 0;
	value |= fgetc(wr->fp) <<  0;
	value |= fgetc(wr->fp) <<  8;
	value |= fgetc(wr->fp) << 16;
	value |= fgetc(wr->fp) << 24;
	return value;
}
static uint16_t read_int16(WR* wr)
{
	uint16_t value = 0;
	value |= fgetc(wr->fp) << 0;
	value |= fgetc(wr->fp) << 8;
	return value;
}
static void skip(FILE *fp, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		fgetc(fp);
	}
}

WavReader* WR_open(const char *filename)
{
	if( !check_consistency() ) {
		return NULL;
	}

	WR* wr = (WR*)malloc(sizeof(*wr));
	memset(wr, 0, sizeof(*wr));

	wr->fp = fopen(filename, "rb");
	if (wr->fp == NULL) {
		goto exit;
	}

	uint32_t tag = read_tag32(wr);
	uint32_t len = read_int32(wr);
	if (tag != TAG('R', 'I', 'F', 'F') || len < 4) {
		goto exit;
	}
	tag = read_tag32(wr); len -= 4;
	if (tag != TAG('W', 'A', 'V', 'E')) {
		goto exit;
	}

	while (len >= 8) {
		uint32_t chunk = read_tag32(wr);
		uint32_t chunk_len = read_int32(wr);
		len -= 8;
		if (chunk_len > len) {
			break;
		}
		len -= chunk_len;
		if (chunk == TAG('f', 'm', 't', ' ')) {
			if (chunk_len < 16) { // Insufficient data for 'fmt '
				break;
			}
			wr->format          = read_int16(wr);
			wr->channels        = read_int16(wr);
			wr->sample_rate     = read_int32(wr);
			int	byte_rate       = read_int32(wr);
			wr->block_align     = read_int16(wr);
			wr->bits_per_sample = read_int16(wr);		chunk_len -= 16;
			if(wr->block_align == 0 || wr->bits_per_sample == 0) {
				break;
			}
			if (wr->format == WAVE_FORMAT_EXTENSIBLE) {
				if (chunk_len < 12) { // Insufficient data for waveformatex
					return NULL;
				}
				skip(wr->fp, 8);						chunk_len -= 8;
				wr->format = read_int32(wr);			chunk_len -= 4;
			} else if (chunk_len == 4) {
				int cbSize = read_int16(wr);			chunk_len -= 2;
				if (cbSize == 2) {
					unsigned audition = read_int16(wr); chunk_len -= 2;
					if( audition == 1 ) {
						wr->format = WAVE_FORMAT_FLOAT_AUDITION;
					}
				}
			}
			skip(wr->fp, chunk_len);

			if(wr->bits_per_sample == 24 && wr->block_align == 4*wr->channels) {
				wr->format = WAVE_FORMAT_FLOAT_AUDITION;
			}
			if( wr->format != WAVE_FORMAT_PCM && 
				wr->format != WAVE_FORMAT_IEEE_FLOAT && 
				wr->format != WAVE_FORMAT_FLOAT_AUDITION) {
				break;
			}
		} else if (chunk == TAG('d', 'a', 't', 'a')) {
			if (chunk_len) {
				wr->data_length = chunk_len;
			} else {
				long pos = ftell(wr->fp);
				if(0 != fseek(wr->fp, 0, SEEK_END)) {
					break;
				}
				wr->data_length = ftell(wr->fp) - pos;
				if(0 != fseek(wr->fp, pos, SEEK_SET)) {
					break;
				}
			}
			wr->samples_per_channel = wr->data_length/wr->block_align;
			wr->data_length = wr->samples_per_channel*wr->block_align;
			return (WavReader*)wr;
		} else {
			skip(wr->fp, chunk_len);
		}	
	}

exit:
	if (wr->fp) {
		fclose(wr->fp);
	}
	free(wr);
	return NULL;
}

void WR_close(WavReader* wavReader)
{
	WR* wr = (WR*)wavReader;
	fclose(wr->fp);
	if(wr->buf) {
		free(wr->buf);
	}
	free(wr);
}

int WR_readRaw(WavReader* wavReader, uint8_t* data, unsigned spc)
{
	WR* wr = (WR*)wavReader;
	if (spc > wr->data_length/wr->block_align) {
		spc = wr->data_length/wr->block_align;
	}
	unsigned n = (unsigned)fread(data, wr->block_align, spc, wr->fp);
	wr->data_length -= n*wr->block_align;
	return n;
}

typedef enum {
	SMPL_FMT_16,
	SMPL_FMT_32,
	SMPL_FMT_24,
	SMPL_FMT_FLOAT,
	SMPL_FMT_DOUBLE,
} SmplFmt;
static int read_internal(WavReader* wavReader, void* _data, unsigned spc, SmplFmt smpl_fmt)
{
	WR* wr = (WR*)wavReader;
	if(wr->bufSize < spc*wr->block_align) {
		unsigned n = spc*wr->block_align;
		void *buf_new = realloc(wr->buf, n);
		if(!buf_new) {
			return 0;
		}
		wr->buf = buf_new;
		wr->bufSize = n;
	}
	spc = WR_readRaw(wavReader, wr->buf, spc);
	unsigned sample_block = wr->block_align/wr->channels;

	unsigned char* data = (unsigned char*)_data;
	const unsigned char *pcm = wr->buf;
	int format = wr->format, bits_per_sample = wr->bits_per_sample, err_sticky = 0;
	for(unsigned i = 0; i < spc*wr->channels; i++) {
		if(smpl_fmt == SMPL_FMT_16 || smpl_fmt == SMPL_FMT_24 || smpl_fmt == SMPL_FMT_32) {
			int32_t x = pcmconv_to_int32(pcm, format, bits_per_sample, &err_sticky);
			if(smpl_fmt == SMPL_FMT_16) {
				*(short *)data = (short)(x>>16);
				data += 2;
			} else if(smpl_fmt == SMPL_FMT_24) {
				*data++ = (unsigned char)(x>>8);
				*data++ = (unsigned char)(x>>16);
				*data++ = (unsigned char)(x>>24);
			} else {
				*(int32_t *)data = x;
				data += 4;
			}
		} else if (smpl_fmt == SMPL_FMT_FLOAT || smpl_fmt == SMPL_FMT_DOUBLE) {
			double x = pcmconv_to_double(pcm, format, bits_per_sample, &err_sticky);
			if(smpl_fmt == SMPL_FMT_FLOAT) {
				*(float *)data = (float)x;
				data += sizeof(float);
			} else {
				*(double *)data = x;
				data += sizeof(double);
			}
		} else {
			return 0;
		}
		pcm += sample_block;
	}
	return err_sticky ? -1 : spc;
}

int WR_readInt16(WavReader* wavReader, int16_t* data, unsigned spc) {
	return read_internal(wavReader, data, spc, SMPL_FMT_16);
}
int WR_readInt24p(WavReader* wavReader, uint8_t* data, unsigned spc) {
	return read_internal(wavReader, data, spc, SMPL_FMT_24);
}
int WR_readInt32(WavReader* wavReader, int32_t* data, unsigned spc) {
	return read_internal(wavReader, data, spc, SMPL_FMT_32);
}
int WR_readFloat(WavReader* wavReader, float* data, unsigned spc) {
	return read_internal(wavReader, data, spc, SMPL_FMT_FLOAT);
}
int WR_readDouble(WavReader* wavReader, double* data, unsigned spc) {
	return read_internal(wavReader, data, spc, SMPL_FMT_DOUBLE);
}
