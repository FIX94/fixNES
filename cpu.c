/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "mem.h"
#include "ppu.h"
#include "apu.h"

#define P_FLAG_CARRY (1<<0)
#define P_FLAG_ZERO (1<<1)
#define P_FLAG_IRQ_DISABLE (1<<2)
#define P_FLAG_DECIMAL (1<<3)
#define P_FLAG_S1 (1<<4)
#define P_FLAG_S2 (1<<5)
#define P_FLAG_OVERFLOW (1<<6)
#define P_FLAG_NEGATIVE (1<<7)

static uint16_t pc;
static uint8_t p,a,x,y,s;
static uint32_t waitCycles;
static bool reset;
static bool interrupt;
//used externally
bool dmc_interrupt;
bool apu_interrupt;
uint32_t cpu_oam_dma;
extern bool nesPause;

void cpuInit()
{
	reset = true;
	interrupt = false;
	dmc_interrupt = false;
	apu_interrupt = false;
	cpu_oam_dma = 0;
	p = (P_FLAG_IRQ_DISABLE | P_FLAG_S1 | P_FLAG_S2);
	a = 0;
	x = 0;
	y = 0;
	s = 0xFD;
	waitCycles = 0;
}

static void setRegStats(uint8_t reg)
{
	if(reg == 0)
	{
		p |= P_FLAG_ZERO;
		p &= ~P_FLAG_NEGATIVE;
	}
	else
	{
		if(reg & (1<<7))
			p |= P_FLAG_NEGATIVE;
		else
			p &= ~P_FLAG_NEGATIVE;
		p &= ~P_FLAG_ZERO;
	}
}

static void cpuSetARRRegs()
{
	if((a & ((1<<5) | (1<<6))) == ((1<<5) | (1<<6)))
	{
		p |= P_FLAG_CARRY;
		p &= ~P_FLAG_OVERFLOW;
	}
	else if((a & ((1<<5) | (1<<6))) == 0)
	{
		p &= ~P_FLAG_CARRY;
		p &= ~P_FLAG_OVERFLOW;
	}
	else if(a & (1<<5))
	{
		p &= ~P_FLAG_CARRY;
		p |= P_FLAG_OVERFLOW;
	}
	else if(a & (1<<6))
	{
		p |= P_FLAG_CARRY;
		p |= P_FLAG_OVERFLOW;
	}
}

static void cpuRelativeBranch()
{
	int8_t loc = memGet8(pc++);
	uint16_t oldPc = pc;
	pc += loc;
	waitCycles++;
	if((oldPc&0xFF00) != (pc&0xFF00))
		waitCycles++;
}

static void cpuDummyRead(uint16_t addr, uint8_t val, bool alwaysAddCycle)
{
	if(alwaysAddCycle || (addr>>8) != ((addr+val)>>8))
	{   //dummy read
		memGet8((addr&0xFF00)|((addr+val)&0xFF));
		waitCycles++;
	}
}

/* Helper functions for updating reg sets */

static inline void cpuSetA(uint8_t val)
{
	a = val;
	setRegStats(a);
}

static inline void cpuSetX(uint8_t val)
{
	x = val;
	setRegStats(x);
}

static inline void cpuSetY(uint8_t val)
{
	y = val;
	setRegStats(y);
}

static inline uint8_t cpuSetTMP(uint8_t val)
{
	setRegStats(val);
	return val;
}


/* Various functions for different memory access instructions */

static inline uint8_t getImmediate() { return memGet8(pc); };
static inline uint8_t getZeroPage() { return memGet8(pc); }
static inline uint8_t getZeroPagePlus(uint8_t tmp) { return (getZeroPage()+tmp)&0xFF; }
static inline uint16_t getAbsoluteAddr() { return memGet8(pc)|(memGet8(pc+1)<<8); }

static inline uint16_t getAbsolutePlusAddr(uint8_t val, bool alwaysAddCycle)
{
	uint16_t addr = getAbsoluteAddr();
	cpuDummyRead(addr, val, alwaysAddCycle);
	return addr+val;
}

static inline uint16_t getIndirectPlusXaddr() { uint8_t p = getZeroPagePlus(x); return (memGet8(p))|(memGet8((p+1)&0xFF)<<8); }
static inline uint16_t getIndirectPlusYaddr(bool alwaysAddCycle) 
{
	uint8_t p = getZeroPage();
	uint16_t addr = (memGet8(p)|memGet8((p+1)&0xFF)<<8);
	cpuDummyRead(addr, y, alwaysAddCycle);
	return addr+y;
}

static inline void cpuImmediateEnd() { pc++; }
static inline void cpuZeroPageEnd() { pc++; waitCycles++; }
static inline void cpuZeroPagePlusEnd() { pc++; waitCycles+=2; }
static inline void cpuAbsoluteEnd() { pc+=2; waitCycles+=2; }
static inline void cpuIndirectPlusXEnd() { pc++; waitCycles+=4; }
static inline void cpuIndirectPlusYEnd() { pc++; waitCycles+=3; }

//used externally
bool cpuWriteTMP = false;

