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

static uint8_t *p16_prgROM;
static uint8_t *p16_chrROM;
static uint32_t p16_prgROMsize;
static uint32_t p16_chrROMsize;
static uint32_t p16_curPRGBank;
static uint32_t p16_lastPRGBank;

static uint8_t p16_chrRAM[0x2000];

void p16init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p16_prgROM = prgROMin;
	p16_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	p16_curPRGBank = 0;
	p16_lastPRGBank = prgROMsizeIn - 0x4000;
	if(chrROMsizeIn > 0)
	{
		p16_chrROM = chrROMin;
		p16_chrROMsize = chrROMsizeIn;
		printf("p16 ???\n");
	}
	memset(p16_chrRAM,0,0x2000);
	printf("16k PRG Mapper inited\n");
}

uint8_t p16get8(uint16_t addr)
{
	if(addr < 0x8000)
		return 0;
	if(addr < 0xC000)
		return p16_prgROM[(p16_curPRGBank&~0x3FFF)+(addr&0x3FFF)];
	return p16_prgROM[(p16_lastPRGBank&~0x3FFF)+(addr&0x3FFF)];
}

void m2_set8(uint16_t addr, uint8_t val)
{
	//printf("p16set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16_curPRGBank = ((val & 15)<<14)&(p16_prgROMsize-1);
}

void m71_set8(uint16_t addr, uint8_t val)
{
	//printf("p16set8 %04x %02x\n", addr, val);
	if(addr < 0xC000)
		return;
	p16_curPRGBank = ((val & 15)<<14)&(p16_prgROMsize-1);
}

uint8_t p16chrGet8(uint16_t addr)
{
	return p16_chrRAM[addr&0x1FFF];
}

void p16chrSet8(uint16_t addr, uint8_t val)
{
	p16_chrRAM[addr&0x1FFF] = val;
}
