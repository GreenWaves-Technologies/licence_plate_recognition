# Copyright (C) 2017 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

MODEL_SUFFIX?=

MODEL_PYTHON=python3

MODEL_COMMON ?= common
MODEL_COMMON_INC ?= $(GAP_SDK_HOME)/libs/gap_lib/include
MODEL_COMMON_SRC ?= $(GAP_SDK_HOME)/libs/gap_lib/img_io
MODEL_COMMON_SRC_FILES ?= ImgIO.c
MODEL_COMMON_SRCS = $(realpath $(addprefix $(MODEL_COMMON_SRC)/,$(MODEL_COMMON_SRC_FILES)))
MODEL_BUILD = BUILD_MODEL$(MODEL_SUFFIX)

MODEL_TFLITE = $(MODEL_BUILD)/$(MODEL_PREFIX).tflite

TENSORS_DIR = $(MODEL_BUILD)/tensors
MODEL_TENSORS = $(MODEL_BUILD)/$(MODEL_PREFIX)_L3_Flash_Const.dat

MODEL_STATE = $(MODEL_BUILD)/$(MODEL_PREFIX).json
MODEL_SRC = $(MODEL_PREFIX)Model.c
MODEL_HEADER = $(MODEL_PREFIX)Info.h
MODEL_GEN = $(MODEL_BUILD)/$(MODEL_PREFIX)Kernels 
MODEL_GEN_C = $(addsuffix .c, $(MODEL_GEN))
MODEL_GEN_CLEAN = $(MODEL_GEN_C) $(addsuffix .h, $(MODEL_GEN))
MODEL_GEN_EXE = $(MODEL_BUILD)/GenTile

ifdef MODEL_QUANTIZED
  NNTOOL_EXTRA_FLAGS = -q
endif

MODEL_GENFLAGS_EXTRA =

EXTRA_GENERATOR_SRC =

IMAGES = images
RM=rm -f

NNTOOL=nntool

MODEL_SIZE_CFLAGS = -DAT_INPUT_HEIGHT=$(AT_INPUT_HEIGHT) -DAT_INPUT_WIDTH=$(AT_INPUT_WIDTH) -DAT_INPUT_COLORS=$(AT_INPUT_COLORS)
include $(RULES_DIR)/at_common_decl.mk
