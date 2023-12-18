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
        'LPR Model Generator', description="LPR AT Model Generation with NNTool")
    
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

    if not args.NE16:
        G['CONV_2D_0_0_fusion' ].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_2_fusion' ].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_3_fusion' ].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_8_fusion' ].at_options.PARALLELFEATURES = 0
        G['CONV_2D_0_12_fusion'].at_options.PARALLELFEATURES = 0
    else:        
        G.node('input_1').fixed_order = True
        G.quantize(graph_options=quantization_options( scheme='SQ8',use_ne16=True))


    res = G.gen_at_model(
        settings=model_settings(l1_size                           =args.l1_size,
                                l2_size                           =args.l2_size,
                                l3_size                           =args.l3_size, 
                                tensor_directory                  =args.tensors_dir,
                                l3_ram_ext_managed                =False,
                                l3_flash_ext_managed              =False,
                                graph_l1_promotion                =False,
                                gen_name_suffix                   = args.gen_name_suffix,
                                model_file                        = args.model_file,
                                graph_warm_construct              = args.graph_warm_construct,
                                graph_const_exec_from_flash       = True,
                                graph_monitor_cvar_name="LPR_Monitor",
                                graph_produce_operinfos_cvar_name="LPR_Op",
                                graph_produce_node_cvar_name="LPR_Nodes",
                                graph_produce_node_names=True,
                                graph_produce_operinfos=True,
                                graph_monitor_cycles=True
                                ),
        directory=args.gen_model_path,
        at_loglevel=1
    )

