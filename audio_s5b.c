/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include "audio_s5b.h"
#include "audio.h"
#include "mem.h"
#include "cpu.h"

static uint16_t s5BVolTbl[32];
//16 shapes, 2x32 steps
static uint8_t s5BEnvShapeTbl[16][64];
//repeat values for all 16 shapes
static const bool s5BEnvRepeatTbl[16] = { 
	false, false, false, false, false, false, false, false,
	true,  false, true,  false, true,  false, true,  false,
};

//used externally
bool s5Benabled = false;
uint16_t s5BOut = 0;

static uint8_t s5BCurReg;

static uint16_t s5Bfreq[3];
static uint16_t s5BfreqCtr[3];
static uint16_t s5BVol[3];
static uint8_t s5BCycle[3];
static bool s5Bdisable[3];
static bool s5Bnonoise[3];
static bool s5BEnv[3];

static uint8_t s5BCtr;

static uint16_t s5BnoiseFreq;
static uint16_t s5BnoiseFreqCtr;
static uint32_t s5BnoiseReg;

static uint32_t s5BEnvFreq;
static uint32_t s5BEnvFreqCtr;
static uint8_t s5BEnvStep;
static uint8_t s5BEnvShape;
static uint16_t s5BEnvVol;
static bool s5BEnvRepeat;

void s5BAudioInit()
{
	s5Benabled = true;
	s5BOut = 0;
	s5BCurReg = 0xE;

	uint8_t i;
	for(i = 0; i < 3; i++)
	{
		s5Bfreq[i] = 0;
		s5BfreqCtr[i] = 0;
		s5BVol[i] = 0;
		s5BCycle[i] = 0;
		s5Bdisable[i] = true;
		s5Bnonoise[i] = true;
		s5BEnv[i] = false;
	}

	s5BCtr = 0;

	s5BnoiseFreq = 0;
	s5BnoiseFreqCtr = 0;
	s5BnoiseReg = 1;

	s5BEnvFreq = 0;
	s5BEnvFreqCtr = 0;
	s5BEnvStep = 0;
	s5BEnvShape = 0;
	s5BEnvVol = 0;
	s5BEnvRepeat = false;

	//printf("s5B Audio Inited!\n");
	//build volume LUT
	s5BVolTbl[0] = 0;
	//set max channel volume to 10922 (1/3rd of 32768)
	double conv = 10922.0 / pow(10.0, (1.5*31.0) / 20.0);
	for(i = 1; i < 32; i++)
		s5BVolTbl[i] = pow(10.0, (1.5*i) / 20.0) * conv;
	//build envelope LUT
	for(i = 0; i < 32; i++)
	{
		//envelope shapes step 1
		s5BEnvShapeTbl[0x0][i] = s5BEnvShapeTbl[0x1][i] = 
		s5BEnvShapeTbl[0x2][i] = s5BEnvShapeTbl[0x3][i] = 
		s5BEnvShapeTbl[0x8][i] = s5BEnvShapeTbl[0x9][i] = 
		s5BEnvShapeTbl[0xA][i] = s5BEnvShapeTbl[0xB][i] = 31-i;
		s5BEnvShapeTbl[0x4][i] = s5BEnvShapeTbl[0x5][i] = 
		s5BEnvShapeTbl[0x6][i] = s5BEnvShapeTbl[0x7][i] = 
		s5BEnvShapeTbl[0xC][i] = s5BEnvShapeTbl[0xD][i] = 
		s5BEnvShapeTbl[0xE][i] = s5BEnvShapeTbl[0xF][i] = i;
		//envelope shapes step 2
		s5BEnvShapeTbl[0x0][i+32] = s5BEnvShapeTbl[0x1][i+32] = 
		s5BEnvShapeTbl[0x2][i+32] = s5BEnvShapeTbl[0x3][i+32] = 
		s5BEnvShapeTbl[0x4][i+32] = s5BEnvShapeTbl[0x5][i+32] = 
		s5BEnvShapeTbl[0x6][i+32] = s5BEnvShapeTbl[0x7][i+32] =
		s5BEnvShapeTbl[0x9][i+32] = s5BEnvShapeTbl[0xF][i+32] = 0;
		s5BEnvShapeTbl[0xB][i+32] = s5BEnvShapeTbl[0xD][i+32] = 31;
		s5BEnvShapeTbl[0x8][i+32] = s5BEnvShapeTbl[0xE][i+32] = 31-i;
		s5BEnvShapeTbl[0xA][i+32] = s5BEnvShapeTbl[0xC][i+32] = i;
	}
}

