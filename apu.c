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
#include "apu.h"
#include "audio_fds.h"
#include "audio_mmc5.h"
#include "audio_vrc6.h"
#include "audio.h"
#include "mem.h"
#include "cpu.h"

#define P1_ENABLE (1<<0)
#define P2_ENABLE (1<<1)
#define TRI_ENABLE (1<<2)
#define NOISE_ENABLE (1<<3)
#define DMC_ENABLE (1<<4)

#define PULSE_CONST_V (1<<4)
#define PULSE_HALT_LOOP (1<<5)

#define TRI_HALT_LOOP (1<<7)

#define DMC_HALT_LOOP (1<<6)
#define DMC_IRQ_ENABLE (1<<7)

static uint8_t APU_IO_Reg[0x18];

static float lpVal;
static float hpVal;
static float *apuOutBuf;
static uint32_t apuBufSize;
static uint32_t apuBufSizeBytes;
static uint32_t curBufPos;
static uint32_t apuFrequency;
static uint16_t freq1;
static uint16_t freq2;
static uint16_t triFreq;
static uint16_t noiseFreq;
static uint16_t noiseShiftReg;
static uint16_t dmcFreq;
static uint16_t dmcAddr, dmcLen, dmcSampleBuf;
static uint16_t dmcCurAddr, dmcCurLen;
static uint8_t p1LengthCtr, p2LengthCtr, noiseLengthCtr;
static uint8_t triLengthCtr, triLinearCtr, triCurLinearCtr;
static uint8_t dmcVol, dmcCurVol;
static uint8_t dmcSampleRemain;
static bool mode5 = false;
static uint8_t modePos = 0;
static uint16_t modeCurCtr = 0;
static uint16_t p1freqCtr, p2freqCtr, triFreqCtr, noiseFreqCtr, dmcFreqCtr;
static uint8_t p1Cycle, p2Cycle, triCycle;
static bool p1haltloop, p2haltloop, trihaltloop, noisehaltloop, dmchaltloop;
static bool dmcstart;
static bool dmcirqenable;
static bool trireload;
static bool noiseMode1;
static bool apu_enable_irq;

static envelope_t p1Env, p2Env, noiseEnv;

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

static sweep_t p1Sweep, p2Sweep;

static float pulseLookupTbl[32];
static float tndLookupTbl[204];

