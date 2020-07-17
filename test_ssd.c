
#include <stdio.h>

#include "ssdlite_ocr.h"
#include "ssdlite_ocrKernels.h"
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define FIX2FP(Val, Precision)    ((float) (Val) / (float) (1<<(Precision)))

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

#ifndef STACK_SIZE
#define STACK_SIZE     2048
#endif

#define CL_FREQ 175*1000*1000
#define FC_FREQ 250*1000*1000

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

L2_MEM char *Output_1;
L2_MEM char *Output_2;
L2_MEM rt_perf_t *cluster_perf;

int start()
{
  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }
  printf("Graph Constructed\n");
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

  __PREFIX(CNN_Destruct)();
  pmsis_exit(0);
}

int main()
{
  printf("\n\n\t *** OCR SSD ***\n\n");
  return pmsis_kickoff((void *) start);
}