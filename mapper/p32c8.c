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

static uint8_t *p32c8_prgROM;
static uint8_t *p32c8_prgRAM;
static uint8_t *p32c8_chrROM;
static uint8_t p32c8_chrRAM[0x2000];
static uint32_t p32c8_prgROMsize;
static uint32_t p32c8_prgROMand;
static uint32_t p32c8_prgRAMsize;
static uint32_t p32c8_chrROMsize;
static uint32_t p32c8_chrROMand;
static uint32_t p32c8_curPRGBank;
static uint32_t p32c8_curCHRBank;
static uint8_t m36_regstat;
static uint8_t m36_mode;
static bool m41_inner;

void p32c8init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	p32c8_prgROM = prgROMin;
	p32c8_prgROMsize = prgROMsizeIn;
	p32c8_prgROMand = mapperGetAndValue(p32c8_prgROMsize);
	p32c8_prgRAM = prgRAMin;
	p32c8_prgRAMsize = prgRAMsizeIn;
	p32c8_curPRGBank = 0;
	if(chrROMin && chrROMsizeIn)
	{
		p32c8_chrROM = chrROMin;
		p32c8_chrROMsize = chrROMsizeIn;
		p32c8_chrROMand = mapperGetAndValue(p32c8_chrROMsize);
	}
	else
	{
		p32c8_chrROM = p32c8_chrRAM;
		p32c8_chrROMsize = 0x2000;
		p32c8_chrROMand = 0x1FFF;
		memset(p32c8_chrRAM,0,0x2000);
	}
	p32c8_curCHRBank = 0;
	m36_regstat = 0;
	m36_mode = 0;
	m41_inner = false;
	printf("32k PRG 8k CHR Mapper inited\n");
}

uint8_t p32c8get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		return p32c8_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
		return p32c8_prgROM[((p32c8_curPRGBank&~0x7FFF)+(addr&0x7FFF))&p32c8_prgROMand];
	return val;
}

uint8_t m36_p32c8get8(uint16_t addr, uint8_t val)
{
	uint16_t maskedAddr = (addr & 0xE103);
	if(maskedAddr >= 0x4100 && maskedAddr < 0x4104)
		return m36_regstat;
	else if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		return p32c8_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
		return p32c8_prgROM[((p32c8_curPRGBank&~0x7FFF)+(addr&0x7FFF))&p32c8_prgROMand];
	return val;
}

void m0_set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		p32c8_prgRAM[addr&0x1FFF] = val;
}

void m11_set8(uint16_t addr, uint8_t val)
{
	//printf("m11set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		p32c8_prgRAM[addr&0x1FFF] = val;
	else if(addr >= 0x8000)
	{
		p32c8_curPRGBank = ((val & 3)<<15)&p32c8_prgROMand;
		p32c8_curCHRBank = ((val >> 4)<<13)&p32c8_chrROMand;
	}
}

void m36_set8(uint16_t addr, uint8_t val)
{
	//printf("m36set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0xE103);
	if(maskedAddr == 0x4100)
	{
		if(m36_mode == 0)
		{
			m36_regstat &= ~0x30;
			m36_regstat |= (((p32c8_curPRGBank>>15)&3)<<4);
		}
		else
		{
			uint8_t inc = (m36_regstat>>4)&3;
			inc++; inc &= 3;
			m36_regstat &= ~0x30;
			m36_regstat |= (inc<<4);
		}
	}
	else if(maskedAddr == 0x4102)
		p32c8_curPRGBank = (((val>>4)&3)<<15)&p32c8_prgROMand;
	else if(maskedAddr == 0x4103)
		m36_mode = ((val&0x10) != 0);
	maskedAddr = (addr & 0xE200);
	if(maskedAddr == 0x4200)
		p32c8_curCHRBank = ((val & 0xF)<<13)&p32c8_chrROMand;
	else if(addr >= 0x8000)
		p32c8_curPRGBank = (((m36_regstat>>4)&3)<<15)&p32c8_prgROMand;
}

