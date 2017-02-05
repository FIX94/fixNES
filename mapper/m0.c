/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>

static uint8_t *m0_prgROM;
static uint8_t *m0_chrROM;
static uint32_t m0_prgROMsize;
static uint32_t m0_chrROMsize;
static uint8_t m0_chrRAM[0x2000];

void m0init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn, 
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m0_prgROM = prgROMin;
	m0_prgROMsize = prgROMsizeIn;
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	if(chrROMsizeIn > 0)
	{
		m0_chrROM = chrROMin;
		m0_chrROMsize = chrROMsizeIn;
	}
	else
	{
		m0_chrROM = m0_chrRAM;
		m0_chrROMsize = 0x2000;
	}
	memset(m0_chrRAM,0,0x2000);
	printf("Mapper 0 inited\n");
}

uint8_t m0get8(uint16_t addr)
{
	if(addr < 0x8000)
		return m0_chrRAM[addr&0x1FFF];
	if(m0_prgROMsize == 0x8000)
		return m0_prgROM[addr&0x7FFF];
	return m0_prgROM[addr&0x3FFF];
}


void m0set8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}

uint8_t m0chrGet8(uint16_t addr)
{
	return m0_chrROM[addr&0x1FFF];
}

void m0chrSet8(uint16_t addr, uint8_t val)
{
	if(m0_chrROM == m0_chrRAM)
		m0_chrROM[addr&0x1FFF] = val;
}
