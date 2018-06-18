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
#include "audio_fds.h"
#include "audio.h"
#include "mem.h"
#include "cpu.h"

//used externally
extern uint8_t audioExpansion;
uint8_t fdsOut;

static struct {
	uint8_t wave[0x40];
	uint8_t modulation[0x40];
	uint8_t curWavePos, curModPos;
	uint8_t curWave;
	uint8_t envSpeed;
	uint8_t volEnvSpeed, volEnvGain, volEnvMode;
	uint8_t sweepSpeed, sweepModGain, sweepMode;
	uint8_t masterVol;
	int8_t modCounter;
	uint16_t freq, modFreq;
	int16_t modChange;
	uint32_t curMasterClock, curModClock, curVolEnvClock, curSweepClock;
	uint32_t volEnvClock, sweepClock;
	bool masterEnable;
	bool volEnvEnabled;
	bool envMasterEnable;
	bool sweepEnabled;
	bool modEnabled;
	bool wavWrite;
} fds_apu;

static void fdsUpdateClockRates()
{
	fds_apu.volEnvClock = 8 * fds_apu.envSpeed * (fds_apu.volEnvSpeed + 1);
	fds_apu.sweepClock = 8 * fds_apu.envSpeed * (fds_apu.sweepSpeed + 1);
}

void fdsAudioInit()
{
	memset(fds_apu.wave, 0, 0x40);
	memset(fds_apu.modulation, 0, 0x40);
	audioExpansion |= EXP_FDS;
	fdsOut = 0;
	fds_apu.curWavePos = 0, fds_apu.curModPos = 0;
	fds_apu.curWave = 0;
	fds_apu.envSpeed = 0xE8; //from FDS BIOS
	fds_apu.volEnvSpeed = 0, fds_apu.volEnvGain = 0, fds_apu.volEnvMode = 0;
	fds_apu.sweepSpeed = 0, fds_apu.sweepModGain = 0, fds_apu.sweepMode = 0;
	fdsUpdateClockRates();
	fds_apu.masterVol = 0;
	fds_apu.modCounter = 0;
	fds_apu.freq = 0, fds_apu.modFreq = 0;
	fds_apu.modChange = 0;
	fds_apu.curMasterClock = 0;
	fds_apu.curModClock = 0;
	fds_apu.curVolEnvClock = fds_apu.volEnvClock;
	fds_apu.curSweepClock = fds_apu.sweepClock;
	fds_apu.masterEnable = false;
	fds_apu.volEnvEnabled = false;
	fds_apu.envMasterEnable = false;
	fds_apu.sweepEnabled = false;
	fds_apu.modEnabled = false;
	fds_apu.wavWrite = false;
}

FIXNES_NOINLINE void fdsAudioCycle()
{
	if(fds_apu.wavWrite)
		return;
	uint8_t fdsCurVol = fds_apu.volEnvGain;
	if(fdsCurVol > 0x20) //clamp output
		fdsCurVol = 0x20;
	uint16_t tmp = (fds_apu.curWave&0x3F)*fdsCurVol;
	if(fds_apu.masterVol == 1)
		tmp = tmp*20/30;
	else if(fds_apu.masterVol == 2)
		tmp = tmp*15/30;
	else if(fds_apu.masterVol == 3)
		tmp = tmp*12/30;
	fdsOut = (tmp>>5)&0x3F;
}

void fdsAudioClockTimers()
{
	if(!fds_apu.masterEnable || !fds_apu.envMasterEnable)
		return;

	if(fds_apu.volEnvEnabled)
	{
		if(fds_apu.curVolEnvClock)
			fds_apu.curVolEnvClock--;
		else
		{		
			fds_apu.curVolEnvClock = fds_apu.volEnvClock;
			//do env stuff
			if(fds_apu.volEnvMode == 1)
			{
				if(fds_apu.volEnvGain < 0x20)
					fds_apu.volEnvGain++;
			}
			else
			{
				if(fds_apu.volEnvGain > 0)
					fds_apu.volEnvGain--;
			}
		}
	}

	if(fds_apu.sweepEnabled)
	{
		if(fds_apu.curSweepClock)
			fds_apu.curSweepClock--;
		else
		{
			fds_apu.curSweepClock = fds_apu.sweepClock;
			//do sweep stuff
			if(fds_apu.sweepMode == 1)
			{
				if(fds_apu.sweepModGain < 0x20)
					fds_apu.sweepModGain++;
			}
			else
			{
				if(fds_apu.sweepModGain > 0)
					fds_apu.sweepModGain--;
			}
		}
	}
}

