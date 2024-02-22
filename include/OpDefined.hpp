#ifndef MLLM_OPDEFINED_H
#define MLLM_OPDEFINED_H

#include <string>
#include <vector>
using std::string;
using std::vector;

namespace mllm {
enum OpType {
    INVALID_VALUE = 0,
    PARAMETER,
    ADD,
    SOFTMAX,
    SILU,
    MATMUL,
    MATMULINT8,
    SCALE,
    ROPE,
    RMSNORM,
    CAUSALMASK,
    LINEAR,
    LINEARINT8,
    EMBEDDING,
    MUL,
    VIEW,
    KVCACHE,
    RELU,
    RELU2,
    GELU,
    QUICKGLUE,
    LAYERNORM,
    SPLIT,
    GATHER,
    CONVOLUTION2D,
    CONVOLUTION3D,
    AVGPOOL2D,
    MAXPOOL2D,
    CAT,
    TRANSPOSE,
    SUBDIM,
    DIVISION,
    NORM,
    SHAPE,
    MEAN,
    RANGE,
    WHERE,
    REPLACE,
    WNOP,
    QUANTIZE,
    DEQUANTIZE,
    OP_NUM
};

static const vector<string> OpNames = {
    "INVALID_VALUE",
    "Parameter",
    "Add",
    "SoftMax",
    "SiLU",
    "MatMul",
    "MatMulINT8",
    "Scale",
    "RoPE",
    "RMSNorm",
    "CausalMask",
    "Linear",
    "LinearINT8"
    "Embedding",
    "Mul",
    "VIEW",
    "KVCACHE",
    "ReLU",
    "ReLUSquaredActivation",
    "GELU",
    "QuickGELU",
    "LayerNorm",
    "Split",
    "Gqther",
    "Convolution2D",
    "Convolution3D",
    "AvgPool2D",
    "MaxPool2D",
    "Cat",
    "Transpose",
    "SubDim",
    "Division",
    "Norm",
    "Shape",
    "Mean",
    "Range",
    "Where",
    "Replace",
    "WNop",
    "Quantize",
    "Dequantize",
    "OP_NUM"};
} // namespace mllm
#endif
