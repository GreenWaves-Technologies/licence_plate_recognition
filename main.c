/*
 * Copyright (C) 2020 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#include "main.h"
#include "ssdlite_ocrKernels.h"
#include "lprnetKernels.h"
#include "ResizeBasicKernels.h"

#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/ram.h"
#include "bsp/ram/hyperram.h"
#include "bsp/buffer.h"
#include "bsp/display/ili9341.h"

#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define AT_INPUT_SIZE (AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*AT_INPUT_COLORS_SSD)
#define MAX_BB		  (300)

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX1(_L3_Flash) = 0;
AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX2(_L3_Flash) = 0;

struct pi_device HyperRam;
struct pi_device ili;
static uint32_t l3_buff;
uint8_t* img_plate_resized;
signed char* out_lpr;
static pi_buffer_t buffer;
static pi_buffer_t buffer_plate;

L2_MEM bbox_t *out_boxes;

static char *CHAR_DICT [71] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "<Anhui>", "<Beijing>", "<Chongqing>", "<Fujian>", "<Gansu>", "<Guangdong>", "<Guangxi>", "<Guizhou>", "<Hainan>", "<Hebei>", "<Heilongjiang>", "<Henan>", "<HongKong>", "<Hubei>", "<Hunan>", "<InnerMongolia>", "<Jiangsu>", "<Jiangxi>", "<Jilin>", "<Liaoning>", "<Macau>", "<Ningxia>", "<Qinghai>", "<Shaanxi>", "<Shandong>", "<Shanghai>", "<Shanxi>", "<Sichuan>", "<Tianjin>", "<Tibet>", "<Xinjiang>", "<Yunnan>", "<Zhejiang>", "<police>", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "_"};

#ifdef HAVE_LCD
static int open_display(struct pi_device *device)
{
  struct pi_ili9341_conf ili_conf;
  pi_ili9341_conf_init(&ili_conf);
  pi_open_from_conf(device, &ili_conf);
  if (pi_display_open(device))
    return -1;
  if (pi_display_ioctl(device, PI_ILI_IOCTL_ORIENTATION, (void *)PI_ILI_ORIENTATION_90))
    return -1;
  return 0;
}

void draw_text(struct pi_device *display, const char *str, unsigned posX, unsigned posY, unsigned fontsize)
{
    writeFillRect(ili, 0, 340, posX, fontsize*8, 0xFFFF);
    setCursor(ili, posX, posY);
    writeText(ili, str, fontsize);
}
#endif

static void RunSSDNetwork()
{
  PRINTF("Running SSD model on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  __PREFIX1(CNN)(l3_buff, out_boxes);
}

static void RunLPRNetwork()
{
  PRINTF("Running LPR model on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  __PREFIX2(CNN)(img_plate_resized, out_lpr);
  int max_prob;
  int predicted_char = 70;
  PRINTF("OUTPUT: \n");
  for (int i=0; i<88; i++){
    max_prob = 0x80000000;
    for (int j=0; j<71; j++){
      if (out_lpr[i+j*88]>max_prob){
        max_prob = out_lpr[i+j*88];
        predicted_char = j;
      }
    }
    if (predicted_char==70) continue;
    PRINTF("%s, ", CHAR_DICT[predicted_char]);
  }
  PRINTF("\n");
}

static void Resize(KerResizeBilinear_ArgT *KerArg)
{
    PRINTF("Resizing...\n");
    AT_FORK(gap_ncore(), (void *) KerResizeBilinear, (void *) KerArg);
}

int start()
{
  #ifdef MEASUREMENTS
	pi_gpio_pin_configure(NULL, PI_GPIO_A0_PAD_8_A4, PI_GPIO_OUTPUT);
	pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
  #endif
#ifdef HAVE_LCD
	if (open_display(&ili)){
		printf("Failed to open display\n");
		pmsis_exit(-1);
	}
    writeFillRect(&ili, 0, 0, 240, 320, 0xFFFF);
    writeText(&ili, "GreenWaves Technologies", 2);
#endif
  /* Init & open ram. */
  struct pi_hyperram_conf hyper_conf;
  pi_hyperram_conf_init(&hyper_conf);
  pi_open_from_conf(&HyperRam, &hyper_conf);
  if (pi_ram_open(&HyperRam))
  {
    printf("Error ram open !\n");
    pmsis_exit(-3);
  }

  if (pi_ram_alloc(&HyperRam, &l3_buff, (uint32_t) AT_INPUT_SIZE))
  {
    printf("Ram malloc failed !\n");
    pmsis_exit(-4);
  }

