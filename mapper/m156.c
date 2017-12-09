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

static uint8_t *m156_prgROM;
static uint8_t *m156_prgRAM;
static uint8_t *m156_chrROM;
static uint32_t m156_prgROMsize;
static uint32_t m156_prgRAMsize;
static uint32_t m156_chrROMsize;
static uint32_t m156_prgROMand;
static uint32_t m156_prgRAMand;
static uint32_t m156_chrROMand;
static uint32_t m156_curPRGBank;
static uint32_t m156_lastPRGBank;
static uint32_t m156_CHRBank[8];

void m156init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m156_prgROM = prgROMin;
	m156_prgROMsize = prgROMsizeIn;
	m156_prgROMand = mapperGetAndValue(prgROMsizeIn);
	m156_prgRAM = prgRAMin;
	m156_prgRAMsize = prgRAMsizeIn;
	m156_prgRAMand = mapperGetAndValue(prgRAMsizeIn);
	m156_curPRGBank = 0;
	m156_lastPRGBank = (prgROMsizeIn - 0x4000);
	if(chrROMsizeIn > 0)
	{
		m156_chrROM = chrROMin;
		m156_chrROMsize = chrROMsizeIn;
		m156_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
		printf("m156???\n");
	memset(m156_CHRBank,0,8*sizeof(uint32_t));
	ppuSetNameTblSingleLower(); //seems to be default state?
	printf("Mapper 156 inited\n");
}

uint8_t m156get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m156_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xC000)
			return m156_prgROM[((m156_curPRGBank<<14)+(addr&0x3FFF))&m156_prgROMand];
		return m156_prgROM[(m156_lastPRGBank+(addr&0x3FFF))&m156_prgROMand];
	}
	return val;
}

void m156set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("m156set8 %04x %02x\n", addr, val);
		m156_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0xC000 && addr < 0xC020)
	{
		switch(addr&0x1F)
		{
			case 0x0:
				m156_CHRBank[0] = (m156_CHRBank[0]&0xFF00)|val;
				break;
			case 0x1:
				m156_CHRBank[1] = (m156_CHRBank[1]&0xFF00)|val;
				break;
			case 0x2:
				m156_CHRBank[2] = (m156_CHRBank[2]&0xFF00)|val;
				break;
			case 0x3:
				m156_CHRBank[3] = (m156_CHRBank[3]&0xFF00)|val;
				break;
			case 0x4:
				m156_CHRBank[0] = (m156_CHRBank[0]&0xFF)|(val<<8);
				break;
			case 0x5:
				m156_CHRBank[1] = (m156_CHRBank[1]&0xFF)|(val<<8);
				break;
			case 0x6:
				m156_CHRBank[2] = (m156_CHRBank[2]&0xFF)|(val<<8);
				break;
			case 0x7:
				m156_CHRBank[3] = (m156_CHRBank[3]&0xFF)|(val<<8);
				break;
			case 0x8:
				m156_CHRBank[4] = (m156_CHRBank[4]&0xFF00)|val;
				break;
			case 0x9:
				m156_CHRBank[5] = (m156_CHRBank[5]&0xFF00)|val;
				break;
			case 0xA:
				m156_CHRBank[6] = (m156_CHRBank[6]&0xFF00)|val;
				break;
			case 0xB:
				m156_CHRBank[7] = (m156_CHRBank[7]&0xFF00)|val;
				break;
			case 0xC:
				m156_CHRBank[4] = (m156_CHRBank[4]&0xFF)|(val<<8);
				break;
			case 0xD:
				m156_CHRBank[5] = (m156_CHRBank[5]&0xFF)|(val<<8);
				break;
			case 0xE:
				m156_CHRBank[6] = (m156_CHRBank[6]&0xFF)|(val<<8);
				break;
			case 0xF:
				m156_CHRBank[7] = (m156_CHRBank[7]&0xFF)|(val<<8);
				break;
			case 0x10:
				m156_curPRGBank = val;
				break;
			case 0x14:
				if(val&1)
					ppuSetNameTblHorizontal();
				else
					ppuSetNameTblVertical();
				break;
			default:
				break;
		}
	}
}

uint8_t m156chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(addr < 0x400)
		return m156_chrROM[((m156_CHRBank[0]<<10)+(addr&0x3FF))&m156_chrROMand];
	else if(addr < 0x800)
		return m156_chrROM[((m156_CHRBank[1]<<10)+(addr&0x3FF))&m156_chrROMand];
	else if(addr < 0xC00)
		return m156_chrROM[((m156_CHRBank[2]<<10)+(addr&0x3FF))&m156_chrROMand];
	else if(addr < 0x1000)
		return m156_chrROM[((m156_CHRBank[3]<<10)+(addr&0x3FF))&m156_chrROMand];
	else if(addr < 0x1400)
		return m156_chrROM[((m156_CHRBank[4]<<10)+(addr&0x3FF))&m156_chrROMand];
	else if(addr < 0x1800)
		return m156_chrROM[((m156_CHRBank[5]<<10)+(addr&0x3FF))&m156_chrROMand];
	else if(addr < 0x1C00)
		return m156_chrROM[((m156_CHRBank[6]<<10)+(addr&0x3FF))&m156_chrROMand];
	return m156_chrROM[((m156_CHRBank[7]<<10)+(addr&0x3FF))&m156_chrROMand];
}

void m156chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m156chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

