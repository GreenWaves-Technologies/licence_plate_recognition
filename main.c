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
#include "ssdlite_ocrInfo.h"

#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/ram.h"
#include "bsp/buffer.h"
#include "bsp/display/ili9341.h"
#include "bsp/camera.h"
#include "bsp/camera/himax.h"

#include "gaplib/ImgIO.h"

#ifndef SILENT
    #define PRINTF printf
#else
    #define PRINTF(...) ((void) 0)
#endif

#ifdef TEST
  #define BOX_Y 137
  #define BOX_X 80
  #define BOX_H 82
  #define BOX_W 225
#endif

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define AT_INPUT_SIZE   (AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*AT_INPUT_COLORS_SSD)
#define MAX_BB    		  (300)
#define CAMERA_WIDTH    (324)
#define CAMERA_HEIGHT   (244)
#define CAMERA_COLORS   (1)
#define CAMERA_SIZE     (CAMERA_WIDTH*CAMERA_HEIGHT*CAMERA_COLORS)


AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX1(_L3_Flash) = 0;
AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX2(_L3_Flash) = 0;

#ifndef __GAP9__
static struct pi_device dmacpy;
#endif
struct pi_device camera;
struct pi_device DefaultRam;
struct pi_device ili;
static uint32_t l3_buff;
static pi_buffer_t buffer;
static pi_buffer_t buffer_plate;
static pi_event_t event_himax;

char OUT_CHAR[100];

L2_MEM short int out_boxes[40];
L2_MEM signed char out_scores[10];
L2_MEM signed char out_classes[10];
#ifdef NE16
typedef unsigned char OUT_T;
#else
typedef signed char OUT_T;
#endif
OUT_T * img_plate_resized;
OUT_T * out_lpr;

#define NUM_STRIPES     88
#define NUM_CHARS_DICT  71
#define BLANK_CHAR_IDX  NUM_CHARS_DICT - 1
#define SCORE_THR       0
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
    writeFillRect(display, 0, 340, posX, fontsize*10, 0xFFFF);
    setCursor(display, posX, posY);
    setTextColor(display, 0x03E0);
    writeText(display, str, fontsize);
}
#endif

static int open_camera_himax(struct pi_device *device)
{
  struct pi_himax_conf cam_conf;
  pi_himax_conf_init(&cam_conf);
  pi_open_from_conf(device, &cam_conf);
  if (pi_camera_open(device))
    return -1;
  return 0;
}

static void RunSSDNetwork()
{
  PRINTF("Running SSD model on cluster\n");
#ifdef PERF
  gap_cl_starttimer();
  gap_cl_resethwtimer();
  int start = gap_cl_readhwtimer();
#endif
  __PREFIX1(CNN)((OUT_T *) l3_buff, out_boxes, out_classes, out_scores);
#ifdef PERF
  int end = gap_cl_readhwtimer();
  printf("SSD PERF: %d cycles\n", end - start);
#endif
}

static void RunLPRNetwork()
{
  PRINTF("Running LPR model on cluster\n");
#ifdef PERF
  gap_cl_starttimer();
  gap_cl_resethwtimer();
  int start = gap_cl_readhwtimer();
#endif
  __PREFIX2(CNN)(img_plate_resized, out_lpr);
#ifdef PERF
  int end = gap_cl_readhwtimer();
  printf("LPR PERF: %d cycles\n", end - start);
#endif
  PRINTF("OUTPUT: \n");
  strcpy(OUT_CHAR, "");

  // greed search decoder ALGO
  int max_prob;
  int predicted_char = BLANK_CHAR_IDX;
  int prev_char = BLANK_CHAR_IDX;
  for (int i=0; i<NUM_STRIPES; i++){
    max_prob = 0x80000000;
    for (int j=0; j<NUM_CHARS_DICT; j++){
      #ifdef NE16
      OUT_T char_score = out_lpr[i*71+j];
      #else
      OUT_T char_score = out_lpr[i+j*88];
      #endif
      if (char_score>max_prob){
        max_prob = char_score;
        predicted_char = j;
      }
    }
    if (predicted_char == BLANK_CHAR_IDX || prev_char == predicted_char) continue;
    prev_char = predicted_char;

    strcat(OUT_CHAR, CHAR_DICT[predicted_char]);
    PRINTF("%s, ", CHAR_DICT[predicted_char]);
  }
  PRINTF("\n");
  strcat(OUT_CHAR, "\0");
  #ifdef TEST
    //test for image: china_1
    if (strcmp(OUT_CHAR, "<Shandong>Q3X5U3")){
      printf("Error predicting characters on china_1, should be <Shandong>Q3X5U3\n");
      pmsis_exit(-1);
    }
  #endif
}

