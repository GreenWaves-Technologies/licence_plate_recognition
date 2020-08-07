# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

ifndef GAP_SDK_HOME
  $(error Source sourceme in gap_sdk first)
endif

include common.mk

io=host

MEAS?=0
QUANT_BITS=8
#BUILD_DIR=BUILD
MODEL_SQ8=1

$(info Building $(TARGET_CHIP_FAMILY) mode with $(QUANT_BITS) bit quantization)

ifeq ($(MODEL),1)
	NNTOOL_SCRIPT=model/nntool_script_lprnet
	TRAINED_TFLITE_MODEL=model/$(MODEL_PREFIX).tflite
	MODEL_SUFFIX = _SQ8BIT_LPRNET
	IMAGE=$(CURDIR)/images/0m_1_resized.ppm
else
	NNTOOL_SCRIPT=model/nntool_script_ssdlite
	TRAINED_TFLITE_MODEL=model/$(MODEL_PREFIX).tflite
	MODEL_SUFFIX = _SQ8BIT_SSD
	IMAGE=$(CURDIR)/images/2m_3.ppm
endif

MODEL_QUANTIZED = 1

include common/model_decl.mk

MODEL_GENFLAGS_EXTRA+=
# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
CLUSTER_STACK_SIZE=4096
CLUSTER_SLAVE_STACK_SIZE=1024
TOTAL_STACK_SIZE=$(shell expr $(CLUSTER_STACK_SIZE) \+ $(CLUSTER_SLAVE_STACK_SIZE) \* 7)
ifeq '$(TARGET_CHIP_FAMILY)' 'GAP9'
	FREQ_CL?=50
	FREQ_FC?=50
	MODEL_L1_MEMORY=$(shell expr 125000 \- $(TOTAL_STACK_SIZE))
	MODEL_L2_MEMORY=1300000
	MODEL_L3_MEMORY=8388608
else
	ifeq '$(TARGET_CHIP)' 'GAP8_V3'
		FREQ_CL?=175
	else
		FREQ_CL?=50
	endif
	FREQ_FC?=250
	MODEL_L1_MEMORY=$(shell expr 60000 \- $(TOTAL_STACK_SIZE))
	MODEL_L2_MEMORY=300000
	MODEL_L3_MEMORY=8388608
endif
# hram - HyperBus RAM
# qspiram - Quad SPI RAM
MODEL_L3_EXEC=hram
# hflash - HyperBus Flash
# qpsiflash - Quad SPI Flash
MODEL_L3_CONST=hflash

TEST?=0
ifeq ($(TEST),1)
	MAIN=test_ssd.c
else
	MAIN=$(MODEL_PREFIX).c
endif

APP = OCRssd
APP_SRCS += $(MAIN) $(MODEL_GEN_C) $(MODEL_COMMON_SRCS) $(CNN_LIB)

APP_CFLAGS += -O3
APP_CFLAGS += -I. -I$(MODEL_COMMON_INC) -I$(TILER_EMU_INC) -I$(TILER_INC) $(CNN_LIB_INCLUDE) -I$(MODEL_BUILD)
APP_CFLAGS += -DAT_MODEL_PREFIX=$(MODEL_PREFIX) $(MODEL_SIZE_CFLAGS)
ifeq ($(MEAS),1)
	APP_CFLAGS += -DSILENT -DMEASUREMENTS
else
	APP_CFLAGS += -DPERF
endif
APP_CFLAGS += -DSTACK_SIZE=$(CLUSTER_STACK_SIZE) -DSLAVE_STACK_SIZE=$(CLUSTER_SLAVE_STACK_SIZE)
APP_CFLAGS += -DAT_IMAGE=$(IMAGE) -DFREQ_CL=$(FREQ_CL) -DFREQ_FC=$(FREQ_FC)

READFS_FILES=$(abspath $(MODEL_TENSORS))
PLPBRIDGE_FLAGS += -f

# all depends on the model
all:: model

clean:: #clean_model

include common/model_rules.mk
$(info APP_SRCS... $(APP_SRCS))
$(info APP_CFLAGS... $(APP_CFLAGS))
include $(RULES_DIR)/pmsis_rules.mk

