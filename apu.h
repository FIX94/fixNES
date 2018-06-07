/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _apu_h_
#define _apu_h_

#define NUM_BUFFERS 4

#define EXP_VRC6 (1<<0)
#define EXP_VRC7 (1<<1)
#define EXP_FDS (1<<2)
#define EXP_MMC5 (1<<3)
#define EXP_N163 (1<<4)
#define EXP_S5B (1<<5)

void apuInitBufs();
void apuDeinitBufs();
void apuInit();
void apuCycle();
void apuWriteDMCBuf(uint8_t val);
uint8_t *apuGetBuf();
uint32_t apuGetBufSize();
uint32_t apuGetFrequency();
void apuSet8(uint8_t reg, uint8_t val);
uint8_t apuGet8(uint8_t reg);
bool apuUpdate();

typedef struct _envelope_t {
	bool start;
	bool constant;
	bool loop;
	uint8_t vol;
	//uint8_t envelope;
	uint8_t divider;
	uint8_t decay;
} envelope_t;

typedef struct _sweep_t {
	bool enabled;
	bool start;
	bool negative;
	bool mute;
	bool chan1;
	uint8_t period;
	uint8_t divider;
	uint8_t shift;
} sweep_t;

void doEnvelopeLogic(envelope_t *env);

#endif