FIXNES_NOINLINE void fdsAudioMasterUpdate()
{
	if(fds_apu.masterEnable)
	{
		fds_apu.curMasterClock += (fds_apu.freq + fds_apu.modChange);
		if(fds_apu.curMasterClock&(~0xFFFF))
		{
			fds_apu.curMasterClock &= 0xFFFF;
			//do master stuff
			fds_apu.curWave = fds_apu.wave[fds_apu.curWavePos];
			fds_apu.curWavePos++;
			if(fds_apu.curWavePos >= 0x40)
				fds_apu.curWavePos = 0;
		}
	}

	if(fds_apu.modEnabled)
	{
		fds_apu.curModClock += fds_apu.modFreq;
		if(fds_apu.curModClock&(~0xFFFF))
		{
			fds_apu.curModClock &= 0xFFFF;
			//do mod stuff
			switch((fds_apu.modulation[fds_apu.curModPos]&7))
			{
				case 1:
					fds_apu.modCounter++;
					break;
				case 2:
					fds_apu.modCounter += 2;
					break;
				case 3:
					fds_apu.modCounter += 4;
					break;
				case 4:
					fds_apu.modCounter = 0;
					break;
				case 5:
					fds_apu.modCounter -= 4;
					break;
				case 6:
					fds_apu.modCounter -= 2;
					break;
				case 7:
					fds_apu.modCounter -= 1;
					break;
				default:
					break;
			}
			if(fds_apu.modCounter & 0x40) //7-bit signed
				fds_apu.modCounter |= 0x80;
			else
				fds_apu.modCounter &= ~0x80;
			//move along mod pos
			fds_apu.curModPos++; fds_apu.curModPos &= 0x3F;
			// from https://forums.nesdev.com/viewtopic.php?f=3&t=10233
			// 1. multiply counter by gain, lose lowest 4 bits of result but "round" in a strange way
			int16_t temp = fds_apu.modCounter * fds_apu.sweepModGain;
			uint8_t remainder = temp & 0xF;
			temp >>= 4;
			if((remainder > 0) && ((temp & 0x80) == 0))
			{
				if (fds_apu.modCounter < 0)
					temp -= 1;
				else
					temp += 2;
			}
			// 2. wrap if a certain range is exceeded
			if(temp >= 192)
				temp -= 256;
			else if(temp < -64)
				temp += 256;
			// 3. multiply result by pitch, then round to nearest while dropping 6 bits
			temp = fds_apu.freq * temp;
			remainder = temp & 0x3F;
			temp >>= 6;
			if(remainder >= 32)
				temp += 1;
			// final mod result is in temp
			fds_apu.modChange = temp;
		}
	}
}

void fdsAudioSet8(uint8_t reg, uint8_t val)
{
	if(reg == 0)
	{
		fds_apu.volEnvEnabled = ((val&0x80) == 0);
		fds_apu.volEnvMode = ((val&0x40) != 0);
		fds_apu.volEnvSpeed = val&0x3F; //always set speed
		if(!fds_apu.volEnvEnabled) //only set gain when unit is disabled
			fds_apu.volEnvGain = val&0x3F;
		fdsUpdateClockRates(); //always resets clock
		fds_apu.curVolEnvClock = fds_apu.volEnvClock;
	}
	else if(reg == 2)
		fds_apu.freq = ((fds_apu.freq&~0xFF) | val);
	else if(reg == 3)
	{
		fds_apu.masterEnable = ((val&0x80) == 0);
		fds_apu.freq = (fds_apu.freq&0xFF) | ((val&0xF)<<8);
		if(fds_apu.freq == 0)
			fds_apu.masterEnable = false;
		if(!fds_apu.masterEnable)
		{
			//disabling resets clock and wave pos
			fds_apu.curMasterClock = 0;
			fds_apu.curWavePos = 0;
			fds_apu.curWave = fds_apu.wave[fds_apu.curWavePos];
		}
		fds_apu.envMasterEnable = ((val&0x40) == 0);
		if(!fds_apu.envMasterEnable)
		{
			//disabling resets both env clocks
			fds_apu.curVolEnvClock = fds_apu.volEnvClock;
			fds_apu.curSweepClock = fds_apu.sweepClock;
		}
	}
	else if(reg == 4)
	{
		fds_apu.sweepEnabled = ((val&0x80) == 0);
		fds_apu.sweepMode = ((val&0x40) != 0);
		fds_apu.sweepSpeed = val&0x3F; //always set speed
		if(!fds_apu.sweepEnabled) //only set gain when unit is disabled
			fds_apu.sweepModGain = val&0x3F;
		fdsUpdateClockRates(); //always resets clock
		fds_apu.curSweepClock = fds_apu.sweepClock;
	}
	else if(reg == 5)
	{
		fds_apu.modCounter = (val&0x7F);
		if(val&0x40) //7-bit signed
			fds_apu.modCounter |= 0x80;
		else
			fds_apu.modCounter &= ~0x80;
	}
	else if(reg == 6)
		fds_apu.modFreq = ((fds_apu.modFreq&~0xFF) | val);
	else if(reg == 7)
	{
		fds_apu.modEnabled = ((val&0x80) == 0);
		fds_apu.modFreq = (fds_apu.modFreq&0xFF) | ((val&0xF)<<8);
		if(fds_apu.modFreq == 0)
			fds_apu.modEnabled = false;
		if(!fds_apu.modEnabled)
		{
			//disabling resets clock 
			fds_apu.curModClock = 0;
			//make sure to clear old change too
			fds_apu.modChange = 0;
		}
	}
	else if(reg == 8)
	{
		if(!fds_apu.modEnabled)
		{
			fds_apu.modulation[fds_apu.curModPos] = val;
			fds_apu.curModPos++; fds_apu.curModPos&=0x3F;
			fds_apu.modulation[fds_apu.curModPos] = val;
			fds_apu.curModPos++; fds_apu.curModPos&=0x3F;
		}
	}
	else if(reg == 9)
	{
		fds_apu.wavWrite = ((val&0x80) != 0);
		fds_apu.masterVol = val&3;
	}
	else if(reg == 0xA)
	{
		fds_apu.envSpeed = val;
		fdsUpdateClockRates();
	}
}

uint8_t fdsAudioGet8(uint8_t reg)
{
	if(reg == 0)
		return fds_apu.volEnvGain|0x40;
	else if(reg == 2)
		return fds_apu.sweepModGain|0x40;
	return 0;
}

void fdsAudioSetWave(uint8_t pos, uint8_t val)
{
	if(!fds_apu.wavWrite)
		return;
	fds_apu.wave[pos] = val;
}

uint8_t fdsAudioGetWave(uint8_t pos)
{
	return fds_apu.wave[pos];
}
