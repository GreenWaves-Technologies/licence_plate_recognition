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
    set(LPR_KERNEL_C                )
elseif(${CONFIG_LPR_ONLY})
    set(MAIN                lprnet.c)
    set(IMAGE               ${CMAKE_CURRENT_SOURCE_DIR}/images/china_1_cropped.ppm)
    set(SSD_KERNEL_C                )
else()
    set(MAIN                main.c)
    set(IMAGE               ${CMAKE_CURRENT_SOURCE_DIR}/images/china_1.ppm)
endif()
if (CONFIG_MODEL_NE16)
    set(SSD_NNTOOL_SCRIPT     model/nntool_script_ssdlite_ne16)
    set(LPR_NNTOOL_SCRIPT     model/nntool_script_lprnet_ne16)
    list(APPEND TARGET_PREPROCESSOR -DNE16)
else()
    set(SSD_NNTOOL_SCRIPT     model/nntool_script_ssdlite)
    set(LPR_NNTOOL_SCRIPT     model/nntool_script_lprnet)
endif()

set(SSD_MODEL_PATH        model/ssdlite_ocr.tflite)
autotiler_define_gen_names("ssdlite_ocr")
nntool_setup("ssdlite_ocr" ${SSD_MODEL_PATH} ${SSD_NNTOOL_SCRIPT} ${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_SSD)
set(AT_SSD_MODEL_PATH     ${AT_MODEL_PATH})
set(AT_SSD_TENSORS_DIR    ${AT_TENSORS_DIR})
autotiler_setup("ssdlite_ocr" ${AT_SSD_MODEL_PATH} ${AT_SSD_TENSORS_DIR})
set(SSD_KERNEL_C          ${COMMON_MODEL_BUILDDIR}/${KERNEL_C})

set(LPR_MODEL_PATH        model/lprnet.tflite)
autotiler_define_gen_names("lprnet")
nntool_setup("lprnet"     ${LPR_MODEL_PATH} ${LPR_NNTOOL_SCRIPT} ${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_LPR)
set(AT_LPR_MODEL_PATH     ${AT_MODEL_PATH})
set(AT_LPR_TENSORS_DIR    ${AT_TENSORS_DIR})
autotiler_setup("lprnet"  ${AT_LPR_MODEL_PATH} ${AT_LPR_TENSORS_DIR})
set(LPR_KERNEL_C          ${COMMON_MODEL_BUILDDIR}/${KERNEL_C})

message(STATUS "${APP_MSG_PREFIX} Main file: ${MAIN}")
###############################################################################
# Application dependencies
###############################################################################

list(APPEND TARGET_SRCS ${MAIN}
                        ${CNN_LIB}
                        ${SSD_KERNEL_C}
                        ${LPR_KERNEL_C}
)

list(APPEND TARGET_INCS -I${TILER_INC}
                        -I${TILER_EMU_INC}
                        -I${GAP_SDK_HOME}/libs/gap_lib/include
                          ${CNN_LIB_INCLUDE}
                        -I${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_SSD
                        -I${CMAKE_CURRENT_BINARY_DIR}/BUILD_MODEL_LPR
                        -I${CMAKE_CURRENT_SOURCE_DIR}
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

