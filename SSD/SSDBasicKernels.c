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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "SSDBasicKernels.h"

#define FIX2FP(Val, Precision)      ((float) (Val) / (float) (1<<(Precision)))
#define FP2FIXR(Val, Precision)     ((int)((Val)*((1 << (Precision))-1) + 0.5))
#define FP2FIX(Val, Precision)      ((int)((Val)*((1 << (Precision))-1)))


static inline unsigned int __attribute__((always_inline)) ChunkSize(unsigned int X)

{
    unsigned int NCore;
    unsigned int Log2Core;
    unsigned int Chunk;

    NCore = rt_nb_pe();
    Log2Core = gap_fl1(NCore);
    Chunk = (X>>Log2Core) + ((X&(NCore-1))!=0);
    return Chunk;
}



#define Abs(a)      (((int)(a)<0)?(-(a)):(a))
#define Min(a, b)   (((a)<(b))?(a):(b))
#define Max(a, b)   (((a)>(b))?(a):(b))

PI_L1 static unsigned short int IntegerExpLUT[] =
{
    0x0001, 0x0002, 0x0007, 0x0014, 0x0036, 0x0094, 0x0193, 0x0448, 0x0BA4, 0x1FA7, 0x560A, 0xE9E2
};

PI_L1 static unsigned short int FractionExpLUT[] =
{
    0x0000, 0x5BF1, 0x31CD, 0x0AF3, 0x4C90, 0x34E2, 0x36E3, 0x510B, 0x7A9F, 0x0ABE, 0x3B9F, 0x1224
};

/* 17.15 fixed point format */
PI_L1 static unsigned short int ExpCoeffLUT[] =
{
    0x7FFF, 0x7FFF, 0x4000, 0x1555, 0x0555, 0x0111, 0x002E, 0x0007, 0x0001
};


#define ARRAYSIZE(x)    (sizeof(x) / sizeof(x[ 0 ]))

/* X : fixed point, format Q17.15, returns in Q17.15 */
static unsigned int Exp_fp_17_15(unsigned int X)

{
    int  Y, Result, IntX, FractX, ScaledInt;
    short int Z_s, FractX_s;
    unsigned short int  ScaledFract;

    if (!X) return 0x8000;
    Y = Abs(X);
    IntX = (Y >> 15);
    if (IntX >= (int) ARRAYSIZE (IntegerExpLUT))
    {
        if (Y == X) return 0x7FFFFFFF;
        else return 0;
    }
    FractX = (Y & 0x7FFF);
    if (gap_bitextractu(FractX, 1, 14))
    {
        /* Taylor series converges quickly only when | FractX | < 0.5 */
        FractX -= 0x8000;
        IntX++;
    }
    ScaledInt = IntegerExpLUT[IntX];
    ScaledFract = FractionExpLUT[IntX];
    /* Taylor's series: exp(x) = 1 + x + x ^ 2 / 2 + x ^ 3 / 3! + x ^ 4 / 4! + x ^ 5 / 5! + x ^ 6 / 6! + x ^ 7 / 7! + x ^ 8 / 8!  */
    FractX_s = FractX;
    Z_s = FractX;
    Result = 0;
    for (int i = 1; i < ARRAYSIZE (ExpCoeffLUT); i++)
    {
        Result += Z_s * ExpCoeffLUT[i]; // gap8_macs(Result, Z, ExpCoeffLUT[ i ]);
        Z_s = gap_mulsRN(Z_s, FractX_s, 15);
    }
    Result = gap_roundnorm(Result, 15) + ExpCoeffLUT[0];
    unsigned short int U_Res = Result;
    Result = gap_muluRN(U_Res, ScaledFract, 15) + U_Res * ScaledInt;
    if (Result && (X > 0x7FFFFFFF))
        Result = ((0x7FFFFFFF / Result) >> 1);      /* negative value */
    return (unsigned int) Result;
}

