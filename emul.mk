# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

ifndef GAP_SDK_HOME
  $(error Source sourceme in gap_sdk first)
endif

include common.mk

#IMAGE=$(CURDIR)/images/0m_1_resized.ppm
IMAGE=$(CURDIR)/images/2m_3.ppm

io=host

QUANT_BITS=8
#BUILD_DIR=BUILD
MODEL_SQ8=1

$(info Building GAP8 mode with $(QUANT_BITS) bit quantization)

NNTOOL_SCRIPT=model/nntool_script_ssdlite
MODEL_SUFFIX = _SQ8BIT
TRAINED_TFLITE_MODEL=model/ssdlite_v2_quant_ocr_nntool.tflite
#TRAINED_TFLITE_MODEL=model/china_ocr.tflite
MODEL_QUANTIZED = 1

include common/model_decl.mk

# The training of the model is slightly different depending on
# the quantization. This is because in 8 bit mode we used signed
# 8 bit so the input to the model needs to be shifted 1 bit

CC = gcc
CFLAGS += -g -m32 -O1 -D__EMUL__ -DAT_MODEL_PREFIX=$(MODEL_PREFIX) $(MODEL_SIZE_CFLAGS)
INCLUDES = -I. -I$(TILER_EMU_INC) -I$(TILER_INC) $(CNN_LIB_INCLUDE) -I$(MODEL_BUILD) -I$(MODEL_COMMON_INC)
LFLAGS =
LIBS =
SRCS = $(MODEL_PREFIX).c $(MODEL_GEN_C) $(MODEL_COMMON_SRCS) $(CNN_LIB)
$(info CNN_LIB++ $(CNN_LIB))
$(info SRCS++ $(SRCS))
BUILD_DIR = BUILD_EMUL

OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

MAIN = $(MODEL_PREFIX)_emul
# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
CLUSTER_STACK_SIZE=4096
CLUSTER_SLAVE_STACK_SIZE=1024
TOTAL_STACK_SIZE=$(shell expr $(CLUSTER_STACK_SIZE) \+ $(CLUSTER_SLAVE_STACK_SIZE) \* 7)
MODEL_L1_MEMORY=$(shell expr 60000 \- $(TOTAL_STACK_SIZE))
MODEL_L2_MEMORY=150000
MODEL_L3_MEMORY=8388608

# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
# MODEL_L1_MEMORY=52000
# MODEL_L2_MEMORY=307200
# MODEL_L3_MEMORY=8388608
# hram - HyperBus RAM
# qspiram - Quad SPI RAM
MODEL_L3_EXEC=hram
# hflash - HyperBus Flash
# qpsiflash - Quad SPI Flash
MODEL_L3_CONST=hflash

all: $(MAIN)

$(OBJS) : $(BUILD_DIR)/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< $(INCLUDES) -MD -MF $(basename $@).d -o $@

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -MMD -MP $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

clean: #clean_model
	#$(RM) -r $(BUILD_DIR)
	$(RM) $(MAIN)

.PHONY: depend clean

#include common/model_rules.mk
