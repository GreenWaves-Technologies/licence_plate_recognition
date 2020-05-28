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

#include <stdint.h>
#include <stdio.h>

// AutoTiler Libraries
#include "AutoTilerLib.h"

void LoadLibKernels(){

    LibKernel("KerPredecoderInt8", CALL_PARALLEL,
        CArgs(13,
            TCArg("int8_t * __restrict__", "Classes"),
            TCArg("int8_t * __restrict__", "Boxes"),
            TCArg("unsigned int ",         "Classes_W"),
            TCArg("unsigned int ",         "Classes_H"),
            TCArg("unsigned int ",         "Classes_TileIndex"),
            TCArg("unsigned int ",         "Classes_Std_H"),
            TCArg("void  * __restrict__",  "Ancor_layer"),
            TCArg("void  * __restrict__",  "BoundingBoxes"),
            TCArg("uint8_t",               "Classes_QScale"),
            TCArg("uint8_t",               "Classes_QNorm"),
            TCArg("uint8_t",               "Boxes_QScale"),
            TCArg("uint8_t",               "Boxes_QNorm"),
            TCArg("uint16_t ",             "n_classes")
            ),
        "KerPredecoderInt8_ArgT", NULL
    );

}

void GeneratePredecoder(char *Name, int FeaturesMap_W, int FeaturesMap_H, int n_ancors, int offsets,int classes)
{   

    //Classes can be used to contrain the input
    UserKernel(Name,
        KernelIterSpace(1, IterTiledSpace(KER_ITER_TILE0)),
        TILE_HOR,
        CArgs(8, 
            TCArg("int8_t *", "Classes"), 
            TCArg("int8_t *", "Boxes"),
            TCArg("void  *",  "Ancor_layer"),
            TCArg("void  *",  "BoundingBoxes"),
            TCArg("uint8_t",  "Classes_QScale"),
            TCArg("uint8_t",  "Classes_QNorm"),
            TCArg("uint8_t",  "Boxes_QScale"),
            TCArg("uint8_t",  "Boxes_QNorm")
        ),

        Calls(1, Call("KerPredecoderInt8", LOC_LOOP,
            Bindings(13, K_Arg("Classes",    KER_ARG_TILE),
                        K_Arg("Boxes",      KER_ARG_TILE),
                        K_Arg("Classes",    KER_ARG_TILE_W),
                        K_Arg("Classes",    KER_ARG_TILE_H),
                        K_Arg("Classes",    KER_ARG_TILEINDEX),
                        K_Arg("Classes",    KER_ARG_TILE_H0),
                        C_Arg("Ancor_layer"),
                        C_Arg("BoundingBoxes"),
                        C_Arg("Classes_QScale"),
                        C_Arg("Classes_QNorm"),
                        C_Arg("Boxes_QScale"),
                        C_Arg("Boxes_QNorm"),
                        Imm(classes)
                        ))),
        KerArgs(2,
            KerArg("Classes", KerArgSpace(1,KER_ITER_TILE0), OBJ_IN_DB, classes, FeaturesMap_W * FeaturesMap_H*n_ancors, sizeof(int8_t), 0, 0, 0, "Classes"),
            KerArg("Boxes",   KerArgSpace(1,KER_ITER_TILE0), OBJ_IN_DB, offsets, FeaturesMap_W * FeaturesMap_H*n_ancors, sizeof(int8_t), 0, 0, 0, "Boxes")
        )
    );
}




int main(int argc, char **argv)
{
    // This will parse AutoTiler options and perform various initializations
    if (TilerParseOptions(argc, argv))
    {
        printf("Failed to initialize or incorrect output arguments directory.\n");
        return 1;
    }

    int n_classes = 2;
    int n_offsets = 4;
    int n_anchors = 6;

    int L1Memory = 38000;
    int L2Memory = 100000;
    int L3Memory = 1024*1024*4;


    SetInlineMode(ALWAYS_INLINE);
    SetSymbolNames("SSDKernels_L1_Memory", "SSDKernels_L2_Memory");
    SetSymbolDynamics();
    SetKernelOpts(KER_OPT_NONE, KER_OPT_BUFFER_PROMOTE);

    SetUsedFilesNames(0, 1, "SSDBasicKernels.h");

    SetGeneratedFilesNames("SSDKernels.c", "SSDKernels.h");
    
    SetL1MemorySize(L1Memory);
    SetMemorySizes(L1Memory,L2Memory,L3Memory);

    SetMemoryDeviceInfos(3,
        AT_MEM_L1, L1Memory, "SSDKernels_L2_Memory", 0, 0,
        AT_MEM_L2, L2Memory, "SSDKernels_L2_Memory", 0, 0,
        AT_MEM_L3_HRAM, L3Memory, "SSDKernels_L3_Memory", 0, 1
    );
    LoadLibKernels();


    GeneratePredecoder("Predecoder_1", 20, 15, n_anchors, n_offsets, n_classes);
    GeneratePredecoder("Predecoder_2", 20, 15, n_anchors, n_offsets, n_classes);
    GeneratePredecoder("Predecoder_3", 20, 15, n_anchors, n_offsets, n_classes);
    GeneratePredecoder("Predecoder_4", 20, 15, n_anchors, n_offsets, n_classes);

    // Now that we are done with model parsing we generate the code
    GenerateTilingCode();
    return 0;
}
