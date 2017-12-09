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

static uint8_t *vrc3_prgROM;
static uint8_t *vrc3_prgRAM;
static uint8_t *vrc3_chrROM;
static uint8_t vrc3_chrRAM[0x2000];
static uint32_t vrc3_prgROMsize;
static uint32_t vrc3_prgRAMsize;
static uint32_t vrc3_chrROMsize;
static uint32_t vrc3_curPRGBank;
static uint32_t vrc3_lastPRGBank;
static uint32_t vrc3_prgROMand;
static uint32_t vrc3_chrROMand;
static uint16_t vrc3_irqCtr;
static uint16_t vrc3_irqReloadCtr;
static bool vrc3_irqEnable;
static bool vrc3_irqEnable_after_ack;
static bool vrc3_irq8Bit;
extern bool mapper_interrupt;

void vrc3init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	vrc3_prgROM = prgROMin;
	vrc3_prgROMsize = prgROMsizeIn;
	vrc3_prgRAM = prgRAMin;
	vrc3_prgRAMsize = prgRAMsizeIn;
	vrc3_curPRGBank = 0;
	vrc3_lastPRGBank = (prgROMsizeIn - 0x4000);
	vrc3_prgROMand = mapperGetAndValue(prgROMsizeIn);
	if(chrROMin && chrROMsizeIn)
	{
		vrc3_chrROM = chrROMin;
		vrc3_chrROMsize = chrROMsizeIn;
		vrc3_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
	{
		vrc3_chrROM = vrc3_chrRAM;
		vrc3_chrROMsize = 0x2000;
		vrc3_chrROMand = 0x1FFF;
	}
	memset(vrc3_chrRAM, 0, 0x2000);
	vrc3_irqCtr = 0;
	vrc3_irqReloadCtr = 0;
	vrc3_irqEnable = false;
	vrc3_irqEnable_after_ack = false;
	vrc3_irq8Bit = false;
	printf("vrc3 Mapper inited\n");
}

uint8_t vrc3get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return vrc3_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xC000)
			return vrc3_prgROM[(((vrc3_curPRGBank<<14)+(addr&0x3FFF))&vrc3_prgROMand)];
		return vrc3_prgROM[((vrc3_lastPRGBank+(addr&0x3FFF))&vrc3_prgROMand)];
	}
	return val;
}

void vrc3set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		vrc3_prgRAM[addr&0x1FFF] = val;
		return;
	}
	else if(addr < 0x9000)
		vrc3_irqReloadCtr = (vrc3_irqReloadCtr&0xFFF0)|((val&0xF)<<0);
	else if(addr < 0xA000)
		vrc3_irqReloadCtr = (vrc3_irqReloadCtr&0xFF0F)|((val&0xF)<<4);
	else if(addr < 0xB000)
		vrc3_irqReloadCtr = (vrc3_irqReloadCtr&0xF0FF)|((val&0xF)<<8);
	else if(addr < 0xC000)
		vrc3_irqReloadCtr = (vrc3_irqReloadCtr&0x0FFF)|((val&0xF)<<12);
	else if(addr < 0xD000)
	{
		mapper_interrupt = false;
		vrc3_irqEnable_after_ack = !!(val&1);
		vrc3_irqEnable = !!(val&2);
		vrc3_irq8Bit = !!(val&4);
		if(vrc3_irqEnable)
			vrc3_irqCtr = vrc3_irqReloadCtr;
	}
	else if(addr < 0xE000)
	{
		mapper_interrupt = false;
		vrc3_irqEnable = vrc3_irqEnable_after_ack;
	}
	else if(addr >= 0xF000)
		vrc3_curPRGBank = (val&7);
}

uint8_t vrc3chrGet8(uint16_t addr)
{
	return vrc3_chrROM[addr&0x1FFF];
}

void vrc3chrSet8(uint16_t addr, uint8_t val)
{
	if(vrc3_chrROM == vrc3_chrRAM)
		vrc3_chrRAM[addr&0x1FFF] = val;
}

void vrc3cycle()
{
	if(vrc3_irqEnable)
	{
		if(vrc3_irq8Bit)
		{
			uint8_t tmp = vrc3_irqCtr&0xFF;
			tmp++;
			vrc3_irqCtr = (vrc3_irqCtr&0xFF00)|tmp;
			if(tmp == 0)
			{
				vrc3_irqEnable = false;
				mapper_interrupt = true;
				vrc3_irqCtr = (vrc3_irqCtr&0xFF00)|(vrc3_irqReloadCtr&0xFF);
			}
		}
		else
		{
			vrc3_irqCtr++;
			if(vrc3_irqCtr == 0)
			{
				vrc3_irqEnable = false;
				mapper_interrupt = true;
				vrc3_irqCtr = vrc3_irqReloadCtr;
			}
		}
	}
}
