/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <vector>
#include <cstddef>
#include <cstdlib>

namespace dspugen::util {

template<typename T, size_t N = 65536>
class MemPool final {
public:
    MemPool() noexcept {
        pool_ = new std::vector<T *>;
        pool_->reserve(N);
    }
    template<typename...V>
    T *alloc(const V &...v) {
        T *ptr;
        if (pool_->empty()) {
            ptr = static_cast<T*>(malloc(sizeof(T)));
        } else {
            ptr = pool_->back();
            pool_->pop_back();
        }
        return new(ptr) T(v...);
    }
    void release(T *v) {
        if (pool_->size() < N) {
            v->~T();
            pool_->push_back(v);
        } else {
            delete v;
        }
    }
private:
    std::vector<T *> *pool_;
};

}
