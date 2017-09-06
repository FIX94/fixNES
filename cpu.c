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
#include "mem.h"
#include "ppu.h"
#include "apu.h"
#include "cpu.h"
#include "mapper_h/nsf.h"

#define P_FLAG_CARRY (1<<0)
#define P_FLAG_ZERO (1<<1)
#define P_FLAG_IRQ_DISABLE (1<<2)
#define P_FLAG_DECIMAL (1<<3)
#define P_FLAG_S1 (1<<4)
#define P_FLAG_S2 (1<<5)
#define P_FLAG_OVERFLOW (1<<6)
#define P_FLAG_NEGATIVE (1<<7)

static uint16_t pc, oldPc, indVal;
static uint8_t p,a,x,y,s;
static uint32_t waitCycles;
static uint8_t p_irq_req;
static bool reset;
static bool interrupt;
//used externally
bool dmc_interrupt;
bool mmc5_dmc_interrupt;
bool apu_interrupt;
bool fds_interrupt;
bool fds_transfer_interrupt;
bool cpu_odd_cycle;
bool cpu_oam_dma;
bool cpu_oam_dma_pause;
uint16_t cpu_oam_dma_addr;
bool cpu_dmc_dma;
uint16_t cpu_dmc_dma_addr;
extern bool nesPause;
static bool needsIndFix;
static bool takeBranch;
static uint8_t cpuTmp, zPage;
static uint16_t absAddr;

