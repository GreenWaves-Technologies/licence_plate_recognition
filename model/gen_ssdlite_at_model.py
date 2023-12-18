import argparse, random, os
from PIL import Image
import numpy as np
from scipy.spatial import distance
from numpy.linalg import norm
from tqdm import tqdm
from nntool.api import NNGraph
from nntool.api.types import ConstantInputNode
from nntool.api.utils import model_settings, quantization_options

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        'LPR Model Generator', description="SSD Lite OCR AT Model Generation with NNTool")
    
    parser.add_argument("--model_path", default="lprnet.tflite", type=str, required=True,
                        help="Path to onnx file")
    parser.add_argument("--tensors_dir", default="None", type=str, required=True,
                        help="Path for generated tensors")
    parser.add_argument("--gen_model_path", default="None", type=str, required=True,
                        help="Path for generated Autotiler Model")
    
    parser.add_argument("--l1_size", default=128000, type=int, required=False,
                        help="L1 memory size for AT code generation")
    parser.add_argument("--l2_size", default=1300000, type=int, required=False,
                        help="L2 memory size for AT code generation")
    parser.add_argument("--l3_size", default=8000000, type=int, required=False,
                        help="L3 memory size for AT code generation")
    parser.add_argument("--model_file", default="", type=str, required=False,
                        help="AT model file name")
    parser.add_argument("--gen_name_suffix", default="", type=str, required=False,
                        help="AT generated file suffix")
    parser.add_argument("--NE16", default=False, type=bool, required=False,
                        help="AT generated file suffix")
    parser.add_argument("--graph_warm_construct", default=0, type=int, required=False,
                        help="Warm Constructor in Autotiler generated code")
    

    args = parser.parse_args()
    
    #Load Graph, adjust and fusions
    G = NNGraph.load_graph(args.model_path,load_quantization=True,old_dsp_lib=False)
    G.adjust_order()
    G.fusions('scaled_match_group')
    G.fusions('expression_matcher')
    G.name=args.gen_name_suffix
    
    #print(G.show())

    if not args.NE16:
        G['DW_CONV_2D_0_1_fusion'].at_options.PARALLELFEATURES = 0
        G['DW_CONV_2D_0_4_fusion'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_5'].at_options.PARALLELFEATURES = 0
        G['DW_CONV_2D_0_7_fusion'].at_options.PARALLELFEATURES = 0
        G['DW_CONV_2D_0_11_fusion'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_13_fusion'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_19'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_26'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_30'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_34'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_43_fusion'].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_45'].at_options.PARALLELFEATURES = 0

        #G['CONV_2D_0_47_fusion'].at_options.OUT_HOME_MEM_LOC =  $(MODEL_L3_RAM)
        #G['CONV_2D_0_47_fusion'].at_options.OUT_EXEC_MEM_LOC =  $(MODEL_L3_RAM)
    else:        
        G.node('input_1').fixed_order = True
        G.quantize(graph_options=quantization_options( scheme='SQ8',use_ne16=True))


    res = G.gen_at_model(
        settings=model_settings(l1_size                           =args.l1_size,
                                l2_size                           =args.l2_size,
                                l3_size                           =args.l3_size,
                                default_input_exec_location='AT_MEM_L3_DEFAULTRAM',
                                default_input_home_location='AT_MEM_L3_DEFAULTRAM',
                                tensor_directory                  =args.tensors_dir,
                                l3_ram_ext_managed                =True,
                                l3_flash_ext_managed              =False,
                                graph_l1_promotion                =False,
                                gen_name_suffix                   = args.gen_name_suffix,
                                basic_kernel_header_file          = args.gen_name_suffix+"Expression_Kernels.h",
                                basic_kernel_source_file          = args.gen_name_suffix+"Expression_Kernels.c",
                                model_file                        = args.model_file,
                                graph_warm_construct              = args.graph_warm_construct,
                                graph_const_exec_from_flash       = True,
                                graph_monitor_cvar_name="SSD_Monitor",
                                graph_produce_operinfos_cvar_name="SSD_Op",
                                graph_produce_node_cvar_name="SSD_Nodes",
                                graph_produce_node_names=True,
                                graph_produce_operinfos=True,
                                graph_monitor_cycles=True
                                ),
        directory=args.gen_model_path,
        at_loglevel=1
    )

