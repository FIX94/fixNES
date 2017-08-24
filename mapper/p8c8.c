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
#include "../mapper.h"

static uint8_t *p8c8_prgROM;
static uint8_t *p8c8_prgRAM;
static uint8_t *p8c8_chrROM;
static uint32_t p8c8_prgROMsize;
static uint32_t p8c8_prgROMand;
static uint32_t p8c8_prgRAMsize;
static uint32_t p8c8_chrROMsize;
static uint32_t p8c8_chrROMand;
static uint32_t p8c8_curPRGBank;
static uint32_t p8c8_curCHRBank;
static bool m185_CHRDisable;

void p8c8init(uint8_t *prgROMin, uint32_t prgROMsizeIn,
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p8c8_prgROM = prgROMin;
	p8c8_prgROMsize = prgROMsizeIn;
	p8c8_prgROMand = mapperGetAndValue(p8c8_prgROMand);
	p8c8_prgRAM = prgRAMin;
	p8c8_prgRAMsize = prgRAMsizeIn;
	p8c8_chrROM = chrROMin;
	p8c8_chrROMsize = chrROMsizeIn;
	p8c8_chrROMand = mapperGetAndValue(p8c8_chrROMsize);
	p8c8_curPRGBank = 0;
	p8c8_curCHRBank = 0;
	m185_CHRDisable = false;
	printf("8k PRG 8k CHR Mapper inited\n");
}

uint8_t p8c8get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return p8c8_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return p8c8_prgROM[(p8c8_curPRGBank+(addr&0x1FFF))&p8c8_prgROMand];
		return p8c8_prgROM[addr&p8c8_prgROMand];
	}
	return val;
}

void m3_set8(uint16_t addr, uint8_t val)
{
	//printf("m3set8 %04x %02x\n", addr, val);
	if(addr < 0x8000 && addr >= 0x6000)
		p8c8_prgRAM[addr&0x1FFF] = val;
	else if(addr >= 0x8000)
		p8c8_curCHRBank = ((val<<13)&p8c8_chrROMand);
}

void m87_set8(uint16_t addr, uint8_t val)
{
	//printf("m87set8 %04x %02x\n", addr, val);
	if(addr < 0x8000 && addr >= 0x6000)
		p8c8_curCHRBank = ((((val&1)<<1)|((val&2)>>1))<<13)&p8c8_chrROMand;
}

void m99_set8(uint16_t addr, uint8_t val)
{
	//printf("m99set8 %04x %02x\n", addr, val);
	if(addr == 0x4016)
	{
		p8c8_curCHRBank = (((val>>2)&1)<<13)&p8c8_chrROMand;
		p8c8_curPRGBank = p8c8_curCHRBank;
	}
	if(addr < 0x8000 && addr >= 0x6000)
		p8c8_prgRAM[addr&0x1FFF] = val;
}

void m101_set8(uint16_t addr, uint8_t val)
{
	//printf("m87set8 %04x %02x\n", addr, val);
	if(addr < 0x8000 && addr >= 0x6000)
		p8c8_curCHRBank = ((val&3)<<13)&p8c8_chrROMand;
}

void m145_set8(uint16_t addr, uint8_t val)
{
	//printf("m145set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0xE100);
	if(maskedAddr == 0x4100)
		p8c8_curCHRBank = ((val>>7)<<13)&p8c8_chrROMand;
}

void m149_set8(uint16_t addr, uint8_t val)
{
	//printf("m149set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0x8000);
	if(maskedAddr == 0x8000)
		p8c8_curCHRBank = ((val>>7)<<13)&p8c8_chrROMand;
}

void m185_set8(uint16_t addr, uint8_t val)
{
	//printf("m185set8 %04x %02x\n", addr, val);
	if(addr < 0x8000 && addr >= 0x6000)
		p8c8_prgRAM[addr&0x1FFF] = val;
	else if(addr >= 0x8000)
		m185_CHRDisable ^= true;
}

uint8_t p8c8chrGet8(uint16_t addr)
{
	return p8c8_chrROM[(p8c8_curCHRBank+(addr&0x1FFF))&p8c8_chrROMand];
}

uint8_t m185_chrGet8(uint16_t addr)
{
	if(m185_CHRDisable)	return 1;
	return p8c8chrGet8(addr);
}

void p8c8chrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}