static void KerEstimate_bbox(KerEstimateBB_ArgT*Arg){
    int index = Arg->index;
    int global_index = Arg->global_index;
    int confidence = Arg->confidence;
    int8_t *Boxes = Arg->Boxes;
    uint8_t Boxes_QSCALE = Arg->Boxes_QSCALE;
    uint8_t Boxes_QNORM = Arg->Boxes_QNORM;
    Alps *Anchors = Arg->Anchors;
    bboxs_t *bbxs = Arg->bbxs;
    uint16_t class = Arg->class;
    uint16_t bbnum = Arg->bbnum;

    int anchor_location;
    int i_loc;
    int j_loc;
    int anchor_id;
    int32_t cx_offset, cy_offset, w_offset, h_offset;
    int32_t cx, cy;
    float w, h;

    //This can be done offline
    int zero_height = Anchors->offset_height * Anchors->step_height;
    int zero_width  = Anchors->offset_width  * Anchors->step_width;

    // step 1: decode the anchor location and its id
    anchor_location  = global_index/Anchors->n_anchors;
    anchor_id        = global_index%Anchors->n_anchors;
    i_loc    = anchor_location/Anchors->feature_map_width;
    j_loc    = anchor_location%Anchors->feature_map_width;

    // step 2: Identify the offsets
    //conversion bring them to Q8
    cx_offset = (int16_t) (gap_roundnorm_reg((int)Boxes[index*Anchors->anchor_params+0] * Boxes_QSCALE, Boxes_QNORM));
    cy_offset = (int16_t) (gap_roundnorm_reg((int)Boxes[index*Anchors->anchor_params+1]  * Boxes_QSCALE, Boxes_QNORM));
    w_offset  = (int16_t) (gap_roundnorm_reg((int)Boxes[index*Anchors->anchor_params+2]  * Boxes_QSCALE, Boxes_QNORM));
    h_offset  = (int16_t) (gap_roundnorm_reg((int)Boxes[index*Anchors->anchor_params+3]  * Boxes_QSCALE, Boxes_QNORM));

    // step 3: calculate default anchor parameters for this location
    cx = FP2FIX(((zero_width  + i_loc * Anchors->step_width)/(float)Anchors->img_width)  , 20);
    cy = FP2FIX(((zero_height + j_loc * Anchors->step_height)/(float)Anchors->img_height), 20);

    //[TODO] This can be done offline
    anchorWH_t anchorWH = Anchors->anchorsWH[anchor_id];
    w = anchorWH.w / (float)Anchors->img_width;
    h = anchorWH.h / (float)Anchors->img_height;
    
    //printf("\nAnchor info: %f, %f, %f, %f\n", cx, cy, w, h);
    int16_t var_0_f = FP2FIX(Anchors->variances[0] ,12);
    int16_t var_1_f = FP2FIX(Anchors->variances[1] ,12);
    int16_t var_2_f = FP2FIX(Anchors->variances[2] ,12);
    int16_t var_3_f = FP2FIX(Anchors->variances[3] ,12);

    int32_t w_fix = FP2FIX(w,10);
    int32_t h_fix = FP2FIX(h,10);


    // step 4: apply offsets to the default anchor box 
    //(Q8 * Q12 )* Q10 + Q20 => output Q20
    bbxs->bbs[bbnum].x  = ((int)(cx_offset * var_0_f)*h_fix >> 10 ) + cx;
    bbxs->bbs[bbnum].y  = ((int)(cy_offset * var_1_f)*h_fix >> 10 ) + cy;
    
    //Q8 * Q12 >> 5 to have Q15 for exp
    unsigned int Exp;
    Exp = Exp_fp_17_15( (w_offset * var_2_f) >> (5));
    bbxs->bbs[bbnum].w       = (int)(Exp * w_fix) >>5; //Output Q20
    Exp = Exp_fp_17_15((h_offset * var_3_f) >> (5));
    bbxs->bbs[bbnum].h       = (int)(Exp * h_fix)>>5; //Output Q20
    
    bbxs->bbs[bbnum].class   = class;
    bbxs->bbs[bbnum].score = confidence;

 }


