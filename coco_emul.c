
#include "ssd_mobv1Info.h"
#include "ssd_mobv1Kernels.h"
#include "SSD_BasicKernels.h"

#include "gaplib/ImgIO.h"
#include <stdio.h>

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

typedef signed char IMAGE_IN_T;
char *ImageName;
L2_MEM bbox_t *bboxes;

static void RunNetwork()
{
  __PREFIX(CNN)(Input_1, bboxes);
}

int start()
{

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }

/* -------------------- Read Image from bridge ---------------------*/
  printf("Reading image\n");
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS, Input_1, AT_INPUT_SIZE*sizeof(unsigned char), IMGIO_OUTPUT_CHAR, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  PRINTF("Finished reading image\n");

  RunNetwork();

  __PREFIX(CNN_Destruct)();

  printf("Ended\n");
  return 0;
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: ./exe [image_file]\n");
        exit(-1);
    }
    ImageName = argv[1];
    printf("\n\n\t *** COCO SSD Emul ***\n\n");
    start();
    return 0;
}

