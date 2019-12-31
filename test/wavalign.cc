/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <getopt.h>
#include <memory>
#include <algorithm>
#include <assert.h>

#include "xcorr.h"
#include "ssd.h"
#include "bestoffset.h"
#include <wavreader.h>
#include <wavwriter.h>

#define DEFAUL_CORRLEN_MS 3000 
#define DEFAUL_NUMCORR_MS  500
static void usage(void)
{
    printf(
"Insert zeros or remove samples for the beginning of the file under test to\n"
"align it (minimize Sum of Square Difference) with the reference file.\n"
"\n"
"Usage:\n"
"  wavalign [options] ref.wav tst.wav\n"
"  wavalign [options] ref.wav tst.wav out.wav\n"
"  wavalign [options] ref.wav tst.wav -o out.wav\n"
"\n"
"Options:\n"
"  -h, --help       Print this help.\n"
"  -l, --length M   SSD interval length in samples [default: %dms].\n"
"  -n, --offset N   Max offset in samples [default: %dms].\n"
"  -q, --quiet      Only print best offset value.\n"
"  --back N         Max backward offset (number of zeros to insert) [default: 1].\n"
"                   As a rule, to achieve the correct alignment, one need to drop\n"
"                   the beginning of tst.wav. Usually, the negative offset\n"
"                   indicates error.\n"
"                   The negative value of N allows unlimited backward offset.\n"
"  -o file          Output file name.\n"
"\n"
"Options to control output file format:\n"
"  -f, --format X   0 - same as ref.wav.\n"
"                   1 - same as tst.wav (default).\n"
"  --bps N          Integer with 16, 24 or 32 bits per sample.\n"
"  --float          Float, 32 bits per samples.\n"
"\n"
"Note, setting a search interval far beyond the length of the SSD increases the\n"
"likelihood of finding a minimum SSD that does not align files.\n"
"\n"
, DEFAUL_CORRLEN_MS, DEFAUL_NUMCORR_MS
	);
}

#ifdef NDEBUG
#define TRACE_ERR(cond, fmt, ...) if (cond) { fprintf(stderr, "error:" fmt "\n", ##__VA_ARGS__); return 1; }
#else
#define TRACE_ERR(cond, fmt, ...) if (cond) { fprintf(stderr, __FILE__ "(%u): error:" fmt "\n", __LINE__, ##__VA_ARGS__); return 1; }
#endif
#define SCOPE_ARRAY(type, name, len) std::unique_ptr<type[]> name##_buf(new type[len]); auto name=name##_buf.get();

static void readInput(WavReader* wr, double *pcmBuf, unsigned spcRequired, unsigned& numZeros, unsigned& numLow, unsigned& numSamples)
{
	numZeros = numLow = numSamples = 0;
	while(true) {
		unsigned spcRead = WR_readDouble(wr, pcmBuf, spcRequired);
		if (spcRead == 0) {
			return;
		}
		unsigned i = 0;
		for(; i < spcRead*wr->channels; i++) {
			if(pcmBuf[i] != 0) {
				break;
			}
		}
		i /= wr->channels; // integer div
		numZeros += i;
		if (i != spcRead) {
			numSamples = spcRead - i;
			memmove(pcmBuf, pcmBuf + i*wr->channels, numSamples*wr->channels*sizeof(*pcmBuf));
			break;
		}
	}
	if(numSamples < spcRequired) {
		unsigned spcRead = WR_readDouble(wr, pcmBuf + numSamples*wr->channels, spcRequired - numSamples);
		numSamples += spcRead;
	}
	#define MA_LEN 128
	if(numSamples < MA_LEN*wr->channels) {
		return;
	}

	// only search within [0, spcRequired) region
	{
		#define POW2(x) ((x)*(x))
		double en=0, ma;
		for(unsigned i = 0; i < numSamples*wr->channels; i++) {
			en += POW2(pcmBuf[i]);
			if(i == MA_LEN*wr->channels) {
				ma = en;
			}
		}
		en /= numSamples;
		en *= MA_LEN;
		unsigned i = MA_LEN*wr->channels;
		for(; i < numSamples*wr->channels; i++) {
			if(ma*POW2(16) > en) {
				break;
			}
			ma += POW2(pcmBuf[i]) - POW2(pcmBuf[i - MA_LEN*wr->channels]);
		}
		numLow = (i - MA_LEN)/wr->channels;
		numSamples -= numLow;
		memmove(pcmBuf, pcmBuf + numLow*wr->channels, numSamples*wr->channels*sizeof(*pcmBuf));
	}
	if(numSamples < spcRequired) {
		unsigned spcRead = WR_readDouble(wr, pcmBuf + numSamples*wr->channels, spcRequired - numSamples);
		numSamples += spcRead;
	}
}