void s5BAudioClockTimers()
{
	//every 16 cpu ticks this will be updated
	if((s5BCtr&0xF) == 0)
	{
		uint8_t i;
		for(i = 0; i < 3; i++)
		{
			if(s5BfreqCtr[i])
				s5BfreqCtr[i]--;
			if(s5BfreqCtr[i] == 0)
			{
				s5BfreqCtr[i] = s5Bfreq[i];
				s5BCycle[i]++;
			}
		}
		if(s5BnoiseFreqCtr)
			s5BnoiseFreqCtr--;
		if(s5BnoiseFreqCtr == 0)
		{
			s5BnoiseFreqCtr = s5BnoiseFreq;
			if(s5BnoiseReg&1) s5BnoiseReg ^= 0x24000;
			s5BnoiseReg >>= 1;
		}
		if(s5BEnvFreqCtr)
			s5BEnvFreqCtr--;
		if(s5BEnvFreqCtr == 0)
		{
			s5BEnvFreqCtr = s5BEnvFreq;
			if(s5BEnvStep >= 63)
			{
				if(s5BEnvRepeat)
					s5BEnvStep = 0;
			}
			else
				s5BEnvStep++;
			s5BEnvVol = s5BVolTbl[s5BEnvShapeTbl[s5BEnvShape][s5BEnvStep]];
		}
	}
	s5BCtr++;
}

static inline void _s5BsetChanFreqLow(uint8_t chan, uint8_t val)
{
	s5Bfreq[chan] = ((s5Bfreq[chan]&0xF00) | val);
}

static inline void _s5BsetChanFreqHigh(uint8_t chan, uint8_t val)
{
	s5Bfreq[chan] = (s5Bfreq[chan]&0xFF) | ((val&0xF)<<8);
}

static inline void _s5BsetChanVolEnv(uint8_t chan, uint8_t val)
{
	s5BVol[chan] = (val&0xF) ? (s5BVolTbl[((val&0xF)<<1)+1]) : 0;
	s5BEnv[chan] = (val&0x10);
}

void s5BAudioSet8(uint16_t addr, uint8_t val)
{
	//printf("s5BAudioSet8 %04x %02x\n", addr, val);
	if(addr < 0xE000)
		s5BCurReg = val&0xF;
	else
	{
		switch(s5BCurReg)
		{
			case 0x0:
				_s5BsetChanFreqLow(0, val);
				break;
			case 0x1:
				_s5BsetChanFreqHigh(0, val);
				break;
			case 0x2:
				_s5BsetChanFreqLow(1, val);
				break;
			case 0x3:
				_s5BsetChanFreqHigh(1, val);
				break;
			case 0x4:
				_s5BsetChanFreqLow(2, val);
				break;
			case 0x5:
				_s5BsetChanFreqHigh(2, val);
				break;
			case 0x6:
				s5BnoiseFreq = (val&0x1F)<<1;
				break;
			case 0x7:
				s5Bdisable[0] = (val&1);
				s5Bdisable[1] = (val&2);
				s5Bdisable[2] = (val&4);
				s5Bnonoise[0] = (val&0x08);
				s5Bnonoise[1] = (val&0x10);
				s5Bnonoise[2] = (val&0x20);
				break;
			case 0x8:
				_s5BsetChanVolEnv(0, val);
				break;
			case 0x9:
				_s5BsetChanVolEnv(1, val);
				break;
			case 0xA:
				_s5BsetChanVolEnv(2, val);
				break;
			case 0xB:
				s5BEnvFreq = (s5BEnvFreq&0xFF00) | val;
				break;
			case 0xC:
				s5BEnvFreq = (s5BEnvFreq&0xFF) | (val<<8);
				break;
			case 0xD:
				s5BEnvStep = 0;
				s5BEnvShape = (val&0xF);
				s5BEnvRepeat = s5BEnvRepeatTbl[s5BEnvShape];
				break;
			default:
				break;
		}
	}
}

void s5BAudioCycle()
{
	s5BOut = 0;
	uint8_t i;
	for(i = 0; i < 3; i++)
	{
		if((s5Bdisable[i] || (s5Bfreq[i] && ((s5BCycle[i])&1))) && (s5Bnonoise[i] || (s5BnoiseFreq && (s5BnoiseReg&1))))
			s5BOut += s5BEnv[i] ? s5BEnvVol : s5BVol[i];
	}
}
