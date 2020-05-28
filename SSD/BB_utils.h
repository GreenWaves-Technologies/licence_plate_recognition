/*
 * Copyright (C) 2020 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */


#include "SSDParams.h"
#include "setup.h"



void convertCoordBboxes(bboxs_t *boundbxs,float scale_x,float scale_y);

void printBboxes(bboxs_t *boundbxs);

void printBboxes_forPython(bboxs_t *boundbxs);

int rect_intersect_area( short a_x, short a_y, short a_w, short a_h,
                         short b_x, short b_y, short b_w, short b_h );

void non_max_suppress(bboxs_t * boundbxs);