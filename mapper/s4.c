/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "../ppu.h"
#include "../mapper.h"

static uint8_t *s4_prgROM;
static uint8_t *s4_prgRAM;
static uint8_t *s4_chrROM;
static uint32_t s4_prgROMsize;
static uint32_t s4_prgRAMsize;
static uint32_t s4_chrROMsize;
static uint32_t s4_prgROMand;
static uint32_t s4_chrROMand;
static uint8_t s4_VRAM[0x800];
static uint32_t s4_curPRGBank;
static uint32_t s4_lastPRGBank;
static uint32_t s4_CHRBank[4];
static uint32_t s4_CHRVRAMBank[8];
static bool s4_enableRAM;
static bool s4_chrVRAM;

void s4init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	s4_prgROM = prgROMin;
	s4_prgROMsize = prgROMsizeIn;
	s4_prgROMand = mapperGetAndValue(prgROMsizeIn);
	s4_prgRAM = prgRAMin;
	s4_prgRAMsize = prgRAMsizeIn;
	s4_curPRGBank = 0;
	s4_lastPRGBank = (prgROMsizeIn - 0x4000);
	if(chrROMsizeIn > 0)
	{
		s4_chrROM = chrROMin;
		s4_chrROMsize = chrROMsizeIn;
		s4_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
		printf("s4???\n");
	memset(s4_CHRBank,0,4*sizeof(uint32_t));
	memset(s4_CHRVRAMBank,0,2*sizeof(uint32_t));
	s4_enableRAM = false;
	s4_chrVRAM = false;
	printf("Sunsoft 4 Mapper inited\n");
}

uint8_t s4get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		if(s4_enableRAM)
			return s4_prgRAM[addr&0x1FFF];
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0xC000)
			return s4_prgROM[((s4_curPRGBank<<14)+(addr&0x3FFF))&s4_prgROMand];
		return s4_prgROM[(s4_lastPRGBank+(addr&0x3FFF))&s4_prgROMand];
	}
	return val;
}

void s4set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("s4set8 %04x %02x\n", addr, val);
		if(s4_enableRAM)
			s4_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0x9000)
			s4_CHRBank[0] = val;
		else if(addr < 0xA000)
			s4_CHRBank[1] = val;
		else if(addr < 0xB000)
			s4_CHRBank[2] = val;
		else if(addr < 0xC000)
			s4_CHRBank[3] = val;
		else if(addr < 0xD000)
			s4_CHRVRAMBank[0] = (val&0x7F) | 0x80;
		else if(addr < 0xE000)
			s4_CHRVRAMBank[1] = (val&0x7F) | 0x80;
		else if(addr < 0xF000)
		{
			switch(val&3)
			{
				case 0:
					ppuSetNameTblVertical();
					break;
				case 1:
					ppuSetNameTblHorizontal();
					break;
				case 2:
					ppuSetNameTblSingleLower();
					break;
				case 3:
					ppuSetNameTblSingleUpper();
					break;
				default:
					break;
			}
			s4_chrVRAM = !!(val&0x10);
		}
		else
		{
			s4_curPRGBank = val&0xF;
			s4_enableRAM = !!(val&0x10);
		}
	}
}

uint8_t s4chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(addr < 0x800)
		return s4_chrROM[((s4_CHRBank[0]<<11)+(addr&0x7FF))&s4_chrROMand];
	else if(addr < 0x1000)
		return s4_chrROM[((s4_CHRBank[1]<<11)+(addr&0x7FF))&s4_chrROMand];
	else if(addr < 0x1800)
		return s4_chrROM[((s4_CHRBank[2]<<11)+(addr&0x7FF))&s4_chrROMand];
	return s4_chrROM[((s4_CHRBank[3]<<11)+(addr&0x7FF))&s4_chrROMand];
}

void s4chrSet8(uint16_t addr, uint8_t val)
{
	//printf("s4chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

uint8_t s4vramGet8(uint16_t addr)
{
	if(!s4_chrVRAM)
		return s4_VRAM[addr&0x7FF];
	else if(addr < 0x400)
		return s4_chrROM[((s4_CHRVRAMBank[0]<<10)+(addr&0x3FF))&s4_chrROMand];
	else
		return s4_chrROM[((s4_CHRVRAMBank[1]<<10)+(addr&0x3FF))&s4_chrROMand];
}

void s4vramSet8(uint16_t addr, uint8_t val)
{
	if(!s4_chrVRAM)
		s4_VRAM[addr&0x7FF] = val;
}
