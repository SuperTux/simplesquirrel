#pragma once

#include "allocators.hpp"

#include <cassert>
#include <functional>
#include <cstring>
#include <string>

namespace ssq {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
        // function_traits and make_function credits by @tinlyx https://stackoverflow.com/a/21665705

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

        template <typename T> struct Param {static const char type = '.';};

        template <> struct Param<bool> {static const std::string type;};
        template <> struct Param<char> {static const std::string type;};
        template <> struct Param<signed char> {static const std::string type;};
        template <> struct Param<short> {static const std::string type;};
        template <> struct Param<int> {static const std::string type;};
        template <> struct Param<long> {static const std::string type;};
        template <> struct Param<unsigned char> {static const std::string type;};
        template <> struct Param<unsigned short> {static const std::string type;};
        template <> struct Param<unsigned int> {static const std::string type;};
        template <> struct Param<unsigned long> {static const std::string type;};
#ifdef _SQ64
        template <> struct Param<long long> {static const std::string type;};
        template <> struct Param<unsigned long long> {static const std::string type;};
#endif
        template <> struct Param<float> {static const char type = 'n';};
        template <> struct Param<double> {static const char type = 'n';};
#ifdef SQUNICODE
        template <> struct Param<std::wstring> {static const char type = 's';};
#else
        template <> struct Param<std::string> {static const char type = 's';};
#endif
        template <> struct Param<Class> {static const char type = 'y';};
        template <> struct Param<Function> {static const char type = 'c';};
        template <> struct Param<Table> {static const char type = 't';};
        template <> struct Param<Array> {static const char type = 'a';};
        template <> struct Param<Instance> {static const char type = 'x';};
        template <> struct Param<std::nullptr_t> {static const char type = 'o';};

        template <typename A>
        static void paramPackerType(std::string& str) {
            str += Param<typename std::remove_const<typename std::remove_reference<A>::type>::type>::type;
        }

        template <typename ...B>
        static void paramPacker(std::string& str) {
            int _[] = { 0, (paramPackerType<B>(str), 0)... };
            (void)_; // Fix unused parameter warning.
        }

        template<typename Ret, typename... Args>
        static void bindUserData(HSQUIRRELVM vm, const std::function<Ret(Args...)>& func) {
            auto funcStruct = reinterpret_cast<detail::FuncPtr<Ret(Args...)>*>(sq_newuserdata(vm, sizeof(detail::FuncPtr<Ret(Args...)>)));
            funcStruct->ptr = new std::function<Ret(Args...)>(func);
            sq_setreleasehook(vm, -1, &detail::funcReleaseHook<Ret, Args...>);
        }

        template<typename T, typename... Args>
        static Object addClass(HSQUIRRELVM vm, const char* name, const std::function<T*(Args...)>& allocator, HSQOBJECT& base, bool release = true) {
            static_assert(std::is_base_of<ExposableClass, T>::value, "Exposed classes must inherit ssq::ExposableClass.");

            static const auto hashCode = typeid(T*).hash_code();
            static const std::size_t nparams = sizeof...(Args);

            Object clsObj(vm);
            
            sq_pushstring(vm, name, strlen(name));
            if (sq_isnull(base)) {
              sq_newclass(vm, SQFalse);
            }
            else {
              sq_pushobject(vm, base);
              assert(sq_gettype(vm, -1) == OT_CLASS);

              sq_newclass(vm, SQTrue);
            }

            HSQOBJECT obj;
            sq_getstackobj(vm, -1, &obj);
            addClassObj(hashCode, obj);

            sq_getstackobj(vm, -1, &clsObj.getRaw());
            sq_addref(vm, &clsObj.getRaw());

            sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(hashCode));

            sq_pushstring(vm, "constructor", -1);
            bindUserData<T*>(vm, allocator);
            std::string params;
            paramPacker<T*, Args...>(params);

            if (release) {
                sq_newclosure(vm, &detail::classAllocator<T, Args...>, 1);
            } else {
                sq_newclosure(vm, &detail::classAllocatorNoRelease<T, Args...>, 1);
            }

            sq_setparamscheck(vm, nparams + 1, params.c_str());

