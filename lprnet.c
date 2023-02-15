/*
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#include "lprnet.h"
#include "lprnetKernels.h"
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#ifdef __EMUL__
char *ImageName;
#endif

#define AT_INPUT_SIZE (AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*AT_INPUT_COLORS_LPR)

AT_HYPERFLASH_FS_EXT_ADDR_TYPE lprnet_L3_Flash = 0;

static char *CHAR_DICT [71] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "<Anhui>", "<Beijing>", "<Chongqing>", "<Fujian>", "<Gansu>", "<Guangdong>", "<Guangxi>", "<Guizhou>", "<Hainan>", "<Hebei>", "<Heilongjiang>", "<Henan>", "<HongKong>", "<Hubei>", "<Hunan>", "<InnerMongolia>", "<Jiangsu>", "<Jiangxi>", "<Jilin>", "<Liaoning>", "<Macau>", "<Ningxia>", "<Qinghai>", "<Shaanxi>", "<Shandong>", "<Shanghai>", "<Shanxi>", "<Sichuan>", "<Tianjin>", "<Tibet>", "<Xinjiang>", "<Yunnan>", "<Zhejiang>", "<police>", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "_"};

typedef signed char IMAGE_IN_T;
unsigned char * Input_1;
#ifdef NE16
typedef unsigned char OUT_T;
#else
typedef signed char OUT_T;
#endif
OUT_T * Output_1;

static void RunNetwork()
{
#ifdef PERF
	printf("Start timer\n");
	gap_cl_starttimer();
	gap_cl_resethwtimer();
#endif
 	lprnetCNN(Input_1, Output_1);

	printf("Runner completed\n");
	int max_prob;
	int predicted_char = 70;
  	int prev_char = 70;
	printf("OUTPUT: \n");
	for (int i=0; i<88; i++){
		max_prob = 0x80000000;
		for (int j=0; j<71; j++){
			#ifdef NE16
			OUT_T char_score = Output_1[i*71+j];
			#else
			OUT_T char_score = Output_1[i+j*88];
			#endif
			if (char_score>max_prob){
				max_prob = char_score;
				predicted_char = j;
			}
		}
		if (predicted_char==70|| prev_char == predicted_char) continue;
    	prev_char = predicted_char;
		printf("%s, ", CHAR_DICT[predicted_char]);
	}
	printf("\n");
}

int start()
{	
#ifndef __EMUL__
	#ifdef MEASUREMENTS
    pi_gpio_pin_configure(NULL, PI_GPIO_A0_PAD_8_A4, PI_GPIO_OUTPUT);
    pi_gpio_pin_write(NULL, PI_GPIO_A0_PAD_8_A4, 0);
    #endif

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

	char *ImageName = __XSTR(AT_IMAGE);
	//Reading Image from Bridge
	Input_1 = (uint8_t*) pi_l2_malloc(AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*3*sizeof(char));
	if(Input_1==NULL){
		printf("Error allocating image buffer\n");
		pmsis_exit(-1);
	}
#endif
	/* -------------------- Read Image from bridge ---------------------*/
	printf("Reading image\n");
	if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH_LPR, AT_INPUT_HEIGHT_LPR, 1, Input_1, AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR*sizeof(char), IMGIO_OUTPUT_CHAR, 0)) {
		printf("Failed to load image %s\n", ImageName);
		return 1;
	}
	for(int i=0; i<AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR; i++){
		#ifdef NE16
		int temp = Input_1[i];
		#else
		int temp = Input_1[i] - 128;
		#endif
		Input_1[i] = temp;
		Input_1[i+AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR] = temp;
		Input_1[i+2*AT_INPUT_WIDTH_LPR*AT_INPUT_HEIGHT_LPR] = temp;
	}
	printf("Finished reading image\n");

	//Allocate output buffers:
	Output_1  = (OUT_T *)AT_L2_ALLOC(0, 71*88);
	if(Output_1==NULL){
		printf("Error Allocating CNN output buffers");
		return 1;
	}

	// IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
	if (lprnetCNN_Construct())
	{
		printf("Graph constructor exited with an error\n");
		return 1;
	}
	printf("Graph constructor was OK\n");

#ifndef __EMUL__
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
#else
	RunNetwork();
#endif

#ifdef PERF
	{
		unsigned int TotalCycles = 0, TotalOper = 0;
		printf("\n");
		for (unsigned int i=0; i<(sizeof(LPR_Monitor)/sizeof(unsigned int)); i++) {
			TotalCycles += LPR_Monitor[i]; TotalOper += LPR_Op[i];
		}
		for (unsigned int i=0; i<(sizeof(LPR_Monitor)/sizeof(unsigned int)); i++) {
			printf("%45s: Cycles: %12u, Cyc%%: %5.1f%%, Operations: %12u, Op%%: %5.1f%%, Operations/Cycle: %f\n", LPR_Nodes[i], LPR_Monitor[i], 100*((float) (LPR_Monitor[i]) / TotalCycles), LPR_Op[i], 100*((float) (LPR_Op[i]) / TotalOper), ((float) LPR_Op[i])/ LPR_Monitor[i]);
		}
		printf("\n");
		printf("%45s: Cycles: %12u, Cyc%%: 100.0%%, Operations: %12u, Op%%: 100.0%%, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
		printf("\n");
	}
#endif

	lprnetCNN_Destruct();

#ifndef __EMUL__
	pmsis_exit(0);
#endif
	printf("Ended\n");
	return 0;
}

int main(int argc, char *argv[])
{
#ifdef __EMUL__
    if (argc < 2)
    {
        printf("Usage: mnist [image_file]\n");
        exit(-1);
    }
    ImageName = argv[1];
#endif
    printf("\n\n\t *** NNTOOL LPRNET ***\n\n");
    start();
    return 0;
}