static inline void cpuSaveTMPStart(uint16_t addr, uint8_t val)
{
	memSet8(addr, val);
	waitCycles++;
	cpuWriteTMP = true;
}

static inline void cpuSaveTMPEnd(uint16_t addr, uint8_t val)
{
	memSet8(addr, val);
	waitCycles++;
	cpuWriteTMP = false;
}


/* Various Instructions used multiple times */

static inline void cpuAND(uint8_t val)
{
	a &= val;
	setRegStats(a);
}

static inline void cpuORA(uint8_t val)
{
	a |= val;
	setRegStats(a);
}

static inline void cpuEOR(uint8_t val)
{
	a ^= val;
	setRegStats(a);
}

static inline uint8_t cpuASL(uint8_t val)
{
	if(val & (1<<7))
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
	val <<= 1;
	setRegStats(val);
	return val;
}

static inline uint8_t cpuLSR(uint8_t val)
{
	if(val & (1<<0))
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
	val >>= 1;
	setRegStats(val);
	return val;
}

static inline uint8_t cpuROL(uint8_t val)
{
	uint8_t oldP = p;
	if(val & (1<<7))
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
	val<<=1;
	if(oldP & P_FLAG_CARRY)
		val |= (1<<0); //bit 0
	setRegStats(val);
	return val;
}

static inline uint8_t cpuROR(uint8_t val)
{
	uint8_t oldP = p;
	if(val & (1<<0))
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
	val>>=1;
	if(oldP & P_FLAG_CARRY)
		val |= (1<<7); //bit 7
	setRegStats(val);
	return val;
}

static void cpuKIL()
{
	printf("Processor Requested Lock-Up at %04x\n", pc-1);
	nesPause = true;
}

static void cpuADC(uint8_t tmp)
{
	//use uint16_t here to easly detect carry
	uint16_t res = a + tmp;

	if(p & P_FLAG_CARRY)
		res++;

	if(res > 0xFF)
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;

	if(!(a & (1<<7)) && !(tmp & (1<<7)) && (res & (1<<7)))
		p |= P_FLAG_OVERFLOW;
	else if((a & (1<<7)) && (tmp & (1<<7)) && !(res & (1<<7)))
		p |= P_FLAG_OVERFLOW;
	else
		p &= ~P_FLAG_OVERFLOW;

	cpuSetA(res);
}

static inline void cpuSBC(uint8_t tmp) { cpuADC(~tmp); }

static uint8_t cpuCMP(uint8_t v1, uint8_t v2)
{
	if(v1 >= v2)
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;

	uint8_t cmpVal = (v1 - v2);
	setRegStats(cmpVal);
	return cmpVal;
}

static void cpuBIT(uint8_t tmp)
{
	if((a & tmp) == 0)
		p |= P_FLAG_ZERO;
	else
		p &= ~P_FLAG_ZERO;

	if(tmp & P_FLAG_OVERFLOW)
		p |= P_FLAG_OVERFLOW;
	else
		p &= ~P_FLAG_OVERFLOW;

	if(tmp & P_FLAG_NEGATIVE)
		p |= P_FLAG_NEGATIVE;
	else
		p &= ~P_FLAG_NEGATIVE;
}

/* For Interrupt Handling */

#define DEBUG_INTR 0
#define DEBUG_JSR 0

static void intrBackup()
{
	uint16_t tmp16 = pc;//+1;
	//back up pc on stack
	memSet8(0x100+s,tmp16>>8);
	s--;
	memSet8(0x100+s,tmp16&0xFF);
	s--;
	//back up p on stack
	memSet8(0x100+s,p);
	s--;
}


