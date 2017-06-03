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
#include "../vrc_irq.h"

static uint8_t *vrc2_4_prgROM;
static uint8_t *vrc2_4_prgRAM;
static uint8_t *vrc2_4_chrROM;
static uint32_t vrc2_4_prgROMsize;
static uint32_t vrc2_4_prgRAMsize;
static uint32_t vrc2_4_chrROMsize;
static uint32_t vrc2_4_curPRGBank0;
static uint32_t vrc2_4_curPRGBank1;
static uint32_t vrc2_4_lastM1PRGBank;
static uint32_t vrc2_4_lastPRGBank;
static uint32_t vrc2_4_CHRBank[8];
static uint32_t vrc2_4_prgROMand;
static uint32_t vrc2_4_chrROMand;
static bool vrc2_4_prg_bank_flip;

void vrc2_4_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	vrc2_4_prgROM = prgROMin;
	vrc2_4_prgROMsize = prgROMsizeIn;
	vrc2_4_prgRAM = prgRAMin;
	vrc2_4_prgRAMsize = prgRAMsizeIn;
	vrc2_4_curPRGBank0 = 0;
	vrc2_4_curPRGBank1 = 0x2000;
	vrc2_4_lastPRGBank = (prgROMsizeIn - 0x2000);
	vrc2_4_lastM1PRGBank = vrc2_4_lastPRGBank - 0x2000;
	vrc2_4_prgROMand = prgROMsizeIn-1;
	vrc2_4_chrROM = chrROMin;
	vrc2_4_chrROMsize = chrROMsizeIn;
	vrc2_4_chrROMand = chrROMsizeIn-1;
	memset(vrc2_4_CHRBank, 0, 8*sizeof(uint32_t));
	vrc2_4_prg_bank_flip = false;
	vrc_irq_init();
	printf("vrc2/4 Mapper inited\n");
}

uint8_t vrc2_4_get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return vrc2_4_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
		{
			if(vrc2_4_prg_bank_flip)
				return vrc2_4_prgROM[(((vrc2_4_lastM1PRGBank+(addr&0x1FFF)))&vrc2_4_prgROMand)];
			else
				return vrc2_4_prgROM[(((vrc2_4_curPRGBank0<<13)+(addr&0x1FFF))&vrc2_4_prgROMand)];
		}
		else if(addr < 0xC000)
			return vrc2_4_prgROM[(((vrc2_4_curPRGBank1<<13)+(addr&0x1FFF))&vrc2_4_prgROMand)];
		else if(addr < 0xE000)
		{
			if(vrc2_4_prg_bank_flip)
				return vrc2_4_prgROM[(((vrc2_4_curPRGBank0<<13)+(addr&0x1FFF))&vrc2_4_prgROMand)];
			else
				return vrc2_4_prgROM[((vrc2_4_lastM1PRGBank+(addr&0x1FFF))&vrc2_4_prgROMand)];
		}
		return vrc2_4_prgROM[((vrc2_4_lastPRGBank+(addr&0x1FFF))&vrc2_4_prgROMand)];
	}
	return val;
}

