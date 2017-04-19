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
#include "../audio_vrc7.h"
#include "../vrc_irq.h"

static uint8_t *vrc7_prgROM;
static uint8_t *vrc7_prgRAM;
static uint8_t *vrc7_chrROM;
static uint8_t vrc7_chrRAM[0x2000];
static uint32_t vrc7_prgROMsize;
static uint32_t vrc7_prgRAMsize;
static uint32_t vrc7_chrROMsize;
static uint32_t vrc7_curPRGBank0;
static uint32_t vrc7_curPRGBank1;
static uint32_t vrc7_curPRGBank2;
static uint32_t vrc7_lastPRGBank;
static uint32_t vrc7_CHRBank[8];
static uint32_t vrc7_prgROMand;
static uint32_t vrc7_chrROMand;
static uint8_t vrc7_audioReg;

void vrc7init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	vrc7_prgROM = prgROMin;
	vrc7_prgROMsize = prgROMsizeIn;
	vrc7_prgRAM = prgRAMin;
	vrc7_prgRAMsize = prgRAMsizeIn;
	vrc7_curPRGBank0 = 0;
	vrc7_curPRGBank1 = 0x2000;
	vrc7_curPRGBank2 = 0x4000;
	vrc7_lastPRGBank = (prgROMsizeIn - 0x2000);
	vrc7_prgROMand = prgROMsizeIn-1;
	if(chrROMin && chrROMsizeIn)
	{
		vrc7_chrROM = chrROMin;
		vrc7_chrROMsize = chrROMsizeIn;
		vrc7_chrROMand = chrROMsizeIn-1;
	}
	else
	{
		vrc7_chrROM = vrc7_chrRAM;
		vrc7_chrROMsize = 0x2000;
		vrc7_chrROMand = 0x1FFF;
	}
	memset(vrc7_CHRBank, 0, 8*sizeof(uint32_t));
	vrc7AudioInit();
	vrc7_audioReg = 0;
	vrc_irq_init();
	printf("vrc7 Mapper inited\n");
}

uint8_t vrc7get8(uint16_t addr)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return vrc7_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return vrc7_prgROM[(((vrc7_curPRGBank0<<13)+(addr&0x1FFF))&vrc7_prgROMand)];
		if(addr < 0xC000)
			return vrc7_prgROM[(((vrc7_curPRGBank1<<13)+(addr&0x1FFF))&vrc7_prgROMand)];
		else if(addr < 0xE000)
			return vrc7_prgROM[(((vrc7_curPRGBank2<<13)+(addr&0x1FFF))&vrc7_prgROMand)];
		return vrc7_prgROM[((vrc7_lastPRGBank+(addr&0x1FFF))&vrc7_prgROMand)];
	}
	return 0;
}

void vrc7set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc7_prgRAM[addr&0x1FFF] = val;
		return;
	}
	uint16_t addrHigh = (addr & 0xF000);
	uint8_t addrLow = (addr & 0x18);
	switch(addrHigh)
	{
		case 0x8000:
			if(addrLow == 0)
				vrc7_curPRGBank0 = (val&0x3F);
			else //0x8 or 0x10
				vrc7_curPRGBank1 = (val&0x3F);
			break;
		case 0x9000:
			if(addrLow == 0)
				vrc7_curPRGBank2 = (val&0x3F);
			else if(addr == 0x9010)
				vrc7_audioReg = (val&0x3F);
			else if(addr == 0x9030)
				vrc7AudioSet8(vrc7_audioReg, val);
			break;
		case 0xA000:
			if(addrLow == 0)
				vrc7_CHRBank[0] = val;
			else //0x8 or 0x10
				vrc7_CHRBank[1] = val;
			break;
		case 0xB000:
			if(addrLow == 0)
				vrc7_CHRBank[2] = val;
			else //0x8 or 0x10
				vrc7_CHRBank[3] = val;
			break;
		case 0xC000:
			if(addrLow == 0)
				vrc7_CHRBank[4] = val;
			else //0x8 or 0x10
				vrc7_CHRBank[5] = val;
			break;
		case 0xD000:
			if(addrLow == 0)
				vrc7_CHRBank[6] = val;
			else //0x8 or 0x10
				vrc7_CHRBank[7] = val;
			break;
		case 0xE000:
			if(addrLow == 0)
			{
				if(!ppu4Screen)
				{
					if((val & 0x3) == 0)
						ppuSetNameTblVertical();
					else if((val & 0x3) == 1)
						ppuSetNameTblHorizontal();
					else if((val & 0x3) == 2)
						ppuSetNameTblSingleLower();
					else //if((val & 0x3) == 3)
						ppuSetNameTblSingleUpper();
				}
			}
			else //0x8 or 0x10
				vrc_irq_setlatch(val);
			break;
		case 0xF000:
			if(addrLow == 0)
				vrc_irq_control(val);
			else //0x8 or 0x10
				vrc_irq_ack();
			break;
		default: //should never happen
			break;
	}
}

uint8_t vrc7chrGet8(uint16_t addr)
{
	if(addr < 0x400)
		return vrc7_chrROM[(((vrc7_CHRBank[0]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else if(addr < 0x800)
		return vrc7_chrROM[(((vrc7_CHRBank[1]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else if(addr < 0xC00)
		return vrc7_chrROM[(((vrc7_CHRBank[2]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else if(addr < 0x1000)
		return vrc7_chrROM[(((vrc7_CHRBank[3]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else if(addr < 0x1400)
		return vrc7_chrROM[(((vrc7_CHRBank[4]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else if(addr < 0x1800)
		return vrc7_chrROM[(((vrc7_CHRBank[5]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else if(addr < 0x1C00)
		return vrc7_chrROM[(((vrc7_CHRBank[6]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
	else // < 0x2000
		return vrc7_chrROM[(((vrc7_CHRBank[7]<<10)+(addr&0x3FF))&vrc7_chrROMand)];
}

void vrc7chrSet8(uint16_t addr, uint8_t val)
{
	if(vrc7_chrROM == vrc7_chrRAM)
		vrc7_chrRAM[addr&0x1FFF] = val;
}

void vrc7cycle()
{
	vrc_irq_cycle();
}