void cpuInit()
{
	cpuSetupActionArr();
	reset = true;
	interrupt = false;
	dmc_interrupt = false;
	mmc5_dmc_interrupt = false;
	apu_interrupt = false;
	fds_interrupt = false;
	fds_transfer_interrupt = false;
	needsIndFix = false;
	takeBranch = false;
	cpuTmp = 0; zPage = 0;
	absAddr = 0;
	cpu_oam_dma = false;
	cpu_oam_dma_pause = false;
	cpu_oam_dma_addr = 0;
	cpu_dmc_dma = false;
	cpu_dmc_dma_addr = 0;
	pc = 0;
	oldPc = 0;
	indVal = 0;
	p = 0;
	a = 0;
	x = 0;
	y = 0;
	s = 0;
	waitCycles = 0;
	p_irq_req = 0;
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

//used externally
bool cpuWriteTMP = false;

/* Various Instructions used multiple times */

static inline void cpuAND()
{
	a &= cpuTmp;
	setRegStats(a);
}

static inline void cpuORA()
{
	a |= cpuTmp;
	setRegStats(a);
}

static inline void cpuEOR()
{
	a ^= cpuTmp;
	setRegStats(a);
}

static uint8_t cpuASL(uint8_t val)
{
	if(val & (1<<7))
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
	val <<= 1;
	setRegStats(val);
	return val;
}

static void cpuASLa() { cpuSetA(cpuASL(a)); };
static void cpuASLt() { cpuTmp = cpuASL(cpuTmp); };

static uint8_t cpuLSR(uint8_t val)
{
	if(val & (1<<0))
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
	val >>= 1;
	setRegStats(val);
	return val;
}

static void cpuLSRa() { cpuSetA(cpuLSR(a)); };
static void cpuLSRt() { cpuTmp = cpuLSR(cpuTmp); };

static uint8_t cpuROL(uint8_t val)
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

static void cpuROLa() { cpuSetA(cpuROL(a)); };
static void cpuROLt() { cpuTmp = cpuROL(cpuTmp); };

static uint8_t cpuROR(uint8_t val)
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

static void cpuRORa() { cpuSetA(cpuROR(a)); };
static void cpuRORt() { cpuTmp = cpuROR(cpuTmp); };

static void cpuKIL()
{
	printf("Processor Requested Lock-Up at %04x\n", pc-1);
	nesPause = true;
}

static void cpuADCv(uint8_t in)
{
	//use uint16_t here to easly detect carry
	uint16_t res = a + in;

	if(p & P_FLAG_CARRY)
		res++;

	if(res > 0xFF)
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;

	if(!(a & (1<<7)) && !(in & (1<<7)) && (res & (1<<7)))
		p |= P_FLAG_OVERFLOW;
	else if((a & (1<<7)) && (in & (1<<7)) && !(res & (1<<7)))
		p |= P_FLAG_OVERFLOW;
	else
		p &= ~P_FLAG_OVERFLOW;

	cpuSetA(res);
}

static void cpuADC() { cpuADCv(cpuTmp); }

static void cpuSBC() { cpuADCv(~cpuTmp); }

static void cpuISC() { cpuTmp = cpuSetTMP(cpuTmp+1); cpuSBC(); }


static uint8_t cpuCMP(uint8_t reg)
{
	if(reg >= cpuTmp)
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;

	uint8_t cmpVal = (reg - cpuTmp);
	setRegStats(cmpVal);
	return cmpVal;
}

static void cpuNoAction() { ; };

static void cpuCMPa() { cpuCMP(a); }
static void cpuCMPx() { cpuCMP(x); }
static void cpuCMPy() { cpuCMP(y); }
static void cpuCMPax() { x = cpuCMP(a&x); }
static void cpuDCP() { cpuTmp = cpuSetTMP(cpuTmp-1); cpuCMPa(); }

static void cpuBIT()
{
	if((a & cpuTmp) == 0)
		p |= P_FLAG_ZERO;
	else
		p &= ~P_FLAG_ZERO;

	if(cpuTmp & P_FLAG_OVERFLOW)
		p |= P_FLAG_OVERFLOW;
	else
		p &= ~P_FLAG_OVERFLOW;

	if(cpuTmp & P_FLAG_NEGATIVE)
		p |= P_FLAG_NEGATIVE;
	else
		p &= ~P_FLAG_NEGATIVE;
}

void cpuSLO() {	cpuASLt(); cpuORA(); }
void cpuRLA() {	cpuROLt(); cpuAND(); }
void cpuSRE() { cpuLSRt(); cpuEOR(); }
void cpuRRA() { cpuRORt(); cpuADC(); }
void cpuASR() {	cpuAND(); cpuLSRa(); }

void cpuARR()
{
	cpuAND();
	cpuRORa();
	cpuSetARRRegs();
}

void cpuAAC()
{
	cpuAND();
	if(p & P_FLAG_NEGATIVE)
		p |= P_FLAG_CARRY;
	else
		p &= ~P_FLAG_CARRY;
}

void cpuCLC() { p &= ~P_FLAG_CARRY; }
void cpuSEC() { p |= P_FLAG_CARRY; }

void cpuCLI() { p_irq_req = 2; }
void cpuSEI() { p_irq_req = 1; }

void cpuCLV() { p &= ~P_FLAG_OVERFLOW; }

void cpuCLD() { p &= ~P_FLAG_DECIMAL; }
void cpuSED() { p |= P_FLAG_DECIMAL; }

void cpuINC() { cpuSetA(a+1); }
void cpuINX() { cpuSetX(x+1); }
void cpuINY() { cpuSetY(y+1); }
void cpuINCt() { cpuTmp = cpuSetTMP(cpuTmp+1); }

void cpuDEC() { cpuSetA(a-1); }
void cpuDEX() { cpuSetX(x-1); }
void cpuDEY() { cpuSetY(y-1); }
void cpuDECt() { cpuTmp = cpuSetTMP(cpuTmp-1); }

void cpuTXA() { cpuSetA(x); }
void cpuTYA() { cpuSetA(y); }
void cpuTSX() { cpuSetX(s); }
void cpuTXS() { s = x; }

void cpuTAY() { cpuSetY(a); }
void cpuTAX() { cpuSetX(a); }

void cpuLDA() { cpuSetA(cpuTmp); }
void cpuLDX() { cpuSetX(cpuTmp); }
void cpuLDY() { cpuSetY(cpuTmp); }
void cpuXAA() { cpuSetA(x&cpuTmp); }
void cpuAXT() { cpuSetA(cpuTmp); cpuSetX(a); }
void cpuLAX() { cpuSetA(cpuTmp); cpuSetX(cpuTmp); }
void cpuLAR() { cpuSetA(cpuTmp); cpuAND(s); cpuSetX(a); s = a; }

/* For Interrupt Handling */

#define DEBUG_INTR 0
#define DEBUG_JSR 0

/* Set up all CPU State Definitions */
typedef void (*cpu_action_t)(void);

static cpu_action_t cpu_action_func;

enum {
	CPU_GET_INSTRUCTION = 0,
	CPU_NULL_READ8_PC,
	CPU_NULL_READ8_PC_INC,
	CPU_NULL_READ8_PC_CHK,
	CPU_NULL_READ8_PC_ACTION,
	CPU_NULL_READ8_PC_ADDR_ADDX_ZP,
	CPU_NULL_READ8_PC_ADDR_ADDY_ZP,
	CPU_NULL_READ8_PC_TMP_ADDX,
	CPU_NULL_READ8_PC_STACK_INC,
	CPU_NULL_READ8_PC_STACK_DEC,
	CPU_NULL_READ8_PC_STACK_DEC_SET_S1_S2_I,
	CPU_TMP_READ8_PC_INC,
	CPU_TMP_READ8_PC_INC_ACTION,
	CPU_PCL_FROM_TMP_PCH_READ8_PC,
	CPU_ADDR_READ8_PC_INC,
	CPU_ADDRL_READ8_PC_INC,
	CPU_ADDRH_READ8_PC_INC,
	CPU_ADDRH_READ8_PC_INC_ADDX,
	CPU_ADDRH_READ8_PC_INC_ADDY,
	CPU_ADDRL_READ8_TMP_INC,
	CPU_ADDRH_READ8_TMP,
	CPU_ADDRH_READ8_TMP_ADDY,
	CPU_ADDR_READ8,
	CPU_ADDR_READ8_CHK,
	CPU_ADDR_READ8_ACTION,
	CPU_ADDR_READ8_ACTION_CHK,
	CPU_INC_PAGE_ADDR_READ8_SET_PC,
	CPU_ADDR_WRITE8,
	CPU_ADDR_WRITE8_A,
	CPU_ADDR_WRITE8_X,
	CPU_ADDR_WRITE8_Y,
	CPU_ADDR_WRITE8_AX,
	CPU_ADDR_WRITE8_AXA,
	CPU_ADDR_WRITE8_XAS,
	CPU_ADDR_WRITE8_SYA,
	CPU_ADDR_WRITE8_SXA,
	CPU_ADDR_WRITE8_ACTION,
	CPU_CHECK_BCC,
	CPU_CHECK_BCS,
	CPU_CHECK_BNE,
	CPU_CHECK_BEQ,
	CPU_CHECK_BPL,
	CPU_CHECK_BMI,
	CPU_CHECK_BVC,
	CPU_CHECK_BVS,
	CPU_STACK_GET_A,
	CPU_STACK_GET_P,
	CPU_STACK_GET_P_INC,
	CPU_STACK_GET_PCL_INC,
	CPU_STACK_GET_PCH,
	CPU_STACK_STORE_A_DEC,
	CPU_STACK_STORE_P_DEC,
	CPU_STACK_STORE_PCH_DEC,
	CPU_STACK_STORE_PCL_DEC,
	CPU_STACK_STORE_P_DEC_SET_I,
	CPU_STACK_SET_S1_S2_STORE_P_DEC_SET_I,
	CPU_STACK_CLEAR_S1_SET_S2_STORE_P_DEC_SET_I,
	CPU_READ_NMIVEC_PCL,
	CPU_READ_NMIVEC_PCH,
	CPU_READ_RESETVEC_PCL,
	CPU_READ_RESETVEC_PCH,
	CPU_READ_IRQVEC_PCL,
	CPU_READ_IRQVEC_PCH,
	CPU_STATE_END,
};

enum {
	CPU_READ_PC = 0,
	CPU_READ_CPUTMP,
	CPU_READ_ABSADDR,
	CPU_READ_STACK,
	CPU_READ_NMIVECL,
	CPU_READ_NMIVECH,
	CPU_READ_RESETVECL,
	CPU_READ_RESETVECH,
	CPU_READ_IRQVECL,
	CPU_READ_IRQVECH,
	CPU_WRITE_ADDR,
	CPU_WRITE_STACK,
};

// every cpu state does a mem read/write, this defines all
static uint8_t cpu_mem_type[CPU_STATE_END] = {
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_CPUTMP,
	CPU_READ_CPUTMP,
	CPU_READ_CPUTMP,
	CPU_READ_ABSADDR,
	CPU_READ_ABSADDR,
	CPU_READ_ABSADDR,
	CPU_READ_ABSADDR,
	CPU_READ_ABSADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_WRITE_ADDR,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_PC,
	CPU_READ_STACK,
	CPU_READ_STACK,
	CPU_READ_STACK,
	CPU_READ_STACK,
	CPU_READ_STACK,
	CPU_WRITE_STACK,
	CPU_WRITE_STACK,
	CPU_WRITE_STACK,
	CPU_WRITE_STACK,
	CPU_WRITE_STACK,
	CPU_WRITE_STACK,
	CPU_WRITE_STACK,
	CPU_READ_NMIVECL,
	CPU_READ_NMIVECH,
	CPU_READ_RESETVECL,
	CPU_READ_RESETVECH,
	CPU_READ_IRQVECL,
	CPU_READ_IRQVECH,
};

static uint8_t cpu_start_arr[1] = { CPU_GET_INSTRUCTION };
static uint8_t *cpu_action_arr = cpu_start_arr;
static uint8_t cpu_arr_pos = 0;

/* arrays for non-callable instructions */
static uint8_t cpu_reset_arr[6] = { CPU_NULL_READ8_PC_STACK_DEC, CPU_NULL_READ8_PC_STACK_DEC, CPU_NULL_READ8_PC_STACK_DEC_SET_S1_S2_I, CPU_READ_RESETVEC_PCL, CPU_READ_RESETVEC_PCH, CPU_GET_INSTRUCTION };
static uint8_t cpu_nmi_arr[7] = { CPU_NULL_READ8_PC, CPU_STACK_STORE_PCH_DEC, CPU_STACK_STORE_PCL_DEC, CPU_STACK_CLEAR_S1_SET_S2_STORE_P_DEC_SET_I, CPU_READ_NMIVEC_PCL, CPU_READ_NMIVEC_PCH, CPU_GET_INSTRUCTION };
static uint8_t cpu_irq_arr[7] = { CPU_NULL_READ8_PC, CPU_STACK_STORE_PCH_DEC, CPU_STACK_STORE_PCL_DEC, CPU_STACK_STORE_P_DEC_SET_I, CPU_READ_IRQVEC_PCL, CPU_READ_IRQVEC_PCH, CPU_GET_INSTRUCTION };
static uint8_t cpu_kill_arr[2] = { CPU_NULL_READ8_PC_ACTION, CPU_GET_INSTRUCTION };

/* arrays for single special instructions */
static uint8_t cpu_brk_arr[7] = { CPU_NULL_READ8_PC_INC, CPU_STACK_STORE_PCH_DEC, CPU_STACK_STORE_PCL_DEC, CPU_STACK_SET_S1_S2_STORE_P_DEC_SET_I, CPU_READ_IRQVEC_PCL, CPU_READ_IRQVEC_PCH, CPU_GET_INSTRUCTION };
static uint8_t cpu_rti_arr[6] = { CPU_NULL_READ8_PC, CPU_NULL_READ8_PC_STACK_INC, CPU_STACK_GET_P_INC, CPU_STACK_GET_PCL_INC, CPU_STACK_GET_PCH, CPU_GET_INSTRUCTION };
static uint8_t cpu_rts_arr[6] = { CPU_NULL_READ8_PC, CPU_NULL_READ8_PC_STACK_INC, CPU_STACK_GET_PCL_INC, CPU_STACK_GET_PCH, CPU_NULL_READ8_PC_INC, CPU_GET_INSTRUCTION };
static uint8_t cpu_php_arr[3] = { CPU_NULL_READ8_PC, CPU_STACK_STORE_P_DEC, CPU_GET_INSTRUCTION };
static uint8_t cpu_pha_arr[3] = { CPU_NULL_READ8_PC, CPU_STACK_STORE_A_DEC, CPU_GET_INSTRUCTION };
static uint8_t cpu_plp_arr[4] = { CPU_NULL_READ8_PC, CPU_NULL_READ8_PC_STACK_INC, CPU_STACK_GET_P, CPU_GET_INSTRUCTION };
static uint8_t cpu_pla_arr[4] = { CPU_NULL_READ8_PC, CPU_NULL_READ8_PC_STACK_INC, CPU_STACK_GET_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_jsr_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_NULL_READ8_PC, CPU_STACK_STORE_PCH_DEC, CPU_STACK_STORE_PCL_DEC, CPU_PCL_FROM_TMP_PCH_READ8_PC, CPU_GET_INSTRUCTION };
static uint8_t cpu_absjmp_arr[3] = { CPU_TMP_READ8_PC_INC, CPU_PCL_FROM_TMP_PCH_READ8_PC, CPU_GET_INSTRUCTION };
static uint8_t cpu_indjmp_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_READ8, CPU_INC_PAGE_ADDR_READ8_SET_PC, CPU_GET_INSTRUCTION };