//used externally
const uint8_t lengthLookupTbl[0x20] = {
	10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

//used externally
const uint8_t pulseSeqs[4][8] = {
	{ 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 1, 1, 0, 0, 0 },
	{ 1, 0, 0, 1, 1, 1, 1, 1 },
};

static const uint8_t triSeq[32] = {
	15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
};

static const uint16_t noisePeriodNtsc[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

static const uint16_t noisePeriodPal[16] = {
	4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778,
};

static const uint16_t dmcPeriodNtsc[16] = {
	428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54,
};

static const uint16_t dmcPeriodPal[16] = {
	398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50,
};

static const uint16_t mode4CtrNtsc[4] = {
	7456, 7458, 7458, 7458
};

static const uint16_t mode4CtrPal[4] = {
	8314, 8312, 8314, 8314
};

static const uint16_t mode5CtrNtsc[5] = {
	7458, 7456, 7458, 7458, 7452
};

static const uint16_t mode5CtrPal[5] = {
	8314, 8314, 8312, 8314, 8312
};

//used externally
const uint16_t *dmcPeriod, *noisePeriod;
const uint16_t *mode4Ctr, *mode5Ctr;

static const uint8_t *p1seq = pulseSeqs[0], 
					*p2seq = pulseSeqs[1];
extern bool dmc_interrupt;

#define M_2_PI 6.28318530717958647692

extern bool nesPAL;
void apuInitBufs()
{
	noisePeriod = nesPAL ? noisePeriodPal : noisePeriodNtsc;
	dmcPeriod = nesPAL ? dmcPeriodPal : dmcPeriodNtsc;
	mode4Ctr = nesPAL ? mode4CtrPal : mode4CtrNtsc;
	mode5Ctr = nesPAL ? mode5CtrPal : mode5CtrNtsc;
	//effective frequencies for 50.000Hz and 60.000Hz Video out
	apuFrequency = nesPAL ? 831187 : 893415;
	double dt = 1.0/((double)apuFrequency);
	//LP at 22kHz
	double rc = 1.0/(M_2_PI * 22000.0);
	lpVal = dt / (rc + dt);
	//HP at 40Hz
	rc = 1.0/(M_2_PI * 40.0);
	hpVal = rc / (rc + dt);

	apuBufSize = apuFrequency/60;
	apuBufSizeBytes = apuBufSize*sizeof(float);

	apuOutBuf = (float*)malloc(apuBufSizeBytes);

	/* https://wiki.nesdev.com/w/index.php/APU_Mixer#Lookup_Table */
	int i;
	for(i = 0; i < 32; i++)
		pulseLookupTbl[i] = 95.52 / ((8128.0 / i) + 100);
	for(i = 0; i < 204; i++)
		tndLookupTbl[i] = 163.67 / ((24329.0 / i) + 100);
}

void apuDeinitBufs()
{
	if(apuOutBuf)
		free(apuOutBuf);
	apuOutBuf = NULL;
}

void apuInit()
{
	memset(APU_IO_Reg,0,0x18);
	memset(apuOutBuf, 0, apuBufSizeBytes);
	curBufPos = 0;

	freq1 = 0; freq2 = 0; triFreq = 0; noiseFreq = 0, dmcFreq = 0;
	noiseShiftReg = 1;
	p1LengthCtr = 0; p2LengthCtr = 0;
	noiseLengthCtr = 0;	triLengthCtr = 0;
	triLinearCtr = 0; triCurLinearCtr = 0;
	dmcAddr = 0, dmcLen = 0, dmcVol = 0; dmcSampleBuf = 0;
	dmcCurAddr = 0, dmcCurLen = 0; dmcCurVol = 0;
	dmcSampleRemain = 0;
	p1freqCtr = 0; p2freqCtr = 0; triFreqCtr = 0, noiseFreqCtr = 0, dmcFreqCtr = 0;
	p1Cycle = 0; p2Cycle = 0; triCycle = 0;

	memset(&p1Env,0,sizeof(envelope_t));
	memset(&p2Env,0,sizeof(envelope_t));
	memset(&noiseEnv,0,sizeof(envelope_t));

	memset(&p1Sweep,0,sizeof(sweep_t));
	p1Sweep.chan1 = true; //for negative sweep
	memset(&p2Sweep,0,sizeof(sweep_t));
	p2Sweep.chan1 = false;

	p1haltloop = false;	p2haltloop = false;
	trihaltloop = false; noisehaltloop = false;
	dmcstart = false;
	dmcirqenable = false;
	trireload = false;
	noiseMode1 = false;
	//4017 starts out as 0, so enable
	apu_enable_irq = true;
}

extern uint32_t cpu_oam_dma;
void apuClockTimers()
{
	if(p1freqCtr)
		p1freqCtr--;
	if(p1freqCtr == 0)
	{
		p1freqCtr = (freq1+1)*2;
		p1Cycle++;
		if(p1Cycle >= 8)
			p1Cycle = 0;
	}

	if(p2freqCtr)
		p2freqCtr--;
	if(p2freqCtr == 0)
	{
		p2freqCtr = (freq2+1)*2;
		p2Cycle++;
		if(p2Cycle >= 8)
			p2Cycle = 0;
	}

	if(triFreqCtr)
		triFreqCtr--;
	if(triFreqCtr == 0)
	{
		triFreqCtr = triFreq+1;
		triCycle++;
		if(triCycle >= 32)
			triCycle = 0;
	}

	if(noiseFreqCtr)
		noiseFreqCtr--;
	if(noiseFreqCtr == 0)
	{
		noiseFreqCtr = noiseFreq;
		uint8_t cmpBit = noiseMode1 ? (noiseShiftReg>>6)&1 : (noiseShiftReg>>1)&1;
		uint8_t cmpRes = (noiseShiftReg&1)^cmpBit;
		noiseShiftReg >>= 1;
		noiseShiftReg |= cmpRes<<14;
	}

	if(dmcFreqCtr)
		dmcFreqCtr--;
	if(dmcFreqCtr == 0)
	{
		dmcFreqCtr = dmcFreq;
		if(dmcSampleRemain)
		{
			if(dmcSampleBuf&1)
			{
				if(dmcVol <= 125)
					dmcVol += 2;
			}
			else if(dmcVol >= 2)
				dmcVol -= 2;
			dmcSampleBuf>>=1;
			dmcSampleRemain--;
		}
		if(!dmcSampleRemain)
		{
			if(dmcCurLen)
			{
				dmcSampleBuf = memGet8(dmcCurAddr);
				if(cpu_oam_dma > 0)
					cpuIncWaitCycles(2);
				else
					cpuIncWaitCycles(4);
				dmcCurAddr++;
				if(dmcCurAddr < 0x8000)
					dmcCurAddr |= 0x8000;
				dmcCurLen--;
				if(!dmcCurLen)
				{
					if(dmchaltloop)
					{
						dmcCurAddr = dmcAddr;
						dmcCurLen = dmcLen;
					}
					else if(dmcirqenable)
					{
						//printf("DMC IRQ\n");
						dmc_interrupt = true;
					}
				}
				dmcSampleRemain = 8;
			}
		}
	}
}

static float lastHPOut = 0, lastLPOut = 0;
static uint8_t lastP1Out = 0, lastP2Out = 0, lastTriOut = 0, lastNoiseOut = 0;

extern bool emuSkipVsync, emuSkipFrame;

int apuCycle()
{
	if(curBufPos == apuBufSize)
	{
		int updateRes = audioUpdate();
		if(updateRes == 0)
		{
			emuSkipFrame = false;
			emuSkipVsync = false;
			return 0;
		}
		if(updateRes > 6)
		{
			emuSkipVsync = true;
			emuSkipFrame = true;
		}
		else
		{
			emuSkipFrame = false;
			if(updateRes > 2)
				emuSkipVsync = true;
			else
				emuSkipVsync = false;
		}
		curBufPos = 0;
	}
	uint8_t p1Out = lastP1Out, p2Out = lastP2Out, 
		triOut = lastTriOut, noiseOut = lastNoiseOut;
	if(p1LengthCtr && (APU_IO_Reg[0x15] & P1_ENABLE))
	{
		if(p1seq[p1Cycle] && !p1Sweep.mute && freq1 >= 8 && freq1 < 0x7FF)
			lastP1Out = p1Out = (p1Env.constant ? p1Env.vol : p1Env.decay);
		else
			p1Out = 0;
	}
	if(p2LengthCtr && (APU_IO_Reg[0x15] & P2_ENABLE))
	{
		if(p2seq[p2Cycle] && !p2Sweep.mute && freq2 >= 8 && freq2 < 0x7FF)
			lastP2Out = p2Out = (p2Env.constant ? p2Env.vol : p2Env.decay);
		else
			p2Out = 0;
	}
	if(triLengthCtr && triCurLinearCtr && (APU_IO_Reg[0x15] & TRI_ENABLE))
	{
		if(triSeq[triCycle] && triFreq >= 2)
			lastTriOut = triOut = triSeq[triCycle];
		else
			triOut = 0;
	}
	if(noiseLengthCtr && (APU_IO_Reg[0x15] & NOISE_ENABLE))
	{
		if((noiseShiftReg&1) == 0 && noiseFreq > 0)
			lastNoiseOut = noiseOut = (noiseEnv.constant ? noiseEnv.vol : noiseEnv.decay);
		else
			noiseOut = 0;
	}
	float curIn = pulseLookupTbl[p1Out + p2Out] + tndLookupTbl[(3*triOut) + (2*noiseOut) + dmcVol];
	//very rough still
	if(vrc6enabled)
	{
		vrc6AudioCycle();
		curIn += ((float)vrc6Out)*0.008f;
		curIn *= 0.665f;
	}
	if(fdsEnabled)
	{
		fdsAudioCycle();
		curIn += ((float)fdsOut)*0.00617f;
		curIn *= 0.72f;
	}
	if(mmc5enabled)
	{
		mmc5AudioCycle();
		curIn += pulseLookupTbl[mmc5Out];
		curIn *= 0.78f;
	}
	float curLPout = lastLPOut+(lpVal*(curIn-lastLPOut));
	float curHPOut = hpVal*(lastHPOut+curLPout-curIn);
	//set output
	apuOutBuf[curBufPos] = -curHPOut;
	lastLPOut = curLPout;
	lastHPOut = curHPOut;
	curBufPos++;

	return 1;
}

void doEnvelopeLogic(envelope_t *env)
{
	if(env->start)
	{
		env->start = false;
		env->divider = env->vol;
		env->decay = 15;
	}
	else
	{
		if(env->divider == 0)
		{
			env->divider = env->vol;
			if(env->decay == 0)
			{
				if(env->loop)
					env->decay = 15;
			}
			else
				env->decay--;
		}
		else
			env->divider--;
	}
	//too slow on its own?
	//env->envelope = (env->constant ? env->vol : env->decay);
}

void sweepUpdateFreq(sweep_t *sw, uint16_t *freq)
{
	uint16_t inFreq = *freq;
	if(sw->negative)
	{
		inFreq -= (inFreq >> sw->shift);
		if(sw->chan1 == true) inFreq--;
	}
	else
		inFreq += (inFreq >> sw->shift);
	//if(inFreq > 8 && (inFreq < 0x7FF))
	{
		if(sw->enabled && sw->shift)
			*freq = inFreq;
	}
}

void doSweepLogic(sweep_t *sw, uint16_t *freq)
{
	if(sw->start)
	{
		uint8_t prevDiv = sw->divider;
		sw->divider = sw->period;
		sw->start = false;
		if(prevDiv == 0)
			sweepUpdateFreq(sw, freq);
	}
	else
	{
		if(sw->divider == 0)
		{
			sweepUpdateFreq(sw, freq);
			sw->divider = sw->period;
		}
		else
			sw->divider--;
	}
	//gets clocked too little on its own?
	/*if(inFreq < 8 || (inFreq >= 0x7FF))
		sw->mute = true;
	else
		sw->mute = false;*/
}

void apuClockA()
{
	if(p1LengthCtr)
	{
		doSweepLogic(&p1Sweep, &freq1);
		if(!p1haltloop)
			p1LengthCtr--;
	}
	if(p2LengthCtr)
	{
		doSweepLogic(&p2Sweep, &freq2);
		if(!p2haltloop)
			p2LengthCtr--;
	}
	if(triLengthCtr && !trihaltloop)
		triLengthCtr--;
	if(noiseLengthCtr && !noisehaltloop)
		noiseLengthCtr--;
}

void apuClockB()
{
	if(p1LengthCtr)
		doEnvelopeLogic(&p1Env);
	if(p2LengthCtr)
		doEnvelopeLogic(&p2Env);
	if(noiseLengthCtr)
		doEnvelopeLogic(&noiseEnv);
	if(trireload)
		triCurLinearCtr = triLinearCtr;
	else if(triCurLinearCtr)
		triCurLinearCtr--;
	if(!trihaltloop)
		trireload = false;
}

extern bool apu_interrupt;

void apuLenCycle()
{
	if(mmc5enabled)
		mmc5AudioLenCycle();

	if(mode5 == false)
	{
		if(modeCurCtr)
			modeCurCtr--;
		if(modeCurCtr == 0)
		{
			if(modePos == 1)
				apuClockA();
			if(modePos == 3)
			{
				apuClockA();
				if(apu_enable_irq)
					apu_interrupt = true;
			}
			apuClockB();
			modePos++;
			if(modePos >= 4)
				modePos = 0;
			modeCurCtr = mode4Ctr[modePos];
		}
	}
	else
	{
		if(modeCurCtr)
			modeCurCtr--;
		if(modeCurCtr == 0)
		{
			if(modePos == 0 || modePos == 2)
				apuClockA();
			if(modePos != 4)
				apuClockB();
			modePos++;
			if(modePos >= 5)
				modePos = 0;
			modeCurCtr = mode5Ctr[modePos];
		}
	}
}

void apuSet8(uint8_t reg, uint8_t val)
{
	//printf("%02x %02x %04x\n", reg, val, cpuGetPc());
	APU_IO_Reg[reg] = val;
	if(reg == 0)
	{
		p1Env.vol = val&0xF;
		p1seq = pulseSeqs[val>>6];
		p1Env.constant = ((val&PULSE_CONST_V) != 0);
		p1Env.loop = p1haltloop = ((val&PULSE_HALT_LOOP) != 0);
	}
	else if(reg == 1)
	{
		//printf("P1 sweep %02x\n", val);
		p1Sweep.enabled = ((val&0x80) != 0);
		p1Sweep.shift = val&7;
		p1Sweep.period = (val>>4)&7;
		p1Sweep.negative = ((val&0x8) != 0);
		p1Sweep.start = true;
		doSweepLogic(&p1Sweep, &freq1);
	}
	else if(reg == 2)
	{
		//printf("P1 time low %02x\n", val);
		freq1 = ((freq1&~0xFF) | val);
	}
	else if(reg == 3)
	{
		p1Cycle = 0;
		if(APU_IO_Reg[0x15] & P1_ENABLE)
			p1LengthCtr = lengthLookupTbl[val>>3];
		freq1 = (freq1&0xFF) | ((val&7)<<8);
		//printf("P1 new freq %04x\n", freq2);
		p1Env.start = true;
	}
	else if(reg == 4)
	{
		p2Env.vol = val&0xF;
		p2seq = pulseSeqs[val>>6];
		p2Env.constant = ((val&PULSE_CONST_V) != 0);
		p2Env.loop = p2haltloop = ((val&PULSE_HALT_LOOP) != 0);
	}
	else if(reg == 5)
	{
		//printf("P2 sweep %02x\n", val);
		p2Sweep.enabled = ((val&0x80) != 0);
		p2Sweep.shift = val&7;
		p2Sweep.period = (val>>4)&7;
		p2Sweep.negative = ((val&0x8) != 0);
		p2Sweep.start = true;
		doSweepLogic(&p2Sweep, &freq2);
	}
	else if(reg == 6)
	{
		//printf("P2 time low %02x\n", val);
		freq2 = ((freq2&~0xFF) | val);
	}
	else if(reg == 7)
	{
		p2Cycle = 0;
		if(APU_IO_Reg[0x15] & P2_ENABLE)
			p2LengthCtr = lengthLookupTbl[val>>3];
		freq2 = (freq2&0xFF) | ((val&7)<<8);
		//printf("P2 new freq %04x\n", freq2);
		p2Env.start = true;
	}
	else if(reg == 8)
	{
		triLinearCtr = val&0x7F;
		trihaltloop = ((val&TRI_HALT_LOOP) != 0);
	}
	else if(reg == 0xA)
	{
		triFreq = ((triFreq&~0xFF) | val);
		//if(triFreq < 2)
		//	triLengthCtr = 0;
	}
	else if(reg == 0xB)
	{
		if(APU_IO_Reg[0x15] & TRI_ENABLE)
			triLengthCtr = lengthLookupTbl[val>>3];
		triFreq = (triFreq&0xFF) | ((val&7)<<8);
		//printf("Tri new freq %04x\n", triFreq);
		//if(triFreq < 2)
		//	triLengthCtr = 0;
		trireload = true;
	}
	else if(reg == 0xC)
	{
		noiseEnv.vol = val&0xF;
		noiseEnv.constant = ((val&PULSE_CONST_V) != 0);
		noiseEnv.loop = noisehaltloop = ((val&PULSE_HALT_LOOP) != 0);
	}
	else if(reg == 0xE)
	{
		noiseMode1 = ((val&0x80) != 0);
		noiseFreq = noisePeriod[val&0xF];
	}
	else if(reg == 0xF)
	{
		if(APU_IO_Reg[0x15] & NOISE_ENABLE)
			noiseLengthCtr = lengthLookupTbl[val>>3];
		noiseEnv.start = true;
	}
	else if(reg == 0x10)
	{
		//printf("Set 0x10 %02x\n", val);
		dmcFreq = dmcPeriod[val&0xF];
		dmchaltloop = ((val&DMC_HALT_LOOP) != 0);
		dmcirqenable = ((val&DMC_IRQ_ENABLE) != 0);
		//printf("%d\n", dmcirqenable);
		if(!dmcirqenable)
			dmc_interrupt = false;
	}
	else if(reg == 0x11)
		dmcVol = val&0x7F;
	else if(reg == 0x12)
		dmcAddr = 0xC000+(val*64);
	else if(reg == 0x13)
	{
		//printf("Set 0x13 %02x\n", val);
		dmcLen = (val*16)+1;
	}
	else if(reg == 0x15)
	{
		//printf("Set 0x15 %02x\n",val);
		if(!(val & P1_ENABLE))
			p1LengthCtr = 0;
		if(!(val & P2_ENABLE))
			p2LengthCtr = 0;
		if(!(val & TRI_ENABLE))
			triLengthCtr = 0;
		if(!(val & NOISE_ENABLE))
			noiseLengthCtr = 0;
		if(!(val & DMC_ENABLE))
			dmcCurLen = 0;
		else if(dmcCurLen == 0)
		{
			dmcCurAddr = dmcAddr;
			dmcCurLen = dmcLen;
		}
		dmc_interrupt = false;
	}
	else if(reg == 0x17)
	{
		apu_enable_irq = ((val&(1<<6)) == 0);
		if(!apu_enable_irq)
			apu_interrupt = false;
		mode5 = ((val&(1<<7)) != 0);
		//printf("Set 0x17 %d %d\n", apu_enable_irq, mode5);
		modePos = 0;
		if(mode5)
			modeCurCtr = 1;
		else
			modeCurCtr = nesPAL ? 8315 : 7459;
	}
}

uint8_t apuGet8(uint8_t reg)
{
	//printf("%08x\n", reg);
	if(reg == 0x15)
	{
		uint8_t intrflags = ((apu_interrupt<<6) | (dmc_interrupt<<7));
		//printf("Get 0x15 %02x\n",intrflags);
		apu_interrupt = false;
		return ((p1LengthCtr > 0) | ((p2LengthCtr > 0)<<1) | ((triLengthCtr > 0)<<2) | ((noiseLengthCtr > 0)<<3) | ((dmcCurLen > 0)<<4) | intrflags);
	}
	return APU_IO_Reg[reg];
}

uint8_t *apuGetBuf()
{
	return (uint8_t*)apuOutBuf;
}

uint32_t apuGetBufSize()
{
	return apuBufSizeBytes;
}

uint32_t apuGetFrequency()
{
	return apuFrequency;
}
