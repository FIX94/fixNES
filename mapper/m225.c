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

static uint8_t *m225_prgROM;
static uint8_t *m225_chrROM;
static uint32_t m225_prgROMsize;
static uint32_t m225_chrROMsize;
static uint8_t m225_chrRAM[0x2000];
static uint8_t m225_regRAM[4];
static uint32_t m225_PRGBank;
static uint32_t m225_CHRBank;
static uint32_t m225_prgROMand;
static uint32_t m225_chrROMand;
static bool m225_prgFull;

void m225init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	m225_prgROM = prgROMin;
	m225_prgROMsize = prgROMsizeIn;
	m225_prgROMand = mapperGetAndValue(prgROMsizeIn);
	m225_PRGBank = 0;
	if(chrROMsizeIn > 0)
	{
		m225_chrROM = chrROMin;
		m225_chrROMsize = chrROMsizeIn;
		m225_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
	{
		m225_chrROM = m225_chrRAM;
		m225_chrROMsize = 0x2000;
		m225_chrROMand = 0x1FFF;
	}
	memset(m225_chrRAM,0,0x2000);
	memset(m225_regRAM,0,4);
	m225_CHRBank = 0;
	m225_prgFull = true;
	printf("Mapper 225 inited\n");
}

uint8_t m225get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x5800 && addr < 0x6000)
		return m225_regRAM[addr&3]&0xF;
	else if(addr >= 0x8000)
	{
		if(m225_prgFull)
			return m225_prgROM[((((m225_PRGBank<<14)&~0x7FFF)+(addr&0x7FFF))&m225_prgROMand)];
		return m225_prgROM[((((m225_PRGBank<<14)&~0x3FFF)+(addr&0x3FFF))&m225_prgROMand)];
	}
	return val;
}

void m225set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x5800 && addr < 0x6000)
		m225_regRAM[addr&3] = val&0xF;
	else if(addr >= 0x8000)
	{
		//printf("m225set8 %04x %02x\n", addr, val);
		m225_CHRBank = (addr&0x3F);
		m225_prgFull = ((addr&0x1000) == 0);
		m225_PRGBank = (addr>>6)&0x3F;
		if(addr&0x4000)
		{
			m225_CHRBank |= 0x80;
			m225_PRGBank |= 0x80;
		}
		if((addr&0x2000) == 0)
		{
			//printf("Vertical mode\n");
			ppuSetNameTblVertical();
		}
		else
		{
			//printf("Horizontal mode\n");
			ppuSetNameTblHorizontal();
		}
		//printf("%4x %8x %d %2x\n",  m225_CHRBank, m225_prgROMadd, m225_prgFull, m225_PRGBank);
	}
}

uint8_t m225chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	return m225_chrROM[((m225_CHRBank<<13)+(addr&0x1FFF))&m225_chrROMand];
}

void m225chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m225chrSet8 %04x %02x\n", addr, val);
	if(m225_chrROM == m225_chrRAM) //Writable
		m225_chrROM[addr&0x1FFF] = val;
}
