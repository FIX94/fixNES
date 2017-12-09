/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "../ppu.h"
#include "../mapper_h/m4.h"

extern uint32_t m4_prgROMadd;
extern uint32_t m4_chrROMadd;
extern uint32_t m4_prgROMand;
extern uint32_t m4_chrROMand;
static bool m4add_regLock;

void m37_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0;
	m4_prgROMand = 0xFFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x1FFFF;
	m4add_regLock = false;
	printf("Mapper 37 (Mapper 4 Game Select) inited\n");
}

void m44_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0;
	m4_prgROMand = 0x1FFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x1FFFF;
	m4add_regLock = false;
	printf("Mapper 44 (Mapper 4 Game Select) inited\n");
}

static uint8_t m45_curReg;

void m45_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0x60000;
	m4_prgROMand = 0x1FFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x1FFFF;
	m4add_regLock = false;
	m45_curReg = 0;
	printf("Mapper 45 (Mapper 4 Game Select) inited\n");
}

void m47_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0;
	m4_prgROMand = 0x1FFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x1FFFF;
	m4add_regLock = false;
	printf("Mapper 47 (Mapper 4 Game Select) inited\n");
}

static uint8_t m49_prgmode;
static uint8_t m49_prgreg;
static uint8_t *m49_prgROM;

void m49_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0;
	m4_prgROMand = 0x1FFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x1FFFF;
	m4add_regLock = false;
	m49_prgmode = 0;
	m49_prgreg = 0;
	//for prg mode
	m49_prgROM = prgROMin;
	printf("Mapper 49 (Mapper 4 Game Select) inited\n");
}

void m52_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0;
	m4_prgROMand = 0x3FFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x3FFFF;
	m4add_regLock = false;
	printf("Mapper 52 (Mapper 4 Game Select) inited\n");
}

void m205_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	//start with default config
	m4_prgROMadd = 0;
	m4_prgROMand = 0x3FFFF;
	m4_chrROMadd = 0;
	m4_chrROMand = 0x3FFFF;
	m4add_regLock = false;
	printf("Mapper 205 (Mapper 4 Game Select) inited\n");
}

void m37_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000 && addr >= 0x6000)
	{
		val &= 7;
		if(val < 3)
		{
			m4_prgROMadd = 0;
			m4_prgROMand = 0xFFFF;
			m4_chrROMadd = 0;
			m4_chrROMand = 0x1FFFF;
		}
		else if(val == 3)
		{
			m4_prgROMadd = 0x10000;
			m4_prgROMand = 0xFFFF;
			m4_chrROMadd = 0;
			m4_chrROMand = 0x1FFFF;
		}
		else if(val < 7)
		{
			m4_prgROMadd = 0x20000;
			m4_prgROMand = 0x1FFFF;
			m4_chrROMadd = 0x20000;
			m4_chrROMand = 0x1FFFF;
		}
		else //val == 7
		{
			m4_prgROMadd = 0x30000;
			m4_prgROMand = 0xFFFF;
			m4_chrROMadd = 0x20000;
			m4_chrROMand = 0x1FFFF;
		}
	}
	else if(addr >= 0x8000)
		m4set8(addr, val);
}

void m44_set8(uint16_t addr, uint8_t val)
{
	if(addr == 0xA001)
	{
		val &= 7;
		switch(val)
		{
			case 0:
				m4_prgROMadd = 0;
				m4_prgROMand = 0x1FFFF;
				m4_chrROMadd = 0;
				m4_chrROMand = 0x1FFFF;
				break;
			case 1:
				m4_prgROMadd = 0x20000;
				m4_prgROMand = 0x1FFFF;
				m4_chrROMadd = 0x20000;
				m4_chrROMand = 0x1FFFF;
				break;
			case 2:
				m4_prgROMadd = 0x40000;
				m4_prgROMand = 0x1FFFF;
				m4_chrROMadd = 0x40000;
				m4_chrROMand = 0x1FFFF;
				break;
			case 3:
				m4_prgROMadd = 0x60000;
				m4_prgROMand = 0x1FFFF;
				m4_chrROMadd = 0x60000;
				m4_chrROMand = 0x1FFFF;
				break;
			case 4:
				m4_prgROMadd = 0x80000;
				m4_prgROMand = 0x1FFFF;
				m4_chrROMadd = 0x80000;
				m4_chrROMand = 0x1FFFF;
				break;
			case 5:
				m4_prgROMadd = 0xA0000;
				m4_prgROMand = 0x1FFFF;
				m4_chrROMadd = 0xA0000;
				m4_chrROMand = 0x1FFFF;
				break;
			default: //6,7
				m4_prgROMadd = 0xC0000;
				m4_prgROMand = 0x3FFFF;
				m4_chrROMadd = 0xC0000;
				m4_chrROMand = 0x3FFFF;
				break;
		}
	}
	else
		m4set8(addr, val);
}