void vrc2_4_set8(uint16_t addr, uint8_t val)
{
	//select all possible addresses
	addr &= 0xF003;
	if(addr == 0x8000 || addr == 0x8001 || addr == 0x8002 || addr == 0x8003)
		vrc2_4_curPRGBank0 = (val&0x1F);
	else if(addr == 0x9000 || addr == 0x9001)
	{
		if(!ppu4Screen)
		{
			val &= 3;
			switch(val)
			{
				case 0:
					ppuSetNameTblVertical();
					break;
				case 1:
					ppuSetNameTblHorizontal();
					break;
				case 2:
					ppuSetNameTblSingleLower();
					break;
				default: //case 3
					ppuSetNameTblSingleUpper();
					break;
			}
		}
	}
	else if(addr == 0x9002 || addr == 0x9003)
		vrc2_4_prg_bank_flip = ((val&2) != 0);
	else if(addr == 0xA000 || addr == 0xA001 || addr == 0xA002 || addr == 0xA003)
		vrc2_4_curPRGBank1 = (val&0x1F);
	else if(addr == 0xB000)
		vrc2_4_CHRBank[0] = (vrc2_4_CHRBank[0]&~0xF) | (val&0xF);
	else if(addr == 0xB001)
		vrc2_4_CHRBank[0] = (vrc2_4_CHRBank[0]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xB002)
		vrc2_4_CHRBank[1] = (vrc2_4_CHRBank[1]&~0xF) | (val&0xF);
	else if(addr == 0xB003)
		vrc2_4_CHRBank[1] = (vrc2_4_CHRBank[1]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xC000)
		vrc2_4_CHRBank[2] = (vrc2_4_CHRBank[2]&~0xF) | (val&0xF);
	else if(addr == 0xC001)
		vrc2_4_CHRBank[2] = (vrc2_4_CHRBank[2]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xC002)
		vrc2_4_CHRBank[3] = (vrc2_4_CHRBank[3]&~0xF) | (val&0xF);
	else if(addr == 0xC003)
		vrc2_4_CHRBank[3] = (vrc2_4_CHRBank[3]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xD000)
		vrc2_4_CHRBank[4] = (vrc2_4_CHRBank[4]&~0xF) | (val&0xF);
	else if(addr == 0xD001)
		vrc2_4_CHRBank[4] = (vrc2_4_CHRBank[4]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xD002)
		vrc2_4_CHRBank[5] = (vrc2_4_CHRBank[5]&~0xF) | (val&0xF);
	else if(addr == 0xD003)
		vrc2_4_CHRBank[5] = (vrc2_4_CHRBank[5]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xE000)
		vrc2_4_CHRBank[6] = (vrc2_4_CHRBank[6]&~0xF) | (val&0xF);
	else if(addr == 0xE001)
		vrc2_4_CHRBank[6] = (vrc2_4_CHRBank[6]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xE002)
		vrc2_4_CHRBank[7] = (vrc2_4_CHRBank[7]&~0xF) | (val&0xF);
	else if(addr == 0xE003)
		vrc2_4_CHRBank[7] = (vrc2_4_CHRBank[7]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xF000)
		vrc_irq_setlatchLo(val);
	else if(addr == 0xF001)
		vrc_irq_setlatchHi(val);
	else if(addr == 0xF002)
		vrc_irq_control(val);
	else if(addr == 0xF003)
		vrc_irq_ack();
}

void m21_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc2_4_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m21_set8 %04x %02x\n", addr, val);
	//vrc2_4_4 M21 Address Setup
	uint8_t tmp1 = (addr>>1)&3;
	uint8_t tmp2 = (addr>>6)&3;
	addr &= 0xF000; addr |= (tmp1 | tmp2);
	vrc2_4_set8(addr, val);
}

void m22_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	//printf("m22_set8 %04x %02x\n", addr, val);
	//vrc2_4_2 only supports vertical/horizonal
	if(addr == 0x9000 || addr == 0x9002)
		val &= 1;
	//vrc2_4_2 M22 Address Setup
	uint8_t tmp = addr&3;
	addr &= ~0x3; addr |= (((tmp&1)<<1) | ((tmp&2)>>1));
	vrc2_4_set8(addr, val);
}

void m23_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc2_4_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m23_set8 %04x %02x\n", addr, val);
	//vrc2_4_2 Wai Wai World, avoid wrong mirroring
	if(addr == 0x9000 && val == 0xFF)
		val &= 1; //only supports vertical/horizonal
	//vrc2_4_4 M23 Address Setup
	uint8_t tmp = (addr>>2)&3;
	addr &= ~0xC; addr |= tmp;
	vrc2_4_set8(addr, val);
}

void m25_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc2_4_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m25_set8 %04x %02x\n", addr, val);
	//vrc2_4_4 M25 Address Setup
	uint8_t tmp1 = (((addr&1)<<1) | ((addr&2)>>1));
	uint8_t tmp2 = ((((addr>>2)&1)<<1) | (((addr>>2)&2)>>1));
	addr &= 0xF000; addr |= (tmp1 | tmp2);
	vrc2_4_set8(addr, val);
}

uint8_t vrc2_4_chrGet8(uint16_t addr)
{
	if(addr < 0x400)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[0]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x800)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[1]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0xC00)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[2]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1000)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[3]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1400)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[4]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1800)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[5]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1C00)
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[6]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else // < 0x2000
		return vrc2_4_chrROM[(((vrc2_4_CHRBank[7]<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
}

uint8_t m22_chrGet8(uint16_t addr)
{
	if(addr < 0x400)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[0]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x800)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[1]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0xC00)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[2]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1000)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[3]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1400)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[4]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1800)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[5]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else if(addr < 0x1C00)
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[6]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
	else // < 0x2000
		return vrc2_4_chrROM[((((vrc2_4_CHRBank[7]>>1)<<10)+(addr&0x3FF))&vrc2_4_chrROMand)];
}

void vrc2_4_chrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}

void vrc2_4_cycle()
{
	vrc_irq_cycle();
}
