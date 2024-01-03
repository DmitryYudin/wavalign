/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#include "ssd.h"
#include "xcorr.h"

#include <cassert>
#include <memory>

#define SCOPE_ARRAY(type, name, len) \
    std::unique_ptr<type[]> name##_buf(new type[len]); \
    auto name = name##_buf.get();

void ssd_x2(double *out[2],      // ncorr
            const double *in[2], // ncorr + corr_len
            unsigned channels, unsigned ncorr, unsigned corr_len)
{
    SCOPE_ARRAY(kiss_fft_scalar, xcorr0, ncorr * channels)
    SCOPE_ARRAY(kiss_fft_scalar, xcorr1, ncorr * channels)
    kiss_fft_scalar *xcorr[2] = {xcorr0, xcorr1};
    xcorr_x2(xcorr, in, ncorr * channels, corr_len * channels);

#define POW2(x) ((x) * (x))
    const double *x0 = in[0], *x1 = in[1];
    double en0 = 0, en1 = 0;
    for (unsigned i = 0; i < corr_len * channels; i++) {
        en0 += POW2(x0[i]);
        en1 += POW2(x1[i]);
    }

    double *ssd0 = out[0], *ssd1 = out[1];
    double EN0 = en0, EN1 = en1;
    for (unsigned i = 0; i < ncorr; i++) {
        ssd0[i] = en0 + EN1 - xcorr0[i * channels];
        ssd1[i] = EN0 + en1 - xcorr1[i * channels];
        for (unsigned j = 0; j < channels; j++) {
            en0 += POW2(x0[(i + corr_len) * channels + j]) - POW2(x0[i * channels + j]);
            en1 += POW2(x1[(i + corr_len) * channels + j]) - POW2(x1[i * channels + j]);
        }
    }
}

bool test_ssd_x2()
{
    unsigned ncorr = 10, corr_len = 6;
    SCOPE_ARRAY(double, x, ncorr + corr_len)
    SCOPE_ARRAY(double, y, ncorr + corr_len)
    for (unsigned i = 0; i < ncorr + corr_len; i++) {
        x[i] = i + 1;
        y[i] = i * i + 1;
    }
    SCOPE_ARRAY(double, ssd0, ncorr)
    SCOPE_ARRAY(double, ssd1, ncorr)
    SCOPE_ARRAY(double, ssd2, ncorr)
    SCOPE_ARRAY(double, ssd3, ncorr)
    double *out[2] = {ssd0, ssd1};
    const double *in[2] = {x, y};
    ssd_x2(out, in, 1, ncorr, corr_len);

    for (unsigned i = 0; i < ncorr; i++) {
        ssd2[i] = ssd3[i] = 0;
        for (unsigned j = 0; j < corr_len; j++) {
            ssd2[i] += POW2(x[i + j] - y[j]);
            ssd3[i] += POW2(y[i + j] - x[j]);
        }
    }

    bool success = true;
    for (unsigned i = 0; i < ncorr; i++) {
        success &= fabs(ssd0[i] - ssd2[i]) < 1;
        success &= fabs(ssd1[i] - ssd3[i]) < 1;
        assert(success);
    }
    return success;
}
