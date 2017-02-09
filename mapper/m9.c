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

static uint8_t *m9_prgROM;
static uint8_t *m9_prgRAM;
static uint8_t *m9_chrROM;
static uint32_t m9_prgROMsize;
static uint32_t m9_prgRAMsize;
static uint32_t m9_chrROMsize;
static uint32_t m9_curPRGBank;
static uint32_t m9_lastPRGBank;
static uint32_t m9_curCHRBank00;
static uint32_t m9_curCHRBank01;
static uint32_t m9_curCHRBank10;
static uint32_t m9_curCHRBank11;
static bool m9_CHRSelect0;
static bool m9_CHRSelect1;

void m9init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m9_prgROM = prgROMin;
	m9_prgROMsize = prgROMsizeIn;
	m9_prgRAM = prgRAMin;
	m9_prgRAMsize = prgRAMsizeIn;
	m9_curPRGBank = 0;
	m9_lastPRGBank = (prgROMsizeIn - 0x8000)&0x3FFFF;
	m9_chrROM = chrROMin;
	m9_chrROMsize = chrROMsizeIn;
	m9_curCHRBank00 = 0;
	m9_curCHRBank01 = 0;
	m9_curCHRBank10 = 0;
	m9_curCHRBank11 = 0;
	m9_CHRSelect0 = 0;
	m9_CHRSelect1 = 0;
	printf("Mapper 9 inited, last bank=%04x\n", m9_lastPRGBank);
}

uint8_t m9get8(uint16_t addr)
{
	if(addr < 0x6000)
		return 0;
	else if(addr < 0x8000)
		return m9_prgRAM[addr&0x1FFF];
	else
	{
		if(addr < 0xA000)
			return m9_prgROM[(m9_curPRGBank&~0x1FFF)+(addr&0x1FFF)];
		return m9_prgROM[m9_lastPRGBank+(addr&0x7FFF)];
	}
}

void m9set8(uint16_t addr, uint8_t val)
{
	//printf("m9set8 %04x %02x\n", addr, val);
	if(addr < 0x6000)
		return;
	else if(addr < 0x8000)
		m9_prgRAM[addr&0x1FFF] = val;
	else if(addr < 0xA000)
		return;
	else if(addr < 0xB000)
		m9_curPRGBank = ((val&0xF)<<13)&(m9_prgROMsize-1);
	else if(addr < 0xC000)
		m9_curCHRBank00 = ((val&0x1F)<<12)&(m9_chrROMsize-1);
	else if(addr < 0xD000)
		m9_curCHRBank01 = ((val&0x1F)<<12)&(m9_chrROMsize-1);
	else if(addr < 0xE000)
		m9_curCHRBank10 = ((val&0x1F)<<12)&(m9_chrROMsize-1);
	else if(addr < 0xF000)
		m9_curCHRBank11 = ((val&0x1F)<<12)&(m9_chrROMsize-1);
	else
	{
		if((val&1) == 0)
		{
			//printf("Vertical mode\n");
			ppuScreenMode = PPU_MODE_VERTICAL;
		}
		else
		{
			//printf("Horizontal mode\n");
			ppuScreenMode = PPU_MODE_HORIZONTAL;
		}
	}
}

uint8_t m9chrGet8(uint16_t addr)
{
	uint8_t ret;
	if(addr < 0x1000)
	{
		if(m9_CHRSelect0 == false)
			ret = m9_chrROM[(m9_curCHRBank00&~0xFFF)+(addr&0xFFF)];
		else
			ret = m9_chrROM[(m9_curCHRBank01&~0xFFF)+(addr&0xFFF)];
		if(addr == 0xFD8)
			m9_CHRSelect0 = false;
		else if(addr == 0xFE8)
			m9_CHRSelect0 = true;
	}
	else
	{
		if(m9_CHRSelect1 == false)
			ret = m9_chrROM[(m9_curCHRBank10&~0xFFF)+(addr&0xFFF)];
		else
			ret = m9_chrROM[(m9_curCHRBank11&~0xFFF)+(addr&0xFFF)];
		if(addr >= 0x1FD8 && addr <= 0x1FDF)
			m9_CHRSelect1 = false;
		else if(addr >= 0x1FE8 && addr <= 0x1FEF)
			m9_CHRSelect1 = true;
	}
	return ret;
}

void m9chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m9chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}
