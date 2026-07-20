/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once
#include "instrumentation.hh"

#include <vector>
#include <cstddef>
#include <cstdlib>

namespace dspugen::util {

template<typename T, size_t N = 65536>
class MemPool final {
public:
    MemPool() noexcept = default;
    MemPool(const MemPool &) = delete;
    MemPool &operator=(const MemPool &) = delete;
    ~MemPool() {
        for (auto *ptr: pool_) {
            ptr->~T();
            free(ptr);
        }
    }
    T *alloc() {
        T *ptr;
        if (pool_.empty()) {
            ptr = static_cast<T*>(malloc(sizeof(T)));
            DSP_INSTRUMENT(++instrumentation::local().poolFreshAllocations);
            return new(ptr) T();
        } else {
            ptr = pool_.back();
            pool_.pop_back();
            DSP_INSTRUMENT(++instrumentation::local().poolReuseAllocations);
            return ptr;
        }
    }
    void release(T *v) {
        v->reset();
        if (pool_.size() < N) {
            pool_.push_back(v);
            DSP_INSTRUMENT(
                auto &highWater = instrumentation::local().poolFreeListHighWater;
                if (pool_.size() > highWater) highWater = pool_.size();
            );
        } else {
            v->~T();
            free(v);
        }
    }
private:
    std::vector<T *> pool_;
};

}
