import numpy as np
import tensorflow as tf
from PIL import Image, ImageDraw, ImageFont
from glob import glob
import os


TEST_IMG='images/0m_3.png'
OUTPUT_FILE='inference_results.png'
GRAPH_FILE='model/ssdlite_v2_quant_ocr.tflite'
INFERENCE_THRESHOLD=0.5

def draw_image(image, results, size, labels=None):
    out_images = []
    result_size = len(results)
    for idx, obj in enumerate(results):
        print(obj)
        # Prepare image for drawing
        draw = ImageDraw.Draw(image)

        # Prepare boundary box
        xmin, ymin, xmax, ymax = obj['bounding_box']
        xmin = int(xmin * size[1])
        xmax = int(xmax * size[1])
        ymin = int(ymin * size[0])
        ymax = int(ymax * size[0])
        print("xmin: "+str(xmin))
        print("ymin: "+str(ymin))
        print("xmax: "+str(xmax))
        print("ymax: "+str(ymax))


        # Draw rectangle to desired thickness
        for x in range( 0, 4 ):
            draw.rectangle((ymin, xmin, ymax, xmax), outline=(255, 255, 0))

        # Annotate image with label and confidence score
        if labels is not None:
            display_str = labels[obj['class_id']] + ": " + str(round(obj['score']*100, 2)) + "%"
        else:
            display_str = str(obj['class_id']) + ": " + str(round(obj['score']*100, 2)) + "%"
        draw.text((obj['bounding_box'][0], obj['bounding_box'][1]), display_str)

        displayImage = np.asarray( image )
        out_images.append(image)
    #image.show()
    return image

def set_input_tensor(interpreter, image):
    """Sets the input tensor."""
    tensor_index = interpreter.get_input_details()[0]['index']
    input_tensor = interpreter.tensor(tensor_index)()[0]
    input_tensor[:, :] = image


def get_output_tensor(interpreter, index):
    """Returns the output tensor at the given index."""
    output_details = interpreter.get_output_details()[index]
    tensor = np.squeeze(interpreter.get_tensor(output_details['index']))
    return tensor

def detect_objects(interpreter, image, threshold):
    """Returns a list of detection results, each a dictionary of object info."""
    set_input_tensor(interpreter, image)
    interpreter.invoke()

    # Get all °output details
    boxes = get_output_tensor(interpreter, 0)
    classes = get_output_tensor(interpreter, 1)
    scores  = get_output_tensor(interpreter, 2)
    count   = int(get_output_tensor(interpreter, 3))

    results = []
    for i in range(count):
        if scores[i] >= threshold:
            result = {
                'bounding_box': boxes[i],
                'class_id': classes[i],
                'score': scores[i]
            }
            results.append(result)
    return results


print(tf.__version__)
# Load TFLite model and allocate tensors.
model_path=GRAPH_FILE

interpreter = tf.lite.Interpreter(model_path=model_path)
interpreter.allocate_tensors()
_, input_height, input_width, _ = interpreter.get_input_details()[0]['shape']

# Get input and output tensors.
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

file = TEST_IMG
print(file)
img_in = Image.open(file).convert('RGB')
img_in = img_in.resize((input_details[0]['shape'][2], input_details[0]['shape'][1]))
input_array = np.array(img_in, dtype=np.uint8)
input_array = np.reshape(input_array, input_details[0]['shape']).astype(np.uint8)

#for float inteference
#input_array = np.array(img_in, dtype=np.float)
#input_array = np.reshape(input_array, input_details[0]['shape']).astype(np.float)
#input_array = input_array*0.007843137718737125
#input_array = input_array-1
results = detect_objects(interpreter, input_array, 0.5)


out_images = draw_image(img_in, results, img_in.size)

out_images.save(OUTPUT_FILE)

print("Output image save in %s" % OUTPUT_FILE)