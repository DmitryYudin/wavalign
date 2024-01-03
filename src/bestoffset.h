/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#pragma once

#define NUM_BEST 3

void bestOffset(float ssd[NUM_BEST],   // ... left  | ... right
                int offsets[NUM_BEST], //  negative | positive
                const double *left, const double *right, unsigned channels, unsigned ncorr, unsigned corr_len,
                const int initialOffset);
