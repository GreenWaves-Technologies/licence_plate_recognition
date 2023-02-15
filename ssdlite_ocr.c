/*
 * Copyright (C) 2020 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#include "ssdlite_ocr.h"
#include "ssdlite_ocrKernels.h"
#include "ssdlite_ocrInfo.h"

#ifndef __EMUL__
#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/ram.h"
#endif
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define FIX2FP(Val, Precision)    ((float) (Val) / (float) (1<<(Precision)))

#define AT_INPUT_SIZE (AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*AT_INPUT_COLORS_SSD)

#define MAX_BB          (300)
#define CAMERA_WIDTH    (324)
#define CAMERA_HEIGHT   (244)
#define CAMERA_COLORS   (1)
#define CAMERA_SIZE     (CAMERA_WIDTH*CAMERA_HEIGHT*CAMERA_COLORS)
#define SCORE_THR       0

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

#ifdef __EMUL__
  char *ImageName;
  uint8_t Input_1[AT_INPUT_SIZE];
#else
  struct pi_device camera;
  struct pi_device DefaultRam;
  struct pi_device ili;
  static uint32_t l3_buff;
#endif

L2_MEM short int out_boxes[40];
L2_MEM signed char out_scores[10];
L2_MEM signed char out_classes[10];

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
#endif

#ifdef HAVE_HIMAX
static int open_camera_himax(struct pi_device *device)
{
  struct pi_himax_conf cam_conf;
  pi_himax_conf_init(&cam_conf);
  pi_open_from_conf(device, &cam_conf);
  if (pi_camera_open(device))
    return -1;
  return 0;
}
#endif

static void RunNetwork()
{
  printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
#ifndef __EMUL__
  __PREFIX(CNN)(l3_buff, out_boxes, out_classes, out_scores);
#else
  __PREFIX(CNN)(Input_1, out_boxes, out_classes, out_scores);
#endif
}

int start()
{
  #ifdef MEASUREMENTS
  pi_gpio_pin_configure(NULL, PI_GPIO_A0_PAD_8_A4, PI_GPIO_OUTPUT);
  pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
  #endif   
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
  while(1){
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
    #else
      uint8_t* Input_1 = (uint8_t*) pi_l2_malloc(AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char));
      char *ImageName = __XSTR(AT_IMAGE);
      //Reading Image from Bridge
      if(Input_1==NULL){
        printf("Error allocating image buffer\n");
        pmsis_exit(-1);
      }
    /* -------------------- Read Image from bridge ---------------------*/
      printf("Reading image\n");
      if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD, 1, Input_1, AT_INPUT_WIDTH_SSD*AT_INPUT_HEIGHT_SSD*sizeof(char), IMGIO_OUTPUT_CHAR, 0)) {
        printf("Failed to load image %s\n", ImageName);
        return 1;
      }
    #endif
    #ifdef HAVE_LCD
      #ifdef HAVE_HIMAX
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
      buffer.data = Input_1;
      buffer.stride = 0;
      // WIth Himax, propertly configure the buffer to skip boarder pixels
      pi_buffer_init(&buffer, PI_BUFFER_TYPE_L2, Input_1);
      pi_buffer_set_stride(&buffer, 0);
      pi_buffer_set_format(&buffer, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD, 1, PI_BUFFER_FORMAT_GRAY);
      pi_display_write(&ili, &buffer, 0, 0, AT_INPUT_WIDTH_SSD, AT_INPUT_HEIGHT_SSD);
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
  printf("Finished reading image\n");

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }
  printf("Graph constructor was OK\n");

/*--------------------------TASK SETUP------------------------------*/
  struct pi_cluster_task task;
  pi_cluster_task(&task, (void (*)(void *))RunNetwork, NULL);
  pi_cluster_task_stacks(&task, NULL, SLAVE_STACK_SIZE);
  #ifdef MEASUREMENTS
  for (int i=0; i<1000; i++){
    pi_time_wait_us(50000);
    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 1);
    // Execute the function "RunNetwork" on the cluster.
    pi_cluster_send_task_to_cl(&cluster_dev, &task);
    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
  }
  #else
    // Execute the function "RunNetwork" on the cluster.
    pi_cluster_send_task_to_cl(&cluster_dev, &task);
  #endif

  #ifdef PERF
  {
    unsigned int TotalCycles = 0, TotalOper = 0;
    printf("\n");
    for (unsigned int i=0; i<(sizeof(SSD_Monitor)/sizeof(unsigned int)); i++) {
      TotalCycles += SSD_Monitor[i]; TotalOper += SSD_Op[i];
    }
    for (unsigned int i=0; i<(sizeof(SSD_Monitor)/sizeof(unsigned int)); i++) {
      printf("%45s: Cycles: %12u, Cyc%%: %5.1f%%, Operations: %12u, Op%%: %5.1f%%, Operations/Cycle: %f\n", SSD_Nodes[i], SSD_Monitor[i], 100*((float) (SSD_Monitor[i]) / TotalCycles), SSD_Op[i], 100*((float) (SSD_Op[i]) / TotalOper), ((float) SSD_Op[i])/ SSD_Monitor[i]);
    }
    printf("\n");
    printf("%45s: Cycles: %12u, Cyc%%: 100.0%%, Operations: %12u, Op%%: 100.0%%, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
    printf("\n");
  }
  #endif

  __PREFIX(CNN_Destruct)();
    if(out_scores[0] > SCORE_THR){
      int box_y_min = (int)(FIX2FP(( (int) out_boxes[0] - ssdlite_ocr_Output_1_OUT_ZERO_POINT )*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*240);
      int box_x_min = (int)(FIX2FP(( (int) out_boxes[1] - ssdlite_ocr_Output_1_OUT_ZERO_POINT )*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*320);
      int box_y_max = (int)(FIX2FP(( (int) out_boxes[2] - ssdlite_ocr_Output_1_OUT_ZERO_POINT )*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*240);
      int box_x_max = (int)(FIX2FP(( (int) out_boxes[3] - ssdlite_ocr_Output_1_OUT_ZERO_POINT )*ssdlite_ocr_Output_1_OUT_QSCALE,ssdlite_ocr_Output_1_OUT_QNORM)*320);
      int box_h = box_y_max - box_y_min;
      int box_w = box_x_max - box_x_min;
      printf("BBOX (x, y, w, h): (%d, %d, %d, %d) SCORE: %f\n", box_x_min, box_y_min, box_w, box_h, FIX2FP(out_scores[0],7));

      #ifdef HAVE_LCD
        writeFillRect(&ili, box_x_min, box_y_min, 2, box_h, 0x03E0);
        writeFillRect(&ili, box_x_min, box_y_min, box_w, 2, 0x03E0);
        writeFillRect(&ili, box_x_min, box_y_max, box_w, 2, 0x03E0);
        writeFillRect(&ili, box_x_max, box_y_min, 2, box_h, 0x03E0);
      #endif
  }
  printf("Ended\n");
#ifndef __EMUL__
  #ifdef PERF
    break;
  #endif
}
  pmsis_exit(0);
#endif
}

int main(int argc, char *argv[])
{
#ifdef __EMUL__
    if (argc < 2)
    {
        printf("Usage: ./exe [image_file]\n");
        exit(-1);
    }
    ImageName = argv[1];
#endif
    printf("\n\n\t *** OCR SSD ***\n\n");
    start();
    return 0;
}
