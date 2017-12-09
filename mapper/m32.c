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

static uint8_t *m32_prgROM;
static uint8_t *m32_prgRAM;
static uint8_t *m32_chrROM;
static uint32_t m32_prgROMsize;
static uint32_t m32_prgRAMsize;
static uint32_t m32_chrROMsize;
static uint32_t m32_prgROMand;
static uint32_t m32_prgRAMand;
static uint32_t m32_chrROMand;
static uint32_t m32_lastPRGBank;
static uint32_t m32_lastM1PRGBank;
static uint32_t m32_PRGBank[2];
static uint32_t m32_CHRBank[8];
static uint8_t m32_prgMode;
//used externally
bool m32_singlescreen;

void m32init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m32_prgROM = prgROMin;
	m32_prgROMsize = prgROMsizeIn;
	m32_prgROMand = mapperGetAndValue(prgROMsizeIn);
	m32_prgRAM = prgRAMin;
	m32_prgRAMsize = prgRAMsizeIn;
	m32_prgRAMand = mapperGetAndValue(prgRAMsizeIn);
	m32_lastPRGBank = (prgROMsizeIn - 0x2000);
	m32_lastM1PRGBank = m32_lastPRGBank - 0x2000;
	if(chrROMsizeIn > 0)
	{
		m32_chrROM = chrROMin;
		m32_chrROMsize = chrROMsizeIn;
		m32_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
		printf("m32???\n");
	memset(m32_PRGBank,0,2*sizeof(uint32_t));
	memset(m32_CHRBank,0,8*sizeof(uint32_t));
	m32_prgMode = 0;
	if(m32_singlescreen)
		ppuSetNameTblSingleLower();
	printf("Mapper 32 inited\n");
}

uint8_t m32get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m32_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(m32_prgMode == 0)
		{
			if(addr < 0xA000)
				return m32_prgROM[((m32_PRGBank[0]<<13)+(addr&0x1FFF))&m32_prgROMand];
			else if(addr < 0xC000)
				return m32_prgROM[((m32_PRGBank[1]<<13)+(addr&0x1FFF))&m32_prgROMand];
			else if(addr < 0xE000)
				return m32_prgROM[(m32_lastM1PRGBank+(addr&0x1FFF))&m32_prgROMand];
			return m32_prgROM[(m32_lastPRGBank+(addr&0x1FFF))&m32_prgROMand];
		}
		else
		{
			if(addr < 0xA000)
				return m32_prgROM[(m32_lastM1PRGBank+(addr&0x1FFF))&m32_prgROMand];
			else if(addr < 0xC000)
				return m32_prgROM[((m32_PRGBank[1]<<13)+(addr&0x1FFF))&m32_prgROMand];
			else if(addr < 0xE000)
				return m32_prgROM[((m32_PRGBank[0]<<13)+(addr&0x1FFF))&m32_prgROMand];
			return m32_prgROM[(m32_lastPRGBank+(addr&0x1FFF))&m32_prgROMand];
		}
	}
	return val;
}

void m32set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("m32set8 %04x %02x\n", addr, val);
		m32_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0x9000)
			m32_PRGBank[0] = val&0x1F;
		else if(addr < 0xA000)
		{
			if(!m32_singlescreen)
			{
				if(val&1)
					ppuSetNameTblHorizontal();
				else
					ppuSetNameTblVertical();
				m32_prgMode = !!(val&2);
			}
		}
		else if(addr < 0xB000)
			m32_PRGBank[1] = val;
		else if(addr < 0xC000)
		{
			switch(addr&7)
			{
				case 0x0:
					m32_CHRBank[0] = val;
					break;
				case 0x1:
					m32_CHRBank[1] = val;
					break;
				case 0x2:
					m32_CHRBank[2] = val;
					break;
				case 0x3:
					m32_CHRBank[3] = val;
					break;
				case 0x4:
					m32_CHRBank[4] = val;
					break;
				case 0x5:
					m32_CHRBank[5] = val;
					break;
				case 0x6:
					m32_CHRBank[6] = val;
					break;
				case 0x7:
					m32_CHRBank[7] = val;
					break;
				default:
					break;
			}
		}
	}
}

uint8_t m32chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(addr < 0x400)
		return m32_chrROM[((m32_CHRBank[0]<<10)+(addr&0x3FF))&m32_chrROMand];
	else if(addr < 0x800)
		return m32_chrROM[((m32_CHRBank[1]<<10)+(addr&0x3FF))&m32_chrROMand];
	else if(addr < 0xC00)
		return m32_chrROM[((m32_CHRBank[2]<<10)+(addr&0x3FF))&m32_chrROMand];
	else if(addr < 0x1000)
		return m32_chrROM[((m32_CHRBank[3]<<10)+(addr&0x3FF))&m32_chrROMand];
	else if(addr < 0x1400)
		return m32_chrROM[((m32_CHRBank[4]<<10)+(addr&0x3FF))&m32_chrROMand];
	else if(addr < 0x1800)
		return m32_chrROM[((m32_CHRBank[5]<<10)+(addr&0x3FF))&m32_chrROMand];
	else if(addr < 0x1C00)
		return m32_chrROM[((m32_CHRBank[6]<<10)+(addr&0x3FF))&m32_chrROMand];
	return m32_chrROM[((m32_CHRBank[7]<<10)+(addr&0x3FF))&m32_chrROMand];
}

void m32chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m32chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