static uint8_t cpu_zpsta_arr[3] = { CPU_ADDR_READ8_PC_INC, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpstx_arr[3] = { CPU_ADDR_READ8_PC_INC, CPU_ADDR_WRITE8_X, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpsty_arr[3] = { CPU_ADDR_READ8_PC_INC, CPU_ADDR_WRITE8_Y, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpaax_arr[3] = { CPU_ADDR_READ8_PC_INC, CPU_ADDR_WRITE8_AX, CPU_GET_INSTRUCTION };

static uint8_t cpu_zpXsta_arr[4] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDX_ZP, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpXsty_arr[4] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDX_ZP, CPU_ADDR_WRITE8_Y, CPU_GET_INSTRUCTION };

static uint8_t cpu_zpYstx_arr[4] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDY_ZP, CPU_ADDR_WRITE8_X, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpYaax_arr[4] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDY_ZP, CPU_ADDR_WRITE8_AX, CPU_GET_INSTRUCTION };

static uint8_t cpu_abssta_arr[4] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_absstx_arr[4] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_WRITE8_X, CPU_GET_INSTRUCTION };
static uint8_t cpu_abssty_arr[4] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_WRITE8_Y, CPU_GET_INSTRUCTION };
static uint8_t cpu_absaax_arr[4] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_WRITE8_AX, CPU_GET_INSTRUCTION };

static uint8_t cpu_absXsta_arr[7] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDX, CPU_ADDR_READ8_CHK, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_absXsya_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDX, CPU_ADDR_READ8, CPU_ADDR_WRITE8_SYA, CPU_GET_INSTRUCTION };

static uint8_t cpu_absYsta_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDY, CPU_ADDR_READ8_CHK, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_absYaxa_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDY, CPU_ADDR_READ8_CHK, CPU_ADDR_WRITE8_AXA, CPU_GET_INSTRUCTION };
static uint8_t cpu_absYxas_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDY, CPU_ADDR_READ8, CPU_ADDR_WRITE8_XAS, CPU_GET_INSTRUCTION };
static uint8_t cpu_absYsxa_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDY, CPU_ADDR_READ8, CPU_ADDR_WRITE8_SXA, CPU_GET_INSTRUCTION };

static uint8_t cpu_indXsta_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_NULL_READ8_PC_TMP_ADDX, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_indXaax_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_NULL_READ8_PC_TMP_ADDX, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP, CPU_ADDR_WRITE8_AX, CPU_GET_INSTRUCTION };

static uint8_t cpu_indYsta_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP_ADDY, CPU_ADDR_READ8_CHK, CPU_ADDR_WRITE8_A, CPU_GET_INSTRUCTION };
static uint8_t cpu_indYaxa_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP_ADDY, CPU_ADDR_READ8_CHK, CPU_ADDR_WRITE8_AXA, CPU_GET_INSTRUCTION };

static uint8_t cpu_bcc_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BCC, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_bcs_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BCS, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_bne_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BNE, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_beq_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BEQ, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_bpl_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BPL, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_bmi_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BMI, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_bvc_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BVC, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };
static uint8_t cpu_bvs_arr[4] = { CPU_TMP_READ8_PC_INC, CPU_CHECK_BVS, CPU_NULL_READ8_PC_CHK, CPU_GET_INSTRUCTION };

/* useful for the branch checks */
bool cpuBranchCheck()
{
	if(takeBranch)
	{
		indVal = pc + (int8_t)cpuTmp;
		//only need extra cycle if it needs fixup
		if((pc&0xFF00) != (indVal&0xFF00))
		{
			//printf("branch %04x %04x %02x\n", pc, indVal, cpuTmp);
			needsIndFix = true;
		}
		else
			cpu_arr_pos++;
		pc = (pc&0xFF00)|(indVal&0x00FF);
		return true;
	}
	else //no branch taken, do next instruction this cycle
	{
		cpu_action_arr = cpu_start_arr;
		cpu_arr_pos = 0;
		return false;
	}
}

/* arrays for multiple similar instructions */
static uint8_t cpu_direct_arr[2] = { CPU_NULL_READ8_PC_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_imm_arr[2] = { CPU_TMP_READ8_PC_INC_ACTION, CPU_GET_INSTRUCTION };

static uint8_t cpu_zpread_arr[3] = { CPU_ADDR_READ8_PC_INC, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpreadwrite_arr[5] = { CPU_ADDR_READ8_PC_INC, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };

static uint8_t cpu_zpXread_arr[4] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDX_ZP, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpYread_arr[4] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDY_ZP, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_zpXreadwrite_arr[6] = { CPU_ADDR_READ8_PC_INC, CPU_NULL_READ8_PC_ADDR_ADDX_ZP, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };

static uint8_t cpu_absread_arr[4] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_absreadwrite_arr[6] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };

static uint8_t cpu_absXread_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDX, CPU_ADDR_READ8_ACTION_CHK, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_absYread_arr[5] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDY, CPU_ADDR_READ8_ACTION_CHK, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_absXreadwrite_arr[7] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDX, CPU_ADDR_READ8_CHK, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };
static uint8_t cpu_absYreadwrite_arr[7] = { CPU_ADDRL_READ8_PC_INC, CPU_ADDRH_READ8_PC_INC_ADDY, CPU_ADDR_READ8_CHK, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };

static uint8_t cpu_indXread_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_NULL_READ8_PC_TMP_ADDX, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_indXreadwrite_arr[8] = { CPU_TMP_READ8_PC_INC, CPU_NULL_READ8_PC_TMP_ADDX, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };
static uint8_t cpu_indYread_arr[6] = { CPU_TMP_READ8_PC_INC, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP_ADDY, CPU_ADDR_READ8_ACTION_CHK, CPU_ADDR_READ8_ACTION, CPU_GET_INSTRUCTION };
static uint8_t cpu_indYreadwrite_arr[8] = { CPU_TMP_READ8_PC_INC, CPU_ADDRL_READ8_TMP_INC, CPU_ADDRH_READ8_TMP_ADDY, CPU_ADDR_READ8_CHK, CPU_ADDR_READ8, CPU_ADDR_WRITE8_ACTION, CPU_ADDR_WRITE8, CPU_GET_INSTRUCTION };