void m38_set8(uint16_t addr, uint8_t val)
{
	//printf("m38set8 %04x %02x\n", addr, val);
	if(addr >= 0x7000 && addr < 0x8000)
	{
		p32c8_curCHRBank = (((val>>2)&3)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = ((val & 3)<<15)&p32c8_prgROMand;
	}
}

void m41_set8(uint16_t addr, uint8_t val)
{
	//printf("m41set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x6800)
	{
		p32c8_curCHRBank &= 0x7FFF;
		p32c8_curCHRBank |= (((addr&0x18)>>3)<<15)&p32c8_chrROMand;
		p32c8_curPRGBank = ((addr&7)<<15)&p32c8_prgROMand;
		m41_inner = (addr&4) != 0;
		if((addr&0x20) != 0)
			ppuSetNameTblHorizontal();
		else
			ppuSetNameTblVertical();
	}
	else if(addr >= 0x8000 && m41_inner)
	{
		p32c8_curCHRBank &= ~0x7FFF;
		p32c8_curCHRBank |= ((val&3)<<13)&p32c8_chrROMand;
	}
}

void m46_set8(uint16_t addr, uint8_t val)
{
	//printf("m46set8 %04x %02x\n", addr, val);
	if(addr >= 0x8000)
	{
		p32c8_curPRGBank &= ~0xFFFF;
		p32c8_curPRGBank |= ((val & 1)<<15)&p32c8_prgROMand;
		p32c8_curCHRBank &= ~0xFFFF;
		p32c8_curCHRBank |= (((val>>4)&7)<<13)&p32c8_chrROMand;
	}
	else if(addr >= 0x6000)
	{
		p32c8_curPRGBank &= 0xFFFF;
		p32c8_curPRGBank |= ((val & 0xF)<<16)&p32c8_prgROMand;
		p32c8_curCHRBank &= 0xFFFF;
		p32c8_curCHRBank |= ((val >> 4)<<16)&p32c8_chrROMand;
	}
}

void m66_set8(uint16_t addr, uint8_t val)
{
	//printf("m66set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		p32c8_prgRAM[addr&0x1FFF] = val;
	else if(addr >= 0x8000)
	{
		p32c8_curPRGBank = (((val>>4)&3)<<15)&p32c8_prgROMand;
		p32c8_curCHRBank = ((val & 3)<<13)&p32c8_chrROMand;
	}
}

void m79_set8(uint16_t addr, uint8_t val)
{
	//printf("m79set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0xE100);
	if(maskedAddr == 0x4100)
	{
		p32c8_curCHRBank = ((val & 7)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = (((val>>3)&1)<<15)&p32c8_prgROMand;
	}
}

void m113_set8(uint16_t addr, uint8_t val)
{
	//printf("m113set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0xE100);
	if(maskedAddr == 0x4100)
	{
		p32c8_curCHRBank = ((((val>>3)&8)|(val&7))<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = (((val>>3)&7)<<15)&p32c8_prgROMand;
		if((val&(1<<7)) == 0)
			ppuSetNameTblHorizontal();
		else
			ppuSetNameTblVertical();
	}
}

void m133_set8(uint16_t addr, uint8_t val)
{
	//printf("m133set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0x6100);
	if(maskedAddr == 0x4100)
	{
		p32c8_curCHRBank = ((val & 3)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = (((val>>2)&1)<<15)&p32c8_prgROMand;
	}
}

void m140_set8(uint16_t addr, uint8_t val)
{
	//printf("m140set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000)
	{
		p32c8_curCHRBank = ((val & 0xF)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = (((val>>4)&3)<<15)&p32c8_prgROMand;
	}
}

void m144_set8(uint16_t addr, uint8_t val)
{
	//printf("m144set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		p32c8_prgRAM[addr&0x1FFF] = val;
	else if(addr > 0x8000) //ignore 0x8000
	{
		p32c8_curPRGBank = ((val & 3)<<15)&p32c8_prgROMand;
		p32c8_curCHRBank = ((val >> 4)<<13)&p32c8_chrROMand;
	}
}

void m147_set8(uint16_t addr, uint8_t val)
{
	//printf("m147set8 %04x %02x\n", addr, val);
	uint16_t maskedAddr = (addr & 0x4103);
	if(maskedAddr == 0x4102)
	{
		p32c8_curCHRBank = (((val>>3)&0xF)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = ((((val>>6)&2)|((val>>2)&1))<<15)&p32c8_prgROMand;
	}
}

void m148_set8(uint16_t addr, uint8_t val)
{
	//printf("m148set8 %04x %02x\n", addr, val);
	if(addr >= 0x8000)
	{
		p32c8_curCHRBank = ((val & 7)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = (((val>>3)&1)<<15)&p32c8_prgROMand;
	}
}

void m201_set8(uint16_t addr, uint8_t val)
{
	(void)val;
	//printf("m148set8 %04x %02x\n", addr, val);
	if(addr >= 0x8000)
	{
		p32c8_curCHRBank = ((addr&0xFF)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = ((addr&0xFF)<<15)&p32c8_prgROMand;
	}
}

void m240_set8(uint16_t addr, uint8_t val)
{
	//printf("m240set8 %04x %02x\n", addr, val);
	if(addr >= 0x4020 && addr < 0x6000)
	{
		p32c8_curCHRBank = ((val & 0xF)<<13)&p32c8_chrROMand;
		p32c8_curPRGBank = ((val >> 4)<<15)&p32c8_prgROMand;
	}
	else if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		p32c8_prgRAM[addr&0x1FFF] = val;
}

void m242_set8(uint16_t addr, uint8_t val)
{
	//printf("m242set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000 && p32c8_prgRAMsize)
		p32c8_prgRAM[addr&0x1FFF] = val;
	else if(addr >= 0x8000)
	{
		val = addr; //ignore value, address location is val here
		p32c8_curPRGBank = (((val>>3)&0xF)<<15)&p32c8_prgROMand;
		if((val&2) == 0)
			ppuSetNameTblVertical();
		else
			ppuSetNameTblHorizontal();
	}
}

uint8_t p32c8chrGet8(uint16_t addr)
{
	if(p32c8_chrROM == p32c8_chrRAM) //Writable
		return p32c8_chrROM[addr&0x1FFF];
	return p32c8_chrROM[((p32c8_curCHRBank&~0x1FFF)+(addr&0x1FFF))&p32c8_chrROMand];
}

void p32c8chrSet8(uint16_t addr, uint8_t val)
{
	//printf("p32c8chrSet8 %04x %02x\n", addr, val);
	if(p32c8_chrROM == p32c8_chrRAM) //Writable
		p32c8_chrROM[addr&0x1FFF] = val;
}