static int writeOutput(int offset, const char *namein, const char *nameout, int format, unsigned bps)
{
	WavReader* wr = WR_open(namein);
	TRACE_ERR(!wr, "can't open for reading: %s", namein)

	WavWriter* ww = WW_open(nameout, format, wr->channels, wr->sample_rate, bps);
	TRACE_ERR(!ww, "can't open for writing: %s", nameout)

	if(offset != 0) {
		SCOPE_ARRAY(short, buf, wr->channels*(offset > 0 ? offset : -offset))
		if(offset < 0) {
			offset = -offset;
			memset(buf, 0, sizeof(short)*wr->channels*offset);
			WW_writeInt16(ww, reinterpret_cast<short*>(buf), offset);
		} else {
			int n = WR_readInt16(wr, buf, offset);
			TRACE_ERR(n != offset, "can't remove %d samples from %s", offset, namein)
		}
	} 
	unsigned len=8192;
	SCOPE_ARRAY(float, buf, wr->channels*len);
	while(true) {
		int n = WR_readFloat(wr, buf, len);
		if (n == 0) {
			break;
		}
		TRACE_ERR(n < 0, "reading from %s", namein)
		int m = WW_writeFloat(ww, buf, n);
		TRACE_ERR(m != n, "%d samples of %d written to %s", m, n, namein)
	}
	WW_close(ww);
	WR_close(wr);
	return 0;
}