static void KerEstimate_bbox_fp(KerEstimateBB_ArgT*Arg){
    
    int index = Arg->index;
    int global_index = Arg->global_index;
    int confidence = Arg->confidence;
    int8_t *Boxes = Arg->Boxes;
    uint8_t Boxes_QSCALE = Arg->Boxes_QSCALE;
    uint8_t Boxes_QNORM = Arg->Boxes_QNORM;
    Alps *Anchors = Arg->Anchors;
    bboxs_t *bbxs = Arg->bbxs;
    uint16_t class = Arg->class;
    uint16_t bbnum = Arg->bbnum;


    int anchor_location;
    int i_loc;
    int j_loc;
    int anchor_id;
    float cx_offset, cy_offset, w_offset, h_offset;
    float cx, cy;
    float w, h;
    float out_x,out_y,out_w, out_h;


    //This can be done offline
    float zero_height = Anchors->offset_height * Anchors->step_height;
    float zero_width  = Anchors->offset_width  * Anchors->step_width;

    // step 1: decode the anchor location and its id
    anchor_location  = global_index/Anchors->n_anchors;
    anchor_id        = global_index%Anchors->n_anchors;
    i_loc    = anchor_location/Anchors->feature_map_width;
    j_loc    = anchor_location%Anchors->feature_map_width;

    // step 2: Identify the offsets
    int cx_offset_int = gap_roundnorm_reg(Boxes[index*Anchors->anchor_params+0] * Boxes_QSCALE, Boxes_QNORM);
    int cy_offset_int = gap_roundnorm_reg(Boxes[index*Anchors->anchor_params+1] * Boxes_QSCALE, Boxes_QNORM);
    int w_offset_int  = gap_roundnorm_reg(Boxes[index*Anchors->anchor_params+2] * Boxes_QSCALE, Boxes_QNORM);
    int h_offset_int  = gap_roundnorm_reg(Boxes[index*Anchors->anchor_params+3] * Boxes_QSCALE, Boxes_QNORM);
    cx_offset =  FIX2FP(cx_offset_int,8);
    cy_offset =  FIX2FP(cy_offset_int,8);
    w_offset  =  FIX2FP(w_offset_int ,8);
    h_offset  =  FIX2FP(h_offset_int ,8);
    

    //printf("%f %f %f %f\n",cx_offset,cy_offset,w_offset,h_offset);

    // step 3: calculate default anchor parameters for this location
    cx = ((zero_width  + (float)i_loc * Anchors->step_width)/(float)Anchors->img_width)  ;
    cy = ((zero_height + (float)j_loc * Anchors->step_height)/(float)Anchors->img_height);


    //[TODO] This can be done offline
    anchorWH_t anchorWH = Anchors->anchorsWH[anchor_id];
    w = anchorWH.w / (float)Anchors->img_width;
    h = anchorWH.h / (float)Anchors->img_height;
    
    //printf("\nAnchor info: %f, %f, %f, %f\n", cx, cy, w, h);
    float var_0_f = Anchors->variances[0];
    float var_1_f = Anchors->variances[1];
    float var_2_f = Anchors->variances[2];
    float var_3_f = Anchors->variances[3];

    //[TODO]The variances can be saved with their reciprocal numbers to avoid division
    out_x = (cx_offset * var_0_f)*w + cx;
    out_y = (cy_offset * var_1_f)*h + cy;

    bbxs->bbs[bbnum].x  = (int32_t)FP2FIX(out_x, 20);
    bbxs->bbs[bbnum].y  = (int32_t)FP2FIX(out_y, 20);

    out_w = exp(w_offset * var_2_f)*w;
    bbxs->bbs[bbnum].w  = (int32_t)FP2FIX(out_w,20);

    out_h = exp(h_offset * var_3_f)*h;
    bbxs->bbs[bbnum].h  = (int32_t)FP2FIX(out_h,20); //Output Q20


    bbxs->bbs[bbnum].class   = class;
    bbxs->bbs[bbnum].score = confidence;

}

void KerPredecoderInt8(KerPredecoderInt8_ArgT *Arg){

    int8_t * __restrict__ Classes = Arg->Classes;
    int8_t * __restrict__ Boxes   = Arg->Boxes;
    unsigned int W          = Arg->Classes_W;  //Same info as n_classes
    unsigned int H          = Arg->Classes_H;
    unsigned int TileIndex  = Arg->Classes_TileIndex;
    unsigned int Std_H      = Arg->Classes_Std_H;
    uint16_t n_classes      = Arg->n_classes;
    uint8_t Classes_QScale     = Arg->Classes_QScale;
    uint8_t Classes_QNorm      = Arg->Classes_QNorm;
    uint8_t Boxes_QScale     = Arg->Boxes_QScale;
    uint8_t Boxes_QNorm      = Arg->Boxes_QNorm;
    Alps * anch             = (Alps*) Arg->Ancor_layer;
    bboxs_t* bbxs           = (bboxs_t*) Arg->BoundingBoxes;
    
    
    unsigned int CoreId = gap_coreid();
    unsigned int ChunkCell = ChunkSize(H);
    unsigned int First = CoreId*ChunkCell, Last  = Min(H, First+ChunkCell);



    for(int i=First;i<Last;i++){
        int global_index = TileIndex*Std_H;
        //we start from 1 since we skip the Background
        for(uint16_t n=1;n<W;n++){

            int cl = gap_roundnorm_reg(Classes[i*n_classes+n] * Classes_QScale, Classes_QNorm);
            //Here it would be nice to add a different confidence for each class
            if(cl > anch->confidence_thr)
            {
                //Here we pass the row index to find the correct row in the boxes
                if(bbxs->num_bb>=MAX_BB){
                    PRINTF("Reached Max BB number...\n");
                    return;
                }
                KerEstimateBB_ArgT Arg;
                Arg.index=i;
                Arg.global_index=TileIndex*Std_H+i;
                Arg.confidence=cl<<7;
                Arg.Boxes=Boxes;
                Arg.Boxes_QSCALE=Boxes_QScale;
                Arg.Boxes_QNORM=Boxes_QNorm;
                Arg.Anchors=anch;
                Arg.bbxs=bbxs;
                Arg.class=n;
                pi_cl_team_critical_enter();
                Arg.bbnum=bbxs->num_bb++;
                pi_cl_team_critical_exit();
                //KerEstimate_bbox_fp(&Arg);
                KerEstimate_bbox(&Arg);
                    
                }
            }
        }
    
}
