# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

MODEL=1

ifeq ($(MODEL),1)
	MODEL_PREFIX=lprnet
	AT_INPUT_WIDTH=94
	AT_INPUT_HEIGHT=24
	AT_INPUT_COLORS=3
else
	MODEL_PREFIX=ssdlite_v2_quant_ocr_nntool
	AT_INPUT_WIDTH=320
	AT_INPUT_HEIGHT=240
	AT_INPUT_COLORS=3
endif
