# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

MODEL?=0

MODEL_PREFIX_LPR=lprnet
MODEL_PREFIX_SSD=ssdlite_ocr

AT_INPUT_WIDTH_LPR=94
AT_INPUT_HEIGHT_LPR=24
AT_INPUT_COLORS_LPR=3

AT_INPUT_WIDTH_SSD=320
AT_INPUT_HEIGHT_SSD=240
AT_INPUT_COLORS_SSD=3

AT_SIZES_SSD += -DAT_INPUT_HEIGHT_SSD=$(AT_INPUT_HEIGHT_SSD) -DAT_INPUT_WIDTH_SSD=$(AT_INPUT_WIDTH_SSD) -DAT_INPUT_COLORS_SSD=$(AT_INPUT_COLORS_SSD)
AT_SIZES_LPR += -DAT_INPUT_HEIGHT_LPR=$(AT_INPUT_HEIGHT_LPR) -DAT_INPUT_WIDTH_LPR=$(AT_INPUT_WIDTH_LPR) -DAT_INPUT_COLORS_LPR=$(AT_INPUT_COLORS_LPR)
