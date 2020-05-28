### Copyright (C) 2020 GreenWaves Technologies
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.


import numpy as np
from PIL import Image, ImageDraw, ImageFont


#INTPUT_FILE = "test_images/test_000_0000084_resized_rgb.ppm"
INTPUT_FILE = "test_images/test_000_0000193_resized.ppm"
OUTPUT_FILE = "result.png"


img_in = Image.open(INTPUT_FILE).convert('RGB')
#img_in = img_in.resize((input_details[0]['shape'][2], input_details[0]['shape'][1]))
draw = ImageDraw.Draw(img_in)


# Copy Paste code here:
#draw.rectangle((ymin, xmin, ymax, xmax), outline=(255, 255, 0))

draw.rectangle((168,167,218,209), outline=(255, 255, 0))
draw.rectangle((69,41,117,89), outline=(255, 255, 0))
draw.rectangle((61,170,131,202), outline=(255, 255, 0))
draw.rectangle((217,141,305,178), outline=(255, 255, 0))
draw.rectangle((252,97,317,128), outline=(255, 255, 0))
draw.rectangle((251,34,315,64), outline=(255, 255, 0))
draw.rectangle((155,18,186,58), outline=(255, 255, 0))


# #boxes from tflite
# draw.rectangle((59,75,103,102), outline=(255, 255, 255))
# draw.rectangle((126,156,157,208), outline=(255, 255, 255))
# draw.rectangle((104,49,136,87), outline=(255, 255, 255))

# #All NNtool
# draw.rectangle((59,74,104,100), outline=(255, 255, 0))
# draw.rectangle((126,156,157,209), outline=(255, 255, 0))
# draw.rectangle((104,49,136,87), outline=(255, 255, 0))


#GT from tflite
#draw.rectangle((57,69,105,106), outline=(255, 0, 0))
#draw.rectangle((124,155,157,206), outline=(255, 0, 0))
#draw.rectangle((104,48,136,87), outline=(255, 0, 0))





################################

img_in.show()
img_in.save(OUTPUT_FILE)
