/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _apu_h_
#define _apu_h_

#define NUM_BUFFERS 8

void apuInitBufs();
void apuDeinitBufs();
void apuInit();
int apuCycle();
void apuClockTimers();
uint8_t *apuGetBuf();
uint32_t apuGetBufSize();
uint32_t apuGetFrequency();
void apuSet8(uint8_t reg, uint8_t val);
uint8_t apuGet8(uint8_t reg);
void apuLenCycle();


typedef struct _envelope_t {
	bool start;
	bool constant;
	bool loop;
	uint8_t vol;
	//uint8_t envelope;
	uint8_t divider;
	uint8_t decay;
} envelope_t;

void doEnvelopeLogic(envelope_t *env);

#endif
