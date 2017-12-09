/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "../ppu.h"
#include "../mapper.h"

static uint8_t *p16c8_prgROM;
static uint8_t *p16c8_chrROM;
static uint32_t p16c8_prgROMsize;
static uint32_t p16c8_prgROMand;
static uint32_t p16c8_chrROMsize;
static uint32_t p16c8_chrROMand;
static uint32_t p16c8_firstPRGBank;
static uint32_t p16c8_curPRGBank;
static uint32_t p16c8_curCHRBank;
static uint32_t p16c8_lastPRGBank;
static bool p1632_p16;
static uint8_t m57_regA, m57_regB;

static uint8_t p16c8_chrRAM[0x2000];

void p16c8init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p16c8_prgROM = prgROMin;
	p16c8_prgROMsize = prgROMsizeIn;
	p16c8_prgROMand = mapperGetAndValue(p16c8_prgROMsize);
	(void)prgRAMin;
	(void)prgRAMsizeIn;
	p16c8_firstPRGBank = 0;
	p16c8_curPRGBank = p16c8_firstPRGBank;
	p16c8_lastPRGBank = prgROMsizeIn - 0x4000;
	if(chrROMsizeIn > 0)
	{
		p16c8_chrROM = chrROMin;
		p16c8_chrROMsize = chrROMsizeIn;
		p16c8_chrROMand = mapperGetAndValue(p16c8_chrROMsize);
	}
	else
	{
		p16c8_chrROM = p16c8_chrRAM;
		p16c8_chrROMsize = 0x2000;
		p16c8_chrROMand = 0x1FFF;
		memset(p16c8_chrRAM,0,0x2000);
	}
	p16c8_curCHRBank = 0;
	p1632_p16 = false;
	m57_regA = 0, m57_regB = 0;
	printf("16k PRG 8k CHR Mapper inited\n");
}

static bool m60_ready;
static uint8_t m60_state;
static uint32_t m60_prgROMadd;
static uint32_t m60_chrROMadd;

void m60_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p16c8init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	m60_ready = true;
	m60_state = 0;
	m60_prgROMadd = 0;
	m60_chrROMadd = 0;
}


void m174_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p16c8init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//reversed compared to other mappers using this...
	p1632_p16 = true;
}

uint8_t p16c8get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(addr < 0xC000)
		return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
	return p16c8_prgROM[((p16c8_lastPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
}

uint8_t p1632c8get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(p1632_p16)
		return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
	return p16c8_prgROM[((p16c8_curPRGBank&~0x7FFF)+(addr&0x7FFF))&p16c8_prgROMand];
}

uint8_t m60_get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	//determine reset by reading reset vector
	if(addr == 0xFFFC && m60_ready)
	{
		m60_ready = false;
		switch(m60_state)
		{
			case 0:
				m60_prgROMadd = 0;
				m60_chrROMadd = 0;
				break;
			case 1:
				m60_prgROMadd = 0x4000;
				m60_chrROMadd = 0x2000;
				break;
			case 2:
				m60_prgROMadd = 0x8000;
				m60_chrROMadd = 0x4000;
				break;
			case 3:
				m60_prgROMadd = 0xC000;
				m60_chrROMadd = 0x6000;
				break;
			default:
				break;
		}
		m60_state++;
		m60_state&=3;
	}
	//only allow another reset after full reset vector read
	if(addr == 0xFFFD && !m60_ready)
		m60_ready = true;
	return p16c8_prgROM[(addr&0x3FFF)+m60_prgROMadd];
}

uint8_t m97_get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(addr < 0xC000)
		return p16c8_prgROM[((p16c8_lastPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
	return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
}

uint8_t m180_get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(addr < 0xC000)
		return p16c8_prgROM[((p16c8_firstPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
	return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
}

uint8_t m200_get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
}

uint8_t m231_get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(addr < 0xC000)
		return p16c8_prgROM[((p16c8_curPRGBank&~0x7FFF)+(addr&0x3FFF))&p16c8_prgROMand];
	return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
}

uint8_t m232_get8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return val;
	if(addr < 0xC000)
		return p16c8_prgROM[((p16c8_curPRGBank&~0x3FFF)+(addr&0x3FFF))&p16c8_prgROMand];
	return p16c8_prgROM[((p16c8_curPRGBank&~0xFFFF)+addr)&p16c8_prgROMand];
}

void m2_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 0xF)<<14)&p16c8_prgROMand;
}

void m57_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	if((addr&0x800) == 0)
	{
		m57_regA = (val&7)|((val>>3)&8);
		p16c8_curCHRBank = ((m57_regA|m57_regB)<<13)&p16c8_chrROMand;
	}
	else
	{
		m57_regB = val&7;
		p16c8_curPRGBank = (((val>>5)&7)<<14)&p16c8_prgROMand;
		p16c8_curCHRBank = ((m57_regA|m57_regB)<<13)&p16c8_chrROMand;
		p1632_p16 = ((val&0x10) == 0);
		if((val&8) != 0)
			ppuSetNameTblHorizontal();
		else
			ppuSetNameTblVertical();
	}
}

