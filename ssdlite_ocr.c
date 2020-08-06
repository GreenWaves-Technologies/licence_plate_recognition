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
#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/ram.h"
#include "bsp/ram/hyperram.h"
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#ifdef SILENT
  #define PRINTF(...) ((void) 0)
#else
  #define PRINTF printf
#endif

#define FIX2FP(Val, Precision)    ((float) (Val) / (float) (1<<(Precision)))

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

#ifndef STACK_SIZE
#define STACK_SIZE     2048
#endif

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;
struct pi_device HyperRam;
static uint32_t l3_buff;

typedef signed char IMAGE_IN_T;
#ifdef __EMUL__
  char *ImageName;
#endif

L2_MEM bbox_t *Output_1;

void printBboxes_forPython(bbox_t *boundbxs){
    PRINTF("\n\n======================================================");
    PRINTF("\nThis can be copy-pasted to python DrawBB.py to draw BoudingBoxes on output image:");
    PRINTF("\n\n");

    for (int counter=0;counter< 200;counter++){
        if(boundbxs[counter].alive)
            PRINTF("draw.rectangle((%d,%d,%d,%d), outline=(255, 255, 0))\n",
                (int)(FIX2FP(boundbxs[counter].x,14)*320),
                (int)(FIX2FP(boundbxs[counter].y,14)*240),
                (int)(FIX2FP(boundbxs[counter].w+boundbxs[counter].x,14)*320),
                (int)(FIX2FP(boundbxs[counter].h+boundbxs[counter].y,14)*240)
                );
    }
}

static void RunNetwork()
{
  PRINTF("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  __PREFIX(CNN)(l3_buff, Output_1);
}

int start()
{
  
#ifndef __EMUL__
  #ifdef MEASUREMENTS
  pi_gpio_pin_configure(NULL, PI_GPIO_A0_PAD_8_A4, PI_GPIO_OUTPUT);
  pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
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


  char *ImageName = __XSTR(AT_IMAGE);
  pi_freq_set(PI_FREQ_DOMAIN_CL, FREQ_CL*1000*1000);
  pi_freq_set(PI_FREQ_DOMAIN_FC, FREQ_FC*1000*1000);

/*-----------------------OPEN THE CLUSTER--------------------------*/
  struct pi_device cluster_dev;
  struct pi_cluster_conf conf;
  pi_cluster_conf_init(&conf);
  pi_open_from_conf(&cluster_dev, (void *)&conf);
  pi_cluster_open(&cluster_dev);

/*--------------------------TASK SETUP------------------------------*/
  struct pi_cluster_task *task = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
  if(task==NULL) {
    printf("pi_cluster_task alloc Error!\n");
    pmsis_exit(-1);
  }
  PRINTF("Stack size is %d and %d\n",STACK_SIZE,SLAVE_STACK_SIZE );
  memset(task, 0, sizeof(struct pi_cluster_task));
  task->entry = &RunNetwork;
  task->stack_size = STACK_SIZE;
  task->slave_stack_size = SLAVE_STACK_SIZE;
  task->arg = NULL;
#endif

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }

  //Reading Image from Bridge
  uint8_t* Input_1 = (uint8_t*)pmsis_l2_malloc(AT_INPUT_SIZE);
  if(Input_1==NULL){
    PRINTF("Error allocating image buffer\n");
    pmsis_exit(-1);
  }
  //Allocate output buffers:
  Output_1  = (unsigned char*)AT_L2_ALLOC(0, 200*sizeof(bbox_t));
  if(Output_1==NULL){
    printf("Error Allocating CNN output buffers");
    return 1;
  }
/* -------------------- Read Image from bridge ---------------------*/
  PRINTF("Reading image\n");
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS, Input_1, AT_INPUT_SIZE*sizeof(unsigned char), IMGIO_OUTPUT_CHAR, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  pi_ram_write(&HyperRam, (l3_buff), Input_1, (uint32_t) AT_INPUT_SIZE);

  pmsis_l2_malloc_free(Input_1, AT_INPUT_SIZE);
  PRINTF("Finished reading image\n");

#ifndef __EMUL__
  #ifdef MEASUREMENTS
  for (int i=0; i<1000; i++){
    pi_time_wait_us(50000);
    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 1);
    // Execute the function "RunNetwork" on the cluster.
    pi_cluster_send_task_to_cl(&cluster_dev, task);
    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
  }
  #else
    // Execute the function "RunNetwork" on the cluster.
    pi_cluster_send_task_to_cl(&cluster_dev, task);
  #endif
#else
  RunNetwork();
#endif

  __PREFIX(CNN_Destruct)();

  //printBboxes(&bbxs);
  printBboxes_forPython(Output_1);
  
#ifndef __EMUL__
  #ifdef PERF
	{
		unsigned int TotalCycles = 0, TotalOper = 0;
		printf("\n");
		for (int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
			printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", AT_GraphNodeNames[i], AT_GraphPerf[i], AT_GraphOperInfosNames[i], ((float) AT_GraphOperInfosNames[i])/ AT_GraphPerf[i]);
			TotalCycles += AT_GraphPerf[i]; TotalOper += AT_GraphOperInfosNames[i];
		}
		printf("\n");
		printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
		printf("\n");
	}
  #endif
  pmsis_exit(0);
#endif

  PRINTF("Ended\n");
  return 0;
}

#ifndef __EMUL__
int main(void)
{
  PRINTF("\n\n\t *** OCR SSD ***\n\n");
  return pmsis_kickoff((void *) start);
}
#else
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        PRINTF("Usage: ./exe [image_file]\n");
        exit(-1);
    }
    ImageName = argv[1];
    PRINTF("\n\n\t *** OCR SSD Emul ***\n\n");
    start();
    return 0;
}
#endif
