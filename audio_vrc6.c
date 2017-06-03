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
#include "audio_vrc6.h"
#include "audio.h"
#include "mem.h"
#include "cpu.h"

//used externally
bool vrc6enabled = false;
uint8_t vrc6Out = 0;

static uint16_t vrc6_freq1, vrc6_freq2, vrc6_sawFreq;
static uint16_t vrc6_p1freqCtr, vrc6_p2freqCtr, vrc6_sawFreqCtr;
static uint8_t vrc6_p1Cycle, vrc6_p2Cycle, vrc6_sawCycle;
static uint8_t vrc6_p1Vol, vrc6_p2Vol, vrc6_sawVol;
static uint8_t vrc6_p1Duty, vrc6_p2Duty, vrc6_sawAdd;
static uint8_t vrc6_speed;
static bool vrc6_p1const, vrc6_p2const;
static bool vrc6_p1enable, vrc6_p2enable, vrc6_sawenable;
static bool vrc6_halt;

void vrc6AudioInit()
{
	vrc6enabled = true;
	vrc6_freq1 = 0, vrc6_freq2 = 0, vrc6_sawFreq = 0;
	vrc6_p1freqCtr = 0, vrc6_p2freqCtr = 0, vrc6_sawFreqCtr = 0;
	vrc6_p1Cycle = 0, vrc6_p2Cycle = 0, vrc6_sawCycle = 0;
	vrc6_p1Vol = 0, vrc6_p2Vol = 0, vrc6_sawVol = 0;
	vrc6_p1Duty = 0, vrc6_p2Duty = 0, vrc6_sawAdd = 0;
	vrc6_speed = 0;
	vrc6_p1const = false, vrc6_p2const = false;
	vrc6_p1enable = false, vrc6_p2enable = false, vrc6_sawenable = false;
	vrc6_halt = false;
	//printf("VRC6 Audio Inited!\n");
}

static uint8_t vrc6_lastP1Out = 0, vrc6_lastP2Out = 0, vrc6_lastSawOut = 0;

void vrc6AudioCycle()
{
	uint8_t p1Out = vrc6_lastP1Out, p2Out = vrc6_lastP2Out, sawOut = vrc6_lastSawOut;
	if(vrc6_p1enable)
	{
		if(vrc6_p1const || vrc6_p1Cycle <= vrc6_p1Duty)
			vrc6_lastP1Out = p1Out = vrc6_p1Vol;
		else
			p1Out = 0;
	}
	if(vrc6_p2enable)
	{
		if(vrc6_p2const || vrc6_p2Cycle <= vrc6_p2Duty)
			vrc6_lastP2Out = p2Out = vrc6_p2Vol;
		else
			p2Out = 0;
	}
	if(vrc6_sawenable)
		vrc6_lastSawOut = sawOut = (vrc6_sawVol>>3);

	vrc6Out = p1Out+p2Out+sawOut;
}

void vrc6AudioClockTimers()
{
	if(vrc6_halt) return;

	if(vrc6_p1freqCtr)
		vrc6_p1freqCtr--;
	if(vrc6_p1freqCtr == 0)
	{
		if(vrc6_speed == 0)
			vrc6_p1freqCtr = (vrc6_freq1+1);
		else if(vrc6_speed == 1)
			vrc6_p1freqCtr = (vrc6_freq1+1)>>4;
		else
			vrc6_p1freqCtr = (vrc6_freq1+1)>>8;
		vrc6_p1Cycle++;
		if(vrc6_p1Cycle >= 16)
			vrc6_p1Cycle = 0;
	}

	if(vrc6_p2freqCtr)
		vrc6_p2freqCtr--;
	if(vrc6_p2freqCtr == 0)
	{
		if(vrc6_speed == 0)
			vrc6_p2freqCtr = (vrc6_freq2+1);
		else if(vrc6_speed == 1)
			vrc6_p2freqCtr = (vrc6_freq2+1)>>4;
		else
			vrc6_p2freqCtr = (vrc6_freq2+1)>>8;
		vrc6_p2Cycle++;
		if(vrc6_p2Cycle >= 16)
			vrc6_p2Cycle = 0;
	}

	if(vrc6_sawFreqCtr)
		vrc6_sawFreqCtr--;
	if(vrc6_sawFreqCtr == 0)
	{
		if(vrc6_speed == 0)
			vrc6_sawFreqCtr = (vrc6_sawFreq+1)*2;
		else if(vrc6_speed == 1)
			vrc6_sawFreqCtr = (vrc6_sawFreq+1)*2>>4;
		else
			vrc6_sawFreqCtr = (vrc6_sawFreq+1)*2>>8;
		vrc6_sawCycle++;
		vrc6_sawVol += vrc6_sawAdd;
		if(vrc6_sawCycle >= 7)
		{
			vrc6_sawCycle = 0;
			vrc6_sawVol = 0;
		}
	}
}

void vrc6AudioSet8(uint16_t addr, uint8_t val)
{
	if(addr == 0x9000)
	{
		vrc6_p1Vol = val&0xF;
		vrc6_p1Duty = (val>>4)&7;
		vrc6_p1const = ((val&0x80) != 0);
	}
	else if(addr == 0x9001)
	{
		vrc6_freq1 = ((vrc6_freq1&~0xFF) | val);
	}
	else if(addr == 0x9002)
	{
		vrc6_freq1 = (vrc6_freq1&0xFF) | ((val&0xF)<<8);
		vrc6_p1enable = ((val&0x80) != 0);
		if(!vrc6_p1enable)
			vrc6_p1Cycle = 0;
	}
	else if(addr == 0x9003)
	{
		vrc6_halt = ((val&1) != 0);
		if((val&6) == 0) vrc6_speed = 0;
		if((val&2) != 0) vrc6_speed = 1;
		if((val&4) != 0) vrc6_speed = 2;
	}
	else if(addr == 0xA000)
	{
		vrc6_p2Vol = val&0xF;
		vrc6_p2Duty = (val>>4)&7;
		vrc6_p2const = ((val&0x80) != 0);
	}
	else if(addr == 0xA001)
	{
		vrc6_freq2 = ((vrc6_freq2&~0xFF) | val);
	}
	else if(addr == 0xA002)
	{
		vrc6_freq2 = (vrc6_freq2&0xFF) | ((val&0xF)<<8);
		vrc6_p2enable = ((val&0x80) != 0);
		if(!vrc6_p2enable)
			vrc6_p2Cycle = 0;
	}
	else if(addr == 0xB000)
	{
		vrc6_sawAdd = (val&0x3F);
	}
	else if(addr == 0xB001)
	{
		vrc6_sawFreq = ((vrc6_sawFreq&~0xFF) | val);
	}
	else if(addr == 0xB002)
	{
		vrc6_sawFreq = (vrc6_sawFreq&0xFF) | ((val&0xF)<<8);
		vrc6_sawenable = ((val&0x80) != 0);
		if(!vrc6_sawenable)
		{
			vrc6_sawCycle = 0;
			vrc6_sawVol = 0;
		}
	}
}
