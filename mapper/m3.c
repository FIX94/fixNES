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

static uint8_t *m3_prgROM;
static uint8_t *m3_chrROM;
static uint32_t m3_prgROMsize;
static uint32_t m3_chrROMsize;
static uint32_t m3_curCHRBank;

void m3init(uint8_t *prgROMin, uint32_t prgROMsizeIn,
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m3_prgROM = prgROMin;
	m3_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	m3_chrROM = chrROMin;
	m3_chrROMsize = chrROMsizeIn;
	m3_curCHRBank = 0;
	printf("Mapper 3 inited\n");
}

uint8_t m3get8(uint16_t addr)
{
	if(addr < 0x8000)
		return 0;
	if(m3_prgROMsize == 0x8000)
		return m3_prgROM[addr&0x7FFF];
	return m3_prgROM[addr&0x3FFF];
}

void m3set8(uint16_t addr, uint8_t val)
{
	//printf("%04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	m3_curCHRBank = ((val * 0x2000)&(m3_chrROMsize-1));
}

uint8_t m3chrGet8(uint16_t addr)
{
	return m3_chrROM[m3_curCHRBank+(addr&0x1FFF)];
}

void m3chrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}
