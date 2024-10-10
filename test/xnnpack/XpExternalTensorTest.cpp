#include "Module.hpp"
#include "Types.hpp"
#include "backends/xnnpack/XpWrapper.hpp"
#include <gtest/gtest.h>

using namespace mllm;

class AddModule : public Module {
public:
    AddModule() = default;

    vector<Tensor> Forward(vector<Tensor> inputs, vector<std::any> args) override {
        auto x1 = inputs[0];
        auto x2 = inputs[1];

        auto out = x1 + x2;

        return {out};
    }
};

TEST(XpExternalTensorTest, AddModule) {
    auto model = ::mllm::xnnpack::wrap2xnn<AddModule>(2, 1);

    EXPECT_EQ(Backend::global_backends[MLLM_XNNPACK] != nullptr, true);

    Tensor x1(1, 1, 4, 4, Backend::global_backends[MLLM_XNNPACK], true);
    Tensor x2(1, 1, 4, 4, Backend::global_backends[MLLM_XNNPACK], true);
    x1.setTtype(TensorType::INPUT_TENSOR);
    x2.setTtype(TensorType::INPUT_TENSOR);

    float cnt = 0.f;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            x1.setDataAt<float>(0, 0, i, j, cnt++);
        }
    }

    x1.printData<float>();
    x2.printData<float>();

    auto out = model({x1, x2})[0];

    cnt = 0.f;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_EQ(out.dataAt<float>(0, 0, i, j), cnt++);
        }
    }
}