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
	printf("Mapper 44 (Mapper 4 Game Select) inited\n");
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
	printf("Mapper 47 (Mapper 4 Game Select) inited\n");
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
