#include "wavwriter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "pcm_conv.h"

typedef struct {
	unsigned format;
	unsigned channels;
	uint32_t sample_rate;
	unsigned bits_per_sample;
	unsigned block_align;

	unsigned data_length;
	FILE *fp;
	unsigned char *buf; // internal buffer to perform data transformation from user to file format
	unsigned bufSize;
} WW;
static int check_consistency()
{
	WavWriter *wavWriter = NULL;
	WW *ww = NULL;
	return \
		&wavWriter->format == &ww->format && \
		&wavWriter->channels == &ww->channels && \
		&wavWriter->sample_rate == &ww->sample_rate && \
		&wavWriter->bits_per_sample == &ww->bits_per_sample && \
		&wavWriter->block_align == &ww->block_align;
}

static void write_tag32(WW* ww, const char *str)
{
	fputc(str[0], ww->fp);
	fputc(str[1], ww->fp);
	fputc(str[2], ww->fp);
	fputc(str[3], ww->fp);
}
static void write_int32(WW* ww, int value)
{
	fputc(value >>  0, ww->fp);
	fputc(value >>  8, ww->fp);
	fputc(value >> 16, ww->fp);
	fputc(value >> 24, ww->fp);
}
static void write_int16(WW* ww, int value)
{
	fputc(value >> 0, ww->fp);
	fputc(value >> 8, ww->fp);
}

static void write_header(WW* ww, unsigned length)
{
	write_tag32(ww, "RIFF");
	write_int32(ww, 4 + 8 + 16 + 8 + length);
	write_tag32(ww, "WAVE");

	write_tag32(ww, "fmt ");
	write_int32(ww, 16); // chunk_len
	write_int16(ww, ww->format);
	write_int16(ww, ww->channels);
	write_int32(ww, ww->sample_rate);
	write_int32(ww, ww->block_align*ww->sample_rate); // Bytes/sec
	write_int16(ww, ww->block_align);
	write_int16(ww, ww->bits_per_sample);

	write_tag32(ww, "data");
	write_int32(ww, length); // chunk_len
}

WavWriter* WW_open(const char* filename, unsigned format, unsigned channels, uint32_t sample_rate, unsigned bits_per_sample)
{
	if( !check_consistency() ) {
		return NULL;
	}
	if (channels == 0 || sample_rate == 0 || bits_per_sample == 0) {
		return NULL;
	}
	int format_ok;
	switch (format) {
		case WAVE_FORMAT_PCM:
			format_ok = bits_per_sample == 32 || bits_per_sample == 24 || bits_per_sample == 16;
			break;
		case WAVE_FORMAT_IEEE_FLOAT:
			format_ok = bits_per_sample == 32;
			break;
		case WAVE_FORMAT_FLOAT_AUDITION:
			format_ok = bits_per_sample == 32 || bits_per_sample == 24;
			break;
		default:
			format_ok = 0;
			break;
	}
	if(!format_ok) {
		return NULL;
	}

	WW* ww = (WW*) malloc(sizeof(*ww));
	memset(ww, 0, sizeof(*ww));

	ww->fp = fopen(filename, "wb");
	if (ww->fp == NULL) {
		free(ww);
		return NULL;
	}
	ww->data_length = 0;

	ww->format = format;
	ww->channels = channels;
	ww->sample_rate = sample_rate;
	ww->bits_per_sample = bits_per_sample;
	ww->block_align = ((bits_per_sample+7)>>3)*channels;

	write_header(ww, ww->data_length);
	return (WavWriter*)ww;
}

void WW_close(WavWriter* wavWriter)
{
	WW* ww = (WW*) wavWriter;
	if (ww->fp) {
		fseek(ww->fp, 0, SEEK_SET);
		write_header(ww, ww->data_length);
		fclose(ww->fp);
	}
	if(ww->buf) {
		free(ww->buf);
	}
	free(ww);
}

int WW_writeRaw(WavWriter* wavWriter, const unsigned char* data, unsigned spc)
{
	WW* ww = (WW*) wavWriter;
	unsigned n = (unsigned)fwrite(data, ww->block_align, spc, ww->fp);
	ww->data_length += n*ww->block_align;
	return n;
}

typedef enum {
	SMPL_FMT_16,
	SMPL_FMT_24,
	SMPL_FMT_32,
	SMPL_FMT_FLOAT,
	SMPL_FMT_DOUBLE,
} SmplFmt;
static int write_internal(WavWriter* wavWriter, const void* data, unsigned spc, int format, int bits_per_sample)
{
	WW* ww = (WW*) wavWriter;

	unsigned n = spc*ww->block_align;
	if(ww->bufSize < n) {
		if(ww->buf) {
			free(ww->buf);
		}
		ww->buf = malloc(n);
		if(!ww->buf) {
			return 0;
		}
		ww->bufSize = n;
	}

	const unsigned char *pcm = (const unsigned char *)data;
	unsigned char *output = ww->buf;
	int err_sticky = 0;
	for(unsigned i = 0; i < spc*ww->channels; i++) {
		if(ww->format == WAVE_FORMAT_PCM) {
			int32_t x = pcmconv_to_int32(pcm, format, bits_per_sample, &err_sticky);
			if(ww->bits_per_sample == 16) {
				*(short *)output = (short)(x>>16);
				output += 2;
			} else if(ww->bits_per_sample == 24) {
				*output++ = (unsigned char)(x>>8);
				*output++ = (unsigned char)(x>>16);
				*output++ = (unsigned char)(x>>24);
			} else {
				*(int32_t *)output = x;
				output += 4;
			}
		} else {
			double x = pcmconv_to_double(pcm, format, bits_per_sample, &err_sticky);
			*(float *)output = (float)x;
			output += sizeof(float);
		}
		pcm += (bits_per_sample+7)>>3;
	}

	if(err_sticky) {
		return -1;
	}
	return WW_writeRaw(wavWriter, ww->buf, (unsigned)(output - ww->buf)/ww->block_align);
}

int WW_writeInt16(WavWriter* wavWriter, const int16_t* data, unsigned spc) {
	return write_internal(wavWriter, data, spc, WAVE_FORMAT_PCM, 16);
}
int WW_writeInt24(WavWriter* wavWriter, const uint8_t* data, unsigned spc) {
	return write_internal(wavWriter, data, spc, WAVE_FORMAT_PCM, 24);
}
int WW_writeInt32(WavWriter* wavWriter, const int32_t* data, unsigned spc) {
	return write_internal(wavWriter, data, spc, WAVE_FORMAT_PCM, 32);
}
int WW_writeFloat(WavWriter* wavWriter, const float* data, unsigned spc) {
	return write_internal(wavWriter, data, spc, WAVE_FORMAT_IEEE_FLOAT, 32);
}
