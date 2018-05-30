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
bool fdsEnabled;
uint8_t fdsOut;

static uint8_t fds_wave[0x40];
static uint8_t fds_modulation[0x40];
static uint8_t fdsCurWavePos, fdsCurModPos;
static uint8_t fdsCurWave;
static uint8_t fdsEnvSpeed;
static uint8_t fdsVolEnvSpeed, fdsVolEnvGain, fdsVolEnvMode;
static uint8_t fdsSweepSpeed, fdsSweepModGain, fdsSweepMode;
static uint8_t fdsMasterVol;
static int8_t fdsModCounter;
static uint16_t fdsFreq, fdsModFreq;
static int16_t fdsModChange;
static uint32_t fdsCurMasterClock, fdsCurModClock, fdsCurVolEnvClock, fdsCurSweepClock;
static uint32_t fdsVolEnvClock, fdsSweepClock;
static bool fdsMasterEnable;
static bool fdsVolEnvEnabled;
static bool fdsEnvMasterEnable;
static bool fdsSweepEnabled;
static bool fdsModEnabled;
static bool fdsWavWrite;

static void fdsUpdateClockRates()
{
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
	fdsSweepSpeed = 0, fdsSweepModGain = 0, fdsSweepMode = 0;
	fdsUpdateClockRates();
	fdsMasterVol = 0;
	fdsModCounter = 0;
	fdsFreq = 0, fdsModFreq = 0;
	fdsModChange = 0;
	fdsCurMasterClock = 0;
	fdsCurModClock = 0;
	fdsCurVolEnvClock = fdsVolEnvClock;
	fdsCurSweepClock = fdsSweepClock;
	fdsMasterEnable = false;
	fdsVolEnvEnabled = false;
	fdsEnvMasterEnable = false;
	fdsSweepEnabled = false;
	fdsModEnabled = false;
	fdsWavWrite = false;
}

void fdsAudioCycle()
{
	if(fdsWavWrite)
		return;
	uint8_t fdsCurVol = fdsVolEnvGain;
	if(fdsCurVol > 0x20) //clamp output
		fdsCurVol = 0x20;
	uint16_t tmp = (fdsCurWave&0x3F)*fdsCurVol;
	if(fdsMasterVol == 1)
		tmp = tmp*20/30;
	else if(fdsMasterVol == 2)
		tmp = tmp*15/30;
	else if(fdsMasterVol == 3)
		tmp = tmp*12/30;
	fdsOut = (tmp>>5)&0x3F;
}

void fdsAudioClockTimers()
{
	if(!fdsMasterEnable || !fdsEnvMasterEnable)
		return;

	if(fdsVolEnvEnabled)
	{
		if(fdsCurVolEnvClock)
			fdsCurVolEnvClock--;
		else
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
		}
	}

	if(fdsSweepEnabled)
	{
		if(fdsCurSweepClock)
			fdsCurSweepClock--;
		else
		{
			fdsCurSweepClock = fdsSweepClock;
			//do sweep stuff
			if(fdsSweepMode == 1)
			{
				if(fdsSweepModGain < 0x20)
					fdsSweepModGain++;
			}
			else
			{
				if(fdsSweepModGain > 0)
					fdsSweepModGain--;
			}
		}
	}
}