/* Main CPU Interpreter */
//static int intrPrintUpdate = 0;
static int p_irq_req = 0;
static bool cpu_interrupt_req = false;
static bool ppu_nmi_handler_req = false;
//set externally
bool mapper_interrupt = false;
bool cpuCycle()
{
	//make sure to wait if needed
	if(waitCycles)
	{
		waitCycles--;
		return true;
	}
	if(cpu_oam_dma)
	{
		cpu_oam_dma--;
		return true;
	}
	//grab reset from vector
	if(reset)
	{
		pc = memGet8(0xFFFC)|(memGet8(0xFFFD)<<8);
		#if DEBUG_INTR
		printf("Reset at %04x\n",pc);
		#endif
		reset = false;
		return true;
	}
	//update irq flag if requested
	if(p_irq_req)
	{
		if(p_irq_req == 1)
		{
			#if DEBUG_INTR
			printf("Setting irq disable %02x %02x %d %d %d %d\n", p, P_FLAG_IRQ_DISABLE, cpu_interrupt_req, apu_interrupt, 
				!(p & P_FLAG_IRQ_DISABLE), (p & P_FLAG_IRQ_DISABLE) == 0);
			#endif
			p |= P_FLAG_IRQ_DISABLE;
		}
		else
		{
			#if DEBUG_INTR
			printf("Clearing irq disable %02x %02x %d %d %d %d\n", p, P_FLAG_IRQ_DISABLE, cpu_interrupt_req, apu_interrupt, 
				!(p & P_FLAG_IRQ_DISABLE), (p & P_FLAG_IRQ_DISABLE) == 0);
			#endif
			p &= ~P_FLAG_IRQ_DISABLE;
		}
		p_irq_req = 0;
	}
	if(ppu_nmi_handler_req)
	{
		#if DEBUG_INTR
		printf("NMI from p %02x pc %04x ",p,pc);
		#endif
		p |= P_FLAG_S2;
		p &= ~P_FLAG_S1;
		intrBackup();
		p |= P_FLAG_IRQ_DISABLE;
		//jump to NMI
		pc = memGet8(0xFFFA)|(memGet8(0xFFFB)<<8);
		#if DEBUG_INTR
		printf("to pc %04x\n",pc);
		#endif
		waitCycles+=5;
		ppu_nmi_handler_req = false;
	}
	else if(cpu_interrupt_req)
	{
		#if DEBUG_INTR
		printf("INTR %d %d %d from p %02x pc %04x ",interrupt,dmc_interrupt,apu_interrupt,p,pc);
		#endif
		intrBackup();
		p |= P_FLAG_IRQ_DISABLE;
		pc = memGet8(0xFFFE)|(memGet8(0xFFFF)<<8);
		#if DEBUG_INTR
		printf("to pc %04x\n",pc);
		#endif
		if(interrupt)
			interrupt = false;
		waitCycles+=5;
	}
	uint16_t instrPtr = pc;
	/*if(intrPrintUpdate == 100)
	{
		printf("%04x\n", instrPtr);
		intrPrintUpdate = 0;
	}
	else
		intrPrintUpdate++;*/
	uint8_t instr = memGet8(instrPtr);
	//printf("%04x %02x\n", instrPtr, instr);
	uint8_t tmp, zPage;
	uint16_t absAddr, val;
	pc++; waitCycles++;
	switch(instr)
	{
		case 0x00: //BRK
			memGet8(pc);
			#if DEBUG_INTR
			printf("BRK\n");
			#endif
			interrupt = true;
			p |= (P_FLAG_S1 | P_FLAG_S2);
			pc++;
			break;
		case 0x01: //ORA (Indirect, X)
			cpuORA(memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0x02: //KIL
			cpuKIL();
			break;
		case 0x03: //SLO (Indirect, X)
			absAddr = getIndirectPlusXaddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuORA(tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0x04: //DOP Zero Page
			cpuZeroPageEnd();
			break;
		case 0x05: //ORA Zero Page
			cpuORA(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0x06: //ASL Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPageEnd();
			break;
		case 0x07: //SLO Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuORA(tmp);
			cpuZeroPageEnd();
			break;
		case 0x08: //PHP
			memGet8(pc);
			p |= (P_FLAG_S1 | P_FLAG_S2);
			memSet8(0x100+s,p);
			s--;
			waitCycles++;
			break;
		case 0x09: //ORA Immediate
			cpuORA(getImmediate());
			cpuImmediateEnd();
			break;
		case 0x0A: //ASL A
			memGet8(pc);
			cpuSetA(cpuASL(a));
			break;
		case 0x0B: //AAC Immediate
			cpuAND(getImmediate());
			if(p & P_FLAG_NEGATIVE)
				p |= P_FLAG_CARRY;
			else
				p &= ~P_FLAG_CARRY;
			cpuImmediateEnd();
			break;
		case 0x0C: //TOP Absolute
			cpuAbsoluteEnd();
			break;
		case 0x0D: //ORA Absolute
			cpuORA(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0x0E: //ASL Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr,tmp);
			cpuAbsoluteEnd();
			break;
		case 0x0F: //SLO Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr,tmp);
			cpuORA(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x10: //BPL
			if(p & P_FLAG_NEGATIVE)
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0x11: //ORA (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuORA(memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0x12: //KIL
			cpuKIL();
			break;
		case 0x13: //SLO (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr,tmp);
			cpuORA(tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0x14: //DOP Zero Page, X
			cpuZeroPagePlusEnd();
			break;
		case 0x15: //ORA Zero Page, X
			cpuORA(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0x16: //ASL Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(zPage,tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x17: //SLO Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuORA(tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x18: //CLC
			memGet8(pc);
			p &= ~P_FLAG_CARRY;
			break;
		case 0x19: //ORA Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuORA(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x1A: //NOP
			memGet8(pc);
			break;
		case 0x1B: //SLO Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuORA(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x1C: //TOP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAbsoluteEnd();
			break;
		case 0x1D: //ORA Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuORA(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x1E: //ASL Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x1F: //SLO Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuASL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuORA(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x20: //JSR Absolute
			absAddr = (pc+1);
			#if DEBUG_JSR
			printf("JSR Abs from s %04x pc %04x to ",s,pc);
			#endif
			memSet8(0x100+s,absAddr>>8);
			s--;
			memSet8(0x100+s,absAddr&0xFF);
			s--;
			pc = memGet8(pc)|(memGet8(pc+1)<<8);
			#if DEBUG_JSR
			printf("%04x\n",pc);
			#endif
			waitCycles+=4;
			break;
		case 0x21: //AND (Indirect, X)
			cpuAND(memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0x22: //KIL
			cpuKIL();
			break;
		case 0x23: //RLA (Indirect, X)
			absAddr = getIndirectPlusXaddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAND(tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0x24: //BIT Zero Page
			cpuBIT(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0x25: //AND Zero Page
			cpuAND(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0x26: //ROL Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPageEnd();
			break;
		case 0x27: //RLA Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuAND(tmp);
			cpuZeroPageEnd();
			break;
		case 0x28: //PLP
			memGet8(pc);
			s++;
			tmp = memGet8(0x100+s);
			p &= P_FLAG_IRQ_DISABLE;
			p |= (tmp & ~P_FLAG_IRQ_DISABLE);
			//will do it for us after
			if(tmp & P_FLAG_IRQ_DISABLE)
			{
				//printf("PLP IRQ Set 1\n");
				p_irq_req = 1;
			}
			else
			{
				//printf("PLP IRQ Set 2\n");
				p_irq_req = 2; 
			}
			waitCycles+=2;
			break;
		case 0x29: //AND Immediate
			cpuAND(getImmediate());
			cpuImmediateEnd();
			break;
		case 0x2A: //ROL A
			memGet8(pc);
			cpuSetA(cpuROL(a));
			break;
		case 0x2B: //AAC Immediate
			cpuAND(getImmediate());
			if(p & P_FLAG_NEGATIVE)
				p |= P_FLAG_CARRY;
			else
				p &= ~P_FLAG_CARRY;
			cpuImmediateEnd();
			break;
		case 0x2C: //BIT Absolute
			cpuBIT(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0x2D: //AND Absolute
			cpuAND(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0x2E: //ROL Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x2F: //RLA Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAND(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x30: //BMI
			if(!(p & P_FLAG_NEGATIVE))
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0x31: //AND (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuAND(memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0x32: //KIL
			cpuKIL();
			break;
		case 0x33: //RLA (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAND(tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0x34: //DOP Zero Page, X
			cpuZeroPagePlusEnd();
			break;
		case 0x35: //AND Zero Page, X
			cpuAND(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0x36: //ROL Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x37: //RLA Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuAND(tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x38: //SEC
			memGet8(pc);
			p |= P_FLAG_CARRY;
			break;
		case 0x39: //AND Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuAND(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x3A: //NOP
			memGet8(pc);
			break;
		case 0x3B: //RLA Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAND(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x3C: //TOP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAbsoluteEnd();
			break;
		case 0x3D: //AND Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAND(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x3E: //ROL Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x3F: //RLA Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROL(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAND(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x40: //RTI
			memGet8(pc);
			#if DEBUG_INTR
			printf("RTI from p %02x pc %04x ",p,pc);
			#endif
			//get back p from stack
			s++;
			p = memGet8(0x100+s);
			//get back pc from stack
			s++;
			pc = memGet8(0x100+s);
			s++;
			pc |= memGet8(0x100+s)<<8;
			//jump back
			#if DEBUG_INTR
			printf("to p %02x pc %04x\n",p,pc);
			#endif
			waitCycles+=4;
			break;
		case 0x41: //EOR (Indirect, X)
			cpuEOR(memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0x42: //KIL
			cpuKIL();
			break;
		case 0x43: //SRE (Indirect, X)
			absAddr = getIndirectPlusXaddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuEOR(tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0x44: //DOP Zero Page
			cpuZeroPageEnd();
			break;
		case 0x45: //EOR Zero Page
			cpuEOR(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0x46: //LSR Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPageEnd();
			break;
		case 0x47: //SRE Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuEOR(tmp);
			cpuZeroPageEnd();
			break;
		case 0x48: //PHA
			memGet8(pc);
			memSet8(0x100+s,a);
			s--;
			waitCycles++;
			break;
		case 0x49: //EOR Immediate
			cpuEOR(getImmediate());
			cpuImmediateEnd();
			break;
		case 0x4A: //LSR A
			memGet8(pc);
			cpuSetA(cpuLSR(a));
			break;
		case 0x4B: //ASR Immediate
			cpuAND(getImmediate());
			cpuSetA(cpuLSR(a));
			cpuImmediateEnd();
			break;
		case 0x4C: //JMP Absolute
			#if DEBUG_JSR
			printf("JMP from %04x to ",pc);
			#endif
			pc = getAbsoluteAddr();
			#if DEBUG_JSR
			printf("%04x\n",pc);
			#endif
			waitCycles++;
			break;
		case 0x4D: //EOR Absolute
			cpuEOR(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0x4E: //LSR Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x4F: //SRE Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuEOR(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x50: //BVC
			if(p & P_FLAG_OVERFLOW)
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0x51: //EOR (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuEOR(memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0x52: //KIL
			cpuKIL();
			break;
		case 0x53: //SRE (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuEOR(tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0x54: //DOP Zero Page, X
			cpuZeroPagePlusEnd();
			break;
		case 0x55: //EOR Zero Page, X
			cpuEOR(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0x56: //LSR Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x57: //SRE Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuEOR(tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x58: //CLI
			memGet8(pc);
			#if DEBUG_INTR
			printf("CLI\n");
			#endif
			p_irq_req = 2; //will do it for us later
			break;
		case 0x59: //EOR Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuEOR(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x5A: //NOP
			memGet8(pc);
			break;
		case 0x5B: //SRE Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuEOR(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x5C: //TOP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAbsoluteEnd();
			break;
		case 0x5D: //EOR Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuEOR(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x5E: //LSR Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x5F: //SRE Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuLSR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuEOR(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x60: //RTS
			memGet8(pc);
			#if DEBUG_JSR
			printf("RTS from s %04x pc %04x to ",s,pc);
			#endif
			s++;
			absAddr = memGet8(0x100+s);
			s++;
			absAddr |= (memGet8(0x100+s) << 8);
			pc = (absAddr+1);
			#if DEBUG_JSR
			printf("%04x\n",pc);
			#endif
			waitCycles+=4;
			break;
		case 0x61: //ADC (Indirect, X)
			cpuADC(memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0x62: //KIL
			cpuKIL();
			break;
		case 0x63: //RRA (Indirect, X)
			absAddr = getIndirectPlusXaddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuADC(tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0x64: //DOP Zero Page
			cpuZeroPageEnd();
			break;
		case 0x65: //ADC Zero Page
			cpuADC(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0x66: //ROR Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPageEnd();
			break;
		case 0x67: //RRA Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuADC(tmp);
			cpuZeroPageEnd();
			break;
		case 0x68: //PLA
			memGet8(pc);
			s++;
			cpuSetA(memGet8(0x100+s));
			waitCycles+=2;
			break;
		case 0x69: //ADC Immediate
			cpuADC(getImmediate());
			cpuImmediateEnd();
			break;
		case 0x6A: //ROR A
			memGet8(pc);
			cpuSetA(cpuROR(a));
			break;
		case 0x6B: //ARR Immediate
			cpuAND(getImmediate());
			cpuSetA(cpuROR(a));
			cpuSetARRRegs();
			cpuImmediateEnd();
			break;
		case 0x6C: //JMP Indirect
			#if DEBUG_JSR
			printf("JMP from %04x to ",pc);
			#endif
			tmp = memGet8(pc++);
			absAddr = (memGet8(pc)<<8);
			pc = memGet8(absAddr+tmp);
			//emulate 6502 jmp wrap bug 
			tmp++; //possibly FF->00 without page increase!
			pc |= (memGet8(absAddr+tmp)<<8);
			#if DEBUG_JSR
			printf("%04x\n",pc);
			#endif
			waitCycles+=3;
			break;
		case 0x6D: //ADC Absolute
			cpuADC(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0x6E: //ROR Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x6F: //RRA Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuADC(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x70: //BVS
			if(!(p & P_FLAG_OVERFLOW))
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0x71: //ADC (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuADC(memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0x72: //KIL
			cpuKIL();
			break;
		case 0x73: //RRA (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuADC(tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0x74: //DOP Zero Page, X
			cpuZeroPagePlusEnd();
			break;
		case 0x75: //ADC Zero Page, X
			cpuADC(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0x76: //ROR Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x77: //RRA Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(zPage, tmp);
			cpuADC(tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0x78: //SEI
			memGet8(pc);
			#if DEBUG_INTR
			printf("SEI\n");
			#endif
			p_irq_req = 1; //will do it for us after
			break;
		case 0x79: //ADC Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuADC(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x7A: //NOP
			memGet8(pc);
			break;
		case 0x7B: //RRA Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuADC(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x7C: //TOP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAbsoluteEnd();
			break;
		case 0x7D: //ADC Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuADC(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0x7E: //ROR Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0x7F: //RRA Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuROR(tmp);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuADC(tmp);
			cpuAbsoluteEnd();
			break;
		case 0x80: //DOP Immediate
			cpuImmediateEnd();
			break;
		case 0x81: //STA (Indirect, X)
			memSet8(getIndirectPlusXaddr(), a);
			cpuIndirectPlusXEnd();
			break;
		case 0x82: //DOP Immediate
			cpuImmediateEnd();
			break;
		case 0x83: //AAX (Indirect, X)
			memSet8(getIndirectPlusXaddr(), a&x);
			cpuIndirectPlusXEnd();
			break;
		case 0x84: //STY Zero Page
			memSet8(getZeroPage(), y);
			cpuZeroPageEnd();
			break;
		case 0x85: //STA Zero Page
			memSet8(getZeroPage(), a);
			cpuZeroPageEnd();
			break;
		case 0x86: //STX Zero Page
			memSet8(getZeroPage(), x);
			cpuZeroPageEnd();
			break;
		case 0x87: //AAX Zero Page
			memSet8(getZeroPage(), a&x);
			cpuZeroPageEnd();
			break;
		case 0x88: //DEY
			memGet8(pc);
			cpuSetY(y-1);
			break;
		case 0x89: //DOP Immediate
			cpuImmediateEnd();
			break;
		case 0x8A: //TXA
			memGet8(pc);
			cpuSetA(x);
			break;
		case 0x8B: //XAA Immediate
			cpuSetA(x&getImmediate());
			cpuImmediateEnd();
			break;
		case 0x8C: //STY Absolute
			memSet8(getAbsoluteAddr(), y);
			cpuAbsoluteEnd();
			break;
		case 0x8D: //STA Absolute
			memSet8(getAbsoluteAddr(), a);
			cpuAbsoluteEnd();
			break;
		case 0x8E: //STX Absolute
			memSet8(getAbsoluteAddr(), x);
			cpuAbsoluteEnd();
			break;
		case 0x8F: //AAX Absolute
			memSet8(getAbsoluteAddr(), a&x);
			cpuAbsoluteEnd();
			break;
		case 0x90: //BCC
			if(p & P_FLAG_CARRY)
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0x91: //STA (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			memSet8(absAddr, a);
			cpuIndirectPlusYEnd();
			break;
		case 0x92: //KIL
			cpuKIL();
			break;
		case 0x93: //AXA (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			memSet8(absAddr, a&x&(absAddr>>8));
			cpuIndirectPlusYEnd();
			break;
		case 0x94: //STY Zero Page, X
			memSet8(getZeroPagePlus(x), y);
			cpuZeroPagePlusEnd();
			break;
		case 0x95: //STA Zero Page, X
			memSet8(getZeroPagePlus(x), a);
			cpuZeroPagePlusEnd();
			break;
		case 0x96: //STX Zero Page, Y
			memSet8(getZeroPagePlus(y), x);
			cpuZeroPagePlusEnd();
			break;
		case 0x97: //AAX Zero Page, Y
			memSet8(getZeroPagePlus(y), a&x);
			cpuZeroPagePlusEnd();
			break;
		case 0x98: //TYA
			memGet8(pc);
			cpuSetA(y);
			break;
		case 0x99: //STA Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			memSet8(absAddr, a);
			cpuAbsoluteEnd();
			break;
		case 0x9A: //TXS
			memGet8(pc);
			s = x;
			break;
		case 0x9B: //XAS Absolute, Y
			val = getAbsolutePlusAddr(y,true);
			absAddr = (val+x)&0xFFFF;
			val = ((val&0xFF00)|(absAddr&0x00FF));
			if((val >> 8) != (absAddr >> 8))
				val &= y << 8;
			s = a & x;
			memSet8(val, a & x & ((val >> 8) + 1));
			cpuAbsoluteEnd();
			break;
		case 0x9C: //SYA Absolute, X
			val = getAbsoluteAddr();
			absAddr = (val+x)&0xFFFF;
			val = ((val&0xFF00)|(absAddr&0x00FF));
			if((val >> 8) != (absAddr >> 8))
			  val &= y << 8;
			memSet8(val, y & ((val >> 8) + 1));
			waitCycles++;
			cpuAbsoluteEnd();
			break;
		case 0x9D: //STA Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			memSet8(absAddr, a);
			cpuAbsoluteEnd();
			break;
		case 0x9E: //SXA Absolute, Y
			val = getAbsoluteAddr();
			absAddr = (val+y)&0xFFFF;
			val = ((val&0xFF00)|(absAddr&0x00FF));
			if((val >> 8) != (absAddr >> 8))
			  val &= x << 8;
			memSet8(val, x & ((val >> 8) + 1));
			waitCycles++;
			cpuAbsoluteEnd();
			break;
		case 0x9F: //AXA Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			memSet8(absAddr, a&x&(absAddr>>8));
			cpuAbsoluteEnd();
			break;
		case 0xA0: //LDY Immediate
			cpuSetY(getImmediate());
			cpuImmediateEnd();
			break;
		case 0xA1: //LDA (Indirect, X)
			cpuSetA(memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0xA2: //LDX Immediate
			cpuSetX(getImmediate());
			cpuImmediateEnd();
			break;
		case 0xA3: //LAX (Indirect, X)
			tmp = memGet8(getIndirectPlusXaddr());
			cpuSetA(tmp); cpuSetX(tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0xA4: //LDY Zero Page
			cpuSetY(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xA5: //LDA Zero Page
			cpuSetA(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xA6: //LDX Zero Page
			cpuSetX(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xA7: //LAX Zero Page
			tmp = memGet8(getZeroPage());
			cpuSetA(tmp); cpuSetX(tmp);
			cpuZeroPageEnd();
			break;
		case 0xA8: //TAY
			memGet8(pc);
			cpuSetY(a);
			break;
		case 0xA9: //LDA Immediate
			cpuSetA(getImmediate());
			cpuImmediateEnd();
			break;
		case 0xAA: //TAX
			memGet8(pc);
			cpuSetX(a);
			break;
		case 0xAB: //AXT Immediate
			cpuSetA(getImmediate());
			cpuSetX(a);
			cpuImmediateEnd();
			break;
		case 0xAC: //LDY Absolute
			cpuSetY(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xAD: //LDA Absolute
			cpuSetA(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xAE: //LDX Absolute
			cpuSetX(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xAF: //LAX Absolute
			tmp = memGet8(getAbsoluteAddr());
			cpuSetA(tmp); cpuSetX(tmp);
			cpuAbsoluteEnd();
			break;
		case 0xB0: //BCS
			if(!(p & P_FLAG_CARRY))
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0xB1: //LDA (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuSetA(memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0xB2: //KIL
			cpuKIL();
			break;
		case 0xB3: //LAX (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			tmp = memGet8(absAddr);
			cpuSetA(tmp); cpuSetX(tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0xB4: //LDY Zero Page, X
			cpuSetY(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0xB5: //LDA Zero Page, X
			cpuSetA(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0xB6: //LDX Zero Page, Y
			cpuSetX(memGet8(getZeroPagePlus(y)));
			cpuZeroPagePlusEnd();
			break;
		case 0xB7: //LAX Zero Page, Y
			tmp = memGet8(getZeroPagePlus(y));
			cpuSetA(tmp); cpuSetX(tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0xB8: //CLV
			memGet8(pc);
			p &= ~P_FLAG_OVERFLOW;
			break;
		case 0xB9: //LDA Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuSetA(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xBA: //TSX
			memGet8(pc);
			cpuSetX(s);
			break;
		case 0xBB: //LAR Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuSetA(memGet8(absAddr));
			cpuAND(s); cpuSetX(a); s = a;
			cpuAbsoluteEnd();
			break;
		case 0xBC: //LDY Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuSetY(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xBD: //LDA Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuSetA(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xBE: //LDX Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuSetX(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xBF: //LAX Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			tmp = memGet8(absAddr);
			cpuSetA(tmp); cpuSetX(tmp);
			cpuAbsoluteEnd();
			break;
		case 0xC0: //CPY Immediate
			cpuCMP(y,getImmediate());
			cpuImmediateEnd();
			break;
		case 0xC1: //CMP (Indirect, X)
			cpuCMP(a,memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0xC2: //DOP Immediate
			cpuImmediateEnd();
			break;
		case 0xC3: //DCP (Indirect, X)
			absAddr = getIndirectPlusXaddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuCMP(a,tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0xC4: //CPY Zero Page
			cpuCMP(y,memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xC5: //CMP Zero Page
			cpuCMP(a,memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xC6: //DEC Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPageEnd();
			break;
		case 0xC7: //DCP Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuCMP(a,tmp);
			cpuZeroPageEnd();
			break;
		case 0xC8: //INY
			memGet8(pc);
			cpuSetY(y+1);
			break;
		case 0xC9: //CMP Immediate
			cpuCMP(a,getImmediate());
			cpuImmediateEnd();
			break;
		case 0xCA: //DEX
			memGet8(pc);
			cpuSetX(x-1);
			break;
		case 0xCB: //AXS Immediate
			x = cpuCMP(a&x,getImmediate());
			cpuImmediateEnd();
			break;
		case 0xCC: //CPY Absolute
			cpuCMP(y,memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xCD: //CMP Absolute
			cpuCMP(a,memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xCE: //DEC Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0xCF: //DCP Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuCMP(a,tmp);
			cpuAbsoluteEnd();
			break;
		case 0xD0: //BNE
			if(p & P_FLAG_ZERO)
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0xD1: //CMP (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuCMP(a,memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0xD2: //KIL
			cpuKIL();
			break;
		case 0xD3: //DCP (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuCMP(a,tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0xD4: //DOP Zero Page, X
			cpuZeroPagePlusEnd();
			break;
		case 0xD5: //CMP Zero Page, X
			cpuCMP(a,memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0xD6: //DEC Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0xD7: //DCP Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuCMP(a,tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0xD8: //CLD
			memGet8(pc);
			p &= ~P_FLAG_DECIMAL;
			break;
		case 0xD9: //CMP Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuCMP(a,memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xDA: //NOP
			memGet8(pc);
			break;
		case 0xDB: //DCP Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr ,tmp);
			cpuCMP(a,tmp);
			cpuAbsoluteEnd();
			break;
		case 0xDC: //TOP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAbsoluteEnd();
			break;
		case 0xDD: //CMP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuCMP(a,memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xDE: //DEC Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0xDF: //DCP Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp-1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuCMP(a,tmp);
			cpuAbsoluteEnd();
			break;
		case 0xE0: //CPX Immediate
			cpuCMP(x,getImmediate());
			cpuImmediateEnd();
			break;
		case 0xE1: //SBC (Indirect, X)
			cpuSBC(memGet8(getIndirectPlusXaddr()));
			cpuIndirectPlusXEnd();
			break;
		case 0xE2: //DOP Immediate
			cpuImmediateEnd();
			break;
		case 0xE3: //ISC (Indirect, X)
			absAddr = getIndirectPlusXaddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuSBC(tmp);
			cpuIndirectPlusXEnd();
			break;
		case 0xE4: //CPX Zero Page
			cpuCMP(x,memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xE5: //SBC Zero Page
			cpuSBC(memGet8(getZeroPage()));
			cpuZeroPageEnd();
			break;
		case 0xE6: //INC Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPageEnd();
			break;
		case 0xE7: //ISC Zero Page
			zPage = getZeroPage();
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuSBC(tmp);
			cpuZeroPageEnd();
			break;
		case 0xE8: //INX
			memGet8(pc);
			cpuSetX(x+1);
			break;
		case 0xE9: //SBC Immediate
			cpuSBC(getImmediate());
			cpuImmediateEnd();
			break;
		case 0xEA: //NOP
			memGet8(pc);
			break;
		case 0xEB: //SBC Immediate
			cpuSBC(getImmediate());
			cpuImmediateEnd();
			break;
		case 0xEC: //CPX Absolute
			cpuCMP(x,memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xED: //SBC Absolute
			cpuSBC(memGet8(getAbsoluteAddr()));
			cpuAbsoluteEnd();
			break;
		case 0xEE: //INC Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0xEF: //ISC Absolute
			absAddr = getAbsoluteAddr();
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuSBC(tmp);
			cpuAbsoluteEnd();
			break;
		case 0xF0: //BEQ
			if(!(p & P_FLAG_ZERO))
				pc++;
			else
				cpuRelativeBranch();
			break;
		case 0xF1: //SBC (Indirect), Y
			absAddr = getIndirectPlusYaddr(false);
			cpuSBC(memGet8(absAddr));
			cpuIndirectPlusYEnd();
			break;
		case 0xF2: //KIL
			cpuKIL();
			break;
		case 0xF3: //ISC (Indirect), Y
			absAddr = getIndirectPlusYaddr(true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuSBC(tmp);
			cpuIndirectPlusYEnd();
			break;
		case 0xF4: //DOP Zero Page, X
			cpuZeroPagePlusEnd();
			break;
		case 0xF5: //SBC Zero Page, X
			cpuSBC(memGet8(getZeroPagePlus(x)));
			cpuZeroPagePlusEnd();
			break;
		case 0xF6: //INC Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0xF7: //ISC Zero Page, X
			zPage = getZeroPagePlus(x);
			tmp = memGet8(zPage);
			cpuSaveTMPStart(zPage, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(zPage, tmp);
			cpuSBC(tmp);
			cpuZeroPagePlusEnd();
			break;
		case 0xF8: //SED
			memGet8(pc);
			p |= P_FLAG_DECIMAL;
			break;
		case 0xF9: //SBC Absolute, Y
			absAddr = getAbsolutePlusAddr(y,false);
			cpuSBC(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xFA: //NOP
			memGet8(pc);
			break;
		case 0xFB: //ISC Absolute, Y
			absAddr = getAbsolutePlusAddr(y,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuSBC(tmp);
			cpuAbsoluteEnd();
			break;
		case 0xFC: //TOP Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuAbsoluteEnd();
			break;
		case 0xFD: //SBC Absolute, X
			absAddr = getAbsolutePlusAddr(x,false);
			cpuSBC(memGet8(absAddr));
			cpuAbsoluteEnd();
			break;
		case 0xFE: //INC Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuAbsoluteEnd();
			break;
		case 0xFF: //ISC Absolute, X
			absAddr = getAbsolutePlusAddr(x,true);
			tmp = memGet8(absAddr);
			cpuSaveTMPStart(absAddr, tmp);
			tmp = cpuSetTMP(tmp+1);
			cpuSaveTMPEnd(absAddr, tmp);
			cpuSBC(tmp);
			cpuAbsoluteEnd();
			break;
		default: //should never happen
			printf("Unknown instruction at %04x: %02x\n", instrPtr, instr);
			memDumpMainMem();
			return false;
	}
	//update interrupt values
	ppu_nmi_handler_req = ppuNMI();
	cpu_interrupt_req = (interrupt || ((mapper_interrupt || dmc_interrupt || apu_interrupt) && !(p & P_FLAG_IRQ_DISABLE)));
	//if(instrPtr > 0xa980 && instrPtr < 0xa9C0) printf("%d %d %d %04x %04x\n",a,x,y,instrPtr,memGet8(instrPtr)|(memGet8(instrPtr+1)<<8));
	return true;
}


/* Access for other .c files */

void cpuIncWaitCycles(uint32_t inc)
{
	waitCycles += inc;
}

uint16_t cpuGetPc()
{
	return pc;
}

void cpuPlayNSF(uint16_t addr)
{
	//used in NSF mapper to detect init/play return
	uint16_t initRet = 0x4567-1;
	memSet8(0x100+s,initRet>>8);
	s--;
	memSet8(0x100+s,initRet&0xFF);
	s--;
	pc = addr;
	//printf("Playback at %04x\n", addr);
}

void cpuInitNSF(uint16_t addr, uint8_t newA, uint8_t newX)
{
	//full reset
	cpuInit();
	ppuInit();
	memInit();
	apuInit();
	//do init
	reset = false;
	interrupt = false;
	dmc_interrupt = false;
	apu_interrupt = false;
	cpu_oam_dma = 0;
	p = (P_FLAG_IRQ_DISABLE | P_FLAG_S1 | P_FLAG_S2);
	a = newA;
	x = newX;
	y = 0;
	s = 0xFD;
	waitCycles = 0;
	//initial "play" addr is init
	apuSet8(0x15,0xF);
	apuSet8(0x17,0x40);
	cpuPlayNSF(addr);
}
