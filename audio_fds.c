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
#include "audio.h"
#include "mem.h"
#include "cpu.h"

//used externally
bool fdsEnabled;
uint8_t fdsOut;

static uint8_t fds_wave[0x40];
static uint8_t fds_modulation[0x40];
static uint8_t fdsCurWavePos, fdsCurModPos;
static uint8_t fdsCurWave;
static uint8_t fdsEnvSpeed;
static uint8_t fdsVolEnvSpeed, fdsVolEnvGain, fdsVolEnvMode;
static uint8_t fdsSweepSpeed, fdsSweepGain, fdsSweepMode;
static uint8_t fdsMasterVol, fdsCurVol;
static int8_t fdsSweepBias;
static uint16_t fdsFreq, fdsModFreq;
static int16_t fdsModChange;
static uint32_t fdsCurMasterClock, fdsCurModClock, fdsCurVolEnvClock, fdsCurSweepClock;
static uint32_t fdsMasterClock, fdsModClock, fdsVolEnvClock, fdsSweepClock;
static bool fdsMasterEnable;
static bool fdsEnvEnabled;
static bool fdsEnvMasterEnable;
static bool fdsSweepEnabled;
static bool fdsModEnabled;
static bool fdsWavWrite;

static void fdsUpdateClockRates()
{
	if(fdsFreq && (fdsFreq + fdsModChange))
		fdsMasterClock = 786432 / (fdsFreq + fdsModChange);
	else
		fdsMasterClock = 0;
	if(fdsModFreq)
		fdsModClock = 786432 / fdsModFreq;
	else
		fdsModClock = 0;
	fdsVolEnvClock = 8 * fdsEnvSpeed * (fdsVolEnvSpeed + 1);
	fdsSweepClock = 8 * fdsEnvSpeed * (fdsSweepSpeed + 1);
}

void fdsAudioInit()
{
	memset(fds_wave, 0, 0x40);
	memset(fds_modulation, 0, 0x40);
	fdsEnabled = true;
	fdsOut = 0;
	fdsCurWavePos = 0, fdsCurModPos = 0;
	fdsCurWave = 0;
	fdsEnvSpeed = 0xE8; //from FDS BIOS
	fdsVolEnvSpeed = 0, fdsVolEnvGain = 0, fdsVolEnvMode = 0;
	fdsSweepSpeed = 0, fdsSweepGain = 0, fdsSweepMode = 0;
	fdsMasterVol = 0, fdsCurVol = 0;
	fdsSweepBias = 0;
	fdsFreq = 0, fdsModFreq = 0;
	fdsModChange = 0;
	fdsUpdateClockRates();
	fdsCurMasterClock = fdsMasterClock;
	fdsCurModClock = fdsModClock;
	fdsCurVolEnvClock = fdsVolEnvClock;
	fdsCurSweepClock = fdsSweepClock;
	fdsMasterEnable = false;
	fdsEnvEnabled = false;
	fdsEnvMasterEnable = false;
	fdsSweepEnabled = false;
	fdsModEnabled = false;
	fdsWavWrite = false;
}

int fdsAudioCycle()
{
	if(fdsWavWrite)
		return 1;
	if(fdsCurVol > 0x20)
		fdsCurVol = 0x20;
	uint16_t tmp = (((fdsCurWave&0x3F)*(fdsCurVol)));
	if(fdsMasterVol == 1)
		tmp = tmp*20/30;
	else if(fdsMasterVol == 2)
		tmp = tmp*15/30;
	else if(fdsMasterVol == 3)
		tmp = tmp*12/30;
	fdsOut = (tmp>>5)&0x3F;
	return 1;
}

void fdsAudioClockTimers()
{
	if(!fdsMasterEnable)
		return;

	if(fdsCurVolEnvClock)
		fdsCurVolEnvClock--;
	else if(fdsEnvEnabled && fdsEnvMasterEnable)
	{		
		fdsCurVolEnvClock = fdsVolEnvClock;
		//do env stuff
		if(fdsVolEnvMode == 1)
		{
			if(fdsVolEnvGain < 0x20)
				fdsVolEnvGain++;
		}
		else
		{
			if(fdsVolEnvGain > 0)
				fdsVolEnvGain--;
		}
		fdsCurVol = fdsVolEnvGain;
		if(fdsCurVol > 0x20)
			fdsCurVol = 0x20;
	}
	if(fdsCurSweepClock)
		fdsCurSweepClock--;
	else if(fdsSweepEnabled)
	{
		fdsCurSweepClock = fdsSweepClock;
		//do sweep stuff
		if(fdsSweepMode == 1)
		{
			if(fdsSweepGain < 0x20)
				fdsSweepGain++;
		}
		else
		{
			if(fdsSweepGain > 0)
				fdsSweepGain--;
		}
	}
}

