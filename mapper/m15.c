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

static uint8_t *m15_prgROM;
static uint8_t *m15_prgRAM;
static uint8_t *m15_chrROM;
static uint32_t m15_prgROMsize;
static uint32_t m15_prgRAMsize;
static uint32_t m15_chrROMsize;
static uint32_t m15_curPRGBank;
static uint32_t m15_lastPRGBank;
static uint8_t m15_bankMode;
static bool m15_upperPRGBank;

static uint8_t m15_chrRAM[0x2000];
extern uint16_t ppuForceTableAddr;

void m15init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m15_prgROM = prgROMin;
	m15_prgROMsize = prgROMsizeIn;
	m15_prgRAM = prgRAMin;
	m15_prgRAMsize = prgRAMsizeIn;
	m15_curPRGBank = 0;
	m15_bankMode = 0;
	m15_upperPRGBank = false;
	m15_lastPRGBank = prgROMsizeIn - 0x4000;
	if(chrROMsizeIn > 0)
	{
		m15_chrROM = chrROMin;
		m15_chrROMsize = chrROMsizeIn;
		printf("m15 ???\n");
	}
	memset(m15_chrRAM,0,0x2000);
	printf("Mapper 15 inited\n");
}

uint8_t m15get8(uint16_t addr)
{
	if(addr < 0x6000)
		return 0;
	else if(addr < 0x8000)
		return m15_prgRAM[addr&0x1FFF];
	switch(m15_bankMode)
	{
		case 0:
			if(addr < 0xC000)
				return m15_prgROM[(m15_curPRGBank&~0x3FFF)+(addr&0x3FFF)];
			return m15_prgROM[((m15_curPRGBank|0x4000)&~0x3FFF)+(addr&0x3FFF)];
		case 1:
			if(addr < 0xC000)
				return m15_prgROM[(m15_curPRGBank&~0x3FFF)+(addr&0x3FFF)];
			return m15_prgROM[(m15_lastPRGBank&~0x3FFF)+(addr&0x3FFF)];
		case 2:
			if(m15_upperPRGBank == false)
				return m15_prgROM[(m15_curPRGBank&~0x3FFF)+(addr&0x1FFF)];
			return m15_prgROM[((m15_curPRGBank&~0x3FFF)|0x2000)+(addr&0x1FFF)];
		case 3:
		default:
			return m15_prgROM[(m15_curPRGBank&~0x3FFF)+(addr&0x3FFF)];
	}
}

void m15set8(uint16_t addr, uint8_t val)
{
	//printf("m1set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000)
		m15_prgRAM[addr&0x1FFF] = val;
	else if(addr >= 0x8000)
	{
		m15_bankMode = (addr&3);
		m15_upperPRGBank = ((val&(1<<7)) != 0);
		m15_curPRGBank = ((val&0x3F)<<14)&(m15_prgROMsize-1);
		if((val&(1<<6)) == 0)
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

uint8_t m15chrGet8(uint16_t addr)
{
	return m15_chrRAM[addr&0x1FFF];
}

void m15chrSet8(uint16_t addr, uint8_t val)
{
	m15_chrRAM[addr&0x1FFF] = val;
}