            // Add the constructor method
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind class constructor method!");
            }

            // Add the class
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind class!");
            }

            return clsObj;
        }

        template<typename T>
        static Object addAbstractClass(HSQUIRRELVM vm, const char* name, HSQOBJECT& base) {
            static_assert(std::is_base_of<ExposableClass, T>::value, "Exposed classes must inherit ssq::ExposableClass.");

            static const auto hashCode = typeid(T*).hash_code();
            Object clsObj(vm);

            sq_pushstring(vm, name, strlen(name));
            if (sq_isnull(base)) {
              sq_newclass(vm, SQFalse);
            }
            else {
              sq_pushobject(vm, base);
              assert(sq_gettype(vm, -1) == OT_CLASS);

              sq_newclass(vm, SQTrue);
            }

            HSQOBJECT obj;
            sq_getstackobj(vm, -1, &obj);
            addClassObj(hashCode, obj);

            sq_getstackobj(vm, -1, &clsObj.getRaw());
            sq_addref(vm, &clsObj.getRaw());

            sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(hashCode));

            // Add the class
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind class!");
            }

            return clsObj;
        }

        template<class Ret, class... Args, size_t... Is>
        static Ret callGlobal(HSQUIRRELVM vm, FuncPtr<Ret(Args...)>* funcPtr, index_list<Is...>) {
            (void)vm; // Fix unused parameter warning.
            return funcPtr->ptr->operator()(detail::pop<typename std::remove_reference<Args>::type>(vm, Is + 1)...);
        }

        template<int offet, typename R, typename... Args>
        struct func {
            static SQInteger global(HSQUIRRELVM vm) {
                try {
                    FuncPtr<R(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);

                    push(vm, std::forward<R>(callGlobal(vm, funcPtr, index_range<offet, sizeof...(Args) + offet>())));
                    return 1;
                } catch (std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };
        template<int offet, typename... Args>
        struct func<offet, void, Args...> {
            static SQInteger global(HSQUIRRELVM vm) {
                try {
                    FuncPtr<void(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);

                    callGlobal(vm, funcPtr, index_range<offet, sizeof...(Args) + offet>());
                    return 0;
                } catch (std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };
        template<int offet, typename... Args>
        struct func<offet, SQInteger, Args...> {
            static SQInteger global(HSQUIRRELVM vm) {
                try {
                    FuncPtr<SQInteger(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);

                    return callGlobal(vm, funcPtr, index_range<offet, sizeof...(Args) + offet>());
                } catch (std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };

        template<typename R, typename... Args>
        static void addFunc(HSQUIRRELVM vm, const char* name, const std::function<R(Args...)>& func) {
            static const std::size_t nparams = sizeof...(Args);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            std::string params;
            paramPacker<void, Args...>(params);

            sq_newclosure(vm, &detail::func<1, R, Args...>::global, 1);
            sq_setparamscheck(vm, nparams + 1, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }
        template<typename R, typename... Args>
        static void addFunc(HSQUIRRELVM vm, const char* name, const std::function<R(HSQUIRRELVM, Args...)>& func) {
            static const std::size_t nparams = sizeof...(Args);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            std::string params;
            paramPacker<void, Args...>(params);

            sq_newclosure(vm, &detail::func<0, R, HSQUIRRELVM, Args...>::global, 1);
            sq_setparamscheck(vm, nparams + 1, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }

        template<typename R, typename... Args>
        static void addMemberFunc(HSQUIRRELVM vm, const char* name, const std::function<R(Args...)>& func, bool isStatic) {
            static const std::size_t nparams = sizeof...(Args);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            std::string params;
            paramPacker<Args...>(params);

            sq_newclosure(vm, &detail::func<0, R, Args...>::global, 1);
            sq_setparamscheck(vm, nparams, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }
        template<typename R, typename... Args>
        static void addMemberFunc(HSQUIRRELVM vm, const char* name, const std::function<R(HSQUIRRELVM, Args...)>& func, bool isStatic) {
            static const std::size_t nparams = sizeof...(Args);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            std::string params;
            paramPacker<Args...>(params);

            sq_newclosure(vm, &detail::func<-1, R, HSQUIRRELVM, Args...>::global, 1);
            sq_setparamscheck(vm, nparams, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }
    }
#endif
}
