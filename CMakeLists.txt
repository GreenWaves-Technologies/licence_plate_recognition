# Copyright (c) 2022 GreenWaves Technologies SAS
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of GreenWaves Technologies SAS nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.16)

###############################################################################
# Application related definitions
###############################################################################

set(TARGET_NAME         "licence_plate")

###############################################################################
# CMake pre initialization
###############################################################################

include($ENV{GAP_SDK_HOME}/utils/cmake/setup.cmake)
set(APP_MSG_PREFIX "[${BoldRed}${TARGET_NAME}${ColourReset}]")

###############################################################################
# Application's
###############################################################################

set(AT_INPUT_WIDTH_LPR      94)
set(AT_INPUT_HEIGHT_LPR     24)
set(AT_INPUT_COLORS_LPR     3)

set(AT_INPUT_WIDTH_SSD      320)
set(AT_INPUT_HEIGHT_SSD     240)
set(AT_INPUT_COLORS_SSD     3)




if (${CONFIG_SSD_ONLY})
    set(MAIN                ssdlite_ocr.c)
    set(IMAGE               ${CMAKE_CURRENT_SOURCE_DIR}/images/china_1.ppm)
    set(BUILD_SSD_MODEL YES)
    set(LPR_KERNEL_C          )
elseif(${CONFIG_LPR_ONLY})
    set(MAIN                lprnet.c)
    set(IMAGE               ${CMAKE_CURRENT_SOURCE_DIR}/images/china_1_cropped.ppm)
    set(BUILD_LPR_MODEL YES)
    set(SSDLITE_KERNEL_C      )
else()
    set(MAIN                main.c)
    set(IMAGE               ${CMAKE_CURRENT_SOURCE_DIR}/images/china_1.ppm)
    set(BUILD_SSD_MODEL YES)
    set(BUILD_LPR_MODEL YES)
endif()
if (CONFIG_MODEL_NE16)
    set(USE_NE16 True)
    list(APPEND TARGET_PREPROCESSOR -DNE16)
endif()

# set(SSD_MODEL_PATH        model/ssdlite_ocr.tflite)
# autotiler_define_gen_names("ssdlite_ocr")
# nntool_setup("ssdlite_ocr" ${SSD_MODEL_PATH} ${SSD_NNTOOL_SCRIPT} ${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_SSD)
# set(AT_SSD_MODEL_PATH     ${AT_MODEL_PATH})
# set(AT_SSD_TENSORS_DIR    ${AT_TENSORS_DIR})
# autotiler_setup("ssdlite_ocr" ${AT_SSD_MODEL_PATH} ${AT_SSD_TENSORS_DIR})
# set(SSD_KERNEL_C          ${COMMON_MODEL_BUILDDIR}/${KERNEL_C})

# set(LPR_MODEL_PATH        model/lprnet.tflite)
# autotiler_define_gen_names("lprnet")
# nntool_setup("lprnet"     ${LPR_MODEL_PATH} ${LPR_NNTOOL_SCRIPT} ${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_LPR)
# set(AT_LPR_MODEL_PATH     ${AT_MODEL_PATH})
# set(AT_LPR_TENSORS_DIR    ${AT_TENSORS_DIR})
# autotiler_setup("lprnet"  ${AT_LPR_MODEL_PATH} ${AT_LPR_TENSORS_DIR})
# set(LPR_KERNEL_C          ${COMMON_MODEL_BUILDDIR}/${KERNEL_C})