/*-----------------------OPEN THE CLUSTER--------------------------*/
  struct pi_device cluster_dev;
  struct pi_cluster_conf conf;
  pi_cluster_conf_init(&conf);
  pi_open_from_conf(&cluster_dev, (void *)&conf);
  pi_cluster_open(&cluster_dev);

  pi_freq_set(PI_FREQ_DOMAIN_CL, FREQ_CL*1000*1000);
  pi_freq_set(PI_FREQ_DOMAIN_FC, FREQ_FC*1000*1000);


//------------------------- Aquisition + INFERENCE
  char *ImageName = __XSTR(AT_IMAGE);
  //Reading Image from Bridge
  uint8_t* Input_1 = (uint8_t*) AT_L2_ALLOC(0, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
  if(Input_1==NULL){
    PRINTF("Error allocating image buffer\n");
    pmsis_exit(-1);
  }
/* -------------------- Read Image from bridge ---------------------*/
  PRINTF("Reading image\n");
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD, 1, Input_1, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char), IMGIO_OUTPUT_CHAR, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  buffer.data = Input_1;
  buffer.stride = 0;
  pi_buffer_init(&buffer, PI_BUFFER_TYPE_L2, Input_1);//+AT_INPUT_WIDTH*2+2);
  pi_buffer_set_stride(&buffer, 0);
  #ifdef HAVE_LCD
	pi_display_write(&ili, &buffer, 0, 0, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD);
  #endif
  for(int i=0; i<AT_INPUT_HEIGHT_SSD*AT_INPUT_WIDTH_SSD; i++){
  	Input_1[i] -= 128;
  }
  pi_ram_write(&HyperRam, l3_buff                                         , Input_1, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
  pi_ram_write(&HyperRam, l3_buff+AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD  , Input_1, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
  pi_ram_write(&HyperRam, l3_buff+2*AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD, Input_1, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
  pmsis_l2_malloc_free(Input_1, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
  PRINTF("Finished reading image\n");

/*--------------------------TASK SETUP------------------------------*/
  struct pi_cluster_task *task = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
  if(task==NULL) {
    printf("pi_cluster_task alloc Error!\n");
    pmsis_exit(-1);
  }
  PRINTF("Stack size is %d and %d\n",STACK_SIZE,SLAVE_STACK_SIZE );
  memset(task, 0, sizeof(struct pi_cluster_task));
  task->entry = &RunSSDNetwork;
  task->stack_size = STACK_SIZE;
  task->slave_stack_size = SLAVE_STACK_SIZE;
  task->arg = NULL;


  //Allocate output buffers:
  out_boxes = (short int *)AT_L2_ALLOC(0, MAX_BB*sizeof(bbox_t));
  if(out_boxes==NULL){
    printf("Error Allocating CNN output buffers");
    return 1;
  }

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX1(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }
  PRINTF("Graph constructor was OK\n");

  // Execute the function "RunNetwork" on the cluster.
  pi_cluster_send_task_to_cl(&cluster_dev, task);
  __PREFIX1(CNN_Destruct)();

  bbox_t plate_bbox = out_boxes[0];

  if(plate_bbox.alive){
   	int box_x = (int)(FIX2FP(plate_bbox.x,14)*320);
    int box_y = (int)(FIX2FP(plate_bbox.y,14)*240);
   	int box_w = (int)(FIX2FP(plate_bbox.w,14)*320);
    int box_h = (int)(FIX2FP(plate_bbox.h,14)*240);
    PRINTF("BBOX (x, y, w, h): (%d, %d, %d, %d)\n", box_x, box_y, box_w, box_h);
  	uint8_t* img_plate         = (uint8_t *) pmsis_l2_malloc(box_w*box_h*sizeof(char));
    img_plate_resized = (uint8_t *) pmsis_l2_malloc(AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*sizeof(char));
  	if(img_plate==NULL || img_plate_resized==NULL){
  	  PRINTF("Error allocating image plate buffers\n");
  	  pmsis_exit(-1);
  	}
    pi_task_t end_copy;
    PRINTF("Start Copy 2d\n");
    pi_ram_copy_2d_async(&HyperRam, (uint32_t) (l3_buff+box_y*AT_INPUT_WIDTH_SSD+box_x), (img_plate), \
                         (uint32_t) box_w*box_h, (uint32_t) AT_INPUT_WIDTH_SSD, (uint32_t) box_w, 1, pi_task_block(&end_copy));
    pi_task_wait_on(&end_copy);
    printf("Ended copy\n");

    /*--------------------------TASK SETUP------------------------------*/
    struct pi_cluster_task *task_resize = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
    if(task_resize==NULL) {
      printf("pi_cluster_task alloc Error!\n");
      pmsis_exit(-1);
    }
    PRINTF("Stack size is %d and %d\n",1024, 512 );
    memset(task_resize, 0, sizeof(struct pi_cluster_task));
    task_resize->entry = &Resize;
    task_resize->stack_size = 1024;
    task_resize->slave_stack_size = 512;

    KerResizeBilinear_ArgT ResizeArg;
      ResizeArg.In             = img_plate;
      ResizeArg.Win            = box_w;
      ResizeArg.Hin            = box_h;
      ResizeArg.Out            = img_plate_resized;
      ResizeArg.Wout           = AT_INPUT_WIDTH_LPR;
      ResizeArg.Hout           = AT_INPUT_HEIGHT_LPR;
      ResizeArg.HTileOut       = AT_INPUT_HEIGHT_LPR;
      ResizeArg.FirstLineIndex = 0;
    task_resize->arg = &ResizeArg;
    pi_cluster_send_task_to_cl(&cluster_dev, task_resize);

  	buffer_plate.data = img_plate_resized;
  	buffer_plate.stride = 0;
  	pi_buffer_init(&buffer_plate, PI_BUFFER_TYPE_L2, img_plate_resized);//+AT_INPUT_WIDTH*2+2);
  	pi_buffer_set_stride(&buffer_plate, 0);

  	#ifdef HAVE_LCD
      writeFillRect(&ili, 0, 0, 240, 320, 0xFFFF);
  		pi_display_write(&ili, &buffer_plate, 0, 0, AT_INPUT_WIDTH_LPR, AT_INPUT_HEIGHT_LPR);
  	#endif
        // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
    if (__PREFIX2(CNN_Construct)())
    {
      printf("Graph constructor exited with an error\n");
      return 1;
    }
    PRINTF("Graph constructor was OK\n");
    out_lpr = (char *) pmsis_l2_malloc(71*88*sizeof(char));
    if(out_lpr==NULL){
      printf("out_lpr alloc Error!\n");
      pmsis_exit(-1);
    }
    /*--------------------------TASK SETUP------------------------------*/
    struct pi_cluster_task *task_recogniction = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
    if(task_recogniction==NULL) {
      printf("pi_cluster_task alloc Error!\n");
      pmsis_exit(-1);
    }
    PRINTF("Stack size is %d and %d\n",STACK_SIZE,SLAVE_STACK_SIZE );
    memset(task_recogniction, 0, sizeof(struct pi_cluster_task));
    task_recogniction->entry = &RunLPRNetwork;
    task_recogniction->stack_size = STACK_SIZE;
    task_recogniction->slave_stack_size = SLAVE_STACK_SIZE;
    task_recogniction->arg = NULL;

    // Execute the function "RunNetwork" on the cluster.
    pi_cluster_send_task_to_cl(&cluster_dev, task_recogniction);
    __PREFIX2(CNN_Destruct)();
  }
  while(1);

pmsis_exit(0);
return 0;
}

int main(void)
{
  PRINTF("\n\n\t *** OCR SSD ***\n\n");
  return pmsis_kickoff((void *) start);
}
