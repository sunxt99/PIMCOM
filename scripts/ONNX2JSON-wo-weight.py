# import onnx
# from google.protobuf.json_format import MessageToJson, Parse


# onnx_model = onnx.load("../Models/resnet18-v1-7.onnx")
# # onnx_model.graph.initializer
# message = MessageToJson(onnx_model)
# with open("resnet18-v1-7.json", "w") as fo:
#     fo.write(message)


import onnx
import argparse
import json
import os
from google.protobuf.json_format import MessageToJson, Parse


parser = argparse.ArgumentParser()
parser.add_argument("-m", "--onnx-model", help="model name")
args = parser.parse_args()

model_name = args.model.split("/")[-1].split(".")[0]
onnx_model = onnx.load(args.model)
message = MessageToJson(onnx_model)
info = json.loads(message)
info['graph']['initializer'] = None

with open(os.path.join("./JSON",(model_name + "-wo-weight.json")),"w") as file:
    json.dump(info,file,indent=2)