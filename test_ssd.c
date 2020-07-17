
#include <stdio.h>

#include "ssdlite_ocr.h"
#include "ssdlite_ocrKernels.h"
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define FIX2FP(Val, Precision)    ((float) (Val) / (float) (1<<(Precision)))

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

#define CL_FREQ 175*1000*1000
#define FC_FREQ 250*1000*1000

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

L2_MEM char *Output_1;
L2_MEM char *Output_2;
L2_MEM rt_perf_t *cluster_perf;

static void RunNetwork()
{
  printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
#ifndef __EMUL__
  __PREFIX(CNN)(Output_1, Output_2);
#else
  __PREFIX(CNN)(Output_1, Output_2);
#endif 
  printf("Runner completed\n");
  printf("\n");
  printf("\n\n\n");
  printf("Output_1:\t");
  for (int i=0; i<1554*2; i++){
    printf("%d, ", Output_1[i]);
  }
  printf("\n\n\n");
  printf("Output_2:\t");
  for (int i=0; i<1554*4; i++){
    printf("%d, ", Output_2[i]);
  }
}

int start()
{
    //Allocate output buffers:
  Output_1  = (unsigned char*)AT_L2_ALLOC(0, 1554*2*sizeof(unsigned char));
  Output_2  = (unsigned char*)AT_L2_ALLOC(0, 1554*4*sizeof(unsigned char));

/*-----------------------OPEN THE CLUSTER--------------------------*/
  struct pi_device cluster_dev;
  struct pi_cluster_conf conf;
  pi_cluster_conf_init(&conf);
  pi_open_from_conf(&cluster_dev, (void *)&conf);
  pi_cluster_open(&cluster_dev);
  printf("cluster\n");
/*--------------------------TASK SETUP------------------------------*/
  printf("\n\n\nstruct claster task: %d\n\n\n", sizeof(struct pi_cluster_task));
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

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }
/* -------------------- Read Image from bridge ---------------------*/
  char *ImageName = __XSTR(AT_IMAGE);
  printf("Reading image\n");
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS, Input_1, AT_INPUT_SIZE*sizeof(unsigned char), IMGIO_OUTPUT_CHAR, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  PRINTF("Finished reading image\n");
  __PREFIX(CNN_Destruct)();
  pmsis_exit(0);
}

int main()
{
  printf("\n\n\t *** OCR SSD ***\n\n");
  return pmsis_kickoff((void *) start);
}