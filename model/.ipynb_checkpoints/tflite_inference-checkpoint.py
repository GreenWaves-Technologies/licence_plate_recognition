import numpy as np
import tensorflow as tf
import json
from PIL import Image
print(tf.__version__)

image_file = "/home/marco-gwt/GWT/OCR/model/lp_recognition/LPRNet_china_no_tile_quantized/0m_1_resized.png"
image_grayscale = False
input_grayscale = False

# Load TFLite model and allocate tensors.
model_path = "/home/marco-gwt/GWT/OCR/model/lp_recognition/LPRNet_china_no_tile_quantized/china_quant_ocr.tflite"
interpreter = tf.lite.Interpreter(model_path=model_path)
interpreter.allocate_tensors()

# Get input and output tensors.
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

print("Input details: ", input_details)
print("Output details: ", output_details)

img_in = Image.open(image_file)
if input_grayscale and not image_grayscale:
    img_in = img_in.convert('L')
#img_in = img_in.resize((input_details[0]['shape'][1], input_details[0]['shape'][2]))
input_array = np.array(img_in, dtype=np.uint8)
input_array = np.reshape(input_array, input_details[0]['shape'])
interpreter.set_tensor(input_details[0]['index'], input_array)
interpreter.invoke()

tens_out = {'input': input_array}
for i in range(len(output_details)):
    tens_out.update({output_details[i]['name']: interpreter.get_tensor(output_details[i]['index'])})

print(tens_out)
json_dict = {}
for k, v in tens_out.items():
    json_dict.update({k: v.tolist()})
with open('tensors_out.json', 'w') as f:
    json.dump(json_dict, f)

