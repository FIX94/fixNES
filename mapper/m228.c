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

static uint8_t *m228_prgROM;
static uint8_t *m228_chrROM;
static uint32_t m228_prgROMsize;
static uint32_t m228_chrROMsize;
static uint8_t m228_chrRAM[0x2000];
static uint8_t m228_regRAM[4];
static uint32_t m228_PRGBank;
static uint32_t m228_CHRBank;
static uint32_t m228_prgROMadd;
static uint32_t m228_prgROMand;
static uint32_t m228_chrROMand;
static bool m228_prgFull;

void m228init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	m228_prgROM = prgROMin;
	m228_prgROMsize = prgROMsizeIn;
	if(prgROMsizeIn < 0x80000)
		m228_prgROMand = mapperGetAndValue(prgROMsizeIn);
	else //Action 52 chip size
		m228_prgROMand = 0x7FFFF;
	m228_PRGBank = 0;
	if(chrROMsizeIn > 0)
	{
		m228_chrROM = chrROMin;
		m228_chrROMsize = chrROMsizeIn;
		m228_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
	{
		m228_chrROM = m228_chrRAM;
		m228_chrROMsize = 0x2000;
		m228_chrROMand = 0x1FFF;
	}
	memset(m228_chrRAM,0,0x2000);
	memset(m228_regRAM,0,4);
	m228_CHRBank = 0;
	m228_prgROMadd = 0;
	m228_prgFull = true;
	printf("Mapper 228 inited\n");
}

uint8_t m228get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x4020 && addr < 0x6000)
		return m228_regRAM[addr&3]&0xF;
	else if(addr >= 0x8000)
	{
		if(m228_prgFull)
			return m228_prgROM[((((m228_PRGBank<<14)&~0x7FFF)+(addr&0x7FFF))&m228_prgROMand)+m228_prgROMadd];
		return m228_prgROM[((((m228_PRGBank<<14)&~0x3FFF)+(addr&0x3FFF))&m228_prgROMand)+m228_prgROMadd];
	}
	return val;
}

void m228set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x4020 && addr < 0x6000)
		m228_regRAM[addr&3] = val&0xF;
	else if(addr >= 0x8000)
	{
		//printf("m228set8 %04x %02x\n", addr, val);
		m228_CHRBank = (val&3)|((addr&0xF)<<2);
		switch((addr>>11)&3)
		{
			case 1:
				m228_prgROMadd = 0x80000;
				break;
			case 3:
				m228_prgROMadd = 0x100000;
				break;
			default:
				m228_prgROMadd = 0;
				break;
		}
		if(m228_prgROMadd >= m228_prgROMsize)
			m228_prgROMadd = 0; //For Cheetahmen II
		m228_prgFull = ((addr&0x20) == 0);
		m228_PRGBank = (addr>>6)&0x1F;
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
		//printf("%4x %8x %d %2x\n",  m228_CHRBank, m228_prgROMadd, m228_prgFull, m228_PRGBank);
	}
}

uint8_t m228chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	return m228_chrROM[((m228_CHRBank<<13)+(addr&0x1FFF))&m228_chrROMand];
}

void m228chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m228chrSet8 %04x %02x\n", addr, val);
	if(m228_chrROM == m228_chrRAM) //Writable
		m228_chrROM[addr&0x1FFF] = val;
}
