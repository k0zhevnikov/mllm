#include "Tensor.hpp"

#include <cassert>
#include <cstdlib>
#include <exception>
#include <express/ExpressBase.hpp>
#include "Backend.hpp"
#include "OpDefined.hpp"
#include "Timing.hpp"
#include "Types.hpp"
#include "backends/cpu/CPUTensorFunction.hpp"

#ifdef MLLM_BUILD_XNNPACK_BACKEND
#include "backends/xnnpack/XnnpackBackend.hpp"
#endif

#include <Module.hpp>
#include <memory>
#include <string>
#include <vector>

namespace mllm {

Tensor::Tensor(const int batch, const int head, const int sequence, const int dimension) :
    host_ptr_(), capacity_(0) {
    reshape(batch, head, sequence, dimension);
}
Tensor::Tensor(int batch, int head, int sequence, int dimension, Backend *bn, bool do_alloc) {
    dtype_ = MLLM_TYPE_F32;
    setBackend(bn);
    reshape(batch, head, sequence, dimension);
    if (do_alloc) { alloc(); }
}

Tensor::Tensor(int batch, int head, int sequence, int dimension, BackendType bn_type,
               bool do_alloc) {
    setBackend(Backend::global_backends[bn_type]);
    reshape(batch, head, sequence, dimension);
    if (do_alloc) { alloc(); }
}

Tensor::Tensor(const vector<int> &shape) :
    host_ptr_(), capacity_(0) {
    reshape(shape);
}

Tensor::Tensor(int value, Backend *bn) {
    dtype_ = MLLM_TYPE_F32;
    setBackend(bn);
    reshape(1, 1, 1, 1);
    alloc();
    shouldInGraphs() = false;
    setDataAt<float>(0, 0, 0, 0, (float)value);
}

Tensor::Tensor(int value, BackendType bn_type) {
    dtype_ = MLLM_TYPE_F32;
    setBackend(Backend::global_backends[bn_type]);
    reshape(1, 1, 1, 1);
    alloc();
    shouldInGraphs() = false;
    setDataAt<float>(0, 0, 0, 0, (float)value);
}

bool Tensor::reshape(const int batch, const int head, const int sequence, const int dimension) {
    vector<int> shape(4);
    shape[chls()[BATCH]] = batch;
    shape[chls()[HEAD]] = head;
    shape[chls()[SEQUENCE]] = sequence;
    shape[chls()[DIMENSION]] = dimension;
    return reshape(shape);
}

void Tensor::alloc() {
    if (aggregated_) { return; }
    assert(backend_ != nullptr);
    if (masterTensor() != nullptr) { return; }
    if (!shape_offset_.empty() && !shape_master_.empty()) { return; }
    if (allocated_ != count_) {
        if (host_ptr_ != nullptr) {
            backend_->free(host_ptr_);
            host_ptr_ = nullptr;
        }
        if (count_ > 0) {
            // Arm neon should be 16B
            // AVX 128 should be 16B
            // AVX 256 should be 32B
#if defined(__ARM_NEON) && defined(__aarch64__)
            backend_->alloc(&host_ptr_, cntSize(), 16);
#else
            backend_->alloc(&host_ptr_, cntSize(), 32);
#endif
        }
        allocated_ = count_;
    }
}

bool Tensor::reshape(const int batch, const int channel, const int time, const int height,
                     const int width) {
    if (ctype_ != BTHWC) { ctype_ = BCTHW; }
    vector<int> shape(5);
    shape[chls()[BATCH]] = batch;
    shape[chls()[CHANNLE]] = channel;
    shape[chls()[TIME]] = time;
    shape[chls()[HEIGHT]] = height;
    shape[chls()[WIDTH]] = width;
    return reshape(shape);
}

TensorStatus Tensor::tensor_status;

Tensor &Tensor::getFunc(const std::string &suffix, const TensorFuncType type,
                        vector<float> float_args, vector<Tensor *> other_tensors) {
    assert(module() != nullptr);
    auto &module_tensors = module()->activation_tensors;
    const std::string next_name = name_ + "-" + suffix;
    // if (module_tensors.find(name_) == module_tensors.end()) {
    //     module_tensors[name_] = std::shared_ptr<Tensor>(this, [](Tensor *) {});
    // }
    if (module_tensors.find(next_name) == module_tensors.end()) {
        module_tensors[next_name] = std::make_shared<Tensor>(backend_);
        module_tensors[next_name]->setName(next_name);
        module_tensors[next_name]->setModule(module());
    }
    if (module()->doLoad) { return *module_tensors[next_name]; }
    TensorFunction *func = backend_->funcCreate(type);
    std::vector<Tensor *> tensorPtrs = {module_tensors[name_].get()};
    for (auto &other_tensor : other_tensors) { tensorPtrs.push_back(other_tensor); }
#ifdef DEBUGOPTIME
    auto start_t = mllm_time_us();
#endif
    switch (Tensor::tensor_status) {
    case TENSOR_STATIC_INIT: {
        func->setup({module_tensors[next_name].get()}, tensorPtrs, float_args);
        break;
    }
    case TENSOR_STATIC_READY: {
        func->execute({module_tensors[next_name].get()}, tensorPtrs, float_args);
        break;
    }
    default: {
    }
    }
#ifdef DEBUGOPTIME
    if (Tensor::tensor_status == TENSOR_STATIC_READY) {
        auto end_t = mllm_time_us();
        std::cout << next_name << " | " << Tensor::tensor_status
                  << " time: " << (end_t - start_t) / 1000.0F << "ms" << std::endl;
    }
#endif
#ifdef DEBUGSAVETENSOR
    module_tensors[next_name]->saveNData<float>();
#endif
#ifdef DEBUGOPTIME
    if (Tensor::tensor_status == TENSOR_STATIC_READY) {
        std::cout << module_tensors[next_name]->name() << std::endl;
    }
#endif
    return *module_tensors[next_name];
}

std::vector<std::reference_wrapper<Tensor>> Tensor::getStaticFunc(vector<std::string> out_names,
                                                                  const TensorFuncType type,
                                                                  vector<float> float_args,
                                                                  vector<Tensor *> input_tensors) {
    Module *module;
    if (!input_tensors.empty()) {
        module = input_tensors[0]->module();
    } else {
        module = Module::llm_model_ptr;
    }
    assert(module != nullptr);
    auto &module_tensors = module->activation_tensors;
    auto *backend_h = Backend::global_backends[MLLM_CPU];
    if (!input_tensors.empty() && input_tensors[0]->backend_ != nullptr) {
        backend_h = input_tensors[0]->backend();
    }
    for (auto out_name : out_names) {
        if (module_tensors.find(out_name) == module_tensors.end()) {
            module_tensors[out_name] = std::make_shared<Tensor>(backend_h);
            module_tensors[out_name]->setName(out_name);
            module_tensors[out_name]->setModule(module);
        }
    }
    if (module->doLoad) {
        std::vector<std::reference_wrapper<Tensor>> results;
        for (auto out_name : out_names) { results.push_back(*module_tensors[out_name]); }
        return results;
    }
    TensorFunction *func = backend_h->funcCreate(type);
    // std::vector<Tensor *> tensorPtrs;
    // for (auto input_tensor : input_tensors){ tensorPtrs.push_back(module_tensors[input_tensor->name()].get()); }
    std::vector<Tensor *> outPtrs;
    for (auto out_name : out_names) { outPtrs.push_back(module_tensors[out_name].get()); }
#ifdef DEBUGOPTIME
    auto start_t = mllm_time_us();
#endif
    switch (Tensor::tensor_status) {
    case TENSOR_STATIC_INIT: {
        func->setup(outPtrs, input_tensors, float_args);
        break;
    }
    case TENSOR_STATIC_READY: {
        func->execute(outPtrs, input_tensors, float_args);
        break;
    }
    default: {
    }
    }
#ifdef DEBUGOPTIME
    if (Tensor::tensor_status == TENSOR_STATIC_READY) {
        auto end_t = mllm_time_us();
        std::cout << out_names[0] << " | " << Tensor::tensor_status
                  << " time: " << (end_t - start_t) / 1000.0F << "ms" << std::endl;
    }
#endif
#ifdef DEBUGSAVETENSOR
    for (auto out_name : out_names) { module_tensors[out_name]->saveNData<float>(); }
#endif
    std::vector<std::reference_wrapper<Tensor>> results;
    for (auto out_name : out_names) { results.push_back(*module_tensors[out_name]); }
    return results;
}

Tensor &Tensor::operator+(float data) {
    return getFunc("add", FUNC_ADD, {data});
}

Tensor &Tensor::operator-(float data) {
    return getFunc("sub", FUNC_SUB, {data});
}

Tensor &Tensor::operator*(float data) {
    return getFunc("mul", FUNC_MUL, {data});
}

Tensor &Tensor::operator/(float data) {
    return getFunc("div", FUNC_DIV, {data});
}

Tensor &Tensor::operator/(double data) {
    return getFunc("div", FUNC_DIV, {static_cast<float>(data)});
}

Tensor &Tensor::operator+(Tensor &other) {
    return getFunc("TTadd", FUNC_TTADD, {}, {&other});
}

Tensor &Tensor::operator-(Tensor &other) {
    return getFunc("TTsub", FUNC_TTSUB, {}, {&other});
}

Tensor &Tensor::operator*(Tensor &other) {
    return getFunc("TTmul", FUNC_TTMUL, {}, {&other});
}

Tensor &Tensor::operator/(Tensor &other) {
    return getFunc("TTdiv", FUNC_TTDIV, {}, {&other});
}

Tensor &Tensor::mean(Chl axis) {
    return getFunc("mean", FUNC_MEAN, {(float)axis});
}

Tensor &Tensor::view(int b, int h, int s, int d) {
    return getFunc("view", FUNC_VIEW, {(float)b, (float)h, (float)s, (float)d});
}

Tensor &Tensor::flatten(Chl axis_start, Chl axis_end) {
    return getFunc("flatten", FUNC_FLATTEN, {(float)axis_start, (float)axis_end});
}

Tensor &Tensor::transpose(vector<std::pair<Chl, Chl>> axiss) {
    vector<float> axis_s;
    for (auto &axis : axiss) {
        axis_s.push_back((float)axis.first);
        axis_s.push_back((float)axis.second);
    }
    return getFunc("transpose", FUNC_TRANPOSE, axis_s);
}

Tensor &Tensor::clip(vector<int> b, vector<int> h, vector<int> s, vector<int> d) {
    vector<float> axis_s;
    axis_s.push_back(b.size());
    axis_s.push_back(h.size());
    axis_s.push_back(s.size());
    axis_s.push_back(d.size());
    for (auto &axis : b) { axis_s.push_back((float)axis); }
    for (auto &axis : h) { axis_s.push_back((float)axis); }
    for (auto &axis : s) { axis_s.push_back((float)axis); }
    for (auto &axis : d) { axis_s.push_back((float)axis); }
    return getFunc("clip", FUNC_CLIP, axis_s);
}

Tensor &Tensor::clip(Chl keep_axis, vector<int> b, vector<int> h, vector<int> s, vector<int> d) {
    vector<float> axis_s = {(float)keep_axis};
    axis_s.push_back(b.size());
    axis_s.push_back(h.size());
    axis_s.push_back(s.size());
    axis_s.push_back(d.size());
    for (auto &axis : b) { axis_s.push_back((float)axis); }
    for (auto &axis : h) { axis_s.push_back((float)axis); }
    for (auto &axis : s) { axis_s.push_back((float)axis); }
    for (auto &axis : d) { axis_s.push_back((float)axis); }
    return getFunc("clipaxis", FUNC_CLIPAXIS, axis_s);
}

Tensor &Tensor::norm(int L_n) {
    return getFunc("norm", FUNC_NORM, {(float)L_n});
}

Tensor &Tensor::where(float value, Chl axis) {
    return getFunc("where", FUNC_WHERE, {(float)value, (float)axis});
}

Tensor &Tensor::cat(vector<Tensor> input_tensors, Chl axis) {
    Module *module = input_tensors[0].module();
    vector<Tensor *> inputs = {};
    for (const auto &input_tensor : input_tensors) {
        inputs.push_back(module->activation_tensors[input_tensor.name()].get());
    }
    return getStaticFunc({input_tensors[0].name() + "-cat"}, FUNC_CAT, {(float)axis}, inputs)[0].get();
}

Tensor &Tensor::mm(Tensor &input0, Tensor &input1) {
    Module *module = input0.module();
    return getStaticFunc(
               {input0.name() + "-mm-" + input1.name()}, FUNC_MM, {},
               {module->activation_tensors[input0.name()].get(), module->activation_tensors[input1.name()].get()})[0]
        .get();
}

Tensor &Tensor::range(int start, int end) {
    return getStaticFunc({"range-" + std::to_string(start) + "-" + std::to_string(end)}, FUNC_RANGE,
                         {(float)start, (float)end}, {})[0]
        .get();
}

vector<std::reference_wrapper<Tensor>> Tensor::split(Tensor &input, std::vector<int> each_dims,
                                                     Chl split_dim, int same_dim_size) {
    vector<std::string> next_names;
    std::vector<float> args;
    for (int i = 0; i < each_dims.size(); ++i) {
        args.push_back(each_dims[i]);
        next_names.push_back(input.name() + ".split-" + std::to_string(i));
    }
    args.push_back(split_dim);
    args.push_back(same_dim_size);
    Module *module = input.module();
    return getStaticFunc(next_names, FUNC_SPLIT, args,
                         {module->activation_tensors[input.name()].get()});
}

uint32_t &Tensor::uuid() {
    return uuid_;
}

TensorType &Tensor::xnnTensorType() {
    return xnn_tensor_type_;
}

void Tensor::forceResetHostPointer(void *ptr) {
    host_ptr_ = ptr;
}
} // namespace mllm