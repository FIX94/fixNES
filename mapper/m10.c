/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "../ppu.h"

static uint8_t *m10_prgROM;
static uint8_t *m10_prgRAM;
static uint8_t *m10_chrROM;
static uint32_t m10_prgROMsize;
static uint32_t m10_prgRAMsize;
static uint32_t m10_chrROMsize;
static uint32_t m10_curPRGBank;
static uint32_t m10_lastPRGBank;
static uint32_t m10_curCHRBank00;
static uint32_t m10_curCHRBank01;
static uint32_t m10_curCHRBank10;
static uint32_t m10_curCHRBank11;
static bool m10_CHRSelect0;
static bool m10_CHRSelect1;

void m10init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m10_prgROM = prgROMin;
	m10_prgROMsize = prgROMsizeIn;
	m10_prgRAM = prgRAMin;
	m10_prgRAMsize = prgRAMsizeIn;
	m10_curPRGBank = 0;
	m10_lastPRGBank = (prgROMsizeIn - 0x4000)&0x3FFFF;
	m10_chrROM = chrROMin;
	m10_chrROMsize = chrROMsizeIn;
	m10_curCHRBank00 = 0;
	m10_curCHRBank01 = 0;
	m10_curCHRBank10 = 0;
	m10_curCHRBank11 = 0;
	m10_CHRSelect0 = 0;
	m10_CHRSelect1 = 0;
	printf("Mapper 10 inited, last bank=%04x\n", m10_lastPRGBank);
}

uint8_t m10get8(uint16_t addr)
{
	if(addr < 0x6000)
		return 0;
	else if(addr < 0x8000)
		return m10_prgRAM[addr&0x1FFF];
	else
	{
		if(addr < 0xC000)
			return m10_prgROM[(m10_curPRGBank&~0x3FFF)+(addr&0x3FFF)];
		return m10_prgROM[m10_lastPRGBank+(addr&0x3FFF)];
	}
}

void m10set8(uint16_t addr, uint8_t val)
{
	//printf("m10set8 %04x %02x\n", addr, val);
	if(addr < 0x6000)
		return;
	else if(addr < 0x8000)
		m10_prgRAM[addr&0x1FFF] = val;
	else if(addr < 0xA000)
		return;
	else if(addr < 0xB000)
		m10_curPRGBank = ((val&0xF)<<14)&(m10_prgROMsize-1);
	else if(addr < 0xC000)
		m10_curCHRBank00 = ((val&0x1F)<<12)&(m10_chrROMsize-1);
	else if(addr < 0xD000)
		m10_curCHRBank01 = ((val&0x1F)<<12)&(m10_chrROMsize-1);
	else if(addr < 0xE000)
		m10_curCHRBank10 = ((val&0x1F)<<12)&(m10_chrROMsize-1);
	else if(addr < 0xF000)
		m10_curCHRBank11 = ((val&0x1F)<<12)&(m10_chrROMsize-1);
	else
	{
		if((val&1) == 0)
		{
			//printf("Vertical mode\n");
			ppuSetNameTblVertical();
		}
		else
		{
			//printf("Horizontal mode\n");
			ppuSetNameTblHorizontal();
		}
	}
}

uint8_t m10chrGet8(uint16_t addr)
{
	uint8_t ret;
	if(addr < 0x1000)
	{
		if(m10_CHRSelect0 == false)
			ret = m10_chrROM[(m10_curCHRBank00&~0xFFF)+(addr&0xFFF)];
		else
			ret = m10_chrROM[(m10_curCHRBank01&~0xFFF)+(addr&0xFFF)];
		if(addr >= 0xFD8 && addr <= 0xFDF)
			m10_CHRSelect0 = false;
		else if(addr >= 0xFE8 && addr <= 0xFEF)
			m10_CHRSelect0 = true;
	}
	else
	{
		if(m10_CHRSelect1 == false)
			ret = m10_chrROM[(m10_curCHRBank10&~0xFFF)+(addr&0xFFF)];
		else
			ret = m10_chrROM[(m10_curCHRBank11&~0xFFF)+(addr&0xFFF)];
		if(addr >= 0x1FD8 && addr <= 0x1FDF)
			m10_CHRSelect1 = false;
		else if(addr >= 0x1FE8 && addr <= 0x1FEF)
			m10_CHRSelect1 = true;
	}
	return ret;
}

void m10chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m10chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}
