/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#include "xcorr.h"

#include <_kiss_fft_guts.h>
#include <kiss_fftr.h>

#include <cassert>
#include <memory>

#define SCOPE_ARRAY(type, name, len) \
    std::unique_ptr<type[]> name##_buf(new type[len]); \
    auto name = name##_buf.get();

void xcorr_x2(kiss_fft_scalar *out[2], // ncorr
              const double *in[2],     // ncorr + corr_len
              unsigned ncorr, unsigned corr_len)
{
    unsigned fftr_size = 4;
    while (ncorr + corr_len > fftr_size) {
        fftr_size <<= 1;
    }
    kiss_fftr_cfg fftr_cfg_fwd = kiss_fftr_alloc(fftr_size, 0, NULL, NULL);
    kiss_fftr_cfg fftr_cfg_inv = kiss_fftr_alloc(fftr_size, 1, NULL, NULL);
    SCOPE_ARRAY(kiss_fft_scalar, x, fftr_size)
    SCOPE_ARRAY(kiss_fft_scalar, y, fftr_size)
    SCOPE_ARRAY(kiss_fft_scalar, z, fftr_size)

    unsigned freq_len = fftr_size / 2 + 1;
    SCOPE_ARRAY(kiss_fft_cpx, X, freq_len)
    SCOPE_ARRAY(kiss_fft_cpx, Y, freq_len)
    SCOPE_ARRAY(kiss_fft_cpx, Z, freq_len)
    for (auto i = 0; i < 2; i++) {
        const double *left = in[i], *right = in[(i + 1) & 0x1];
        for (unsigned j = 0; j < corr_len + ncorr; j++) {
            x[j] = (kiss_fft_scalar)left[j];
        }
        for (unsigned j = 0; j < corr_len; j++) {
            y[j] = (kiss_fft_scalar)right[j];
        }
        memset(x + corr_len + ncorr, 0, sizeof(kiss_fft_scalar) * (fftr_size - corr_len - ncorr));
        memset(y + corr_len, 0, sizeof(kiss_fft_scalar) * (fftr_size - corr_len));
        kiss_fftr(fftr_cfg_fwd, x, X);
        kiss_fftr(fftr_cfg_fwd, y, Y);
        const float fac = 1.f / (fftr_size / 2);
        for (unsigned j = 0; j < freq_len; j++) {
            Y[j].i = -Y[j].i;
            C_MUL(Z[j], X[j], Y[j]);
            C_MULBYSCALAR(Z[j], fac); // scale to 2*(x,y)
        }
        kiss_fftri(fftr_cfg_inv, Z, z); // xcorr(A,B)[k]=sum A[i+k]B[i]
        memcpy(out[i], z, sizeof(kiss_fft_scalar) * ncorr);
    }
    free(fftr_cfg_fwd);
    free(fftr_cfg_inv);
}

bool test_xcorr_x2()
{
    const unsigned ncorr = 10, corr_len = 6;
    SCOPE_ARRAY(double, x, ncorr + corr_len)
    SCOPE_ARRAY(double, y, ncorr + corr_len)
    for (unsigned i = 0; i < ncorr + corr_len; i++) {
        x[i] = i + 1;
        y[i] = i * i + 1;
    }
    SCOPE_ARRAY(kiss_fft_scalar, xcorr0, ncorr)
    SCOPE_ARRAY(kiss_fft_scalar, xcorr1, ncorr)
    SCOPE_ARRAY(double, xcorr2, ncorr)
    SCOPE_ARRAY(double, xcorr3, ncorr)
    kiss_fft_scalar *out[2] = {xcorr0, xcorr1};
    const double *in[2] = {x, y};
    xcorr_x2(out, in, ncorr, corr_len);

    for (unsigned i = 0; i < ncorr; i++) {
        xcorr2[i] = xcorr3[i] = 0;
        for (unsigned j = 0; j < corr_len; j++) {
            xcorr2[i] += 2 * x[i + j] * y[j];
            xcorr3[i] += 2 * y[i + j] * x[j];
        }
    }

    bool success = true;
    for (unsigned i = 0; i < ncorr; i++) {
        success &= fabs(xcorr0[i] - xcorr2[i]) < 1;
        success &= fabs(xcorr1[i] - xcorr3[i]) < 1;
        assert(success);
    }
    return success;
}
