#ifndef MLLM_QNNMEMORY_SYSTEM_H
#define MLLM_QNNMEMORY_SYSTEM_H

#include "Logger.hpp"
#include "MemoryManager.hpp"
#include "PAL/DynamicLoading.hpp"
#include "DynamicLoadUtil.hpp"
#include "QnnTypes.h"
#include <cstddef>
#include <iostream>
#include <set>
#include <vector>
#include <dlfcn.h>

namespace mllm {

typedef void *(*RpcMemAllocFn_t)(int, uint32_t, int);
typedef void (*RpcMemFreeFn_t)(void *);
typedef int (*RpcMemToFdFn_t)(void *);
typedef Qnn_ErrorHandle_t (*QnnInterfaceGetProvidersFn_t)(const QnnInterface_t ***providerList,
                                                          uint32_t *numProviders);

class QNNMemoryManager : public MemoryManager {
public:
    QNNMemoryManager();
    ~QNNMemoryManager();

    void alloc(void **ptr, size_t size, size_t alignment) override;
    void free(void *ptr) override;

    void setQnnInterfaceAndContext(QNN_INTERFACE_VER_TYPE &qnnInterface, Qnn_ContextHandle_t &context) {
        this->qnnInterface_ = &qnnInterface;
        this->context_ = &context;

        if (context == nullptr) {
            QNN_ERROR("qnnInterface or context is nullptr");
            exit(1);
        }
    }

    void registerQnnTensor(void *ptr, Qnn_Tensor_t &qnnTensor);

private:
    QNN_INTERFACE_VER_TYPE *qnnInterface_ = nullptr;
    Qnn_ContextHandle_t *context_ = nullptr;

    std::vector<Qnn_MemHandle_t> qnnMemHandleList_;
    std::vector<void *> qnnMemPtrList_;
    // memHandle set, to check if the ptr is allocted by rpcmem_alloc
    std::set<void *> qnnMemPtrMap_;

    RpcMemAllocFn_t rpcmem_alloc;
    RpcMemFreeFn_t rpcmem_free;
    RpcMemToFdFn_t rpcmem_to_fd;
};

} // namespace mllm
#endif