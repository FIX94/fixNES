/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "audio_vrc7.h"
#include "audio.h"
#include "mem.h"
#include "cpu.h"

//used externally
bool vrc7enabled = false;
int32_t vrc7Out = 0;

//rainwarrior's VRC7 patches
static uint8_t vrc7instrument[16][8] = 
{
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 
	{ 0x03, 0x21, 0x05, 0x06, 0xB8, 0x82, 0x42, 0x27 }, 
	{ 0x13, 0x41, 0x13, 0x0D, 0xD8, 0xD6, 0x23, 0x12 }, 
	{ 0x31, 0x11, 0x08, 0x08, 0xFA, 0x9A, 0x22, 0x02 }, 
	{ 0x31, 0x61, 0x18, 0x07, 0x78, 0x64, 0x30, 0x27 }, 
	{ 0x22, 0x21, 0x1E, 0x06, 0xF0, 0x76, 0x08, 0x28 }, 
	{ 0x02, 0x01, 0x06, 0x00, 0xF0, 0xF2, 0x03, 0xF5 }, 
	{ 0x21, 0x61, 0x1D, 0x07, 0x82, 0x81, 0x16, 0x07 }, 
	{ 0x23, 0x21, 0x1A, 0x17, 0xCF, 0x72, 0x25, 0x17 }, 
	{ 0x15, 0x11, 0x25, 0x00, 0x4F, 0x71, 0x00, 0x11 }, 
	{ 0x85, 0x01, 0x12, 0x0F, 0x99, 0xA2, 0x40, 0x02 }, 
	{ 0x07, 0xC1, 0x69, 0x07, 0xF3, 0xF5, 0xA7, 0x12 }, 
	{ 0x71, 0x23, 0x0D, 0x06, 0x66, 0x75, 0x23, 0x16 }, 
	{ 0x01, 0x02, 0xD3, 0x05, 0xA3, 0x92, 0xF7, 0x52 }, 
	{ 0x61, 0x63, 0x0C, 0x00, 0x94, 0xAF, 0x34, 0x06 }, 
	{ 0x21, 0x62, 0x0D, 0x00, 0xB1, 0xA0, 0x54, 0x17 }, 
};

static const uint8_t vrc7multi[16] = 
{
	1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
};

typedef struct _vrc7slot_t {
	int32_t rate;
	uint32_t phase;
	int32_t out;
	int32_t out2;
	int32_t fbOut;
	uint32_t levelAtten;
	uint32_t kslAtten;
	uint32_t envPhase;
	int32_t state;
	uint32_t attackRate;
	uint32_t decayRate;
	uint32_t sustainRate;
	uint32_t releaseRate;
	uint32_t sustainLevel;
} vrc7slot_t;

typedef struct _vrc7chan_t {
	vrc7slot_t mod;
	vrc7slot_t carry;
	bool enabled;
	bool s;
	uint16_t freq;
	uint8_t block;
	uint8_t instrument;
	uint8_t v;
} vrc7chan_t;

enum
{
	vrc7StateIdle = 0,
	vrc7StateAttack,
	vrc7StateDecay,
	vrc7StateSustain,
	vrc7StateRelease
};

static const uint32_t vrc7MaxAtten = (1<<23);
static const double attenDb = (1<<23) / 48.0;

static inline int32_t vrc7FromDb(double in)
{
	return (int32_t)(in * attenDb);
}

// LUTs and a lot of code based on this code from Disch:
// http://codepad.org/aAQjWXwJ
static uint32_t vrc7AttackLut[256];
static uint32_t vrc7AmLut[256];
static double vrc7FmLut[256];
static uint32_t vrc7SinLut[1024];
static int32_t vrc7LinearLut[65536];
static int32_t vrc7KslLut[0x10];

static vrc7chan_t vrc7channel[6];

static uint32_t vrc7amPhase;
static uint32_t vrc7amOut;
static uint32_t vrc7fmPhase;
static double vrc7fmOut;

#define M_PI 3.14159265358979323846
#define M_2PI 6.28318530717958647692

