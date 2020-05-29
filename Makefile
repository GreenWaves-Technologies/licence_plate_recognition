# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

ifndef GAP_SDK_HOME
  $(error Source sourceme in gap_sdk first)
endif

include common.mk

IMAGE=$(CURDIR)/images/0m_1_resized.ppm
#IMAGE=$(CURDIR)/images/2m_3.ppm

io=host

QUANT_BITS=8
#BUILD_DIR=BUILD
MODEL_SQ8=1

$(info Building GAP8 mode with $(QUANT_BITS) bit quantization)

ifeq ($(MODEL),1)
	NNTOOL_SCRIPT=model/nntool_script
	TRAINED_TFLITE_MODEL=model/lprnet.tflite
else
	NNTOOL_SCRIPT=model/nntool_script_ssdlite
	TRAINED_TFLITE_MODEL=model/ssdlite_v2_quant_ocr_nntool.tflite
endif

MODEL_SUFFIX = _SQ8BIT_EMUL
MODEL_QUANTIZED = 1

include common/model_decl.mk

MODEL_GENFLAGS_EXTRA+=
# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
CLUSTER_STACK_SIZE=4096
CLUSTER_SLAVE_STACK_SIZE=1024
TOTAL_STACK_SIZE=$(shell expr $(CLUSTER_STACK_SIZE) \+ $(CLUSTER_SLAVE_STACK_SIZE) \* 7)
MODEL_L1_MEMORY=$(shell expr 60000 \- $(TOTAL_STACK_SIZE))
MODEL_L2_MEMORY=300000
MODEL_L3_MEMORY=8388608
# hram - HyperBus RAM
# qspiram - Quad SPI RAM
MODEL_L3_EXEC=hram
# hflash - HyperBus Flash
# qpsiflash - Quad SPI Flash
MODEL_L3_CONST=hflash

APP = OCRssd
APP_SRCS += $(MODEL_PREFIX).c $(MODEL_GEN_C) $(MODEL_COMMON_SRCS) $(CNN_LIB)
#APP_SRCS += SSDKernels.c SSD/SSDBasicKernels.c $(MODEL_BUILD)/SSDParams.c SSD/BB_utils.c

APP_CFLAGS += -O3
APP_CFLAGS += -I. -I$(MODEL_COMMON_INC) -I$(TILER_EMU_INC) -I$(TILER_INC) $(CNN_LIB_INCLUDE) -I$(MODEL_BUILD) -ISSD/
APP_CFLAGS += -DPERF -DAT_MODEL_PREFIX=$(MODEL_PREFIX) $(MODEL_SIZE_CFLAGS)
APP_CFLAGS += -DSTACK_SIZE=$(CLUSTER_STACK_SIZE) -DSLAVE_STACK_SIZE=$(CLUSTER_SLAVE_STACK_SIZE)
APP_CFLAGS += -DAT_IMAGE=$(IMAGE)
APP_LDFLAGS += -lm

READFS_FILES=$(abspath $(MODEL_TENSORS))
PLPBRIDGE_FLAGS += -f

#SSD

SSD_MODEL_FILE = SSD/SSDModel.c
SSD_MODEL_GEN  = SSDKernels
SSD_MODEL_GEN_C = $(addsuffix .c, $(SSD_MODEL_GEN))
SSD_MODEL_GEN_CLEAN = $(SSD_MODEL_GEN_C) $(addsuffix .h, $(SSD_MODEL_GEN))



GenSSDTile: $(SSD_MODEL_FILE)
	gcc -g -o GenSSDTile -I"$(TILER_INC)" $(SSD_MODEL_FILE) $(TILER_LIB)

$(SSD_MODEL_GEN_C): GenSSDTile
	./GenSSDTile

SSD_model: $(SSD_MODEL_GEN_C)
	cd SSDParamsGenerator && $(MAKE) all run

clean_ssd:
	cd SSDParamsGenerator && $(MAKE) clean
	rm -rf $(MODEL_BUILD)/SSDParams.c $(MODEL_BUILD)/SSDParams.h
	rm -rf GenSSDTile $(SSD_MODEL_GEN_CLEAN)



# all depends on the model
all:: model #SSD_model

clean:: clean_model #clean_ssd

include common/model_rules.mk
$(info APP_SRCS... $(APP_SRCS))
$(info APP_CFLAGS... $(APP_CFLAGS))
include $(RULES_DIR)/pmsis_rules.mk

