/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>

static uint8_t *m13_prgROM;
static uint32_t m13_prgROMsize;
static uint32_t m13_curCHRBank;
static uint8_t m13_chrRAM[0x4000];

void m13init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn, 
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m13_prgROM = prgROMin;
	m13_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	if(chrROMsizeIn > 0)
		printf("m13 ???\n");
	(void)chrROMin;
	m13_curCHRBank = 0;
	memset(m13_chrRAM,0,0x4000);
	printf("Mapper 13 inited\n");
}

uint8_t m13get8(uint16_t addr)
{
	if(addr < 0x8000)
		return 0;
	if(m13_prgROMsize == 0x8000)
		return m13_prgROM[addr&0x7FFF];
	return m13_prgROM[addr&0x3FFF];
}


void m13set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	m13_curCHRBank = ((val & 3)<<12);
}

uint8_t m13chrGet8(uint16_t addr)
{
	if(addr < 0x1000)
		return m13_chrRAM[addr&0xFFF];
	return m13_chrRAM[(m13_curCHRBank&~0xFFF)+(addr&0xFFF)];
}

void m13chrSet8(uint16_t addr, uint8_t val)
{
	if(addr < 0x1000)
		m13_chrRAM[addr&0xFFF] = val;
	else
		m13_chrRAM[(m13_curCHRBank&~0xFFF)+(addr&0xFFF)] = val;
}