/* Set up array pointers to all instruction types */
static uint8_t *cpu_instr_arr[256] = {
	cpu_brk_arr, cpu_indXread_arr, cpu_kill_arr, cpu_indXreadwrite_arr,
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpreadwrite_arr, cpu_zpreadwrite_arr,
	cpu_php_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr,
	cpu_absread_arr, cpu_absread_arr, cpu_absreadwrite_arr, cpu_absreadwrite_arr,

	cpu_bpl_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYreadwrite_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpXreadwrite_arr, cpu_zpXreadwrite_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYreadwrite_arr,
	cpu_absXread_arr, cpu_absXread_arr, cpu_absXreadwrite_arr, cpu_absXreadwrite_arr,

	cpu_jsr_arr, cpu_indXread_arr, cpu_kill_arr, cpu_indXreadwrite_arr,
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpreadwrite_arr, cpu_zpreadwrite_arr,
	cpu_plp_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr,
	cpu_absread_arr, cpu_absread_arr, cpu_absreadwrite_arr, cpu_absreadwrite_arr,
	
	cpu_bmi_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYreadwrite_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpXreadwrite_arr, cpu_zpXreadwrite_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYreadwrite_arr,
	cpu_absXread_arr, cpu_absXread_arr, cpu_absXreadwrite_arr, cpu_absXreadwrite_arr,
	
	cpu_rti_arr, cpu_indXread_arr, cpu_kill_arr, cpu_indXreadwrite_arr,
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpreadwrite_arr, cpu_zpreadwrite_arr,
	cpu_pha_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr,
	cpu_absjmp_arr, cpu_absread_arr, cpu_absreadwrite_arr, cpu_absreadwrite_arr,
	
	cpu_bvc_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYreadwrite_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpXreadwrite_arr, cpu_zpXreadwrite_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYreadwrite_arr,
	cpu_absXread_arr, cpu_absXread_arr, cpu_absXreadwrite_arr, cpu_absXreadwrite_arr,
	
	cpu_rts_arr, cpu_indXread_arr, cpu_kill_arr, cpu_indXreadwrite_arr,
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpreadwrite_arr, cpu_zpreadwrite_arr,
	cpu_pla_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr,
	cpu_indjmp_arr, cpu_absread_arr, cpu_absreadwrite_arr, cpu_absreadwrite_arr,
	
	cpu_bvs_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYreadwrite_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpXreadwrite_arr, cpu_zpXreadwrite_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYreadwrite_arr,
	cpu_absXread_arr, cpu_absXread_arr, cpu_absXreadwrite_arr, cpu_absXreadwrite_arr,
	
	cpu_imm_arr, cpu_indXsta_arr, cpu_imm_arr, cpu_indXaax_arr,
	cpu_zpsty_arr, cpu_zpsta_arr, cpu_zpstx_arr, cpu_zpaax_arr, 
	cpu_direct_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr, 
	cpu_abssty_arr, cpu_abssta_arr, cpu_absstx_arr, cpu_absaax_arr,
	
	cpu_bcc_arr, cpu_indYsta_arr, cpu_kill_arr, cpu_indYaxa_arr,
	cpu_zpXsty_arr, cpu_zpXsta_arr, cpu_zpYstx_arr, cpu_zpYaax_arr,
	cpu_direct_arr, cpu_absYsta_arr, cpu_direct_arr, cpu_absYxas_arr,
	cpu_absXsya_arr, cpu_absXsta_arr, cpu_absYsxa_arr, cpu_absYaxa_arr,
	
	cpu_imm_arr, cpu_indXread_arr, cpu_imm_arr, cpu_indXread_arr, 
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpread_arr, cpu_zpread_arr,
	cpu_direct_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr, 
	cpu_absread_arr, cpu_absread_arr, cpu_absread_arr, cpu_absread_arr,
	
	cpu_bcs_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYread_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpYread_arr, cpu_zpYread_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYread_arr, 
	cpu_absXread_arr, cpu_absXread_arr, cpu_absYread_arr, cpu_absYread_arr,
	
	cpu_imm_arr, cpu_indXread_arr, cpu_imm_arr, cpu_indXreadwrite_arr,
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpreadwrite_arr, cpu_zpreadwrite_arr,
	cpu_direct_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr, 
	cpu_absread_arr, cpu_absread_arr, cpu_absreadwrite_arr, cpu_absreadwrite_arr,
	
	cpu_bne_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYreadwrite_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpXreadwrite_arr, cpu_zpXreadwrite_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYreadwrite_arr, 
	cpu_absXread_arr, cpu_absXread_arr, cpu_absXreadwrite_arr, cpu_absXreadwrite_arr,
	
	cpu_imm_arr, cpu_indXread_arr, cpu_imm_arr, cpu_indXreadwrite_arr,
	cpu_zpread_arr, cpu_zpread_arr, cpu_zpreadwrite_arr, cpu_zpreadwrite_arr,
	cpu_direct_arr, cpu_imm_arr, cpu_direct_arr, cpu_imm_arr, 
	cpu_absread_arr, cpu_absread_arr, cpu_absreadwrite_arr, cpu_absreadwrite_arr,
	
	cpu_beq_arr, cpu_indYread_arr, cpu_kill_arr, cpu_indYreadwrite_arr,
	cpu_zpXread_arr, cpu_zpXread_arr, cpu_zpXreadwrite_arr, cpu_zpXreadwrite_arr,
	cpu_direct_arr, cpu_absYread_arr, cpu_direct_arr, cpu_absYreadwrite_arr, 
	cpu_absXread_arr, cpu_absXread_arr, cpu_absXreadwrite_arr, cpu_absXreadwrite_arr,
};