void m58_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((addr&7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = (((addr>>3)&7)<<13)&p16c8_chrROMand;
	p1632_p16 = ((addr&0x40) != 0);
	if((addr&0x80) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m60_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	(void)addr;
}

void m61_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((((addr & 0xF)<<1)|((addr&0x20)>>5))<<14)&p16c8_prgROMand;;
	p1632_p16 = ((addr&0x10) != 0);
	if((addr&0x80) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m62_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	p16c8_curPRGBank = (((addr&0x40)|((addr>>8)&0x3F))<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((((addr&0x1F)<<2)|(val&3))<<13)&p16c8_chrROMand;
	p1632_p16 = ((addr&0x20) != 0);
	if((addr&0x80) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m70_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val >> 4)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((val & 0xF)<<13)&p16c8_chrROMand;
}

void m71_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0xC000)
	{
		if(addr == 0x9000) //Fire Hawk
		{
			if((val&0x10) != 0)
				ppuSetNameTblSingleLower();
			else if(val == 0)
				ppuSetNameTblSingleUpper();
		}
		return;
	}
	p16c8_curPRGBank = ((val & 0xF)<<14)&p16c8_prgROMand;
}

void m78a_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = (((val>>4) & 0xF)<<13)&p16c8_chrROMand;
	if((val&0x8) == 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m78b_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = (((val>>4) & 0xF)<<13)&p16c8_chrROMand;
	if((val&0x8) == 0)
		ppuSetNameTblSingleLower();
	else
		ppuSetNameTblSingleUpper();
}

void m89_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = (((val>>4) & 7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((((val&0x80)>>4)|(val & 7))<<13)&p16c8_chrROMand;
	if((val&0x8) == 0)
		ppuSetNameTblSingleLower();
	else
		ppuSetNameTblSingleUpper();
}

void m93_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = (((val>>4) & 7)<<14)&p16c8_prgROMand;
}

void m94_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = (((val>>2) & 0xF)<<14)&p16c8_prgROMand;
}

void m97_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 0xF)<<14)&p16c8_prgROMand;
	switch(val>>6)
	{
		case 0:
			ppuSetNameTblSingleLower();
			break;
		case 1:
			ppuSetNameTblHorizontal();
			break;
		case 2:
			ppuSetNameTblVertical();
			break;
		default: //case 3
			ppuSetNameTblSingleUpper();
			break;
	}
}

void m152_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = (((val >> 4)&7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((val & 0xF)<<13)&p16c8_chrROMand;
	if((val&0x80) == 0)
		ppuSetNameTblSingleLower();
	else
		ppuSetNameTblSingleUpper();
}

void m174_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	p16c8_curPRGBank = (((addr>>4)&7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = (((addr>>1)&7)<<13)&p16c8_chrROMand;
	p1632_p16 = ((addr&0x80) == 0);
	if((addr&1) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m180_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((val & 0xF)<<14)&p16c8_prgROMand;
}

void m200_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr < 0x8000)
		return;
	p16c8_curPRGBank = ((addr&7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((addr&7)<<13)&p16c8_chrROMand;
	if((addr&8) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m202_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	p16c8_curPRGBank = (((addr>>1)&7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = (((addr>>1)&7)<<13)&p16c8_chrROMand;
	p1632_p16 = ((addr&9) != 9);
	if((addr&1) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m203_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	p16c8_curPRGBank = ((val>>2)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((val&3)<<13)&p16c8_chrROMand;
}

void m212_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	p16c8_curPRGBank = ((addr&7)<<14)&p16c8_prgROMand;
	p16c8_curCHRBank = ((addr&7)<<13)&p16c8_chrROMand;
	p1632_p16 = ((addr&0x4000) == 0);
	if((addr&8) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m226_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if((addr&1)==0)
	{
		p16c8_curPRGBank &= ~0xFFFFF;
		p16c8_curPRGBank |= (((val&0x1F)|((val&0x80)>>2))<<14)&p16c8_prgROMand;
		p1632_p16 = ((val&0x20) != 0);
		if((val&0x40) == 0)
			ppuSetNameTblHorizontal();
		else
			ppuSetNameTblVertical();
	}
	else
	{
		p16c8_curPRGBank &= 0xFFFFF;
		p16c8_curPRGBank |= ((val&1)<<20)&p16c8_prgROMand;
	}
}

void m231_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	if(addr < 0x8000)
		return;
	//printf("p16c8set8 %04x %02x\n", addr, val);
	p16c8_curPRGBank = (((addr&0x1E)|((addr>>5)&1))<<14)&p16c8_prgROMand;
	if((addr&0x80) != 0)
		ppuSetNameTblHorizontal();
	else
		ppuSetNameTblVertical();
}

void m232_set8(uint16_t addr, uint8_t val)
{
	//printf("p16c8set8 %04x %02x\n", addr, val);
	if(addr >= 0xC000)
	{
		p16c8_curPRGBank &= ~0xFFFF;
		p16c8_curPRGBank |= ((val & 3)<<14)&p16c8_prgROMand;
	}
	else if(addr >= 0x8000)
	{
		p16c8_curPRGBank &= 0xFFFF;
		p16c8_curPRGBank |= ((val & 0x18)<<13)&p16c8_prgROMand;
	}
}

uint8_t p16c8chrGet8(uint16_t addr)
{
	if(p16c8_chrROM == p16c8_chrRAM) //Writable
		return p16c8_chrROM[addr&0x1FFF];
	return p16c8_chrROM[((p16c8_curCHRBank&~0x1FFF)+(addr&0x1FFF))&p16c8_chrROMand];
}

void p16c8chrSet8(uint16_t addr, uint8_t val)
{
	if(p16c8_chrROM == p16c8_chrRAM) //Writable
		p16c8_chrROM[addr&0x1FFF] = val;
}

uint8_t m60_chrGet8(uint16_t addr)
{
	return p16c8_chrROM[(addr&0x1FFF)+m60_chrROMadd];
}

void m60_chrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}

