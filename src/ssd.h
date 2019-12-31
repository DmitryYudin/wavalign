/*
 * Copyright © 2019 Dmitry Yudin. All rights reserved.
 * Licensed under the Apache License, Version 2.0
 */

#pragma once

void ssd_x2(
	double *out[2],			//  ncorr
	const double *in[2],	//	ncorr + corr_len
	unsigned channels,
	unsigned ncorr, 
	unsigned corr_len
);
bool test_ssd_x2();
