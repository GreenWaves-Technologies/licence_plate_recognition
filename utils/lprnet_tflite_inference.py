import numpy as np
import tensorflow as tf
import json
from PIL import Image
print(tf.__version__)

image_file = "/home/marco-gwt/GWT/OCR/images/0m_1_resized.ppm"
image_grayscale = False
input_grayscale = False

# Load TFLite model and allocate tensors.
model_path = "/home/marco-gwt/GWT/OCR/model/lprnet.tflite"
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
img_in = img_in.resize((input_details[0]['shape'][2], input_details[0]['shape'][1]))
input_array = np.array(img_in, dtype=np.uint8)
input_array = np.reshape(input_array, input_details[0]['shape'])
interpreter.set_tensor(input_details[0]['index'], input_array)
interpreter.invoke()

tens_out = {'input': input_array}
for i in range(len(output_details)):
    tens_out.update({output_details[i]['name']: interpreter.get_tensor(output_details[i]['index'])})

char_dict = {'0': 0, '1': 1, '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7, '8': 8, '9': 9, '<Anhui>': 10, '<Beijing>': 11, '<Chongqing>': 12, '<Fujian>': 13, '<Gansu>': 14, '<Guangdong>': 15, '<Guangxi>': 16, '<Guizhou>': 17, '<Hainan>': 18, '<Hebei>': 19, '<Heilongjiang>': 20, '<Henan>': 21, '<HongKong>': 22, '<Hubei>': 23, '<Hunan>': 24, '<InnerMongolia>': 25, '<Jiangsu>': 26, '<Jiangxi>': 27, '<Jilin>': 28, '<Liaoning>': 29, '<Macau>': 30, '<Ningxia>': 31, '<Qinghai>': 32, '<Shaanxi>': 33, '<Shandong>': 34, '<Shanghai>': 35, '<Shanxi>': 36, '<Sichuan>': 37, '<Tianjin>': 38, '<Tibet>': 39, '<Xinjiang>': 40, '<Yunnan>': 41, '<Zhejiang>': 42, '<police>': 43, 'A': 44, 'B': 45, 'C': 46, 'D': 47, 'E': 48, 'F': 49, 'G': 50, 'H': 51, 'I': 52, 'J': 53, 'K': 54, 'L': 55, 'M': 56, 'N': 57, 'O': 58, 'P': 59, 'Q': 60, 'R': 61, 'S': 62, 'T': 63, 'U': 64, 'V': 65, 'W': 66, 'X': 67, 'Y': 68, 'Z': 69, '_': 70}

out = interpreter.get_tensor(output_details[0]['index'])
print((out.astype(np.int32)-128))
print(out.shape)
out_char_codes = [np.argmax(out[0][i]) for i in range(out.shape[1])]

out_char = []
for i, char_code in enumerate(out_char_codes):
    if char_code == 70:
        continue
    for k, v in char_dict.items():
        if char_code == v:
            out_char.append(k)
            continue

print(out_char)
json_dict = {}
for k, v in tens_out.items():
    json_dict.update({k: v.tolist()})
with open('tensors_out.json', 'w') as f:
    json.dump(json_dict, f)