void m45_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000 && addr >= 0x6000)
	{
		if(m4add_regLock)
			m4set8(addr, val);
		else
		{
			if(m45_curReg == 0)
			{
				m4_chrROMadd = (m4_chrROMadd & ~0x3FFFF) | (val<<10);
				//printf("m4_chrROMadd r0 %08x inVal %02x\n", m4_chrROMadd, val);
			}
			else if(m45_curReg == 1)
			{
				m4_prgROMadd = val<<13;
				//printf("m4_prgROMadd %08x inVal %02x\n", m4_prgROMadd, val);
			}
			else if(m45_curReg == 2)
			{
				m4_chrROMand = (((0xFF >> ((~val)&0xF))+1)<<10)-1;
				m4_chrROMadd = (m4_chrROMadd & 0x3FFFF) | ((val>>4)<<18);
				//printf("m4_chrROMand %08x m4_chrROMadd r1 %08x inVal %02x\n", m4_chrROMand, m4_chrROMadd, val);
			}
			else if(m45_curReg == 3)
			{
				m4add_regLock = ((val&0x40) != 0);
				m4_prgROMand = ((((val^0x3F)&0x3F)+1)<<13)-1;
				//printf("m4add_regLock %d m4_prgROMand %08x inVal %02x\n", m4add_regLock, m4_prgROMand, val);
			}
			m45_curReg++;
			if(m45_curReg >= 4)
				m45_curReg = 0;
		}
	}
	else if(addr >= 0x8000)
		m4set8(addr, val);
}

void m47_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000 && addr >= 0x6000)
	{
		if(val == 0)
		{
			m4_prgROMadd = 0;
			m4_chrROMadd = 0;
		}
		else if(val & 1)
		{
			m4_prgROMadd = 0x20000;
			m4_chrROMadd = 0x20000;
		}
	}
	else if(addr >= 0x8000)
		m4set8(addr, val);
}

void m49_set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		if(!m4add_regLock)
		{
			switch(val>>6)
			{
				case 0:
					m4_prgROMadd = 0;
					m4_prgROMand = 0x1FFFF;
					m4_chrROMadd = 0;
					m4_chrROMand = 0x1FFFF;
					break;
				case 1:
					m4_prgROMadd = 0x20000;
					m4_prgROMand = 0x1FFFF;
					m4_chrROMadd = 0x20000;
					m4_chrROMand = 0x1FFFF;
					break;
				case 2:
					m4_prgROMadd = 0x40000;
					m4_prgROMand = 0x1FFFF;
					m4_chrROMadd = 0x40000;
					m4_chrROMand = 0x1FFFF;
					break;
				case 3:
					m4_prgROMadd = 0x60000;
					m4_prgROMand = 0x1FFFF;
					m4_chrROMadd = 0x60000;
					m4_chrROMand = 0x1FFFF;
					break;
				default:
					break;
			}
			m49_prgmode = val&1;
			m49_prgreg = (val>>4)&3;
		}
	}
	else if(addr >= 0x8000)
	{
		if(addr >= 0xA000 && addr < 0xC000 && (addr&1))
			m4add_regLock = ((val&0x80) == 0);
		m4set8(addr, val);
	}
}

void m52_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000 && addr >= 0x6000)
	{
		if(m4add_regLock)
			m4set8(addr, val);
		else
		{
			if((val&8) != 0)
			{
				m4_prgROMand = 0x1FFFF;
				m4_prgROMadd = (val&7)<<17;
			}
			else
			{
				m4_prgROMand = 0x3FFFF;
				m4_prgROMadd = (val&6)<<17;
			}
			uint8_t chrVal = ((val>>3)&4) | ((val>>1)&2) | ((val>>4)&1);
			if((val&0x40) != 0)
			{
				m4_chrROMand = 0x1FFFF;
				m4_chrROMadd = (chrVal&7)<<17;
			}
			else
			{
				m4_chrROMand = 0x3FFFF;
				m4_chrROMadd = (chrVal&6)<<17;
			}
			m4add_regLock = ((val&0x80) != 0);
		}
	}
	else if(addr >= 0x8000)
		m4set8(addr, val);
}

void m205_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000 && addr >= 0x6000)
	{
		val &= 3;
		if(val == 0)
		{
			m4_prgROMadd = 0;
			m4_prgROMand = 0x3FFFF;
			m4_chrROMadd = 0;
			m4_chrROMand = 0x3FFFF;
		}
		else if(val == 1)
		{
			m4_prgROMadd = 0x20000;
			m4_prgROMand = 0x3FFFF;
			m4_chrROMadd = 0x20000;
			m4_chrROMand = 0x3FFFF;
		}
		else if(val == 2)
		{
			m4_prgROMadd = 0x40000;
			m4_prgROMand = 0x1FFFF;
			m4_chrROMadd = 0x40000;
			m4_chrROMand = 0x1FFFF;
		}
		else //val == 3
		{
			m4_prgROMadd = 0x60000;
			m4_prgROMand = 0x1FFFF;
			m4_chrROMadd = 0x60000;
			m4_chrROMand = 0x1FFFF;
		}
	}
	else if(addr >= 0x8000)
		m4set8(addr, val);
}

uint8_t m49_get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x8000)
	{
		if(m49_prgmode == 0)
			return m49_prgROM[(addr&0x7FFF)|(m49_prgreg<<15)|m4_prgROMadd];
		else
			return m4get8(addr, val);
	}
	return val;
}
