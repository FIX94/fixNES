/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef vrc_irq_h_
#define vrc_irq_h_

void vrc_irq_init();
void vrc_irq_setlatch(uint8_t val);
void vrc_irq_setlatchLo(uint8_t val);
void vrc_irq_setlatchHi(uint8_t val);
void vrc_irq_control(uint8_t val);
void vrc_irq_ack();
void vrc_irq_cycle();

#endif
