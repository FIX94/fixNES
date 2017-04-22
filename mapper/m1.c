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

static uint8_t *m1_prgROM;
static uint8_t *m1_prgRAM;
static uint8_t *m1_chrROM;
static uint32_t m1_prgROMsize;
static uint32_t m1_prgRAMsize;
static uint32_t m1_chrROMsize;
static uint8_t m1_chrRAM[0x2000];
static uint32_t m1_256KPRGBank;
static uint32_t m1_curPRGBank;
static uint32_t m1_firstPRGBank;
static uint32_t m1_lastPRGBank;
static uint32_t m1_curCHRBank0;
static uint32_t m1_curCHRBank1;
static uint8_t m1_sr;
static bool m1_single_prg_bank;
static bool m1_last_bank_fixed;
static bool m1_single_chr_bank;

void m1init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m1_prgROM = prgROMin;
	m1_prgROMsize = prgROMsizeIn;
	m1_prgRAM = prgRAMin;
	m1_prgRAMsize = prgRAMsizeIn;
	m1_firstPRGBank = 0;
	m1_256KPRGBank = 0;
	m1_curPRGBank = m1_firstPRGBank;
	m1_lastPRGBank = (prgROMsizeIn - 0x4000)&0x3FFFF;
	if(chrROMsizeIn > 0)
	{
		m1_chrROM = chrROMin;
		m1_chrROMsize = chrROMsizeIn;
	}
	else
	{
		m1_chrROM = m1_chrRAM;
		m1_chrROMsize = 0x2000;
	}
	m1_curCHRBank0 = 0;
	m1_curCHRBank1 = 0;
	memset(m1_chrRAM,0,0x2000);
	m1_sr = (1<<4);//0;
	m1_single_prg_bank = false;
	m1_last_bank_fixed = true;
	m1_single_chr_bank = false;
	printf("Mapper 1 inited, last bank=%04x sr=%02x\n", m1_lastPRGBank, m1_sr);
}

uint8_t m1get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m1_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(m1_single_prg_bank)
		{
			return m1_prgROM[(m1_curPRGBank&~0x7FFF)+(addr&0x7FFF)+m1_256KPRGBank];
		}
		else if(m1_last_bank_fixed)
		{
			if(addr < 0xC000)
				return m1_prgROM[(m1_curPRGBank&~0x3FFF)+(addr&0x3FFF)+m1_256KPRGBank];
			return m1_prgROM[m1_lastPRGBank+(addr&0x3FFF)+m1_256KPRGBank];
		}
		else //first bank fixed
		{
			if(addr < 0xC000)
				return m1_prgROM[m1_firstPRGBank+(addr&0x3FFF)+m1_256KPRGBank];
			return m1_prgROM[(m1_curPRGBank&~0x3FFF)+(addr&0x3FFF)+m1_256KPRGBank];
		}
	}
	return val;
}

extern bool cpuWriteTMP;
void m1set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("m1set8 %04x %02x\n", addr, val);
		m1_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0x8000)
	{
		// mmc1 regs cant be written to
		// with just 1 cpu cycle delay
		if(cpuWriteTMP)
			return;
		//printf("m1set8 %04x %02x\n", addr, val);
		if(val&(1<<7))
		{
			//printf("Reset (???)\n");
			m1_sr = (1<<4);//0;
			m1_last_bank_fixed = true;
			m1_single_prg_bank = false;
		}
		else if(m1_sr & 1)
		{
			m1_sr >>= 1;
			m1_sr |= ((val&1)<<4);
			//printf("m1 sr full, addr %04x sr %02x\n", addr, m1_sr);
			if(addr < 0xA000)
			{
				if(!ppu4Screen)
				{
					if((m1_sr & 3) == 2)
					{
						//printf("Vertical mode\n");
						ppuSetNameTblVertical();
					}
					else if((m1_sr & 3) == 3)
					{
						//printf("Horizontal mode\n");
						ppuSetNameTblHorizontal();
					}
					else
					{
						if((m1_sr & 3) == 1)
						{
							//printf("Upper CHR Mode\n");
							//m1_small_upper_chr = true;
							ppuSetNameTblSingleUpper();
						}
						else
						{
							
							//printf("Lower CHR Mode\n");
							//m1_small_upper_chr = false;
							ppuSetNameTblSingleLower();
						}
					}
				}
				if(((m1_sr>>2) & 3) == 2)
				{
					//printf("First bank fixed\n");
					m1_last_bank_fixed = false;
					m1_single_prg_bank = false;
				}
				else if(((m1_sr>>2) & 3) == 3)
				{
					//printf("Last bank fixed\n");
					m1_last_bank_fixed = true;
					m1_single_prg_bank = false;
				}
				else
				{
					//printf("Some other PRG Bank mode\n");
					m1_single_prg_bank = true;
				}

				if(m1_sr & (1<<4))
				{
					//printf("2 CHR Banks\n");
					m1_single_chr_bank = false;
				}
				else
				{
					//printf("Single CHR Bank\n");
					m1_single_chr_bank = true;
				}
			}
			else if(addr < 0xC000)
			{
				if(m1_single_chr_bank) m1_sr &= ~1;
				m1_curCHRBank0 = (m1_sr<<12)&(m1_chrROMsize-1);
				//printf("chr bank 0 now %04x\n", m1_curCHRBank0);
				if(m1_prgROMsize > 0x40000)
					m1_256KPRGBank = (m1_sr&0x10)<<14;
			}
			else if(addr < 0xE000)
			{
				if(!m1_single_chr_bank)
				{
					m1_curCHRBank1 = (m1_sr<<12)&(m1_chrROMsize-1);
					//printf("chr bank 1 now %04x\n", m1_curCHRBank1);
					if(m1_prgROMsize > 0x40000)
						m1_256KPRGBank = (m1_sr&0x10)<<14;
				}
			}
			else
			{
				if(m1_single_prg_bank) m1_sr &= ~1;
				m1_curPRGBank = ((m1_sr&15)<<14)&(m1_prgROMsize-1);
				//printf("switchable bank now %04x\n", m1_curPRGBank);
			}
			//m1_sr = 0;//(m1_sr<<4);
			m1_sr = (1<<4);
		}
		else
		{
			//printf("%02x\n", m1_sr);
			m1_sr >>= 1;
			m1_sr |= ((val&1)<<4);
			//printf("%02x\n", m1_sr);
		}
	}
}

uint8_t m1chrGet8(uint16_t addr)
{
	if(m1_single_chr_bank)
		return m1_chrROM[(m1_curCHRBank0&~0x1FFF)+(addr&0x1FFF)];
	else if(addr < 0x1000)
		return m1_chrROM[(m1_curCHRBank0&~0xFFF)+(addr&0xFFF)];
	return m1_chrROM[(m1_curCHRBank1&~0xFFF)+(addr&0xFFF)];
}

void m1chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m1chrSet8 %04x %02x\n", addr, val);
	if(m1_chrROM == m1_chrRAM) //Writable
		m1_chrROM[addr&0x1FFF] = val;
}
