#pragma once

#include "object.hpp"

#include <functional>

namespace ssq {
    template<typename... Args>
    using DefaultArgumentsImpl = std::tuple<Args...>;

    template<typename... Args>
    using DefaultArguments = DefaultArgumentsImpl<typename std::remove_reference<Args>::type...>;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
        template <int... Is>
        struct index_list {
        };

        // Declare primary template for index range builder
        template <int MIN, int N, int... Is>
        struct range_builder;

        // Base step
        template <int MIN, int... Is>
        struct range_builder<MIN, MIN, Is...> {
            typedef index_list<Is...> type;
        };

        // Induction step
        template <int MIN, int N, int... Is>
        struct range_builder : public range_builder<MIN, N - 1, N - 1, Is...> {
        };

        // Meta-function that returns a [MIN, MAX) index range
        template<int MIN, int MAX>
        using index_range = typename detail::range_builder<MIN, MAX>::type;


        /* function_traits and make_function credits by @tinlyx: https://stackoverflow.com/a/21665705 */

        // For generic types that are functors, delegate to its 'operator()'
        template <typename T>
        struct function_traits
            : public function_traits<decltype(&T::operator())>
        {};

        // for pointers to member function
        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const> {
            //enum { arity = sizeof...(Args) };
            typedef std::function<ReturnType (Args...)> f_type;
        };

        // for pointers to member function
        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) > {
            typedef std::function<ReturnType (Args...)> f_type;
        };

        // for function pointers
        template <typename ReturnType, typename... Args>
        struct function_traits<ReturnType (*)(Args...)>  {
            typedef std::function<ReturnType (Args...)> f_type;
        };

        template <typename L>
        inline typename function_traits<L>::f_type make_function(L l){
            return static_cast<typename function_traits<L>::f_type>(l);
        }


        template<class Ret>
        struct FuncPtr {
            const std::function<Ret()>* ptr;
        };

        template<class Ret, typename... Args>
        struct FuncPtr<Ret(Args...)> {
            const std::function<Ret(Args...)>* ptr;
        };

        template<typename... Args>
        struct DefaultArgsPtr {
            const DefaultArguments<Args...>* ptr;
        };
    }

    /**
     * @ingroup simplesquirrel
     */
    template<typename T>
    inline T Object::to() const {
        sq_pushobject(vm, obj);
        try {
            auto ret = detail::pop<T>(vm, -1);
            sq_pop(vm, 1);
            return ret;
        } catch (...) {
            sq_pop(vm, 1);
            std::rethrow_exception(std::current_exception());
        }
    }
#endif
}
