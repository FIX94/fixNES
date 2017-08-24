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
#include "../mapper.h"

static uint8_t *vrc1_prgROM;
static uint8_t *vrc1_prgRAM;
static uint8_t *vrc1_chrROM;
static uint32_t vrc1_prgROMsize;
static uint32_t vrc1_prgRAMsize;
static uint32_t vrc1_chrROMsize;
static uint32_t vrc1_curPRGBank0;
static uint32_t vrc1_curPRGBank1;
static uint32_t vrc1_curPRGBank2;
static uint32_t vrc1_lastPRGBank;
static uint32_t vrc1_curCHRBank0;
static uint32_t vrc1_curCHRBank1;
static uint32_t vrc1_prgROMand;
static uint32_t vrc1_chrROMand;
static bool vrc1_prg_bank_flip;

void vrc1init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	vrc1_prgROM = prgROMin;
	vrc1_prgROMsize = prgROMsizeIn;
	vrc1_prgRAM = prgRAMin;
	vrc1_prgRAMsize = prgRAMsizeIn;
	vrc1_curPRGBank0 = 0;
	vrc1_curPRGBank1 = 0x2000;
	vrc1_curPRGBank2 = 0x4000;
	vrc1_lastPRGBank = (prgROMsizeIn - 0x2000);
	vrc1_prgROMand = mapperGetAndValue(prgROMsizeIn);
	vrc1_chrROM = chrROMin;
	vrc1_chrROMsize = chrROMsizeIn;
	vrc1_chrROMand = mapperGetAndValue(chrROMsizeIn);
	vrc1_curCHRBank0 = 0;
	vrc1_curCHRBank1 = 0;
	vrc1_prg_bank_flip = false;
	printf("vrc1 Mapper inited\n");
}

uint8_t vrc1get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return vrc1_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return vrc1_prgROM[(((vrc1_curPRGBank0<<13)+(addr&0x1FFF))&vrc1_prgROMand)];
		else if(addr < 0xC000)
			return vrc1_prgROM[(((vrc1_curPRGBank1<<13)+(addr&0x1FFF))&vrc1_prgROMand)];
		else if(addr < 0xE000)
			return vrc1_prgROM[(((vrc1_curPRGBank2<<13)+(addr&0x1FFF))&vrc1_prgROMand)];
		return vrc1_prgROM[((vrc1_lastPRGBank+(addr&0x1FFF))&vrc1_prgROMand)];
	}
	return val;
}

void vrc1set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x8000 && addr < 0x9000)
		vrc1_curPRGBank0 = (val&0xF);
	else if(addr >= 0x9000 && addr < 0xA000)
	{
		if((val&0x1) != 0)
			ppuSetNameTblHorizontal();
		else
			ppuSetNameTblVertical();
		vrc1_curCHRBank0 &= 0xF;
		if((val&0x2) != 0)
			vrc1_curCHRBank0 |= 0x10;
		vrc1_curCHRBank1 &= 0xF;
		if((val&0x4) != 0)
			vrc1_curCHRBank1 |= 0x10;
	}
	else if(addr >= 0xA000 && addr < 0xB000)
		vrc1_curPRGBank1 = (val&0xF);
	else if(addr >= 0xC000 && addr < 0xD000)
		vrc1_curPRGBank2 = (val&0xF);
	else if(addr >= 0xE000 && addr < 0xF000)
		vrc1_curCHRBank0 = (vrc1_curCHRBank0&~0xF) | (val&0xF);
	else if(addr >= 0xF000)
		vrc1_curCHRBank1 = (vrc1_curCHRBank1&~0xF) | (val&0xF);
}

uint8_t vrc1chrGet8(uint16_t addr)
{
	if(addr < 0x1000)
		return vrc1_chrROM[(((vrc1_curCHRBank0<<12)+(addr&0xFFF))&vrc1_chrROMand)];
	else // < 0x2000
		return vrc1_chrROM[(((vrc1_curCHRBank1<<12)+(addr&0xFFF))&vrc1_chrROMand)];
}

void vrc1chrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}