void vrc7AudioInit()
{
	vrc7amPhase = 0; vrc7amOut = 0; vrc7fmPhase = 0; vrc7fmOut = 0;
	vrc7enabled = true;
	memset(vrc7channel, 0, sizeof(vrc7chan_t)*6);

	// Half Sine lut
	uint32_t i;
	for(i = 0; i < 1024; ++i)
	{
		double sinx = sin(M_PI * i / 1024);
		if(!sinx)
			vrc7SinLut[i] = vrc7MaxAtten;
		else
		{
			sinx = -20.0 * log10(sinx) * attenDb;
			if(sinx > vrc7MaxAtten)
				vrc7SinLut[i] = vrc7MaxAtten;
			else
				vrc7SinLut[i] = (uint32_t)(sinx);
		}
	}

	// Ksl Lut
	vrc7KslLut[0] = vrc7FromDb(0), vrc7KslLut[1] = vrc7FromDb(18), vrc7KslLut[2] = vrc7FromDb(24), vrc7KslLut[3] = vrc7FromDb(27.75),
	vrc7KslLut[4] = vrc7FromDb(30), vrc7KslLut[5] = vrc7FromDb(32.25), vrc7KslLut[6] = vrc7FromDb(33.75), vrc7KslLut[7] = vrc7FromDb(35.25),
	vrc7KslLut[8] = vrc7FromDb(36), vrc7KslLut[9] = vrc7FromDb(37.5), vrc7KslLut[10] = vrc7FromDb(38.25), vrc7KslLut[11] = vrc7FromDb(39),
	vrc7KslLut[12] = vrc7FromDb(39.75), vrc7KslLut[13] = vrc7FromDb(40.5), vrc7KslLut[14] = vrc7FromDb(41.25), vrc7KslLut[15] = vrc7FromDb(42);

	// Attack Lut
	for(i = 0; i < 256; ++i)
	{
		double baselog = log((double)(vrc7MaxAtten >> 15));
		vrc7AttackLut[i] = vrc7FromDb(48) - (uint32_t)(vrc7FromDb(48) * log((double)(i)) / baselog);
	}

	// Linear Lut
	for(i = 0; i < 65536; ++i)
	{
		int32_t outscale = (1 << 20);  // channel output is 20 bits wide
		double inscale = attenDb / (1 << 7);
		double lin = pow(10.0, (i / -20.0 / inscale));
		vrc7LinearLut[i] = (int32_t)(lin * outscale);
	}

	// Am Lut
	for(i = 0; i < 256; ++i)
		vrc7AmLut[i] = (uint32_t)((1.0 + sin(M_2PI * i / 256)) * vrc7FromDb(0.6));

	// Fm Lut
	for(i = 0; i < 256; ++i)
		vrc7FmLut[i] = pow(2.0, 13.75 / 1200 * sin(M_2PI * i / 256));
}

typedef struct _vrc7Ksr_t
{
	int32_t K;
	int32_t RL;
	int32_t RH;
} vrc7Ksr_t;

static vrc7Ksr_t vrc7Ksr;

void vrc7SetR(vrc7Ksr_t *k, int32_t r)
{
	r = (r * 4) + k->K;
	k->RH = r>>2;
	if(k->RH > 15)
		k->RH = 15;
	k->RL = r&3;
}

