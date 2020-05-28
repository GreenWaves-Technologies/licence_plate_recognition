/*
 * Copyright 2020 GreenWaves Technologies, SAS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __IIBASICKERNELS_H__
#define __IIBASICKERNELS_H__

#include "Gap.h"
#include "SSDParams.h"
#include "setup.h"

#define Max(a, b)               (((a)>(b))?(a):(b))
#define Min(a, b)               (((a)<(b))?(a):(b))

typedef struct {
    int index;
    int global_index;
    int confidence;
    int8_t *Boxes;
    uint8_t Boxes_QSCALE;
    uint8_t Boxes_QNORM;
    Alps *Anchors;
    bboxs_t *bbxs;
    uint16_t class;
    uint16_t bbnum;
}KerEstimateBB_ArgT;

typedef struct {
	int8_t * __restrict__ Classes;
	int8_t * __restrict__ Boxes;
	unsigned int Classes_W;
	unsigned int Classes_H;
	unsigned int Classes_TileIndex;
	unsigned int Classes_Std_H;
	void * Ancor_layer;
	void * BoundingBoxes;
	uint8_t Classes_QScale;
	uint8_t Classes_QNorm;
	uint8_t Boxes_QScale;
	uint8_t Boxes_QNorm;
	uint16_t n_classes;
} KerPredecoderInt8_ArgT;

void KerPredecoderInt8(KerPredecoderInt8_ArgT *Arg);



#endif //__IIBASICKERNELS_H__
