
#include "QNNRoPE.hpp"
#include "Types.hpp"
#include "QNNCommonOp.hpp"
#include <cstdint>

namespace mllm {
QNNRoPE::QNNRoPE(Backend *bn, string opName, bool hf) :
    QNNCommonOp(bn, opName) {
    cos_.setBackend(bn);
    sin_.setBackend(bn);
    hf_ = hf;
}

ErrorCode QNNRoPE::reshape(vector<shared_ptr<Tensor>> inputs, vector<shared_ptr<Tensor>> outputs) {
    CHECK_EQ(inputs.size(), 1);
    CHECK_EQ(outputs.size(), 1);
    outputs[0]->reshape(inputs[0]->shape(0), inputs[0]->shape(1), inputs[0]->shape(2), inputs[0]->shape(3));
    ishape = inputs[0]->shape(3);
    return Op::reshape(inputs, outputs);
}

ErrorCode QNNRoPE::setUp(vector<shared_ptr<Tensor>> inputs, vector<shared_ptr<Tensor>> outputs) {
    pos_max_ = 16384;
    ishape = inputs[0]->dimension();

    if (!sin_.allocted()) {
        if (pose_type_ == 1) {
            sinusoidal_position_embedding_hf(1, 1, pos_max_, ishape, sin_, cos_);
        } else if (pose_type_ == 2) {
            sinusoidal_position_embedding(1, 1, pos_max_, ishape, sin_, cos_);
        } else {
            sinusoidal_position_embedding_hf(1, 1, pos_max_, ishape / 2, sin_, cos_);
        }
    }

    uint32_t sin_dimensions[] = {16384, static_cast<uint32_t>(ishape)};
    uint32_t cos_dimensions[] = {16384, static_cast<uint32_t>(ishape)};
    auto sinName = name() + ".sin";
    qnnBackend_->modelAddTensor(sinName, // Node Name
                                (Qnn_Tensor_t){
                                    .version = QNN_TENSOR_VERSION_1,
                                    {.v1 = {
                                         .id = 0,
                                         .name = sinName.c_str(),
                                         .type = QNN_TENSOR_TYPE_STATIC,
                                         .dataFormat = QNN_TENSOR_DATA_FORMAT_FLAT_BUFFER,
                                         .dataType = QNN_DATATYPE_FLOAT_16,
                                         .quantizeParams = {QNN_DEFINITION_UNDEFINED,
                                                            QNN_QUANTIZATION_ENCODING_UNDEFINED,
                                                            {.scaleOffsetEncoding = {.scale = 0.0000000000000000f, .offset = 0}}},
                                         .rank = 2,
                                         .dimensions = sin_dimensions,
                                         .memType = QNN_TENSORMEMTYPE_RAW,
                                         {.clientBuf = {.data = sin_.hostPtr<void>(),
                                                        .dataSize = static_cast<uint32_t>(sin_.cntSize())}}}}});
    auto cosName = name() + ".cos";
    qnnBackend_->modelAddTensor(cosName, // Node Name
                                (Qnn_Tensor_t){
                                    .version = QNN_TENSOR_VERSION_1,
                                    {.v1 = {
                                         .id = 0,
                                         .name = cosName.c_str(),
                                         .type = QNN_TENSOR_TYPE_STATIC,
                                         .dataFormat = QNN_TENSOR_DATA_FORMAT_FLAT_BUFFER,
                                         .dataType = QNN_DATATYPE_FLOAT_16,
                                         .quantizeParams = {QNN_DEFINITION_UNDEFINED,
                                                            QNN_QUANTIZATION_ENCODING_UNDEFINED,
                                                            {.scaleOffsetEncoding = {.scale = 0.0000000000000000f, .offset = 0}}},
                                         .rank = 2,
                                         .dimensions = cos_dimensions,
                                         .memType = QNN_TENSORMEMTYPE_RAW,
                                         {.clientBuf = {.data = cos_.hostPtr<void>(),
                                                        .dataSize = static_cast<uint32_t>(cos_.cntSize())}}}}});

    uint32_t pose_type = 2;
    vector<Qnn_Param_t> params_rope = {
        {.paramType = QNN_PARAMTYPE_SCALAR,
         .name = "pose_type",
         {.scalarParam = (Qnn_Scalar_t){QNN_DATATYPE_UINT_32, {.uint32Value = pose_type}}}}};

    uint32_t dimOut[4];
    for(int i = 0; i < 4; i++) {
        dimOut[i] = inputs[0]->shape(i);
    }
    auto outName = outputs[0]->name();
    vector<Qnn_Tensor_t> out = {
        (Qnn_Tensor_t){
            .version = QNN_TENSOR_VERSION_1,
            {.v1 = {
                 .id = 0,
                 .name = outName.c_str(),
                 .type = getOutputTensorType(outputs[0]),
                 .dataFormat = QNN_TENSOR_DATA_FORMAT_FLAT_BUFFER,
                 .dataType = QNN_DATATYPE_FLOAT_16,
                 .quantizeParams = {QNN_DEFINITION_UNDEFINED,
                                    QNN_QUANTIZATION_ENCODING_UNDEFINED,
                                    {.scaleOffsetEncoding = {.scale = 0.0000000000000000f, .offset = 0}}},
                 .rank = 4,
                 .dimensions = dimOut,
                 .memType = QNN_TENSORMEMTYPE_RAW,
                 {.clientBuf = {.data = nullptr,
                                .dataSize = 0}}}}}};
    return graphAddNode(name(), "RoPE", {inputs[0]->name(), sinName, cosName}, out, params_rope, "LLaMAPackage");
}

ErrorCode QNNRoPE::load(AbstructLoader &loader) {
    if (hf_) {
        sinusoidal_position_embedding_hf(1, 1, pos_max_, ishape, sin_, cos_);
    } else {
        sinusoidal_position_embedding(1, 1, pos_max_, ishape, sin_, cos_);
    }
    return Op::load(loader);
}

ErrorCode QNNRoPE::free(vector<shared_ptr<Tensor>> inputs, vector<shared_ptr<Tensor>> outputs) {
    //     sin_.free();
    //     cos_.free();
    return Op::free(inputs, outputs);
}

void QNNRoPE::sinusoidal_position_embedding(int batch_size, int nums_head, int seq_len, int output_dim, Tensor &sin, Tensor &cos) {
    sin.reshape(batch_size, nums_head, seq_len, output_dim);
    cos.reshape(batch_size, nums_head, seq_len, output_dim);
    sin.setDtype(MLLM_TYPE_F16);
    cos.setDtype(MLLM_TYPE_F16);
    sin.alloc();
    cos.alloc();
    for (int n = 0; n < batch_size; ++n) {
        for (int h = 0; h < nums_head; ++h) {
            for (int s = 0; s < seq_len; ++s) {
                for (int d = 0; d < output_dim; d += 2) {
                    int i = (int)d / 2;
                    __fp16 sin_value = std::sin(s / std::pow(10000, 2.0 * i / output_dim));
                    __fp16 cos_value = std::cos(s / std::pow(10000, 2.0 * i / output_dim));
                    ((__fp16 *)sin.hostPtr<void>())[n * h * s * d] = sin_value;
                    ((__fp16 *)cos.hostPtr<void>())[n * h * s * d] = cos_value;
                    if (d + 1 < output_dim) {
                        ((__fp16 *)sin.hostPtr<void>())[n * h * s * (d + 1)] = sin_value;
                        ((__fp16 *)cos.hostPtr<void>())[n * h * s * (d + 1)] = cos_value;
                    }
                }
            }
        }
    }
}

void QNNRoPE::sinusoidal_position_embedding_hf(int batch_size, int nums_head, int seq_len, int output_dim, Tensor &sin, Tensor &cos) {
    sin.reshape(batch_size, nums_head, seq_len, output_dim);
    cos.reshape(batch_size, nums_head, seq_len, output_dim);
    sin.alloc();
    cos.alloc();
    for (int n = 0; n < batch_size; ++n) {
        for (int h = 0; h < nums_head; ++h) {
            for (int s = 0; s < seq_len; ++s) {
                for (int d = 0; d < output_dim; d += 2) {
                    int i = (int)d;
                    if (d >= (int)output_dim / 2) {
                        i = (int)(d - output_dim / 2);
                    }
                    __fp16 sin_value = std::sin(s / std::pow(10000, 2.0 * i / output_dim));
                    __fp16 cos_value = std::cos(s / std::pow(10000, 2.0 * i / output_dim));

                    ((__fp16 *)sin.hostPtr<void>())[n * h * s * d] = sin_value;
                    ((__fp16 *)cos.hostPtr<void>())[n * h * s * d] = cos_value;
                    if (d + 1 < output_dim) {
                        ((__fp16 *)sin.hostPtr<void>())[n * h * s * (d + 1)] = sin_value;
                        ((__fp16 *)cos.hostPtr<void>())[n * h * s * (d + 1)] = cos_value;
                    }
                }
            }
        }
    }
}

} // namespace mllm
