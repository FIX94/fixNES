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
#include "apu.h"
#include "audio_mmc5.h"
#include "audio.h"
#include "mem.h"
#include "cpu.h"

//used externally
bool mmc5enabled = false;
uint8_t mmc5Out = 0;
uint8_t mmc5pcm = 0;
bool mmc5_dmcreadmode = false;

#define P1_ENABLE (1<<0)
#define P2_ENABLE (1<<1)

#define PULSE_CONST_V (1<<4)
#define PULSE_HALT_LOOP (1<<5)

#define DMC_READ_MODE (1<<0)
#define DMC_IRQ_ENABLE (1<<7)

static uint8_t MMC5_IO_Reg[0x18];

static uint16_t mmc5_freq1;
static uint16_t mmc5_freq2;
static uint8_t mmc5_p1LengthCtr, mmc5_p2LengthCtr;
static uint8_t mmc5_modePos = 0;
static uint16_t mmc5_modeCurCtr = 0;
static uint16_t mmc5_p1freqCtr, mmc5_p2freqCtr;
static uint8_t mmc5_p1Cycle, mmc5_p2Cycle;
static bool mmc5_p1haltloop, mmc5_p2haltloop;
static bool mmc5_dmcirqenable;

static envelope_t mmc5_p1Env, mmc5_p2Env;

//from apu.c
extern const uint8_t lengthLookupTbl[0x20];
extern const uint8_t pulseSeqs[4][8];
extern const uint16_t *dmcPeriod;
extern const uint16_t *mode4Ctr;

static const uint8_t *mmc5_p1seq = pulseSeqs[0], 
					*mmc5_p2seq = pulseSeqs[1];
extern bool mmc5_dmc_interrupt;

void mmc5AudioInit()
{
	memset(MMC5_IO_Reg,0,0x18);

	mmc5_freq1 = 0; mmc5_freq2 = 0;
	mmc5_p1LengthCtr = 0; mmc5_p2LengthCtr = 0;
	mmc5_p1freqCtr = 0; mmc5_p2freqCtr = 0;
	mmc5_p1Cycle = 0; mmc5_p2Cycle = 0;

	memset(&mmc5_p1Env,0,sizeof(envelope_t));
	memset(&mmc5_p2Env,0,sizeof(envelope_t));

	mmc5_p1haltloop = false; mmc5_p2haltloop = false;
	mmc5_dmcirqenable = false;
	mmc5_dmcreadmode = false;
	mmc5enabled = true;
}

void mmc5AudioClockTimers()
{
	if(mmc5_p1freqCtr)
		mmc5_p1freqCtr--;
	if(mmc5_p1freqCtr == 0)
	{
		mmc5_p1freqCtr = (mmc5_freq1+1)*2;
		mmc5_p1Cycle++;
		if(mmc5_p1Cycle >= 8)
			mmc5_p1Cycle = 0;
	}

	if(mmc5_p2freqCtr)
		mmc5_p2freqCtr--;
	if(mmc5_p2freqCtr == 0)
	{
		mmc5_p2freqCtr = (mmc5_freq2+1)*2;
		mmc5_p2Cycle++;
		if(mmc5_p2Cycle >= 8)
			mmc5_p2Cycle = 0;
	}
}

void mmc5AudioPCMWrite(uint8_t val)
{
	if(val == 0 && mmc5_dmcirqenable)
		mmc5_dmc_interrupt = true;
	mmc5pcm = val;
}

static uint8_t mmc5_p1Out = 0, mmc5_p2Out = 0;

void mmc5AudioCycle()
{
	if(mmc5_p1LengthCtr && (MMC5_IO_Reg[0x15] & P1_ENABLE))
	{
		if(mmc5_freq1 >= 8 && mmc5_freq1 < 0x7FF)
			mmc5_p1Out = mmc5_p1seq[mmc5_p1Cycle] ? (mmc5_p1Env.constant ? mmc5_p1Env.vol : mmc5_p1Env.decay) : 0;
	}
	if(mmc5_p2LengthCtr && (MMC5_IO_Reg[0x15] & P2_ENABLE))
	{
		if(mmc5_freq2 >= 8 && mmc5_freq2 < 0x7FF)
			mmc5_p2Out = mmc5_p2seq[mmc5_p2Cycle] ? (mmc5_p2Env.constant ? mmc5_p2Env.vol : mmc5_p2Env.decay) : 0;
	}
	mmc5Out = mmc5_p1Out + mmc5_p2Out;
}

