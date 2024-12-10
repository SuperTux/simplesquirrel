#pragma once

#include "util.hpp"

namespace ssq {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
        template<class T>
        static T* defaultClassAllocator() {
            return new T();
        }

        template<class T>
        static SQInteger classDestructor(SQUserPointer ptr, SQInteger size) {
            T* p = static_cast<T*>(ptr);
            delete p;
            return 0;
        }

        template<class T>
        static SQInteger classPtrDestructor(SQUserPointer ptr, SQInteger size) {
            T** p = static_cast<T**>(ptr);
            delete *p;
            return 0;
        }


        template<class Ret, typename... Args>
        static SQInteger funcReleaseHook(SQUserPointer p, SQInteger size) {
            auto funcPtr = reinterpret_cast<FuncPtr<Ret(Args...)>*>(p);
            delete funcPtr->ptr;
            return 0;
        }

        template<typename... Args>
        static SQInteger defaultArgsReleaseHook(SQUserPointer p, SQInteger size) {
            auto defaultArgsPtr = reinterpret_cast<DefaultArgsPtr<Args...>*>(p);
            delete defaultArgsPtr->ptr;
            return 0;
        }
    }
#endif
}
