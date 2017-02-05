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

static uint8_t *m2_prgROM;
static uint8_t *m2_chrROM;
static uint32_t m2_prgROMsize;
static uint32_t m2_chrROMsize;
static uint32_t m2_curPRGBank;
static uint32_t m2_lastPRGBank;

static uint8_t m2_chrRAM[0x2000];
extern bool ppuForceTable;
extern uint16_t ppuForceTableAddr;

void m2init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m2_prgROM = prgROMin;
	m2_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	m2_curPRGBank = 0;
	m2_lastPRGBank = prgROMsizeIn - 0x4000;
	if(chrROMsizeIn > 0)
	{
		m2_chrROM = chrROMin;
		m2_chrROMsize = chrROMsizeIn;
		printf("m2 ???\n");
	}
	memset(m2_chrRAM,0,0x2000);
	printf("Mapper 2 inited\n");
}

uint8_t m2get8(uint16_t addr)
{
	if(addr < 0x8000)
		return 0;
	if(addr < 0xC000)
		return m2_prgROM[(m2_curPRGBank&~0x3FFF)+(addr&0x3FFF)];
	return m2_prgROM[(m2_lastPRGBank&~0x3FFF)+(addr&0x3FFF)];
}

void m2set8(uint16_t addr, uint8_t val)
{
	//printf("m1set8 %04x %02x\n", addr, val);
	(void)addr;
	m2_curPRGBank = ((val & 15) * 0x4000)&(m2_prgROMsize-1);
}

uint8_t m2chrGet8(uint16_t addr)
{
	return m2_chrRAM[addr&0x1FFF];
}

void m2chrSet8(uint16_t addr, uint8_t val)
{
	m2_chrRAM[addr&0x1FFF] = val;
}
