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
#include "../ppu.h"
#include "../mapper.h"

static uint8_t *p16c4_prgROM;
static uint8_t *p16c4_chrROM;
static uint32_t p16c4_prgROMsize;
static uint32_t p16c4_chrROMsize;
static uint32_t p16c4_prgROMand;
static uint32_t p16c4_chrROMand;
static uint32_t p16c4_firstPRGBank;
static uint32_t p16c4_curPRGBank;
static uint32_t p16c4_curCHRBank0;
static uint32_t p16c4_curCHRBank1;
static uint32_t p16c4_lastPRGBank;

static uint8_t p16c4_chrRAM[0x2000];

void p16c4init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p16c4_prgROM = prgROMin;
	p16c4_prgROMsize = prgROMsizeIn;
	p16c4_prgROMand = mapperGetAndValue(p16c4_prgROMsize);
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	p16c4_firstPRGBank = 0;
	p16c4_curPRGBank = p16c4_firstPRGBank;
	p16c4_lastPRGBank = prgROMsizeIn - 0x4000;
	if(chrROMsizeIn > 0)
	{
		p16c4_chrROM = chrROMin;
		p16c4_chrROMsize = chrROMsizeIn;
		p16c4_chrROMand = mapperGetAndValue(p16c4_chrROMsize);
	}
	else
	{
		p16c4_chrROM = p16c4_chrRAM;
		p16c4_chrROMsize = 0x2000;
		p16c4_chrROMand = 0x1FFF;
		memset(p16c4_chrRAM,0,0x2000);
	}
	p16c4_curCHRBank0 = 0;
	p16c4_curCHRBank1 = 0;
	printf("16k PRG 4k CHR Mapper inited\n");
}

uint8_t p16c4get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(addr < 0xC000)
		return p16c4_prgROM[((p16c4_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c4_prgROMand];
	return p16c4_prgROM[((p16c4_lastPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c4_prgROMand];
}

void m184_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c4set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000)
	{
		p16c4_curCHRBank0 = ((val & 0x7)<<12)&p16c4_chrROMand;
		p16c4_curCHRBank1 = (((val>>4) & 0x7)<<12)&p16c4_chrROMand;
	}
	else if(addr >= 0x8000)
		p16c4_curPRGBank = (((val>>4) & 0x7)<<14)&p16c4_prgROMand;
}

uint8_t p16c4chrGet8(uint16_t addr)
{
	if(p16c4_chrROM == p16c4_chrRAM) //Writable
		return p16c4_chrROM[addr&0x1FFF];
	if(addr < 0x1000) //Banked
		return p16c4_chrROM[((p16c4_curCHRBank0&~0xFFF)+(addr&0xFFF))&p16c4_chrROMand];
	return p16c4_chrROM[((p16c4_curCHRBank1&~0xFFF)+(addr&0xFFF))&p16c4_chrROMand];
}

void p16c4chrSet8(uint16_t addr, uint8_t val)
{
	if(p16c4_chrROM == p16c4_chrRAM) //Writable
		p16c4_chrROM[addr&0x1FFF] = val;
}
