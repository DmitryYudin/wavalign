/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#include "bestoffset.h"
#include "ssd.h"
#include <memory>
#include <queue>

#define SCOPE_ARRAY(type, name, len) std::unique_ptr<type[]> name##_buf(new type[len]); auto name=name##_buf.get();

void bestOffset(
	float ssd[NUM_BEST],
	int offsets[NUM_BEST],
	const double *left,
	const double *right,
	unsigned channels,
	unsigned ncorr,
	unsigned corr_len,
	const int initialOffset
)
{
#if 0 // direct
	SCOPE_ARRAY(double, ssd2, ncorr)
	SCOPE_ARRAY(double, ssd3, ncorr)
	#define POW2(x) ((x)*(x))
	for(unsigned i = 0; i < ncorr; i++) {
		ssd2[i] = ssd3[i] = 0;
		for(unsigned j = 0; j < corr_len*channels; j++) {
			ssd2[i] += POW2(left[i*channels+j] - right[j]);
			ssd3[i] += POW2(left[j] - right[i*channels+j]);
		}
	}
#endif
	SCOPE_ARRAY(double, ssd0, ncorr)
	SCOPE_ARRAY(double, ssd1, ncorr)
	const double* in[2] = {left, right};
	double *ssd_[2] = {ssd0, ssd1};
	ssd_x2(ssd_, in, channels, ncorr, corr_len);

	std::priority_queue<std::pair<double, unsigned>> q;
	q.push(std::pair<double, unsigned>(-ssd1[0], 0));
	for (signed i = 1; i < (signed)ncorr; i++) {
		q.push(std::pair<double, unsigned>(-ssd1[i],  i)); // min -> max

		// ignore negative offsets from reference
		if(i > initialOffset) {
			continue;
		}
		q.push(std::pair<double, unsigned>(-ssd0[i], -i)); // negative index
	}
	for (unsigned i = 0; i < NUM_BEST; i++) {
		ssd[i] = (float)-q.top().first;
		offsets[i] = q.top().second;
		q.pop();
	}
	for(auto i = 0; i < NUM_BEST; i++) {
		offsets[i] += initialOffset;
	}
}