if(${BUILD_LPR_MODEL})
    message(STATUS "${APP_MSG_PREFIX} GENERATING LPR CODE")

    set(BUILD_MODEL_LPR BUILD_MODEL_LPR)

    set(LPR_TARGET_NAME       lprnet)
    set(LPR_AT_MODEL_DIR      ${CMAKE_CURRENT_BINARY_DIR}/${BUILD_MODEL_LPR})
    set(LPR_MODEL_EXPRESSIONS     ${LPR_AT_MODEL_DIR}/${LPR_TARGET_NAME}Expression_Kernels.c)

    set(LPR_AT_BIN            ${LPR_TARGET_NAME}GenTile)
    set(LPR_AT_BIN_PATH       ${LPR_AT_MODEL_DIR}/${LPR_TARGET_NAME}GenTile)
    set(LPR_AT_MODEL_PATH     ${LPR_AT_MODEL_DIR}/${LPR_TARGET_NAME}model.c)
    set(LPR_AT_TENSORS_DIR    ${LPR_AT_MODEL_DIR}/tensors)
    set(LPR_NNTOOL_PY         ${CMAKE_CURRENT_SOURCE_DIR}/model/gen_lpr_at_model.py)
    set(LPR_NNTOOL_MODEL      lprnet.tflite)
    set(LPR_NNTOOL_MODEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/model/${LPR_NNTOOL_MODEL})
    set(LPR_AT_MODEL_NAME     ${LPR_TARGET_NAME}model.c)
    set(LPR_KERNEL_FILE_C     ${LPR_TARGET_NAME}Kernels.c)
    set(LPR_KERNEL_C          ${LPR_AT_MODEL_DIR}/${LPR_KERNEL_FILE_C})

    autotiler_add_model(
        TARGET_NAME ${LPR_TARGET_NAME}
        MODEL_PATH ${LPR_AT_MODEL_PATH}
        MODEL_TENSORS_DIR ${LPR_AT_TENSORS_DIR}
        MODEL_BUILDDIR ${BUILD_MODEL_LPR}
        GEN_NAME lpr
        PRI1_USED TRUE
        PRI1_FLASH flash
        L1_MEMORY 128000 #${CONFIG_MODEL_L1_MEMORY}
        L2_MEMORY 1200000 #${CONFIG_MODEL_L2_MEMORY}
        #L3_MEMORY ${FACEID_L3_SIZE}
        #L2STATIC_MEMORY ${CONFIG_MODEL_L2STATIC_MEMORY}
        #L3STATIC_MEMORY ${CONFIG_MODEL_L3STATIC_MEMORY}
    )
    

    add_custom_command(
        OUTPUT ${LPR_AT_MODEL_PATH} #${LPR_MODEL_EXPRESSIONS}
        COMMENT "Generate the autotiler model: ${LPR_NNTOOL_MODEL}"
        DEPENDS ${LPR_NNTOOL_PY}
        COMMAND python3 ${LPR_NNTOOL_PY}    --model_path=${LPR_NNTOOL_MODEL_PATH}
                                            --tensors_dir=${LPR_AT_TENSORS_DIR}
                                            --gen_model_path=${LPR_AT_MODEL_DIR}
                                            --model_file=${LPR_AT_MODEL_NAME}
                                            --gen_name_suffix=${LPR_TARGET_NAME}
                                            --NE16=${USE_NE16}
    )
endif()


if(${BUILD_SSD_MODEL})
    message(STATUS "${APP_MSG_PREFIX} GENERATING SSD LITE CODE")

    set(BUILD_MODEL_SSDLITE BUILD_MODEL_SSD)

    set(SSDLITE_TARGET_NAME       ssdlite_ocr)
    set(SSDLITE_AT_MODEL_DIR      ${CMAKE_BINARY_DIR}/${BUILD_MODEL_SSDLITE})
    set(SSDLITE_MODEL_EXPRESSIONS ${SSDLITE_AT_MODEL_DIR}/${SSDLITE_TARGET_NAME}Expression_Kernels.c)

    set(SSDLITE_AT_BIN            ${SSDLITE_TARGET_NAME}GenTile)
    set(SSDLITE_AT_BIN_PATH       ${SSDLITE_AT_MODEL_DIR}/${SSDLITE_TARGET_NAME}GenTile)
    set(SSDLITE_AT_MODEL_PATH     ${SSDLITE_AT_MODEL_DIR}/${SSDLITE_TARGET_NAME}Model.c)
    #set(SSDLITE_AT_MODEL_PATH     ${SSDLITE_TARGET_NAME}Model.c)
    set(SSDLITE_AT_TENSORS_DIR    ${SSDLITE_AT_MODEL_DIR}/tensors)
    set(SSDLITE_NNTOOL_PY         ${CMAKE_SOURCE_DIR}/model/gen_ssdlite_at_model.py)
    set(SSDLITE_NNTOOL_MODEL      ssdlite_ocr.tflite)
    set(SSDLITE_NNTOOL_MODEL_PATH ${CMAKE_SOURCE_DIR}/model/${SSDLITE_NNTOOL_MODEL})
    set(SSDLITE_AT_MODEL_NAME     ${SSDLITE_TARGET_NAME}Model.c)
    set(SSDLITE_KERNEL_NAME       ${SSDLITE_TARGET_NAME}Kernels)
    set(SSDLITE_KERNEL_FILE_C     ${SSDLITE_KERNEL_NAME}.c)
    set(SSDLITE_KERNEL_C          ${BUILD_MODEL_SSDLITE}/${SSDLITE_KERNEL_FILE_C})

    autotiler_add_model(
        TARGET_NAME ${SSDLITE_TARGET_NAME}
        MODEL_PATH  ${SSDLITE_AT_MODEL_PATH}
        MODEL_TENSORS_DIR ${SSDLITE_AT_TENSORS_DIR}
        GEN_NAME ${SSDLITE_AT_BIN}
        MODEL_BUILDDIR ${BUILD_MODEL_SSDLITE}
        PRI1_USED TRUE
        PRI1_FLASH flash
        L1_MEMORY 128000 #${CONFIG_MODEL_L1_MEMORY}
        L2_MEMORY 1200000 #${CONFIG_MODEL_L2_MEMORY}
    )
    
    add_custom_command(
        OUTPUT ${SSDLITE_AT_MODEL_PATH} ${SSDLITE_MODEL_EXPRESSIONS}
        COMMENT "Generate the autotiler model: ${SSDLITE_NNTOOL_MODEL}"
        DEPENDS ${SSDLITE_NNTOOL_PY}
        COMMAND python3 ${SSDLITE_NNTOOL_PY} --model_path=${SSDLITE_NNTOOL_MODEL_PATH}
                                             --tensors_dir=${SSDLITE_AT_TENSORS_DIR}
                                             --gen_model_path=${SSDLITE_AT_MODEL_DIR}
                                             --model_file=${SSDLITE_AT_MODEL_NAME}
                                             --gen_name_suffix=${SSDLITE_TARGET_NAME}
                                             --NE16=${USE_NE16}
    )