/* Set up function pointers for all instruction actions */
static cpu_action_t cpu_actions_arr[256];
void cpuSetupActionArr()
{
	cpu_actions_arr[0x00] = cpuNoAction; cpu_actions_arr[0x01] = cpuORA; cpu_actions_arr[0x02] = cpuKIL; cpu_actions_arr[0x03] = cpuSLO;
	cpu_actions_arr[0x04] = cpuNoAction; cpu_actions_arr[0x05] = cpuORA; cpu_actions_arr[0x06] = cpuASLt; cpu_actions_arr[0x07] = cpuSLO;
	cpu_actions_arr[0x08] = cpuNoAction; cpu_actions_arr[0x09] = cpuORA; cpu_actions_arr[0x0A] = cpuASLa; cpu_actions_arr[0x0B] = cpuAAC; 
	cpu_actions_arr[0x0C] = cpuNoAction; cpu_actions_arr[0x0D] = cpuORA; cpu_actions_arr[0x0E] = cpuASLt; cpu_actions_arr[0x0F] = cpuSLO;

	cpu_actions_arr[0x10] = cpuNoAction; cpu_actions_arr[0x11] = cpuORA; cpu_actions_arr[0x12] = cpuKIL; cpu_actions_arr[0x13] = cpuSLO;
	cpu_actions_arr[0x14] = cpuNoAction; cpu_actions_arr[0x15] = cpuORA; cpu_actions_arr[0x16] = cpuASLt; cpu_actions_arr[0x17] = cpuSLO;
	cpu_actions_arr[0x18] = cpuCLC; cpu_actions_arr[0x19] = cpuORA; cpu_actions_arr[0x1A] = cpuNoAction; cpu_actions_arr[0x1B] = cpuSLO; 
	cpu_actions_arr[0x1C] = cpuNoAction; cpu_actions_arr[0x1D] = cpuORA; cpu_actions_arr[0x1E] = cpuASLt; cpu_actions_arr[0x1F] = cpuSLO;

	cpu_actions_arr[0x20] = cpuNoAction; cpu_actions_arr[0x21] = cpuAND; cpu_actions_arr[0x22] = cpuKIL; cpu_actions_arr[0x23] = cpuRLA;
	cpu_actions_arr[0x24] = cpuBIT; cpu_actions_arr[0x25] = cpuAND; cpu_actions_arr[0x26] = cpuROLt; cpu_actions_arr[0x27] = cpuRLA;
	cpu_actions_arr[0x28] = cpuNoAction; cpu_actions_arr[0x29] = cpuAND; cpu_actions_arr[0x2A] = cpuROLa; cpu_actions_arr[0x2B] = cpuAAC; 
	cpu_actions_arr[0x2C] = cpuBIT; cpu_actions_arr[0x2D] = cpuAND; cpu_actions_arr[0x2E] = cpuROLt; cpu_actions_arr[0x2F] = cpuRLA;

	cpu_actions_arr[0x30] = cpuNoAction; cpu_actions_arr[0x31] = cpuAND; cpu_actions_arr[0x32] = cpuKIL; cpu_actions_arr[0x33] = cpuRLA;
	cpu_actions_arr[0x34] = cpuNoAction; cpu_actions_arr[0x35] = cpuAND; cpu_actions_arr[0x36] = cpuROLt; cpu_actions_arr[0x37] = cpuRLA;
	cpu_actions_arr[0x38] = cpuSEC; cpu_actions_arr[0x39] = cpuAND; cpu_actions_arr[0x3A] = cpuNoAction; cpu_actions_arr[0x3B] = cpuRLA; 
	cpu_actions_arr[0x3C] = cpuNoAction; cpu_actions_arr[0x3D] = cpuAND; cpu_actions_arr[0x3E] = cpuROLt; cpu_actions_arr[0x3F] = cpuRLA;

	cpu_actions_arr[0x40] = cpuNoAction; cpu_actions_arr[0x41] = cpuEOR; cpu_actions_arr[0x42] = cpuKIL; cpu_actions_arr[0x43] = cpuSRE;
	cpu_actions_arr[0x44] = cpuNoAction; cpu_actions_arr[0x45] = cpuEOR; cpu_actions_arr[0x46] = cpuLSRt; cpu_actions_arr[0x47] = cpuSRE;
	cpu_actions_arr[0x48] = cpuNoAction; cpu_actions_arr[0x49] = cpuEOR; cpu_actions_arr[0x4A] = cpuLSRa; cpu_actions_arr[0x4B] = cpuASR; 
	cpu_actions_arr[0x4C] = cpuNoAction; cpu_actions_arr[0x4D] = cpuEOR; cpu_actions_arr[0x4E] = cpuLSRt; cpu_actions_arr[0x4F] = cpuSRE;

	cpu_actions_arr[0x50] = cpuNoAction; cpu_actions_arr[0x51] = cpuEOR; cpu_actions_arr[0x52] = cpuKIL; cpu_actions_arr[0x53] = cpuSRE;
	cpu_actions_arr[0x54] = cpuNoAction; cpu_actions_arr[0x55] = cpuEOR; cpu_actions_arr[0x56] = cpuLSRt; cpu_actions_arr[0x57] = cpuSRE;
	cpu_actions_arr[0x58] = cpuCLI; cpu_actions_arr[0x59] = cpuEOR; cpu_actions_arr[0x5A] = cpuNoAction; cpu_actions_arr[0x5B] = cpuSRE; 
	cpu_actions_arr[0x5C] = cpuNoAction; cpu_actions_arr[0x5D] = cpuEOR; cpu_actions_arr[0x5E] = cpuLSRt; cpu_actions_arr[0x5F] = cpuSRE;

	cpu_actions_arr[0x60] = cpuNoAction; cpu_actions_arr[0x61] = cpuADC; cpu_actions_arr[0x62] = cpuKIL; cpu_actions_arr[0x63] = cpuRRA;
	cpu_actions_arr[0x64] = cpuNoAction; cpu_actions_arr[0x65] = cpuADC; cpu_actions_arr[0x66] = cpuRORt; cpu_actions_arr[0x67] = cpuRRA;
	cpu_actions_arr[0x68] = cpuNoAction; cpu_actions_arr[0x69] = cpuADC; cpu_actions_arr[0x6A] = cpuRORa; cpu_actions_arr[0x6B] = cpuARR; 
	cpu_actions_arr[0x6C] = cpuNoAction; cpu_actions_arr[0x6D] = cpuADC; cpu_actions_arr[0x6E] = cpuRORt; cpu_actions_arr[0x6F] = cpuRRA;

	cpu_actions_arr[0x70] = cpuNoAction; cpu_actions_arr[0x71] = cpuADC; cpu_actions_arr[0x72] = cpuKIL; cpu_actions_arr[0x73] = cpuRRA;
	cpu_actions_arr[0x74] = cpuNoAction; cpu_actions_arr[0x75] = cpuADC; cpu_actions_arr[0x76] = cpuRORt; cpu_actions_arr[0x77] = cpuRRA;
	cpu_actions_arr[0x78] = cpuSEI; cpu_actions_arr[0x79] = cpuADC; cpu_actions_arr[0x7A] = cpuNoAction; cpu_actions_arr[0x7B] = cpuRRA; 
	cpu_actions_arr[0x7C] = cpuNoAction; cpu_actions_arr[0x7D] = cpuADC; cpu_actions_arr[0x7E] = cpuRORt; cpu_actions_arr[0x7F] = cpuRRA;

	cpu_actions_arr[0x80] = cpuNoAction; cpu_actions_arr[0x81] = cpuNoAction; cpu_actions_arr[0x82] = cpuNoAction; cpu_actions_arr[0x83] = cpuNoAction;
	cpu_actions_arr[0x84] = cpuNoAction; cpu_actions_arr[0x85] = cpuNoAction; cpu_actions_arr[0x86] = cpuNoAction; cpu_actions_arr[0x87] = cpuNoAction;
	cpu_actions_arr[0x88] = cpuDEY; cpu_actions_arr[0x89] = cpuNoAction; cpu_actions_arr[0x8A] = cpuTXA; cpu_actions_arr[0x8B] = cpuXAA; 
	cpu_actions_arr[0x8C] = cpuNoAction; cpu_actions_arr[0x8D] = cpuNoAction; cpu_actions_arr[0x8E] = cpuNoAction; cpu_actions_arr[0x8F] = cpuNoAction;

	cpu_actions_arr[0x90] = cpuNoAction; cpu_actions_arr[0x91] = cpuNoAction; cpu_actions_arr[0x92] = cpuKIL; cpu_actions_arr[0x93] = cpuNoAction;
	cpu_actions_arr[0x94] = cpuNoAction; cpu_actions_arr[0x95] = cpuNoAction; cpu_actions_arr[0x96] = cpuNoAction; cpu_actions_arr[0x97] = cpuNoAction;
	cpu_actions_arr[0x98] = cpuTYA; cpu_actions_arr[0x99] = cpuNoAction; cpu_actions_arr[0x9A] = cpuTXS; cpu_actions_arr[0x9B] = cpuNoAction; 
	cpu_actions_arr[0x9C] = cpuNoAction; cpu_actions_arr[0x9D] = cpuNoAction; cpu_actions_arr[0x9E] = cpuNoAction; cpu_actions_arr[0x9F] = cpuNoAction;

	cpu_actions_arr[0xA0] = cpuLDY; cpu_actions_arr[0xA1] = cpuLDA; cpu_actions_arr[0xA2] = cpuLDX; cpu_actions_arr[0xA3] = cpuLAX;
	cpu_actions_arr[0xA4] = cpuLDY; cpu_actions_arr[0xA5] = cpuLDA; cpu_actions_arr[0xA6] = cpuLDX; cpu_actions_arr[0xA7] = cpuLAX;
	cpu_actions_arr[0xA8] = cpuTAY; cpu_actions_arr[0xA9] = cpuLDA; cpu_actions_arr[0xAA] = cpuTAX; cpu_actions_arr[0xAB] = cpuAXT; 
	cpu_actions_arr[0xAC] = cpuLDY; cpu_actions_arr[0xAD] = cpuLDA; cpu_actions_arr[0xAE] = cpuLDX; cpu_actions_arr[0xAF] = cpuLAX;

	cpu_actions_arr[0xB0] = cpuNoAction; cpu_actions_arr[0xB1] = cpuLDA; cpu_actions_arr[0xB2] = cpuKIL; cpu_actions_arr[0xB3] = cpuLAX;
	cpu_actions_arr[0xB4] = cpuLDY; cpu_actions_arr[0xB5] = cpuLDA; cpu_actions_arr[0xB6] = cpuLDX; cpu_actions_arr[0xB7] = cpuLAX;
	cpu_actions_arr[0xB8] = cpuCLV; cpu_actions_arr[0xB9] = cpuLDA; cpu_actions_arr[0xBA] = cpuTSX; cpu_actions_arr[0xBB] = cpuLAR; 
	cpu_actions_arr[0xBC] = cpuLDY; cpu_actions_arr[0xBD] = cpuLDA; cpu_actions_arr[0xBE] = cpuLDX; cpu_actions_arr[0xBF] = cpuLAX;

	cpu_actions_arr[0xC0] = cpuCMPy; cpu_actions_arr[0xC1] = cpuCMPa; cpu_actions_arr[0xC2] = cpuNoAction; cpu_actions_arr[0xC3] = cpuDCP;
	cpu_actions_arr[0xC4] = cpuCMPy; cpu_actions_arr[0xC5] = cpuCMPa; cpu_actions_arr[0xC6] = cpuDECt; cpu_actions_arr[0xC7] = cpuDCP;
	cpu_actions_arr[0xC8] = cpuINY; cpu_actions_arr[0xC9] = cpuCMPa; cpu_actions_arr[0xCA] = cpuDEX; cpu_actions_arr[0xCB] = cpuCMPax; 
	cpu_actions_arr[0xCC] = cpuCMPy; cpu_actions_arr[0xCD] = cpuCMPa; cpu_actions_arr[0xCE] = cpuDECt; cpu_actions_arr[0xCF] = cpuDCP;

	cpu_actions_arr[0xD0] = cpuNoAction; cpu_actions_arr[0xD1] = cpuCMPa; cpu_actions_arr[0xD2] = cpuKIL; cpu_actions_arr[0xD3] = cpuDCP;
	cpu_actions_arr[0xD4] = cpuNoAction; cpu_actions_arr[0xD5] = cpuCMPa; cpu_actions_arr[0xD6] = cpuDECt; cpu_actions_arr[0xD7] = cpuDCP;
	cpu_actions_arr[0xD8] = cpuCLD; cpu_actions_arr[0xD9] = cpuCMPa; cpu_actions_arr[0xDA] = cpuNoAction; cpu_actions_arr[0xDB] = cpuDCP; 
	cpu_actions_arr[0xDC] = cpuNoAction; cpu_actions_arr[0xDD] = cpuCMPa; cpu_actions_arr[0xDE] = cpuDECt; cpu_actions_arr[0xDF] = cpuDCP;

	cpu_actions_arr[0xE0] = cpuCMPx; cpu_actions_arr[0xE1] = cpuSBC; cpu_actions_arr[0xE2] = cpuNoAction; cpu_actions_arr[0xE3] = cpuISC;
	cpu_actions_arr[0xE4] = cpuCMPx; cpu_actions_arr[0xE5] = cpuSBC; cpu_actions_arr[0xE6] = cpuINCt; cpu_actions_arr[0xE7] = cpuISC;
	cpu_actions_arr[0xE8] = cpuINX; cpu_actions_arr[0xE9] = cpuSBC; cpu_actions_arr[0xEA] = cpuNoAction; cpu_actions_arr[0xEB] = cpuSBC; 
	cpu_actions_arr[0xEC] = cpuCMPx; cpu_actions_arr[0xED] = cpuSBC; cpu_actions_arr[0xEE] = cpuINCt; cpu_actions_arr[0xEF] = cpuISC;

	cpu_actions_arr[0xF0] = cpuNoAction; cpu_actions_arr[0xF1] = cpuSBC; cpu_actions_arr[0xF2] = cpuKIL; cpu_actions_arr[0xF3] = cpuISC;
	cpu_actions_arr[0xF4] = cpuNoAction; cpu_actions_arr[0xF5] = cpuSBC; cpu_actions_arr[0xF6] = cpuINCt; cpu_actions_arr[0xF7] = cpuISC;
	cpu_actions_arr[0xF8] = cpuSED; cpu_actions_arr[0xF9] = cpuSBC; cpu_actions_arr[0xFA] = cpuNoAction; cpu_actions_arr[0xFB] = cpuISC; 
	cpu_actions_arr[0xFC] = cpuNoAction; cpu_actions_arr[0xFD] = cpuSBC; cpu_actions_arr[0xFE] = cpuINCt; cpu_actions_arr[0xFF] = cpuISC;

	cpu_action_func = cpuNoAction;
}