static void vrc7CalcSlotVals(vrc7chan_t *chan, vrc7slot_t *slot, uint8_t slotNum)
{
	// Phase Change Rate
	slot->rate = chan->freq * (1 << chan->block) * vrc7multi[(vrc7instrument[chan->instrument][slotNum]&0xF)] / 2;

	// Total level
	slot->levelAtten = slotNum ? (chan->v << 2) : (vrc7instrument[chan->instrument][2] & 0x3F);
	slot->levelAtten = slot->levelAtten * vrc7FromDb(0.75);

	// Sustain level
	slot->sustainLevel = vrc7FromDb(3) * (vrc7instrument[chan->instrument][6 | slotNum] >> 4);

	// Ksl
	uint8_t kslbits = vrc7instrument[chan->instrument][2 | slotNum] >> 6;
	if(kslbits)
	{
		int32_t a = vrc7KslLut[chan->freq >> 5] - vrc7FromDb(6)*(7 - chan->block);
		if(a <= 0)
			slot->kslAtten = 0;
		else
			slot->kslAtten = a >> (3-kslbits);
	}
	else
		slot->kslAtten = 0;

	vrc7Ksr.K = (chan->block << 1) | (chan->freq >> 8);
	if(!(vrc7instrument[chan->instrument][slotNum] & 0x10)) // if Ksr is "off"
		vrc7Ksr.K >>= 2;

	// bits as written to reg
	slot->attackRate = vrc7instrument[chan->instrument][4 | slotNum] >> 4;
	slot->decayRate = vrc7instrument[chan->instrument][4 | slotNum] & 0x0F;
	slot->sustainRate = vrc7instrument[chan->instrument][6 | slotNum] & 0x0F;
	if(chan->s)
		slot->releaseRate = 5;
	else if(vrc7instrument[chan->instrument][0 | slotNum] & 0x20)
		slot->releaseRate = slot->sustainRate;
	else
		slot->releaseRate = 7;

	if(vrc7instrument[chan->instrument][0 | slotNum] & 0x20)
		slot->sustainRate = 0;

	// convert
	if(slot->attackRate)
	{
		vrc7SetR(&vrc7Ksr, slot->attackRate);
		slot->attackRate = (12 * (vrc7Ksr.RL+4)) << vrc7Ksr.RH;
	}
	if(slot->decayRate)
	{
		vrc7SetR(&vrc7Ksr, slot->decayRate);
		slot->decayRate = (vrc7Ksr.RL+4) << (vrc7Ksr.RH-1);
	}
	if(slot->sustainRate)
	{
		vrc7SetR(&vrc7Ksr, slot->sustainRate);
		slot->sustainRate = (vrc7Ksr.RL+4) << (vrc7Ksr.RH-1);
	}
	if(slot->releaseRate)
	{
		vrc7SetR(&vrc7Ksr, slot->releaseRate);
		slot->releaseRate = (vrc7Ksr.RL+4) << (vrc7Ksr.RH-1);
	}
}

static void vrc7KeyOn(vrc7slot_t *slot)
{
	slot->phase = 0;
	slot->envPhase = 0;
	slot->state = vrc7StateAttack;
}

static void vrc7KeyOff(vrc7slot_t *slot)
{
	if(slot->state == vrc7StateAttack)
		slot->envPhase = vrc7AttackLut[(slot->envPhase >> 15) & 0xFF];
	slot->state = vrc7StateRelease;
}

static void vrc7UpdateEnable(vrc7chan_t *chan, vrc7slot_t *slot)
{
	if(chan->enabled)
	{
		if((slot->state == vrc7StateIdle) || (slot->state == vrc7StateRelease))
			vrc7KeyOn(slot);
	}
	else
	{
		if((slot->state != vrc7StateIdle) && (slot->state != vrc7StateRelease))
			vrc7KeyOff(slot);
	}
}

static uint32_t vrc7Env(vrc7chan_t *chan, vrc7slot_t *slot, uint8_t slotNum)
{
	uint32_t out = 0;

	switch(slot->state)
	{
		case vrc7StateAttack:
			out = vrc7AttackLut[(slot->envPhase >> 15) & 0xFF];
			slot->envPhase += slot->attackRate;
			if(slot->envPhase >= vrc7MaxAtten)
			{
				slot->envPhase = 0;
				slot->state = vrc7StateDecay;
			}
			break;
		case vrc7StateDecay:
			out = slot->envPhase;
			slot->envPhase += slot->decayRate;
			if(slot->envPhase >= slot->sustainLevel)
			{
				slot->envPhase = slot->sustainLevel;
				slot->state = vrc7StateSustain;
			}
			break;
		case vrc7StateSustain:
			out = slot->envPhase;
			slot->envPhase += slot->sustainRate;
			if(slot->envPhase >= vrc7MaxAtten)
				slot->state = vrc7StateIdle;
			break;
		case vrc7StateRelease:
			out = slot->envPhase;
			slot->envPhase += slot->releaseRate;
			if(slot->envPhase >= vrc7MaxAtten)
				slot->state = vrc7StateIdle;
			break;
	}

	if(vrc7instrument[chan->instrument][slotNum] & 0x80)
		out += vrc7amOut;

	return out + slot->levelAtten + slot->kslAtten;
}