#define MIN_CORRLEN 1024
#define MIN_NUMCORR 1024
int main(int argc, char *argv[])
{
#ifndef NDEBUG
	test_xcorr_x2();
	test_ssd_x2();
#endif
	if (argc <= 1) {
		usage();
		return 1;
	}
    static const struct option long_options[] = {
        { "help",		no_argument,		0, 'h' },
		{ "length",		required_argument,	0, 'l' },
		{ "offset",		required_argument,	0, 'n' },
		{ "quiet",		required_argument,	0, 'q' },
		{ "format",		required_argument,	0, 'f' },
		{ "bps",		required_argument,	0, 'Z'+1 },
		{ "float",		no_argument,		0, 'Z'+2 },
		{ "back",		required_argument,	0, 'Z'+3 },
		{ 0,            0,                  0,  0  },
    };
	int ch, corrlen=0, numcorr=0, format_id=1, quiet=0, backward_max=1;
	const char *wavname[2] = {NULL, }, *outname = NULL;
    while ((ch = getopt_long(argc, argv, "hl:o:n:f:b:q", long_options, 0)) != EOF) {
        switch (ch) {
			case 'h':
				return usage(), 0;
			case 'l':
				if (sscanf(optarg, "%u", &corrlen) != 1 || corrlen < MIN_CORRLEN) {
					TRACE_ERR(1, "invalid arg for '-l' option: %s. Must be integer greate or equal to %d", optarg, MIN_CORRLEN)
				}
				break;
			case 'n':
				if (sscanf(optarg, "%u", &numcorr) != 1 || numcorr < MIN_NUMCORR) {
					TRACE_ERR(1, "invalid arg for '-n' option: %s. Must be integer greate or equal to %d", optarg, MIN_NUMCORR)
				}
				break;
			case 'q':
				quiet = 1;
				break;
			case 'f':
				if (sscanf(optarg, "%u", &format_id) != 1 || (format_id != 0 && format_id != 1)) {
					TRACE_ERR(1, "invalid arg for '-f' option: %s", optarg)
				}
				break;
			case 'Z'+1: {
				int bps=0;
				if (sscanf(optarg, "%u", &bps) != 1 || (bps != 16 && bps != 24 && bps != 32)) {
					TRACE_ERR(1, "invalid arg for '-b' option: %s", optarg)
				}
				format_id = bps>>3; // 2,3,4
				break;
			}
			case 'o':
				outname = optarg;
				break;
			case 'Z'+2:
				format_id = 5;
				break;
			case 'Z'+3:
				backward_max = 1;
				if (sscanf(optarg, "%u", &backward_max) != 1) {
					TRACE_ERR(1, "invalid arg for '--back' option: %s", optarg)
				}
				backward_max = std::max(backward_max, -1);
				break;
			default:
				usage();
				return 1;
        }
    }
	if(optind < argc && wavname[0] == NULL) {
		wavname[0] = argv[optind++];
	}
	if(optind < argc && wavname[1] == NULL) {
		wavname[1] = argv[optind++];
	}
	if(optind < argc && outname == NULL) {
		outname = argv[optind++];
	}
    if (argc != optind) {
        return usage(), 1;
	}

	TRACE_ERR(wavname[0] == NULL, "reference file name required")
	TRACE_ERR(wavname[1] == NULL, "test file name required")
	unsigned format[2], channels[2], bits_per_sample[2];
	std::unique_ptr<double[]> pcmBuf[2] = {nullptr, };
	int bias;
	{
		WavReader *wrs[2] = {NULL, };
		for(auto i = 0; i < 2; i++) {
			wrs[i] = WR_open(wavname[i]);
			TRACE_ERR(!wrs[i], "can't open for reading: %s", wavname[i])
			format[i] = wrs[i]->format;
			channels[i] = wrs[i]->channels;
			bits_per_sample[i] = wrs[i]->bits_per_sample;
		}
		TRACE_ERR(wrs[0]->channels != wrs[1]->channels, "different channels number %d vs. %d", wrs[0]->channels, wrs[1]->channels)
		TRACE_ERR(wrs[0]->sample_rate != wrs[1]->sample_rate, "different sampling rate %u vs. %u", wrs[0]->sample_rate, wrs[1]->sample_rate)
		if(corrlen == 0) {
			corrlen = wrs[0]->sample_rate*DEFAUL_CORRLEN_MS/1000;
		}
		if(numcorr == 0) {
			numcorr = wrs[0]->sample_rate*DEFAUL_NUMCORR_MS/1000;
		}
		for(auto i = 0; i < 2; i++) {
			pcmBuf[i].reset(new double [wrs[0]->channels*(numcorr + corrlen)]);
		}
		unsigned numZeros[2] = {0, }, numLow[2] = {0, }, numSamples[2] = {0, }, spcAvail = (unsigned)-1;
		for(auto i = 0; i < 2; i++) {
			readInput(wrs[i], pcmBuf[i].get(), numcorr+corrlen, numZeros[i], numLow[i], numSamples[i]);
			TRACE_ERR(numSamples[i] < MIN_NUMCORR + MIN_CORRLEN, "%d zeros removed, not enough samples (%d) to align: %s", 
				numZeros[i], numSamples[i], wavname[i])
			spcAvail=std::min(spcAvail, numSamples[i]);
			WR_close(wrs[i]);
		}
		if (spcAvail < unsigned(numcorr + corrlen)) {
			float r = float(corrlen)/(numcorr + corrlen);
			corrlen = (unsigned)(r*spcAvail);
			corrlen = std::max(corrlen, MIN_CORRLEN);
			numcorr = spcAvail - corrlen;	
		}
		if (!quiet) {
			printf("Samples ignored from reference: %d (%d zeros + %d low energy)\n", numZeros[0] + numLow[0], numZeros[0], numLow[0]);
			printf("Samples ignored from test:      %d (%d zeros + %d low energy)\n", numZeros[1] + numLow[1], numZeros[1], numLow[1]);
			printf("Max offset: %d\n", numcorr);
			printf("SSD length: %d\n", corrlen);
		}
		bias = numZeros[1] + numLow[1] - numZeros[0] - numLow[0];
	}

	float ssd[NUM_BEST];
	int offsets[NUM_BEST];
	bestOffset(ssd, offsets, pcmBuf[0].get(), pcmBuf[1].get(), channels[0], numcorr, corrlen, bias);
	const int offset = offsets[0];
	if (!quiet) {
		for(unsigned i = 0; i < NUM_BEST; i++) {
			printf("offset=%d ssd=%f\n", offsets[i], ssd[i]);
		}
	} else {
		printf("%d\n", offset);
	}
	if (backward_max >= 0 && offset < 0 && offset < -backward_max) {
		fprintf(stderr, "too long backward offset %d, max value is %d\n", -offset, backward_max);
		return 1;
	}
	if(!outname) {
		return 0;
	}

	switch (format_id) {
		case 0:
		case 1:
			format[1] = format[format_id];
			bits_per_sample[1] = bits_per_sample[format_id];
			break;
		case 2:
		case 3:
		case 4:
			format[1] = WAVE_FORMAT_PCM;
			bits_per_sample[1] = format_id<<3;
			break;
		case 5:
			format[1] = WAVE_FORMAT_IEEE_FLOAT;
			bits_per_sample[1] = 32;
			break;
		default:
			return 1;
	}

	if( 0 != writeOutput(offset, wavname[1], outname, format[1], bits_per_sample[1]) ) {
		return 1;
	}
	return 0;
}
