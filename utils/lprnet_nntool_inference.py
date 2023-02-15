from nntool.api import NNGraph
from PIL import Image
import numpy as np

NE16 = False

G = NNGraph.load_graph("model/lprnet.tflite", load_quantization=True)
G.adjust_order()
G.fusions("scaled_match_group")

G.quantize(
	None,
	graph_options={
        "use_ne16": NE16
	}
)
print(G.show())

img_file = "images/china_1_cropped.ppm"
img_in = Image.open(img_file)
print('input image: ', img_file)
img_in = img_in.convert('RGB')
input_data = (np.array(img_in).astype(np.float32) / 128) - 1.0
if not NE16:
	input_data = input_data.transpose((2,0,1))

out = G.execute([input_data], dequantize=True)
if NE16:
	out_tensor = out[-1][0][0]
	out_char_codes = np.argmax(out_tensor, axis=1)
else:
	out_tensor = out[-1][0][:,0,:]
	out_char_codes = np.argmax(out_tensor, axis=0)
print(out_tensor.shape)


char_dict = {'0': 0, '1': 1, '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7, '8': 8, '9': 9, '<Anhui>': 10, '<Beijing>': 11, '<Chongqing>': 12, '<Fujian>': 13, '<Gansu>': 14, '<Guangdong>': 15, '<Guangxi>': 16, '<Guizhou>': 17, '<Hainan>': 18, '<Hebei>': 19, '<Heilongjiang>': 20, '<Henan>': 21, '<HongKong>': 22, '<Hubei>': 23, '<Hunan>': 24, '<InnerMongolia>': 25, '<Jiangsu>': 26, '<Jiangxi>': 27, '<Jilin>': 28, '<Liaoning>': 29, '<Macau>': 30, '<Ningxia>': 31, '<Qinghai>': 32, '<Shaanxi>': 33, '<Shandong>': 34, '<Shanghai>': 35, '<Shanxi>': 36, '<Sichuan>': 37, '<Tianjin>': 38, '<Tibet>': 39, '<Xinjiang>': 40, '<Yunnan>': 41, '<Zhejiang>': 42, '<police>': 43, 'A': 44, 'B': 45, 'C': 46, 'D': 47, 'E': 48, 'F': 49, 'G': 50, 'H': 51, 'I': 52, 'J': 53, 'K': 54, 'L': 55, 'M': 56, 'N': 57, 'O': 58, 'P': 59, 'Q': 60, 'R': 61, 'S': 62, 'T': 63, 'U': 64, 'V': 65, 'W': 66, 'X': 67, 'Y': 68, 'Z': 69, '_': 70}

out_char = []
prev_char = 70
for i, char_code in enumerate(out_char_codes):
    if char_code == 70 or char_code == prev_char:
        continue
    prev_char = char_code
    for k, v in char_dict.items():
        if char_code == v:
            out_char.append(k)
            continue
#print(len(out_char_codes))
print(out_char)

img_in.show()