static int32_t vrc7GetOut(vrc7chan_t *chan, vrc7slot_t *slot, uint8_t slotNum, uint32_t inV)
{
	if(slot->state == vrc7StateIdle)
		return 0;

	slot->out2 = slot->out;
	//mod base
	if(slotNum == 0)
	{
		uint8_t fb = vrc7instrument[chan->instrument][3] & 7;
		if(fb) inV = (slot->fbOut) >> (8-fb);
	}

	uint32_t env = vrc7Env(chan, slot, slotNum);
	if((vrc7instrument[chan->instrument][slotNum]&0x40) != 0)
		slot->phase += (uint32_t)(slot->rate / 2 * vrc7fmOut);
	else
		slot->phase += slot->rate / 2;
	inV += slot->phase;
	env += vrc7SinLut[(inV>>7)&0x3FF];

	if(env >= vrc7MaxAtten)
		return 0;

	int32_t output = vrc7LinearLut[env>>7];
	int32_t Rectify = (vrc7instrument[chan->instrument][3] & (0x8 << slotNum)) ? 0 : -1;
	if(inV & (1<<17))
		output *= Rectify;
	slot->out = output;
	slot->fbOut = (slot->out + slot->out2) / 2;
	return slot->fbOut;
}

void vrc7AudioCycle()
{
	vrc7Out = 0;
	//update am and fm for all chans
	vrc7amPhase += 78;
	vrc7amOut = vrc7AmLut[(vrc7amPhase >> 12) & 0xFF];
	vrc7fmPhase += 105;
	vrc7fmOut = vrc7FmLut[(vrc7fmPhase >> 12) & 0xFF];
	//go through all chans and get final out
	uint8_t i;
	for(i = 0; i < 6; i++)
	{
		vrc7chan_t *c = &vrc7channel[i];
		uint32_t modOut = vrc7GetOut(c, &c->mod, 0, 0);
		int32_t carryOut = vrc7GetOut(c, &c->carry, 1, modOut);
		vrc7Out += carryOut;
	}
}

void vrc7AudioSet8(uint8_t addr, uint8_t val)
{
	if(addr < 8)
		vrc7instrument[0][addr] = val;
	else if(addr >= 0x10 && addr <= 0x15)
	{
		vrc7chan_t *c = &vrc7channel[addr&0xF];
		c->freq &= ~0xFF;
		c->freq |= val;
		vrc7CalcSlotVals(c, &c->mod, 0);
		vrc7CalcSlotVals(c, &c->carry, 1);
	}
	else if(addr >= 0x20 && addr <= 0x25)
	{
		vrc7chan_t *c = &vrc7channel[addr&0xF];
		c->freq &= 0xFF;
		c->freq |= (val&1)<<8;
		c->block = (val>>1)&7;
		c->enabled = ((val&0x10) != 0);
		c->s = ((val&0x20) != 0);
		vrc7UpdateEnable(c, &c->mod);
		vrc7UpdateEnable(c, &c->carry);
		vrc7CalcSlotVals(c, &c->mod, 0);
		vrc7CalcSlotVals(c, &c->carry, 1);
	}
	else if(addr >= 0x30 && addr <= 0x35)
	{
		vrc7chan_t *c = &vrc7channel[addr&0xF];
		c->v = (val&0xF);
		c->instrument = (val>>4)&0xF;
		vrc7CalcSlotVals(c, &c->mod, 0);
		vrc7CalcSlotVals(c, &c->carry, 1);
	}
}