/* Do all IRQ related updates */
//static int intrPrintUpdate = 0;
static bool cpu_interrupt_req = false;
static bool ppu_nmi_handler_req = false;
//set externally
bool mapper_interrupt = false;
static bool cpuHandleIrqUpdates()
{
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
	//handle incoming IRQs
	if(reset)
	{
		cpu_action_arr = cpu_reset_arr;
		cpu_arr_pos = 0;
		cpu_action_func = cpuNoAction;
		reset = false;
		return true;
	}
	else if(ppu_nmi_handler_req)
	{
		cpu_action_arr = cpu_nmi_arr;
		cpu_arr_pos = 0;
		cpu_action_func = cpuNoAction;
		#if DEBUG_INTR
		printf("NMI from p %02x pc %04x\n",p,pc);
		#endif
		ppu_nmi_handler_req = false;
		return true;
	}
	else if(cpu_interrupt_req)
	{
		cpu_action_arr = cpu_irq_arr;
		cpu_arr_pos = 0;
		cpu_action_func = cpuNoAction;
		#if DEBUG_INTR
		printf("INTR %d %d %d from p %02x pc %04x\n",interrupt,dmc_interrupt,apu_interrupt,p,pc);
		#endif
		if(interrupt)
			interrupt = false;
		if(fds_transfer_interrupt)
			fds_transfer_interrupt = false;
		return true;
	}
	//acts similar to an irq
	if(nsf_startPlayback)
	{
		cpuStartPlayNSF();
		nsf_startPlayback = false;
		return true;
	}
	else if(nsf_endPlayback)
	{
		cpuEndPlayNSF();
		nsf_endPlayback = false;
		return true;
	}
	return false;
}

static void cpuSetAddrIndFix()
{
	//first read will be at wrong pos
	//so let cpu know it needs fixup
	if((absAddr&0xFF00) != (indVal&0xFF00))
	{
		//printf("%04x %04x\n", absAddr, indVal);
		needsIndFix = true;
	}
}

static bool cpuDoAddrIndFix()
{
	if(needsIndFix)
	{
		absAddr = indVal;
		needsIndFix = false;
		return true;
	}
	else
		return false;
}

static bool cpuDMATryHalt()
{
	bool ret = true;
	uint8_t cpuType = cpu_mem_type[cpu_action_arr[cpu_arr_pos]];
	switch(cpuType)
	{
		case CPU_READ_PC:
			memGet8(pc);
			break;
		case CPU_READ_CPUTMP:
			memGet8(cpuTmp);
			break;
		case CPU_READ_ABSADDR:
			memGet8(absAddr);
			break;
		case CPU_READ_STACK:
			memGet8(0x100+s);
			break;
		case CPU_READ_NMIVECL:
			memGet8(0xFFFA);
			break;
		case CPU_READ_NMIVECH:
			memGet8(0xFFFB);
			break;
		case CPU_READ_RESETVECL:
			memGet8(0xFFFC);
			break;
		case CPU_READ_RESETVECH:
			memGet8(0xFFFD);
			break;
		case CPU_READ_IRQVECL:
			memGet8(0xFFFE);
			break;
		case CPU_READ_IRQVECH:
			memGet8(0xFFFF);
			break;
		default: //CPU_WRITE
			ret = false;
			break;
	}
	return ret;
}

static bool cpu_currently_dma = false;
static uint8_t cpu_oam_dma_ptr = 0;
static bool cpu_oam_ready = false;
static bool cpu_dmc_dma_dummyread = false;
static uint8_t cpu_oam_dma_val = 0;

static bool cpuTryTakeover()
{
	//printf("DMA Takeover tick\n");
	bool cpu_is_read = cpuDMATryHalt();
	if(cpu_is_read)
	{
		//printf("DMA Takeover done\n");
		cpu_currently_dma = true;	
		//reset OAM DMA pointer
		cpu_oam_dma_ptr = 0;
		//set OAM ready to false
		cpu_oam_ready = false;
		//set dmc dummy read to true
		cpu_dmc_dma_dummyread = true;
		return true;
	}
	return false;
}

