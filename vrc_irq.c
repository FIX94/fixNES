/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "cpu.h"

static uint8_t vrc_irqCtr;
static uint8_t vrc_irqCurCtr;
static uint8_t vrc_irqPrescaler;
static uint8_t vrc_irq_scanTblPos;
static bool vrc_irqEnabled;
static bool vrc_irqEnable_after_ack;
static bool vrc_irqCyclemode;
extern uint8_t interrupt;

static uint8_t vrc_irq_scanTbl[3] = { 113, 113, 112 };

void vrc_irq_init()
{
	vrc_irqCtr = 0;
	vrc_irqCurCtr = 0;
	vrc_irqPrescaler = 0;
	vrc_irqEnabled = false;
	vrc_irqEnable_after_ack = false;
	vrc_irqCyclemode = false;
	vrc_irq_scanTblPos = 0;
}

void vrc_irq_setlatch(uint8_t val)
{
	vrc_irqCtr = val;
}

void vrc_irq_setlatchLo(uint8_t val)
{
	vrc_irqCtr = (vrc_irqCtr&~0xF) | (val&0xF);
}

void vrc_irq_setlatchHi(uint8_t val)
{
	vrc_irqCtr = (vrc_irqCtr&0xF) | ((val&0xF)<<4);
}

void vrc_irq_control(uint8_t val)
{
	interrupt &= ~MAPPER_IRQ;
	vrc_irqEnable_after_ack = ((val&1) != 0);
	vrc_irqCyclemode = ((val&4) != 0);
	if((val&2) != 0)
	{
		vrc_irqEnabled = true;
		vrc_irqCurCtr = vrc_irqCtr;
		vrc_irqPrescaler = 0;
		vrc_irq_scanTblPos = 0;
		//printf("vrc_irq IRQ Reload with in %02x ctr %i\n", val, vrc_irqCtr);
	}
	else
	{
		vrc_irqEnabled = false;
		//printf("vrc_irq IRQ Write with in %02x ctr %i\n", val, vrc_irqCtr);
	}
}

void vrc_irq_ack()
{
	interrupt &= ~MAPPER_IRQ;
	if(vrc_irqEnable_after_ack)
	{
		vrc_irqEnabled = true;
		//printf("vrc_irq ACK IRQ Reload\n");
	}
	//else
	//	printf("vrc_irq ACK No Reload\n");
}

void vrc_irq_ctrcycle()
{
	if(vrc_irqCurCtr == 0xFF)
	{
		if(vrc_irqEnabled)
		{
			//printf("vrc_irq Cycle Interrupt\n");
			interrupt |= MAPPER_IRQ;
			vrc_irqEnabled = false;
		}
		vrc_irqCurCtr = vrc_irqCtr;
	}
	else
		vrc_irqCurCtr++;
}

void vrc_irq_cycle()
{
	if(vrc_irqCyclemode)
		vrc_irq_ctrcycle();
	else
	{
		if(vrc_irqPrescaler >= vrc_irq_scanTbl[vrc_irq_scanTblPos])
		{
			vrc_irq_ctrcycle();
			vrc_irq_scanTblPos++;
			if(vrc_irq_scanTblPos >= 3)
				vrc_irq_scanTblPos = 0;
			vrc_irqPrescaler = 0;
		}
		else
			vrc_irqPrescaler++;
	}
}
