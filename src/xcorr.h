/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#pragma once

void xcorr_x2(kiss_fft_scalar *out[2], // ncorr
              const double *in[2],     // ncorr + corr_len
              unsigned ncorr, unsigned corr_len);

bool test_xcorr_x2();
