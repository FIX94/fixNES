/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "../ppu.h"
#include "../audio_vrc6.h"
#include "../vrc_irq.h"

static uint8_t *vrc6_prgROM;
static uint8_t *vrc6_prgRAM;
static uint8_t *vrc6_chrROM;
static uint32_t vrc6_prgROMsize;
static uint32_t vrc6_prgRAMsize;
static uint32_t vrc6_chrROMsize;
static uint32_t vrc6_curPRGBank0;
static uint32_t vrc6_curPRGBank1;
static uint32_t vrc6_lastPRGBank;
static uint32_t vrc6_CHRBank[8];
static uint8_t vrc6_CHRMode;
static uint32_t vrc6_prgROMand;
static uint32_t vrc6_chrROMand;

void vrc6init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	vrc6_prgROM = prgROMin;
	vrc6_prgROMsize = prgROMsizeIn;
	vrc6_prgRAM = prgRAMin;
	vrc6_prgRAMsize = prgRAMsizeIn;
	vrc6_curPRGBank0 = 0;
	vrc6_curPRGBank1 = 0x2000;
	vrc6_lastPRGBank = (prgROMsizeIn - 0x2000);
	vrc6_prgROMand = prgROMsizeIn-1;
	vrc6_chrROM = chrROMin;
	vrc6_chrROMsize = chrROMsizeIn;
	vrc6_chrROMand = chrROMsizeIn-1;
	memset(vrc6_CHRBank, 0, 8*sizeof(uint32_t));
	vrc6_CHRMode = 0;
	vrc_irq_init();
	vrc6AudioInit();
	printf("VRC6 Mapper inited\n");
}

uint8_t vrc6get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return vrc6_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xC000)
			return vrc6_prgROM[(((vrc6_curPRGBank0<<14)+(addr&0x3FFF))&vrc6_prgROMand)];
		else if(addr < 0xE000)
			return vrc6_prgROM[(((vrc6_curPRGBank1<<13)+(addr&0x1FFF))&vrc6_prgROMand)];
		return vrc6_prgROM[((vrc6_lastPRGBank+(addr&0x1FFF))&vrc6_prgROMand)];
	}
	return val;
}

void vrc6set8(uint16_t addr, uint8_t val)
{
	//select all possible addresses
	addr &= 0xF003;
	if(addr == 0x8000 || addr == 0x8001 || addr == 0x8002 || addr == 0x8003)
		vrc6_curPRGBank0 = (val&0xF);
	else if(addr == 0xB003)
	{
		if((val & 0x3) == 0)
			vrc6_CHRMode = 0;
		else if((val & 0x3) == 1)
			vrc6_CHRMode = 1;
		else
			vrc6_CHRMode = 2;
		if(!ppu4Screen)
		{
			if((val & 0xF) == 0 || (val & 0xF) == 7)
				ppuSetNameTblVertical();
			else if((val & 0xF) == 4 || (val & 0xF) == 3)
				ppuSetNameTblHorizontal();
			if((val & 0xF) == 8 || (val & 0xF) == 0xF)
				ppuSetNameTblSingleLower();
			if((val & 0xF) == 0xC || (val & 0xF) == 0xB)
				ppuSetNameTblSingleUpper();
		}
	}
	else if(addr == 0xC000 || addr == 0xC001 || addr == 0xC002 || addr == 0xC003)
		vrc6_curPRGBank1 = (val&0x1F);
	else if(addr == 0xD000)
		vrc6_CHRBank[0] = val;
	else if(addr == 0xD001)
		vrc6_CHRBank[1] = val;
	else if(addr == 0xD002)
		vrc6_CHRBank[2] = val;
	else if(addr == 0xD003)
		vrc6_CHRBank[3] = val;
	else if(addr == 0xE000)
		vrc6_CHRBank[4] = val;
	else if(addr == 0xE001)
		vrc6_CHRBank[5] = val;
	else if(addr == 0xE002)
		vrc6_CHRBank[6] = val;
	else if(addr == 0xE003)
		vrc6_CHRBank[7] = val;
	else if(addr == 0xF000)
		vrc_irq_setlatch(val);
	else if(addr == 0xF001)
		vrc_irq_control(val);
	else if(addr == 0xF002)
		vrc_irq_ack();
	else
		vrc6AudioSet8(addr, val);
}

void m24_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc6_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m24_set8 %04x %02x\n", addr, val);
	vrc6set8(addr, val);
}

void m26_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc6_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m26_set8 %04x %02x\n", addr, val);
	//vrc6 M26 Address Setup
	uint8_t tmp = addr&3;
	addr &= ~0x3; addr |= (((tmp&1)<<1) | ((tmp&2)>>1));
	vrc6set8(addr, val);
}

uint8_t vrc6chrGet8(uint16_t addr)
{
	if(vrc6_CHRMode == 0)
	{
		if(addr < 0x400)
			return vrc6_chrROM[(((vrc6_CHRBank[0]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x800)
			return vrc6_chrROM[(((vrc6_CHRBank[1]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0xC00)
			return vrc6_chrROM[(((vrc6_CHRBank[2]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x1000)
			return vrc6_chrROM[(((vrc6_CHRBank[3]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x1400)
			return vrc6_chrROM[(((vrc6_CHRBank[4]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x1800)
			return vrc6_chrROM[(((vrc6_CHRBank[5]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x1C00)
			return vrc6_chrROM[(((vrc6_CHRBank[6]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else // < 0x2000
			return vrc6_chrROM[(((vrc6_CHRBank[7]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
	}
	else if(vrc6_CHRMode == 1)
	{
		if(addr < 0x800)
			return vrc6_chrROM[(((vrc6_CHRBank[0]<<11)+(addr&0x7FF))&vrc6_chrROMand)];
		else if(addr < 0x1000)
			return vrc6_chrROM[(((vrc6_CHRBank[1]<<11)+(addr&0x7FF))&vrc6_chrROMand)];
		else if(addr < 0x1800)
			return vrc6_chrROM[(((vrc6_CHRBank[2]<<11)+(addr&0x7FF))&vrc6_chrROMand)];
		else // < 0x200
			return vrc6_chrROM[(((vrc6_CHRBank[3]<<11)+(addr&0x7FF))&vrc6_chrROMand)];
	}
	else
	{
		if(addr < 0x400)
			return vrc6_chrROM[(((vrc6_CHRBank[0]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x800)
			return vrc6_chrROM[(((vrc6_CHRBank[1]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0xC00)
			return vrc6_chrROM[(((vrc6_CHRBank[2]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x1000)
			return vrc6_chrROM[(((vrc6_CHRBank[3]<<10)+(addr&0x3FF))&vrc6_chrROMand)];
		else if(addr < 0x1800)
			return vrc6_chrROM[(((vrc6_CHRBank[4]<<11)+(addr&0x7FF))&vrc6_chrROMand)];
		else // < 0x2000
			return vrc6_chrROM[(((vrc6_CHRBank[5]<<11)+(addr&0x7FF))&vrc6_chrROMand)];
	}
}

void vrc6chrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}

void vrc6cycle()
{
	vrc6AudioClockTimers();
	vrc_irq_cycle();
}