void mmc5AudioLenCycle()
{
	if(mmc5_modeCurCtr)
		mmc5_modeCurCtr--;
	if(mmc5_modeCurCtr == 0)
	{
		if(mmc5_p1LengthCtr)
		{
			doEnvelopeLogic(&mmc5_p1Env);
			if(!mmc5_p1haltloop)
				mmc5_p1LengthCtr--;
		}
		if(mmc5_p2LengthCtr)
		{
			doEnvelopeLogic(&mmc5_p2Env);
			if(!mmc5_p2haltloop)
				mmc5_p2LengthCtr--;
		}
		mmc5_modePos++;
		if(mmc5_modePos >= 4)
			mmc5_modePos = 0;
		mmc5_modeCurCtr = mode4Ctr[mmc5_modePos];
	}
}

void mmc5AudioSet8(uint8_t reg, uint8_t val)
{
	//printf("%02x %02x %04x\n", reg, val, cpuGetPc());
	MMC5_IO_Reg[reg] = val;
	if(reg == 0)
	{
		mmc5_p1Env.vol = val&0xF;
		mmc5_p1seq = pulseSeqs[val>>6];
		mmc5_p1Env.constant = ((val&PULSE_CONST_V) != 0);
		mmc5_p1Env.loop = mmc5_p1haltloop = ((val&PULSE_HALT_LOOP) != 0);
	}
	else if(reg == 2)
	{
		//printf("P1 time low %02x\n", val);
		mmc5_freq1 = ((mmc5_freq1&~0xFF) | val);
	}
	else if(reg == 3)
	{
		mmc5_p1Cycle = 0;
		if(MMC5_IO_Reg[0x15] & P1_ENABLE)
			mmc5_p1LengthCtr = lengthLookupTbl[val>>3];
		mmc5_freq1 = (mmc5_freq1&0xFF) | ((val&7)<<8);
		//printf("P1 new freq %04x\n", mmc5_freq2);
		mmc5_p1Env.start = true;
	}
	else if(reg == 4)
	{
		mmc5_p2Env.vol = val&0xF;
		mmc5_p2seq = pulseSeqs[val>>6];
		mmc5_p2Env.constant = ((val&PULSE_CONST_V) != 0);
		mmc5_p2Env.loop = mmc5_p2haltloop = ((val&PULSE_HALT_LOOP) != 0);
	}
	else if(reg == 6)
	{
		//printf("P2 time low %02x\n", val);
		mmc5_freq2 = ((mmc5_freq2&~0xFF) | val);
	}
	else if(reg == 7)
	{
		mmc5_p2Cycle = 0;
		if(MMC5_IO_Reg[0x15] & P2_ENABLE)
			mmc5_p2LengthCtr = lengthLookupTbl[val>>3];
		mmc5_freq2 = (mmc5_freq2&0xFF) | ((val&7)<<8);
		//printf("P2 new freq %04x\n", mmc5_freq2);
		mmc5_p2Env.start = true;
	}
	else if(reg == 0x10)
	{
		mmc5_dmcreadmode = (val&DMC_READ_MODE)!=0;
		mmc5_dmcirqenable = (val&DMC_IRQ_ENABLE)!=0;
		if(!mmc5_dmcirqenable)
			mmc5_dmc_interrupt = false;
	}
	else if(reg == 0x11)
	{
		if(!mmc5_dmcreadmode)
			mmc5AudioPCMWrite(val);
	}
	else if(reg == 0x15)
	{
		//printf("Set 0x15 %02x\n",val);
		if(!(val & P1_ENABLE))
			mmc5_p1LengthCtr = 0;
		if(!(val & P2_ENABLE))
			mmc5_p2LengthCtr = 0;
	}
}

uint8_t mmc5AudioGet8(uint8_t reg)
{
	//printf("%08x\n", reg);
	if(reg == 0x10)
	{
		uint8_t intrflag = (mmc5_dmc_interrupt<<7);
		//printf("Get 0x10 %02x\n",intrflag);
		mmc5_dmc_interrupt = false;
		return intrflag;
	}
	else if(reg == 0x15)
		return (mmc5_p1LengthCtr > 0) | ((mmc5_p2LengthCtr > 0)<<1);
	return MMC5_IO_Reg[reg];
}
