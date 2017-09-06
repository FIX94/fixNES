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

static uint8_t *m206prgROM;
static uint8_t *m206prgRAM;
static uint8_t *m206chrROM;
static uint32_t m206prgROMsize;
static uint32_t m206prgRAMsize;
static uint32_t m206chrROMsize;
static uint32_t m206curPRGBank0;
static uint32_t m206curPRGBank1;
static uint32_t m206lastPRGBank;
static uint32_t m206lastM1PRGBank;
static uint8_t m206BankSelect;
static uint32_t m206CHRBank[6];
static uint32_t m206prgROMand;
static uint32_t m206prgRAMand;
static uint32_t m206chrROMand;
static uint16_t m95nt0;
static uint16_t m95nt1;

void m206init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m206prgROM = prgROMin;
	m206prgROMsize = prgROMsizeIn;
	m206prgROMand = mapperGetAndValue(prgROMsizeIn);
	m206curPRGBank0 = 0;
	m206curPRGBank1 = 0x2000;
	m206lastPRGBank = (prgROMsizeIn - 0x2000);
	m206lastM1PRGBank = m206lastPRGBank - 0x2000;
	m206prgRAM = prgRAMin;
	m206prgRAMsize = prgRAMsizeIn;
	m206prgRAMand = mapperGetAndValue(prgRAMsizeIn);
	memset(m206prgRAM,0,m206prgRAMsize);
	m206chrROM = chrROMin;
	m206chrROMsize = chrROMsizeIn;
	m206chrROMand = mapperGetAndValue(chrROMsizeIn);
	memset(m206CHRBank,0,6*sizeof(uint32_t));
	m206BankSelect = 0;
	m95nt0 = 0;
	m95nt1 = 0;
	printf("Mapper 206 (and Variants) inited\n");
}

uint8_t m206get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return m206prgROM[((m206curPRGBank0<<13)|(addr&0x1FFF))&m206prgROMand];
		else if(addr < 0xC000)
			return m206prgROM[((m206curPRGBank1<<13)|(addr&0x1FFF))&m206prgROMand];
		else if(addr < 0xE000)
			return m206prgROM[(m206lastM1PRGBank+(addr&0x1FFF))&m206prgROMand];
		return m206prgROM[(m206lastPRGBank+(addr&0x1FFF))&m206prgROMand];
	}
	return val;
}

uint8_t m112get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m206prgRAM[addr&0x1FFF];
	return m206get8(addr, val);
}

void m206set8(uint16_t addr, uint8_t val)
{
	//printf("m206set8 %04x %02x\n", addr, val);
	if(addr >= 0x8000 && addr < 0xA000)
	{
		if((addr&1) == 0)
			m206BankSelect = val&7;
		else
		{
			switch(m206BankSelect)
			{
				case 0:
					m206CHRBank[0] = val;
					break;
				case 1:
					m206CHRBank[1] = val;
					break;
				case 2:
					m206CHRBank[2] = val;
					break;
				case 3:
					m206CHRBank[3] = val;
					break;
				case 4:
					m206CHRBank[4] = val;
					break;
				case 5:
					m206CHRBank[5] = val;
					break;
				case 6:
					m206curPRGBank0 = val;
					break;
				case 7:
					m206curPRGBank1 = val;
					break;
			}
		}
	}
}

void m154set8(uint16_t addr, uint8_t val)
{
	m206set8(addr,val);
	if(addr >= 0x8000)
	{
		if((val&0x40)==0)
			ppuSetNameTblSingleLower();
		else
			ppuSetNameTblSingleUpper();
	}
}

void m95set8(uint16_t addr, uint8_t val)
{
	m206set8(addr,val);
	if(addr >= 0x8000 && addr < 0xA000 && (addr&1))
	{
		m95nt0 = (m206CHRBank[0]&0x20) ? 0x400 : 0;
		m95nt1 = (m206CHRBank[1]&0x20) ? 0x400 : 0;
		ppuSetNameTblCustom(m95nt0,m95nt0,m95nt1,m95nt1);
	}
}

void m112set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		m206prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m112set8 %04x %02x\n", addr, val);
	addr &= 0xE001;
	if(addr == 0x8000)
		m206BankSelect = val&7;
	else if(addr == 0xA000)
	{
		switch(m206BankSelect)
		{
			case 0:
				m206curPRGBank0 = val;
				break;
			case 1:
				m206curPRGBank1 = val;
				break;
			case 2:
				m206CHRBank[0] = val;
				break;
			case 3:
				m206CHRBank[1] = val;
				break;
			case 4:
				m206CHRBank[2] = val;
				break;
			case 5:
				m206CHRBank[3] = val;
				break;
			case 6:
				m206CHRBank[4] = val;
				break;
			case 7:
				m206CHRBank[5] = val;
				break;
		}
	}
	else if(addr == 0xE000)
	{
		if((val&1)==0)
			ppuSetNameTblVertical();
		else
			ppuSetNameTblHorizontal();
	}
}

uint8_t m76chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	if(addr < 0x800)
		return m206chrROM[((m206CHRBank[2]<<11)|(addr&0x7FF))&m206chrROMand];
	else if(addr < 0x1000)
		return m206chrROM[((m206CHRBank[3]<<11)|(addr&0x7FF))&m206chrROMand];
	else if(addr < 0x1800)
		return m206chrROM[((m206CHRBank[4]<<11)|(addr&0x7FF))&m206chrROMand];
	return m206chrROM[((m206CHRBank[5]<<11)|(addr&0x7FF))&m206chrROMand];
}

uint8_t m88chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	if(addr < 0x800)
		return m206chrROM[((((m206CHRBank[0]&~1)<<10)|(addr&0x7FF))&0xFFFF)&m206chrROMand];
	else if(addr < 0x1000)
		return m206chrROM[((((m206CHRBank[1]&~1)<<10)|(addr&0x7FF))&0xFFFF)&m206chrROMand];
	else if(addr < 0x1400)
		return m206chrROM[(((m206CHRBank[2]<<10)|(addr&0x3FF))|0x10000)&m206chrROMand];
	else if(addr < 0x1800)
		return m206chrROM[(((m206CHRBank[3]<<10)|(addr&0x3FF))|0x10000)&m206chrROMand];
	else if(addr < 0x1C00)
		return m206chrROM[(((m206CHRBank[4]<<10)|(addr&0x3FF))|0x10000)&m206chrROMand];
	return m206chrROM[(((m206CHRBank[5]<<10)|(addr&0x3FF))|0x10000)&m206chrROMand];
}

uint8_t m206chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	if(addr < 0x800)
		return m206chrROM[(((m206CHRBank[0]&~1)<<10)|(addr&0x7FF))&m206chrROMand];
	else if(addr < 0x1000)
		return m206chrROM[(((m206CHRBank[1]&~1)<<10)|(addr&0x7FF))&m206chrROMand];
	else if(addr < 0x1400)
		return m206chrROM[((m206CHRBank[2]<<10)|(addr&0x3FF))&m206chrROMand];
	else if(addr < 0x1800)
		return m206chrROM[((m206CHRBank[3]<<10)|(addr&0x3FF))&m206chrROMand];
	else if(addr < 0x1C00)
		return m206chrROM[((m206CHRBank[4]<<10)|(addr&0x3FF))&m206chrROMand];
	return m206chrROM[((m206CHRBank[5]<<10)|(addr&0x3FF))&m206chrROMand];
}

void m206chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m206chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}