static bool cpu_dmc_halted = false;
static bool cpu_oam_halted = false;
static bool dmchaltattempt = false;
static void cpuDoDMA()
{
	if(cpu_dmc_dma)
	{
		if(cpu_currently_dma && cpu_dmc_halted)
		{
			if(cpu_dmc_dma_dummyread) //1st read always dummy read
			{
				cpu_dmc_dma_dummyread = false;
				if(!cpu_oam_dma || cpu_oam_dma_pause)
				{
					cpuDMATryHalt();
					return;
				}
			}
			else if(!cpu_odd_cycle) //READ on even
			{
				//printf("DMC DMA Read %04x %i\n",cpu_dmc_dma_addr,testCounter2);
				uint8_t dmc_dma_val = memGet8(cpu_dmc_dma_addr);
				apuWriteDMCBuf(dmc_dma_val);
				cpu_dmc_dma = false;
				if(!cpu_oam_halted) //done with DMA
					cpu_currently_dma = false;
				//for next dmc dma
				dmchaltattempt = false;
				cpu_dmc_halted = false;
				return;
			}
			else //WRITE on odd, possibly another dummy read
			{
				if(!cpu_oam_dma || cpu_oam_dma_pause)
				{
					cpuDMATryHalt();
					return;
				}
			}
		}
		else
		{
			//First DMC halt attempt ALWAYS during odd (write) cycle
			//Second (or 3rd) halt attempt during any cycle
			if(cpu_odd_cycle || dmchaltattempt)
			{
				if(cpu_currently_dma || cpuTryTakeover())
				{
					cpu_dmc_halted = true;
					//fully ignore OAM DMA if this is out 2nd/3rd attempt
					if(cpu_oam_dma && dmchaltattempt)
						cpu_oam_dma_pause = true;
					else //was first halt attempt, possibly allows next OAM DMA cycle
						cpu_oam_dma_pause = false;
				}
				else
					dmchaltattempt = true;
			}
		}
	}
	if(cpu_oam_dma)
	{
		if(cpu_currently_dma && cpu_oam_halted)
		{
			if(!cpu_odd_cycle) //READ on even
			{
				//printf("OAM DMA Read %04x\n",cpu_oam_dma_addr|cpu_oam_dma_ptr);
				cpu_oam_dma_val = memGet8(cpu_oam_dma_addr|cpu_oam_dma_ptr);
				cpu_oam_ready = true;
			}
			else //WRITE on odd
			{
				if(cpu_oam_ready)
				{
					cpu_oam_ready = false;
					//printf("OAM DMA Write %02x\n", cpu_oam_dma_val);
					memSet8(0x2004,cpu_oam_dma_val);
					cpu_oam_dma_ptr++;
					//wrapped back, done with DMA
					if(cpu_oam_dma_ptr == 0)
					{
						//printf("OAM DMA Done\n");
						cpu_oam_dma = false;
						if(!cpu_dmc_halted) //done with DMA
							cpu_currently_dma = false;
						cpu_oam_halted = false;
					}
				}
				else //alignment cycle
					cpuDMATryHalt();
			}
		}
		else if(cpu_currently_dma || cpuTryTakeover())
			cpu_oam_halted = true;
	}
}

