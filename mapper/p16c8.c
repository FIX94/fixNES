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

static uint8_t *p16c8_prgROM;
static uint8_t *p16c8_chrROM;
static uint32_t p16c8_prgROMsize;
static uint32_t p16c8_chrROMsize;
static uint32_t p16c8_curPRGBank;
static uint32_t p16c8_curCHRBank;
static uint32_t p16c8_lastPRGBank;

static uint8_t p16c8_chrRAM[0x2000];

void p16c8init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p16c8_prgROM = prgROMin;
	p16c8_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	p16c8_curPRGBank = 0;
	p16c8_lastPRGBank = prgROMsizeIn - 0x4000;
	if(chrROMsizeIn > 0)
	{
		p16c8_chrROM = chrROMin;
		p16c8_chrROMsize = chrROMsizeIn;
	}
	else
	{
		p16c8_chrROM = p16c8_chrRAM;
		p16c8_chrROMsize = 0x2000;
		memset(p16c8_chrRAM,0,0x2000);
	}
	p16c8_curCHRBank = 0;
	printf("16k PRG 8k CHR Mapper inited\n");
}

uint8_t p16c8get8(uint16_t addr)
{
	if(addr < 0x8000)
		return 0;
	if(addr < 0xC000)
		return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&(p16c8_prgROMsize-1)];
	return p16c8_prgROM[((p16c8_lastPRGBank&~0x3FFF)+(addr&0x3FFF))&(p16c8_prgROMsize-1)];
}

void m2_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 15)<<14)&(p16c8_prgROMsize-1);
}

void m70_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val >> 4)<<14)&(p16c8_prgROMsize-1);
	p16c8_curCHRBank = ((val & 0xF)<<13)&(p16c8_chrROMsize-1);
}

void m71_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0xC000)
		return;
	p16c8_curPRGBank = ((val & 15)<<14)&(p16c8_prgROMsize-1);
}

void m152_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = (((val >> 4)&7)<<14)&(p16c8_prgROMsize-1);
	p16c8_curCHRBank = ((val & 0xF)<<13)&(p16c8_chrROMsize-1);
	if((val&0x80) == 0)
		ppuSetNameTblSingleLower();
	else
		ppuSetNameTblSingleUpper();
}

void m78a_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 7)<<14)&(p16c8_prgROMsize-1);
	p16c8_curCHRBank = (((val>>4) & 0xF)<<13)&(p16c8_chrROMsize-1);
	if((val&0x8) == 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m78b_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 7)<<14)&(p16c8_prgROMsize-1);
	p16c8_curCHRBank = (((val>>4) & 0xF)<<13)&(p16c8_chrROMsize-1);
	if((val&0x8) == 0)
		ppuSetNameTblSingleLower();
	else
		ppuSetNameTblSingleUpper();
}

uint8_t p16c8chrGet8(uint16_t addr)
{
	if(p16c8_chrROM == p16c8_chrRAM) //Writable
		return p16c8_chrROM[addr&0x1FFF];
	return p16c8_chrROM[((p16c8_curCHRBank&~0x1FFF)+(addr&0x1FFF))&(p16c8_chrROMsize-1)];
}

void p16c8chrSet8(uint16_t addr, uint8_t val)
{
	if(p16c8_chrROM == p16c8_chrRAM) //Writable
		p16c8_chrROM[addr&0x1FFF] = val;
}
