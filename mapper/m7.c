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

static uint8_t *m7_prgROM;
static uint8_t *m7_chrROM;
static uint32_t m7_prgROMsize;
static uint32_t m7_chrROMsize;
static uint32_t m7_curPRGBank;

static uint8_t m7_chrRAM[0x2000];

void m7init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn, 
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m7_prgROM = prgROMin;
	m7_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	m7_curPRGBank = (prgROMsizeIn - 0x8000)&(m7_prgROMsize-1);
	if(chrROMsizeIn > 0)
	{
		m7_chrROM = chrROMin;
		m7_chrROMsize = chrROMsizeIn;
		printf("M7 ???\n");
	}
	memset(m7_chrRAM,0,0x2000);
	ppuSetNameTblSingleLower();
	printf("Mapper 7 inited\n");
}

uint8_t m7get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	return m7_prgROM[(m7_curPRGBank&~0x7FFF)+(addr&0x7FFF)];
}

void m7set8(uint16_t addr, uint8_t val)
{
	//printf("m7set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	if(val & (1<<4))
	{
		//printf("400\n");
		ppuSetNameTblSingleUpper();
	}
	else
	{
		//printf("000\n");
		ppuSetNameTblSingleLower();
	}
	m7_curPRGBank = ((val & 7)<<15)&(m7_prgROMsize-1);
}

uint8_t m7chrGet8(uint16_t addr)
{
	return m7_chrRAM[addr&0x1FFF];
}

void m7chrSet8(uint16_t addr, uint8_t val)
{
	m7_chrRAM[addr&0x1FFF] = val;
}