endif()



message(STATUS "${APP_MSG_PREFIX} Main file: ${MAIN}")
###############################################################################
# Application dependencies
###############################################################################

list(APPEND TARGET_SRCS ${MAIN}
                        ${CNN_LIB}
                        ${SSDLITE_KERNEL_C}
                        ${SSDLITE_MODEL_EXPRESSIONS}
                        ${LPR_KERNEL_C}
)

list(APPEND TARGET_INCS -I${TILER_INC}
                        -I$ENV{GAP_H_INC}
                        -I${GAP_SDK_HOME}/libs/gap_lib/include
                         ${CNN_LIB_INCLUDE}
                        -I${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_SSD
                        -I${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_LPR
                        -I${CMAKE_SOURCE_DIR}
                        -I${GAP_SDK_HOME}/utils/power_meas_utils)

list(APPEND TARGET_CFLAGS   -gdwarf-2
                            -gstrict-dwarf
                            -O3
                            -mno-memcpy
                            -fno-tree-loop-distribute-patterns)

math(EXPR FREQ_CL_MHZ "${CONFIG_FREQUENCY_CLUSTER}/1000/1000")
math(EXPR FREQ_FC_MHZ "${CONFIG_FREQUENCY_FC}/1000/1000")
math(EXPR FREQ_PE_MHZ "${CONFIG_FREQUENCY_PERIPH}/1000/1000")
list(APPEND TARGET_PREPROCESSOR -DPERF
                                -DAT_MODEL_PREFIX=${MODEL_NAME}
                                -DSTACK_SIZE=${CONFIG_CL_MASTER_CORE_STACK_SIZE}
                                -DSLAVE_STACK_SIZE=${CONFIG_CL_SLAVE_CORE_STACK_SIZE}
                                -DAT_IMAGE=${IMAGE}
                                -DFREQ_CL=${FREQ_CL_MHZ}
                                -DFREQ_FC=${FREQ_FC_MHZ}
                                -DFREQ_PE=${FREQ_PE_MHZ}
                                -DAT_CONSTRUCT=${AT_CONSTRUCT}
                                -DAT_DESTRUCT=${AT_DESTRUCT}
                                -DAT_CNN=${AT_CNN}
                                -DAT_INPUT_HEIGHT_SSD=${AT_INPUT_HEIGHT_SSD} -DAT_INPUT_WIDTH_SSD=${AT_INPUT_WIDTH_SSD} -DAT_INPUT_COLORS_SSD=${AT_INPUT_COLORS_SSD}
                                -DAT_INPUT_HEIGHT_LPR=${AT_INPUT_HEIGHT_LPR} -DAT_INPUT_WIDTH_LPR=${AT_INPUT_WIDTH_LPR} -DAT_INPUT_COLORS_LPR=${AT_INPUT_COLORS_LPR}
)
if (${CI})
message(STATUS "Compiling for CI")
list(APPEND TARGET_PREPROCESSOR -DTEST)
endif()
project(${TARGET_NAME} C ASM)
add_executable(${TARGET_NAME} ${TARGET_SRCS})

target_compile_options(${TARGET_NAME} PUBLIC    ${TARGET_CFLAGS} 
                                                ${TARGET_INCS}
                                                ${TARGET_PREPROCESSOR})


###############################################################################
# CMake post initialization
###############################################################################
setupos(${TARGET_NAME})