/* Main CPU Interpreter */
int testCounter = 0;
bool cpuCycle()
{
	cpu_odd_cycle^=true;
	testCounter++;
	//printf("CPU Cycle\n");
	//do DMC and OAM DMA first
	cpuDoDMA();
	if(cpu_currently_dma)
		return true;
	//make sure to wait if needed
	if(waitCycles)
	{
		waitCycles--;
		return true;
	}
	uint8_t instr, cpu_action;
doaction:
	cpu_action = cpu_action_arr[cpu_arr_pos];
	cpu_arr_pos++;
	switch(cpu_action)
	{
		case CPU_GET_INSTRUCTION:
			instr = memGet8(pc);
			//if IRQ occurs end this cycle early
			if(cpuHandleIrqUpdates())
				break;
			cpu_action_arr = cpu_instr_arr[instr];
			cpu_arr_pos = 0;
			cpu_action_func = cpu_actions_arr[instr];
			//printf("%04x %02x %02x %02x %02x %02x\n", pc, instr, a, x, y, p);
			pc++;
			break;
		case CPU_NULL_READ8_PC:
			memGet8(pc);
			break;
		case CPU_NULL_READ8_PC_INC:
			memGet8(pc++);
			break;
		case CPU_NULL_READ8_PC_CHK:
			memGet8(pc);
			if(needsIndFix)
			{
				pc = indVal;
				needsIndFix = false;
			}
			break;
		case CPU_NULL_READ8_PC_ACTION:
			memGet8(pc);
			cpu_action_func();
			break;
		case CPU_NULL_READ8_PC_ADDR_ADDX_ZP:
			memGet8(pc);
			absAddr += x;
			absAddr &= 0xFF;
			break;
		case CPU_NULL_READ8_PC_ADDR_ADDY_ZP:
			memGet8(pc);
			absAddr += y;
			absAddr &= 0xFF;
			break;
		case CPU_NULL_READ8_PC_TMP_ADDX:
			memGet8(pc);
			cpuTmp += x;
			break;
		case CPU_NULL_READ8_PC_STACK_INC:
			memGet8(pc);
			s++;
			break;
		case CPU_NULL_READ8_PC_STACK_DEC:
			memGet8(pc);
			s--;
			break;
		case CPU_NULL_READ8_PC_STACK_DEC_SET_S1_S2_I:
			memGet8(pc);
			s--;
			p = (P_FLAG_IRQ_DISABLE | P_FLAG_S1 | P_FLAG_S2);
			break;
		case CPU_TMP_READ8_PC_INC:
			cpuTmp = memGet8(pc++);
			break;
		case CPU_TMP_READ8_PC_INC_ACTION:
			cpuTmp = memGet8(pc++);
			cpu_action_func();
			break;
		case CPU_PCL_FROM_TMP_PCH_READ8_PC:
			pc = (cpuTmp | (memGet8(pc)<<8));
			break;
		case CPU_ADDR_READ8_PC_INC:
			absAddr = memGet8(pc++);
			break;
		case CPU_ADDRL_READ8_PC_INC:
			absAddr = memGet8(pc++);
			break;
		case CPU_ADDRH_READ8_PC_INC:
			absAddr |= (memGet8(pc++)<<8);
			break;
		case CPU_ADDRH_READ8_PC_INC_ADDX:
			indVal = absAddr + x;
			cpuSetAddrIndFix();
			absAddr += x;
			absAddr &= 0xFF;
			absAddr |= (memGet8(pc++)<<8);
			indVal += (absAddr&0xFF00);
			break;
		case CPU_ADDRH_READ8_PC_INC_ADDY:
			indVal = absAddr + y;
			cpuSetAddrIndFix();
			absAddr += y;
			absAddr &= 0xFF;
			absAddr |= (memGet8(pc++)<<8);
			indVal += (absAddr&0xFF00);
			break;
		case CPU_ADDRL_READ8_TMP_INC:
			absAddr = memGet8(cpuTmp++);
			break;
		case CPU_ADDRH_READ8_TMP:
			absAddr |= (memGet8(cpuTmp)<<8);
			break;
		case CPU_ADDRH_READ8_TMP_ADDY:
			indVal = absAddr + y;
			cpuSetAddrIndFix();
			absAddr += y;
			absAddr &= 0xFF;
			absAddr |= (memGet8(cpuTmp)<<8);
			indVal += (absAddr&0xFF00);
			break;
		case CPU_ADDR_READ8:
			cpuTmp = memGet8(absAddr);
			break;
		case CPU_ADDR_READ8_CHK:
			cpuTmp = memGet8(absAddr);
			cpuDoAddrIndFix();
			break;
		case CPU_ADDR_READ8_ACTION:
			cpuTmp = memGet8(absAddr);
			cpu_action_func();
			break;
		case CPU_ADDR_READ8_ACTION_CHK:
			cpuTmp = memGet8(absAddr);
			if(!cpuDoAddrIndFix())
			{
				cpu_action_func();
				//only execute extra cycle
				//if fixup is needed
				cpu_arr_pos++;
			}
			break;
		case CPU_INC_PAGE_ADDR_READ8_SET_PC:
			pc = cpuTmp; //low pc bytes
			cpuTmp = (absAddr & 0xFF);
			absAddr &= ~0xFF; //clear low address bytes
			//emulate 6502 jmp wrap bug 
			cpuTmp++; //possibly FF->00 without page increase!
			absAddr |= cpuTmp; //add back low address bytes
			pc |= (memGet8(absAddr)<<8); //high pc bytes
			break;
		case CPU_ADDR_WRITE8:
			memSet8(absAddr, cpuTmp);
			cpuWriteTMP = false;
			break;
		case CPU_ADDR_WRITE8_A:
			memSet8(absAddr, a);
			break;
		case CPU_ADDR_WRITE8_X:
			memSet8(absAddr, x);
			break;
		case CPU_ADDR_WRITE8_Y:
			memSet8(absAddr, y);
			break;
		case CPU_ADDR_WRITE8_AX:
			memSet8(absAddr, a&x);
			break;
		case CPU_ADDR_WRITE8_AXA:
			memSet8(absAddr, a&x&(absAddr>>8));
			break;
		case CPU_ADDR_WRITE8_XAS:
			if(needsIndFix)
			{
				absAddr &= y << 8;
				needsIndFix = false;
			}
			s = a & x;
			memSet8(absAddr, a & x & ((absAddr >> 8) + 1));
			break;
		case CPU_ADDR_WRITE8_SYA:
			if(needsIndFix)
			{
				absAddr &= y << 8;
				needsIndFix = false;
			}
			memSet8(absAddr, y & ((absAddr >> 8) + 1));
			break;
		case CPU_ADDR_WRITE8_SXA:
			if(needsIndFix)
			{
				absAddr &= x << 8;
				needsIndFix = false;
			}
			memSet8(absAddr, x & ((absAddr >> 8) + 1));
			break;
		case CPU_ADDR_WRITE8_ACTION:
			memSet8(absAddr, cpuTmp);
			cpuWriteTMP = true;
			cpu_action_func();
			break;
		case CPU_CHECK_BCC:
			takeBranch = !(p & P_FLAG_CARRY);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BCS:
			takeBranch = !!(p & P_FLAG_CARRY);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BNE:
			takeBranch = !(p & P_FLAG_ZERO);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BEQ:
			takeBranch = !!(p & P_FLAG_ZERO);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BPL:
			takeBranch = !(p & P_FLAG_NEGATIVE);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BMI:
			takeBranch = !!(p & P_FLAG_NEGATIVE);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BVC:
			takeBranch = !(p & P_FLAG_OVERFLOW);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_CHECK_BVS:
			takeBranch = !!(p & P_FLAG_OVERFLOW);
			if(!cpuBranchCheck()) goto doaction;
			else memGet8(pc);
			break;
		case CPU_STACK_GET_A:
			cpuSetA(memGet8(0x100+s));
			break;
		case CPU_STACK_GET_P:
			cpuTmp = memGet8(0x100+s);
			p &= P_FLAG_IRQ_DISABLE;
			p |= (cpuTmp & ~P_FLAG_IRQ_DISABLE);
			//will do it for us after
			if(cpuTmp & P_FLAG_IRQ_DISABLE)
			{
				//printf("PLP IRQ Set 1\n");
				p_irq_req = 1;
			}
			else
			{
				//printf("PLP IRQ Set 2\n");
				p_irq_req = 2; 
			}
			break;
		case CPU_STACK_GET_P_INC:
			p = memGet8(0x100+s);
			s++;
			break;
		case CPU_STACK_GET_PCL_INC:
			pc &= ~0xFF;
			pc |= memGet8(0x100+s);
			s++;
			break;
		case CPU_STACK_GET_PCH:
			pc &= 0xFF;
			pc |= memGet8(0x100+s)<<8;
			break;
		case CPU_STACK_STORE_A_DEC:
			memSet8(0x100+s,a);
			s--;
			break;
		case CPU_STACK_STORE_P_DEC:
			p |= (P_FLAG_S1 | P_FLAG_S2);
			memSet8(0x100+s,p);
			s--;
			break;
		case CPU_STACK_STORE_PCH_DEC:
			memSet8(0x100+s,pc>>8);
			s--;
			break;
		case CPU_STACK_STORE_PCL_DEC:
			memSet8(0x100+s,pc&0xFF);
			s--;
			break;
		case CPU_STACK_STORE_P_DEC_SET_I:
			memSet8(0x100+s,p);
			s--;
			p |= P_FLAG_IRQ_DISABLE;
			break;
		case CPU_STACK_SET_S1_S2_STORE_P_DEC_SET_I:
			p |= (P_FLAG_S1 | P_FLAG_S2);
			memSet8(0x100+s,p);
			s--;
			p |= P_FLAG_IRQ_DISABLE;
			break;
		case CPU_STACK_CLEAR_S1_SET_S2_STORE_P_DEC_SET_I:
			p &= ~P_FLAG_S1;
			p |= P_FLAG_S2;
			memSet8(0x100+s,p);
			s--;
			p |= P_FLAG_IRQ_DISABLE;
			break;
		case CPU_READ_NMIVEC_PCL:
			pc &= ~0xFF;
			pc |= memGet8(0xFFFA);
			break;
		case CPU_READ_NMIVEC_PCH:
			pc &= 0xFF;
			pc |= memGet8(0xFFFB)<<8;
			break;
		case CPU_READ_RESETVEC_PCL:
			pc &= ~0xFF;
			pc |= memGet8(0xFFFC);
			break;
		case CPU_READ_RESETVEC_PCH:
			pc &= 0xFF;
			pc |= memGet8(0xFFFD)<<8;
			break;
		case CPU_READ_IRQVEC_PCL:
			pc &= ~0xFF;
			pc |= memGet8(0xFFFE);
			break;
		case CPU_READ_IRQVEC_PCH:
			pc &= 0xFF;
			pc |= memGet8(0xFFFF)<<8;
			break;
		default: //should never happen
			printf("Unknown action %i\n", cpu_action);
			memDumpMainMem();
			return false;
	}
	//update interrupts right before next instruction get cycle
	if(cpu_action_arr[cpu_arr_pos] == CPU_GET_INSTRUCTION)
	{
		ppu_nmi_handler_req = ppuNMI();
		cpu_interrupt_req = (interrupt || ((mapper_interrupt || dmc_interrupt || apu_interrupt || 
				fds_interrupt || fds_transfer_interrupt || mmc5_dmc_interrupt) && !(p & P_FLAG_IRQ_DISABLE)));
	}
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

/* .nsf Playback Handlers */
static uint16_t pc_nsf_bak;
static uint8_t p_nsf_bak;

void cpuStartPlayNSF()
{
	//used in NSF mapper to detect play return
	uint16_t absAddr = 0x456A-1;
	memSet8(0x100+s,absAddr>>8);
	s--;
	memSet8(0x100+s,absAddr&0xFF);
	s--;
	//back up old PC and P
	pc_nsf_bak = pc;
	p_nsf_bak = p;
	pc = nsfGetPlayAddr(); //jump to play
	cpu_action_arr = cpu_start_arr;
	cpu_arr_pos = 0;
	//printf("Playback Start at %04x\n", pc);
}

void cpuEndPlayNSF()
{
	//restore old PC and P
	pc = pc_nsf_bak;
	p = p_nsf_bak;
	cpu_action_arr = cpu_start_arr;
	cpu_arr_pos = 0;
	//printf("Playback End at %04x\n", pc);
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
	//prepare APU regs
	apuSet8(0x15,0xF);
	apuSet8(0x17,0x40);
	//used in NSF mapper to detect init return
	uint16_t initRet = 0x4567-1;
	memSet8(0x100+s,initRet>>8);
	s--;
	memSet8(0x100+s,initRet&0xFF);
	s--;
	pc = addr;
	cpu_action_arr = cpu_start_arr;
	cpu_arr_pos = 0;
	nsf_startPlayback = false;
	nsf_endPlayback = false;
	//printf("Init at %04x\n", addr);
}
