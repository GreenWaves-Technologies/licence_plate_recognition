/*
 * Copyright (C) 2020 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include "BB_utils.h"

#define FIX2FP(Val, Precision)      ((float) (Val) / (float) (1<<(Precision)))
#define FP2FIXR(Val, Precision)     ((int)((Val)*((1 << (Precision))-1) + 0.5))
#define FP2FIX(Val, Precision)      ((int)((Val)*((1 << (Precision))-1)))


void convertCoordBboxes(bboxs_t *boundbxs,float scale_x,float scale_y){

    for (int counter=0;counter< boundbxs->num_bb;counter++){
        //Set Alive for non max this should be done somewhere else
        boundbxs->bbs[counter].alive=1;
        //boundbxs->bbs[counter].x = (int)(FIX2FP(boundbxs->bbs[counter].x,20) * scale_x);
        //boundbxs->bbs[counter].y = (int)(FIX2FP(boundbxs->bbs[counter].y,20) * scale_y);
        //boundbxs->bbs[counter].w = (int)(FIX2FP(boundbxs->bbs[counter].w,26) * scale_x);
        //boundbxs->bbs[counter].h = (int)(FIX2FP(boundbxs->bbs[counter].h,26) * scale_y);
        //printf("%d\n",boundbxs->bbs[counter].x);
        //printf("%f\n",scale_x);
        //printf("%d\n\n",(int32_t)( (float)FIX2FP(boundbxs->bbs[counter].x,20) * scale_x));
        

        boundbxs->bbs[counter].x = (int32_t)( boundbxs->bbs[counter].x * scale_x) >> 20;
        boundbxs->bbs[counter].y = (int32_t)( boundbxs->bbs[counter].y * scale_y) >> 20;
        boundbxs->bbs[counter].w = (int32_t)( boundbxs->bbs[counter].w * scale_x) >> 20;
        boundbxs->bbs[counter].h = (int32_t)( boundbxs->bbs[counter].h * scale_y) >> 20;
        //int32_t tw = boundbxs->bbs[counter].x + boundbxs->bbs[counter].w;
        //int32_t th = boundbxs->bbs[counter].y + boundbxs->bbs[counter].h;
        //When coming out of SSD x and y are center coords so we neet to subtract half the of window
        //
        boundbxs->bbs[counter].x = boundbxs->bbs[counter].x - (boundbxs->bbs[counter].w/2);
        boundbxs->bbs[counter].y = boundbxs->bbs[counter].y - (boundbxs->bbs[counter].h/2);
        //boundbxs->bbs[counter].w = tw ;
        //boundbxs->bbs[counter].h = th ;
        
        
    }
}


void printBboxes(bboxs_t *boundbxs){
    PRINTF("\n\n======================================================");
    PRINTF("\nDetected Bounding boxes                                 ");
    PRINTF("\n======================================================\n");
    PRINTF("BoudingBox:  score     cx     cy     w     h    class");
    PRINTF("\n------------------------------------------------------\n");

    for (int counter=0;counter< boundbxs->num_bb;counter++){
        if(boundbxs->bbs[counter].alive)
            PRINTF("bbox [%02d] : %.2d     %03d    %03d     %03d    %03d     %02d\n",
                counter,
                //FIX2FP(boundbxs->bbs[counter].score,15 ),
                boundbxs->bbs[counter].score/320,
                boundbxs->bbs[counter].x,
                boundbxs->bbs[counter].y,
                boundbxs->bbs[counter].w,
                boundbxs->bbs[counter].h,
                boundbxs->bbs[counter].class);
    }//
}


void printBboxes_forPython(bboxs_t *boundbxs){
    PRINTF("\n\n======================================================");
    PRINTF("\nThis can be copy-pasted to python to draw BoudingBoxs   ");
    PRINTF("\n\n");
    //draw.rectangle((ymin, xmin, ymax, xmax), outline=(255, 255, 0))

    for (int counter=0;counter< boundbxs->num_bb;counter++){
        if(boundbxs->bbs[counter].alive)
            printf("draw.rectangle((%d,%d,%d,%d), outline=(255, 255, 0))\n",
                boundbxs->bbs[counter].y,
                boundbxs->bbs[counter].x,
                boundbxs->bbs[counter].h+boundbxs->bbs[counter].y,
                boundbxs->bbs[counter].w+boundbxs->bbs[counter].x
                
                );
    }
}

int rect_intersect_area( short a_x, short a_y, short a_w, short a_h,
                         short b_x, short b_y, short b_w, short b_h ){

    #define MIN(a,b) ((a) < (b) ? (a) : (b))
    #define MAX(a,b) ((a) > (b) ? (a) : (b))

    int x = MAX(a_x,b_x);
    int y = MAX(a_y,b_y);

    int size_x = MIN(a_x+a_w,b_x+b_w) - x;
    int size_y = MIN(a_y+a_h,b_y+b_h) - y;

    if(size_x <=0 || size_x <=0)
        return 0;
    else
        return size_x*size_y; 

    #undef MAX
    #undef MIN
}


void non_max_suppress(bboxs_t * boundbxs){

    int idx,idx_int;

    //Non-max supression
     for(idx=0;idx<boundbxs->num_bb;idx++){
        //check if rect has been removed (-1)
        if(boundbxs->bbs[idx].alive==0)
            continue;
 
        for(idx_int=0;idx_int<boundbxs->num_bb;idx_int++){

            if(boundbxs->bbs[idx_int].alive==0 || idx_int==idx)
                continue;

            //check the intersection between rects
            int intersection = rect_intersect_area(boundbxs->bbs[idx].x,boundbxs->bbs[idx].y,boundbxs->bbs[idx].w,boundbxs->bbs[idx].h,
                                                   boundbxs->bbs[idx_int].x,boundbxs->bbs[idx_int].y,boundbxs->bbs[idx_int].w,boundbxs->bbs[idx_int].h);

            if(intersection >= NON_MAX_THRES){ //is non-max
                //supress the one that has lower score that is alway the internal index, since the input is sorted
                boundbxs->bbs[idx_int].alive =0;
            }
        }
    }
}