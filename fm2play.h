/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _fm2play_h_
#define _fm2play_h_

bool fm2playInit(char *fName, int fstart, bool wait);
void fm2playUpdate();
bool fm2playRunning();
bool fm2playWaitDMAcycles();

#endif
