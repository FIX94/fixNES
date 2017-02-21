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

static uint8_t *p32c4_prgROM;
static uint8_t *p32c4_prgRAM;
static uint8_t *p32c4_chrROM;
static uint8_t p32c4_chrRAM[0x2000];
static uint32_t p32c4_prgROMsize;
static uint32_t p32c4_prgRAMsize;
static uint32_t p32c4_chrROMsize;
static uint32_t p32c4_curPRGBank;
static uint32_t p32c4_curCHRBank0;
static uint32_t p32c4_curCHRBank1;

void p32c4init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p32c4_prgROM = prgROMin;
	p32c4_prgROMsize = prgROMsizeIn;
	p32c4_prgRAM = prgRAMin;
	p32c4_prgRAMsize = prgRAMsizeIn;
	p32c4_curPRGBank = 0;
	if(chrROMin && chrROMsizeIn)
	{
		p32c4_chrROM = chrROMin;
		p32c4_chrROMsize = chrROMsizeIn;
	}
	else
	{
		p32c4_chrROM = p32c4_chrRAM;
		p32c4_chrROMsize = 0x2000;
		memset(p32c4_chrRAM,0,0x2000);
	}
	p32c4_curCHRBank0 = 0;
	p32c4_curCHRBank1 = 0;
	printf("32k PRG 4k CHR Mapper inited\n");
}

uint8_t p32c4get8(uint16_t addr)
{
	if(addr >= 0x6000 && addr < 0x8000 && p32c4_prgRAMsize)
		return p32c4_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
		return p32c4_prgROM[((p32c4_curPRGBank&~0x7FFF)+(addr&0x7FFF))&(p32c4_prgROMsize-1)];
	return 0;
}

void p32c4set8(uint16_t addr, uint8_t val)
{
	//printf("p32c4set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x7FFD && p32c4_prgRAMsize)
		p32c4_prgRAM[addr&0x1FFF] = val;
	else if(addr == 0x7FFD || addr >= 0x8000)
		p32c4_curPRGBank = ((val & 0xF)<<15)&(p32c4_prgROMsize-1);
	else if(addr == 0x7FFE)
		p32c4_curCHRBank0 = ((val & 0xF)<<12)&(p32c4_chrROMsize-1);
	else if(addr == 0x7FFF)
		p32c4_curCHRBank1 = ((val & 0xF)<<12)&(p32c4_chrROMsize-1);
}

uint8_t p32c4chrGet8(uint16_t addr)
{
	if(p32c4_chrROM == p32c4_chrRAM) //Writable
		return p32c4_chrROM[addr&0x1FFF];
	if(addr < 0x1000) //Banked
		return p32c4_chrROM[((p32c4_curCHRBank0&~0xFFF)+(addr&0xFFF))&(p32c4_chrROMsize-1)];
	return p32c4_chrROM[((p32c4_curCHRBank1&~0xFFF)+(addr&0xFFF))&(p32c4_chrROMsize-1)];
}

void p32c4chrSet8(uint16_t addr, uint8_t val)
{
	//printf("p32c4chrSet8 %04x %02x\n", addr, val);
	if(p32c4_chrROM == p32c4_chrRAM) //Writable
		p32c4_chrROM[addr&0x1FFF] = val;
}