static void Resize(KerResize_ArgT *KerArg)
{
    PRINTF("Resizing...\n");
    AT_FORK(gap_ncore(), (void *) KerResizeBilinear, (void *) KerArg);
}

void start()
{
#ifdef HAVE_HIMAX
  int err = open_camera_himax(&camera);
  if (err) {
    printf("Failed to open camera\n");
    pmsis_exit(-2);
  }
#endif
  /* Init & open ram. */
  struct pi_default_ram_conf hyper_conf;
  pi_default_ram_conf_init(&hyper_conf);
  pi_open_from_conf(&DefaultRam, &hyper_conf);
  if (pi_ram_open(&DefaultRam))
  {
    printf("Error ram open !\n");
    pmsis_exit(-3);
  }

  if (pi_ram_alloc(&DefaultRam, &l3_buff, (uint32_t) AT_INPUT_SIZE))
  {
    printf("Ram malloc failed !\n");
    pmsis_exit(-4);
  }

/*-----------------------OPEN THE CLUSTER--------------------------*/
  struct pi_device cluster_dev;
  struct pi_cluster_conf cl_conf;
  pi_cluster_conf_init(&cl_conf);
  cl_conf.id = 0;
  cl_conf.cc_stack_size = STACK_SIZE;
  pi_open_from_conf(&cluster_dev, (void *) &cl_conf);
  if (pi_cluster_open(&cluster_dev))
  {
    printf("Cluster open failed !\n");
    pmsis_exit(-4);
  }

  pi_freq_set(PI_FREQ_DOMAIN_CL, FREQ_CL*1000*1000);
  pi_freq_set(PI_FREQ_DOMAIN_FC, FREQ_FC*1000*1000);

#ifdef HAVE_LCD
  if (open_display(&ili)){
    printf("Failed to open display\n");
    pmsis_exit(-1);
  }
#endif

while(1) 
  {
  //------------------------- Aquisition + INFERENCE
    #ifdef HAVE_HIMAX
      uint8_t* Input_1 = (uint8_t*) pi_l2_malloc(CAMERA_SIZE*sizeof(char));
      //Reading Image from Bridge
      if(Input_1==NULL){
        printf("Error allocating image buffer\n");
        pmsis_exit(-1);
      }
      // Get an image 
      pi_camera_control(&camera, PI_CAMERA_CMD_START, 0);
      pi_camera_capture(&camera, Input_1, CAMERA_SIZE);
      pi_camera_control(&camera, PI_CAMERA_CMD_STOP, 0);
      #ifdef HAVE_LCD
        // Image Cropping to [ AT_INPUT_HEIGHT_SSD x AT_INPUT_WIDTH_SSD ]
        int idx=0;
        for(int i =0;i<CAMERA_HEIGHT;i++){
          for(int j=0;j<CAMERA_WIDTH;j++){
            if (i<AT_INPUT_HEIGHT_SSD && j<AT_INPUT_WIDTH_SSD){
              Input_1[idx] = Input_1[i*CAMERA_WIDTH+j];
            idx++;
            }
          }
        }
      #endif
    #else
      uint8_t* Input_1 = (uint8_t*) pi_l2_malloc(AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
      char *ImageName = __XSTR(AT_IMAGE);
      //Reading Image from Bridge
      if(Input_1==NULL){
        printf("Error allocating image buffer\n");
        pmsis_exit(-1);
      }
    /* -------------------- Read Image from bridge ---------------------*/
      PRINTF("Reading image\n");
      if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD, 1, Input_1, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char), IMGIO_OUTPUT_CHAR, 0)) {
        printf("Failed to load image %s\n", ImageName);
        pmsis_exit(-1);
      }
    #endif

    #ifndef NE16
    for(int i=0; i<AT_INPUT_HEIGHT_SSD*AT_INPUT_WIDTH_SSD; i++){
      Input_1[i] -= 128;
    }
    #endif
    pi_ram_write(&DefaultRam, l3_buff                                         , Input_1, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
    pi_ram_write(&DefaultRam, l3_buff+AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD  , Input_1, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
    pi_ram_write(&DefaultRam, l3_buff+2*AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD, Input_1, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
    #ifdef HAVE_HIMAX
      pi_l2_free(Input_1, CAMERA_SIZE*sizeof(char));
    #else
      pi_l2_free(Input_1, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
    #endif
    PRINTF("Finished reading image\n");

    // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
    int ssd_constructor_err = __PREFIX1(CNN_Construct)();
    if (ssd_constructor_err)
    {
      printf("SSD Graph constructor exited with an error: %d\n", ssd_constructor_err);
      pmsis_exit(-1);
    }
    PRINTF("SSD Graph constructor was OK\n");

    /*--------------------------TASK SETUP------------------------------*/
    struct pi_cluster_task *task = pi_l2_malloc(sizeof(struct pi_cluster_task));
    if(task==NULL) {
      printf("pi_cluster_task alloc Error!\n");
      pmsis_exit(-1);
    }
    pi_cluster_task(task, (void (*)(void *))&RunSSDNetwork, NULL);

    // Execute the function "RunNetwork" on the cluster.
    pi_cluster_send_task_to_cl(&cluster_dev, task);

    __PREFIX1(CNN_Destruct)();
    pi_l2_free(task, sizeof(struct pi_cluster_task));

    #ifdef HAVE_LCD
      uint8_t* lcd_buffer = (uint8_t*) pi_l2_malloc(AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
      pi_ram_read(&DefaultRam, l3_buff, lcd_buffer, (uint32_t) AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD);
      for(int i=0; i<AT_INPUT_HEIGHT_SSD*AT_INPUT_WIDTH_SSD; i++){
        lcd_buffer[i] += 128;
      }
      buffer.data = lcd_buffer;
      buffer.stride = 0;
      // WIth Himax, propertly configure the buffer to skip boarder pixels
      pi_buffer_init(&buffer, PI_BUFFER_TYPE_L2, lcd_buffer);
      pi_buffer_set_stride(&buffer, 0);
      pi_buffer_set_format(&buffer, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD, 1, PI_BUFFER_FORMAT_GRAY);
      pi_display_write(&ili, &buffer, 0, 0, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD);
      pi_l2_free(lcd_buffer, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
      draw_text(&ili, OUT_CHAR, 0, 0, 2);
    #endif
    if(out_scores[0] > SCORE_THR){
      int box_y_min = (int)(FIX2FP(out_boxes[0]*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*240);
     	int box_x_min = (int)(FIX2FP(out_boxes[1]*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*320);
      int box_y_max = (int)(FIX2FP(out_boxes[2]*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*240);
     	int box_x_max = (int)(FIX2FP(out_boxes[3]*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*320);
      int box_h = box_y_max - box_y_min;
      int box_w = box_x_max - box_x_min;
      //PRINTF("BBOX (x, y, w, h): (%d, %d, %d, %d) SCORE: %f\n", out_boxes[0], out_boxes[1], out_boxes[2], out_boxes[3], FIX2FP(out_scores[0],7));
      printf("BBOX (x, y, w, h): (%d, %d, %d, %d) SCORE: %f\n", box_x_min, box_y_min, box_w, box_h, FIX2FP(out_scores[0],7));
      img_plate_resized      = (OUT_T *) pi_l2_malloc(AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*3*sizeof(char));
      OUT_T* img_plate = (OUT_T *) pi_l2_malloc(box_w*box_h*sizeof(char));
    	if(img_plate==NULL || img_plate_resized==NULL){
    	  printf("Error allocating image plate buffers\n");
    	  pmsis_exit(-1);
    	}
      #ifdef HAVE_LCD
        writeFillRect(&ili, box_x_min, box_y_min, 2, box_h, 0x03E0);
        writeFillRect(&ili, box_x_min, box_y_min, box_w, 2, 0x03E0);
        writeFillRect(&ili, box_x_min, box_y_max, box_w, 2, 0x03E0);
        writeFillRect(&ili, box_x_max, box_y_min, 2, box_h, 0x03E0);
      #endif
      #ifdef TEST
        //test for image: china_1
        if (!(box_x_min>(BOX_X-10) && box_x_min<(BOX_X+10)) || !(box_y_min>(BOX_Y-10) && box_y_min<(BOX_Y+10)) || !(box_w>(BOX_W-10) && box_w<(BOX_W+10)) || !(box_h>(BOX_H-10) && box_h<(BOX_H+10))){
          printf("Error in bounding boxes for image china_1.ppm (%d %d %d %d) instead of (%d %d %d %d)\n", box_x_min, box_y_min, box_w, box_h, BOX_X, BOX_Y, BOX_W, BOX_H);
          pmsis_exit(-1);
        }
      #endif

      pi_event_t end_copy;
      pi_ram_copy_2d_async(&DefaultRam, (uint32_t) (l3_buff+box_y_min*AT_INPUT_WIDTH_SSD+box_x_min), (img_plate), \
                           (uint32_t) box_w*box_h, (uint32_t) AT_INPUT_WIDTH_SSD, (uint32_t) box_w, 1, pi_evt_sig_init(&end_copy));
      pi_evt_wait(&end_copy);

      /*--------------------------TASK SETUP------------------------------*/
      struct pi_cluster_task *task_resize = pi_l2_malloc(sizeof(struct pi_cluster_task));
      if(task_resize==NULL) {
        printf("pi_cluster_task alloc Error!\n");
        pmsis_exit(-1);
      }

      KerResize_ArgT ResizeArg;
        ResizeArg.In             = img_plate;
        ResizeArg.Win            = box_w;
        ResizeArg.HTileIn        = box_h;
        ResizeArg.Out            = img_plate_resized;
        ResizeArg.Wout           = AT_INPUT_WIDTH_LPR;
        ResizeArg.HTileOut       = AT_INPUT_HEIGHT_LPR;
        ResizeArg.WStep          = ((box_w-1)<<16)/(AT_INPUT_WIDTH_LPR-1);
        ResizeArg.HStep          = ((box_h-1)<<16)/(AT_INPUT_HEIGHT_LPR-1);
        ResizeArg.HTileInIndex   = 0;
        ResizeArg.HTileOutIndex  = 0;
        ResizeArg.Channels       = 1;
      pi_cluster_task(task_resize, (void (*)(void *))&Resize, &ResizeArg);
      pi_cluster_send_task_to_cl(&cluster_dev, task_resize);

      pi_l2_free(task_resize, sizeof(struct pi_cluster_task));
      pi_l2_free(img_plate, box_w*box_h*sizeof(char));

      #ifdef DUMP_RESIZED
        printf("\n\n(%d, %d)\n", box_w, box_h);
        for (int i=0; i<box_h; i++){
          for (int j=0; j<box_w; j++){
            printf("%d, ", img_plate[i*box_w+j]);
          }
        }
        printf("\n\n\n");
        for (int i=0; i<AT_INPUT_HEIGHT_LPR; i++){
          for (int j=0; j<AT_INPUT_WIDTH_LPR; j++){
            printf("%d, ", img_plate_resized[i*AT_INPUT_WIDTH_LPR+j]);
          }
        }
      #endif
      #ifdef __GAP9__
      memcpy((void *) img_plate_resized+AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR,   (void *) img_plate_resized, AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR);
      memcpy((void *) img_plate_resized+2*AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR, (void *) img_plate_resized, AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR);
      #else
      /* Init & open dmacpy. */
      struct pi_dmacpy_conf dmacpy_conf = {0};
      pi_dmacpy_conf_init(&dmacpy_conf);
      pi_open_from_conf(&dmacpy, &dmacpy_conf);
      int errors = pi_dmacpy_open(&dmacpy);
      if (errors)
      {
        printf("Error dmacpy open : %ld !\n", errors);
        pmsis_exit(-3);
      }
      // /* Copy buffer from L2 to L2. */
      errors = pi_dmacpy_copy(&dmacpy, (void *) img_plate_resized, (void *) img_plate_resized+AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR, AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR, PI_DMACPY_L2_L2);
      errors = pi_dmacpy_copy(&dmacpy, (void *) img_plate_resized, (void *) img_plate_resized+2*AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR, AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR, PI_DMACPY_L2_L2);
      if(errors){
        printf("Copy from L2 to L2 failed : %ld\n", errors); pmsis_exit(-5);
      }
      #endif
      WriteImageToFile("../../resized_cropped_plate.ppm", AT_INPUT_WIDTH_LPR, AT_INPUT_HEIGHT_LPR, 1, img_plate_resized, GRAY_SCALE_IO);

      // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
      PRINTF("LPR Graph constructor ...\n");
      int lpr_constructor_err = __PREFIX2(CNN_Construct)();
      if (lpr_constructor_err)
      {
        printf("LPR Graph constructor exited with an error: %d\n", lpr_constructor_err);
        continue;
      }
      PRINTF("LPR Graph constructor was OK\n");
      out_lpr = (OUT_T *) pi_l2_malloc(NUM_CHARS_DICT*NUM_STRIPES*sizeof(char));
      if(out_lpr==NULL){
        printf("out_lpr alloc Error!\n");
        pmsis_exit(-1);
      }
      /*--------------------------TASK SETUP------------------------------*/
      struct pi_cluster_task *task_recogniction = pi_l2_malloc(sizeof(struct pi_cluster_task));
      if(task_recogniction==NULL) {
        printf("pi_cluster_task alloc Error!\n");
        pmsis_exit(-1);
      }
      pi_cluster_task(task_recogniction, (void (*)(void *))&RunLPRNetwork, NULL);
#ifdef __GAP8__
      task_recogniction->stack_size = STACK_SIZE;
      task_recogniction->slave_stack_size = SLAVE_STACK_SIZE;
#else
      pi_cluster_task_stacks(task_recogniction, pi_cl_l1_scratch_alloc(&cluster_dev, task_recogniction, SLAVE_STACK_SIZE * pi_cl_cluster_nb_pe_cores()), SLAVE_STACK_SIZE);
#endif

      // Execute the function "RunNetwork" on the cluster.
      pi_cluster_send_task_to_cl(&cluster_dev, task_recogniction);
      __PREFIX2(CNN_Destruct)();
      pi_l2_free(task_recogniction, sizeof(struct pi_cluster_task));
      pi_l2_free(out_lpr, NUM_CHARS_DICT*NUM_STRIPES*sizeof(char));
      pi_l2_free(img_plate_resized, AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*3*sizeof(char));
      #if defined (ONE_ITER) || defined (TEST)
        break;
      #endif
    }
  }

#ifdef PERF
  {
    unsigned int SSDCycles = 0, SSDOperTot = 0;
    for (unsigned int i=0; i<(sizeof(SSD_Monitor)/sizeof(unsigned int)); i++) {
      SSDCycles += SSD_Monitor[i]; SSDOperTot += SSD_Op[i];
    }
    unsigned int LPRCycles = 0, LPROperTot = 0;
    for (unsigned int i=0; i<(sizeof(LPR_Monitor)/sizeof(unsigned int)); i++) {
      LPRCycles += LPR_Monitor[i]; LPROperTot += LPR_Op[i];
    }
    unsigned int TotalCycles = SSDCycles + LPRCycles, TotalOper = SSDOperTot + LPROperTot;
    printf("%45s: Cycles: %12u, Cyc%%: %5.1f%%, Operations: %12u, Op%%: %5.1f%%, Operations/Cycle: %f\n", "SSD", SSDCycles, ((float) 100*SSDCycles)/ TotalCycles, SSDOperTot, ((float) 100*SSDOperTot)/ TotalOper, ((float) SSDOperTot)/ SSDCycles);
    printf("%45s: Cycles: %12u, Cyc%%: %5.1f%%, Operations: %12u, Op%%: %5.1f%%, Operations/Cycle: %f\n", "LPR", LPRCycles, ((float) 100*LPRCycles)/ TotalCycles, LPROperTot, ((float) 100*LPROperTot)/ TotalOper, ((float) LPROperTot)/ LPRCycles);
    printf("%45s: Cycles: %12u, Cyc%%: 100.0%%, Operations: %12u, Op%%: 100.0%%, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
  }
#endif
pmsis_exit(0);
}

int main(void)
{
  PRINTF("\n\n\t *** OCR SSD ***\n\n");
  start();
}