void fdsAudioMasterUpdate()
{
	if(!fdsMasterEnable)
		return;

	if(fdsCurMasterClock)
		fdsCurMasterClock--;
	else if(fdsMasterClock)
	{
		fdsCurMasterClock = fdsMasterClock;
		//do master stuff
		fdsCurWave = fds_wave[fdsCurWavePos];
		fdsCurWavePos++;
		if(fdsCurWavePos >= 0x40)
			fdsCurWavePos = 0;
	}
	if(fdsCurModClock)
		fdsCurModClock--;
	else if(fdsModEnabled && fdsModClock)
	{
		fdsCurModClock = fdsModClock;
		//do mod stuff
		switch((fds_modulation[fdsCurModPos]&7))
		{
			case 1:
				fdsSweepBias++;
				break;
			case 2:
				fdsSweepBias+=2;
				break;
			case 3:
				fdsSweepBias+=4;
				break;
			case 4:
				fdsSweepBias = 0;
				break;
			case 5:
				fdsSweepBias-=4;
				break;
			case 6:
				fdsSweepBias-=2;
				break;
			case 7:
				fdsSweepBias-=1;
				break;
			default:
				break;
		}
		fdsCurModPos++;
		if(fdsCurModPos >= 0x40)
			fdsCurModPos = 0;
		if(fdsSweepBias&0x40) //7-bit signed
			fdsSweepBias |= 0x80;
		else
			fdsSweepBias &= ~0x80;
		int16_t temp = fdsSweepBias * fdsSweepGain;
		if(temp & 0x0F)
		{
			temp /= 16;
			if(fdsSweepBias < 0)
				temp -= 1;
			else
				temp += 2;
		}
		else
			temp /= 16;

		if(temp > 193)
			temp -= 258;  // not a typo... for some reason the wraps
		if(temp < -64)
			temp += 256;  // are inconsistent
		fdsModChange = fdsFreq * temp / 64;
		fdsUpdateClockRates();
	}
}

void fdsAudioSet8(uint8_t reg, uint8_t val)
{
	if(reg == 0)
	{
		fdsEnvEnabled = ((val&0x80) == 0);
		fdsVolEnvMode = ((val&0x40) != 0);
		if(fdsEnvEnabled)
		{
			fdsVolEnvSpeed = val&0x3F;
			fdsUpdateClockRates();
		}
		else
		{
			fdsVolEnvGain = val&0x3F;
			fdsCurVol = val&0x3F;
			if(fdsCurVol > 0x20)
				fdsCurVol = 0x20;
		}
	}
	else if(reg == 2)
	{
		fdsFreq = ((fdsFreq&~0xFF) | val);
		fdsUpdateClockRates();
	}
	else if(reg == 3)
	{
		fdsMasterEnable = ((val&0x80) == 0);
		fdsEnvMasterEnable = ((val&0x40) == 0);
		fdsFreq = (fdsFreq&0xFF) | ((val&0xF)<<8);
		if(fdsFreq == 0)
			fdsMasterEnable = false;
		fdsUpdateClockRates();
	}
	else if(reg == 4)
	{
		fdsSweepEnabled = ((val&0x80) == 0);
		fdsSweepMode = ((val&0x40) != 0);
		if(fdsSweepEnabled)
		{
			fdsSweepSpeed = val&0x3F;
			fdsUpdateClockRates();
		}
		else
			fdsSweepGain = val&0x3F;
	}
	else if(reg == 5)
	{
		fdsCurModPos = 0;
		fdsSweepBias = (val&0x7F);
		if(val&0x40) //7-bit signed
			fdsSweepBias |= 0x80;
		else
			fdsSweepBias &= ~0x80;
	}
	else if(reg == 6)
	{
		fdsModFreq = ((fdsModFreq&~0xFF) | val);
		fdsUpdateClockRates();
	}
	else if(reg == 7)
	{
		fdsModEnabled = ((val&0x80) == 0);
		fdsModFreq = (fdsModFreq&0xFF) | ((val&0xF)<<8);
		if(fdsModFreq == 0)
			fdsModEnabled = false;
		if(!fdsModEnabled)
			fdsModChange = 0;
		fdsUpdateClockRates();
	}
	else if(reg == 8)
	{
		memmove(fds_modulation, fds_modulation+2, 62);
		fds_modulation[0x3E] = val;
		fds_modulation[0x3F] = val;
	}
	else if(reg == 9)
	{
		fdsWavWrite = ((val&0x80) != 0);
		fdsMasterVol = val&3;
	}
	else if(reg == 0xA)
	{
		fdsEnvSpeed = val;
		fdsUpdateClockRates();
	}
}

uint8_t fdsAudioGet8(uint8_t reg)
{
	if(reg == 0)
		return fdsVolEnvGain|0x40;
	else if(reg == 2)
		return fdsSweepGain|0x40;
	return 0;
}

void fdsAudioSetWave(uint8_t pos, uint8_t val)
{
	if(!fdsWavWrite)
		return;
	fds_wave[pos] = val;
}

uint8_t fdsAudioGetWave(uint8_t pos)
{
	return fds_wave[pos];
}

void fdsAudioLenCycle()
{

}