void fdsAudioMasterUpdate()
{
	if(fdsMasterEnable)
	{
		fdsCurMasterClock += (fdsFreq + fdsModChange);
		if(fdsCurMasterClock&(~0xFFFF))
		{
			fdsCurMasterClock &= 0xFFFF;
			//do master stuff
			fdsCurWave = fds_wave[fdsCurWavePos];
			fdsCurWavePos++;
			if(fdsCurWavePos >= 0x40)
				fdsCurWavePos = 0;
		}
	}

	if(fdsModEnabled)
	{
		fdsCurModClock += fdsModFreq;
		if(fdsCurModClock&(~0xFFFF))
		{
			fdsCurModClock &= 0xFFFF;
			//do mod stuff
			switch((fds_modulation[fdsCurModPos]&7))
			{
				case 1:
					fdsModCounter++;
					break;
				case 2:
					fdsModCounter += 2;
					break;
				case 3:
					fdsModCounter += 4;
					break;
				case 4:
					fdsModCounter = 0;
					break;
				case 5:
					fdsModCounter -= 4;
					break;
				case 6:
					fdsModCounter -= 2;
					break;
				case 7:
					fdsModCounter -= 1;
					break;
				default:
					break;
			}
			if(fdsModCounter & 0x40) //7-bit signed
				fdsModCounter |= 0x80;
			else
				fdsModCounter &= ~0x80;
			//move along mod pos
			fdsCurModPos++; fdsCurModPos &= 0x3F;
			// from https://forums.nesdev.com/viewtopic.php?f=3&t=10233
			// 1. multiply counter by gain, lose lowest 4 bits of result but "round" in a strange way
			int16_t temp = fdsModCounter * fdsSweepModGain;
			uint8_t remainder = temp & 0xF;
			temp >>= 4;
			if((remainder > 0) && ((temp & 0x80) == 0))
			{
				if (fdsModCounter < 0)
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
			temp = fdsFreq * temp;
			remainder = temp & 0x3F;
			temp >>= 6;
			if(remainder >= 32)
				temp += 1;
			// final mod result is in temp
			fdsModChange = temp;
		}
	}
}

void fdsAudioSet8(uint8_t reg, uint8_t val)
{
	if(reg == 0)
	{
		fdsVolEnvEnabled = ((val&0x80) == 0);
		fdsVolEnvMode = ((val&0x40) != 0);
		fdsVolEnvSpeed = val&0x3F; //always set speed
		if(!fdsVolEnvEnabled) //only set gain when unit is disabled
			fdsVolEnvGain = val&0x3F;
		fdsUpdateClockRates(); //always resets clock
		fdsCurVolEnvClock = fdsVolEnvClock;
	}
	else if(reg == 2)
		fdsFreq = ((fdsFreq&~0xFF) | val);
	else if(reg == 3)
	{
		fdsMasterEnable = ((val&0x80) == 0);
		fdsFreq = (fdsFreq&0xFF) | ((val&0xF)<<8);
		if(fdsFreq == 0)
			fdsMasterEnable = false;
		if(!fdsMasterEnable)
		{
			//disabling resets clock and wave pos
			fdsCurMasterClock = 0;
			fdsCurWavePos = 0;
			fdsCurWave = fds_wave[fdsCurWavePos];
		}
		fdsEnvMasterEnable = ((val&0x40) == 0);
		if(!fdsEnvMasterEnable)
		{
			//disabling resets both env clocks
			fdsCurVolEnvClock = fdsVolEnvClock;
			fdsCurSweepClock = fdsSweepClock;
		}
	}
	else if(reg == 4)
	{
		fdsSweepEnabled = ((val&0x80) == 0);
		fdsSweepMode = ((val&0x40) != 0);
		fdsSweepSpeed = val&0x3F; //always set speed
		if(!fdsSweepEnabled) //only set gain when unit is disabled
			fdsSweepModGain = val&0x3F;
		fdsUpdateClockRates(); //always resets clock
		fdsCurSweepClock = fdsSweepClock;
	}
	else if(reg == 5)
	{
		fdsModCounter = (val&0x7F);
		if(val&0x40) //7-bit signed
			fdsModCounter |= 0x80;
		else
			fdsModCounter &= ~0x80;
	}
	else if(reg == 6)
		fdsModFreq = ((fdsModFreq&~0xFF) | val);
	else if(reg == 7)
	{
		fdsModEnabled = ((val&0x80) == 0);
		fdsModFreq = (fdsModFreq&0xFF) | ((val&0xF)<<8);
		if(fdsModFreq == 0)
			fdsModEnabled = false;
		if(!fdsModEnabled)
		{
			//disabling resets clock 
			fdsCurModClock = 0;
			//make sure to clear old change too
			fdsModChange = 0;
		}
	}
	else if(reg == 8)
	{
		if(!fdsModEnabled)
		{
			fds_modulation[fdsCurModPos] = val;
			fdsCurModPos++; fdsCurModPos&=0x3F;
			fds_modulation[fdsCurModPos] = val;
			fdsCurModPos++; fdsCurModPos&=0x3F;
		}
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
		return fdsSweepModGain|0x40;
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
