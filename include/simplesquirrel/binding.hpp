#pragma once

#include "args.hpp"

#include <cassert>
#include <functional>
#include <cstring>
#include <string>

namespace ssq {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
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

        template<typename... Args>
        static typename std::enable_if<!sizeof...(Args), void>::type
        bindUserData(HSQUIRRELVM, const DefaultArgumentsImpl<Args...>&) {}

        template<typename... Args>
        static typename std::enable_if<sizeof...(Args), void>::type
        bindUserData(HSQUIRRELVM vm, const DefaultArgumentsImpl<Args...>& defaultArgs) {
            auto defaultArgsStruct = reinterpret_cast<detail::DefaultArgsPtr<Args...>*>(sq_newuserdata(vm, sizeof(detail::DefaultArgsPtr<Args...>)));
            defaultArgsStruct->ptr = new DefaultArguments<Args...>(std::move(defaultArgs));
            sq_setreleasehook(vm, -1, &detail::defaultArgsReleaseHook<Args...>);
        }


        template<class Ret, class... Args, int... Is>
        static inline Ret callFuncImpl(HSQUIRRELVM vm, const std::function<Ret(Args...)>* func, index_list<Is...>) {
            (void)vm; // Fix unused parameter warning.
            return func->operator()(detail::pop<typename std::remove_reference<Args>::type>(vm, Is + 1)...);
        }

        template<class Ret, class... Args, class... DefaultArgs, int... Is, int... DefIs>
        static inline Ret callFuncImpl(HSQUIRRELVM vm, const std::function<Ret(Args...)>* func, const DefaultArgumentsImpl<DefaultArgs...>& defaultArgs,
                                       index_list<Is...>, index_list<DefIs...>) {
            (void)vm; // Fix unused parameter warning.
            return func->operator()(detail::pop<DefIs, typename std::remove_reference<Args>::type>(vm, Is + 1, defaultArgs)...);
        }

        template<int offset, class... DefaultArgs, class Ret, class... Args>
        static inline typename std::enable_if<!sizeof...(DefaultArgs), Ret>::type
        callFunc(HSQUIRRELVM vm, FuncPtr<Ret(Args...)>* funcPtr) {
            return callFuncImpl(vm, funcPtr->ptr,
                    index_range<offset, sizeof...(Args) + offset>());
        }

        template<int offset, class... DefaultArgs, class Ret, class... Args>
        static inline typename std::enable_if<sizeof...(DefaultArgs), Ret>::type
        callFunc(HSQUIRRELVM vm, FuncPtr<Ret(Args...)>* funcPtr) {
            constexpr std::size_t nparams = sizeof...(Args);
            constexpr std::size_t ndefparams = sizeof...(DefaultArgs);

            DefaultArgsPtr<DefaultArgs...>* defaultArgsPtr;
            sq_getuserdata(vm, -1, reinterpret_cast<void**>(&defaultArgsPtr), nullptr);
            sq_pop(vm, 1);

            return callFuncImpl(vm, funcPtr->ptr, *defaultArgsPtr->ptr,
                    index_range<offset, nparams + offset>(),
                    index_range<ndefparams - nparams, ndefparams>());
        }


        template<class T, class DefaultArgs, class... Args>
        struct classAllocatorBinding;

        template<class T, class... Args, class... DefaultArgs>
        struct classAllocatorBinding<T, DefaultArgumentsImpl<DefaultArgs...>, Args...> {
            static SQInteger call(HSQUIRRELVM vm) {
                FuncPtr<T*(Args...)>* funcPtr;
                sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);
                sq_pop(vm, 1);

                T* p = detail::callFunc<1, DefaultArgs...>(vm, funcPtr);
                sq_setinstanceup(vm, 1, p);
                sq_setreleasehook(vm, 1, &detail::classDestructor<T>);

                sq_getclass(vm, 1);
                sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typeid(T*).hash_code()));
                sq_pop(vm, 1); // Pop class

                return sizeof...(Args);
            }
        };

        template<class T, class DefaultArgs, class... Args>
        struct classAllocatorNoReleaseBinding;

        template<class T, class... Args, class... DefaultArgs>
        struct classAllocatorNoReleaseBinding<T, DefaultArgumentsImpl<DefaultArgs...>, Args...> {
            static SQInteger call(HSQUIRRELVM vm) {
                FuncPtr<T*(Args...)>* funcPtr;
                sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);
                sq_pop(vm, 1);

                T* p = detail::callFunc<1, DefaultArgs...>(vm, funcPtr);
                sq_setinstanceup(vm, 1, p);

                sq_getclass(vm, 1);
                sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typeid(T*).hash_code()));
                sq_pop(vm, 1); // Pop class

                return sizeof...(Args);
            }
        };

        template<int offset, typename R, typename DefaultArgs, typename... Args>
        struct funcBinding;

        /* Functions with return values */
        template<int offset, typename R, typename... Args, typename... DefaultArgs>
        struct funcBinding<offset, R, DefaultArgumentsImpl<DefaultArgs...>, Args...> {
            static SQInteger call(HSQUIRRELVM vm) {
                try {
                    FuncPtr<R(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);
                    sq_pop(vm, 1);

                    push(vm, std::forward<R>(detail::callFunc<offset, DefaultArgs...>(vm, funcPtr)));
                    return 1;
                } catch (const std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };
        template<int offset, typename R, typename... Args, typename... DefaultArgs>
        struct funcBinding<offset, std::vector<R>, DefaultArgumentsImpl<DefaultArgs...>, Args...> {
            static SQInteger call(HSQUIRRELVM vm) {
                try {
                    FuncPtr<std::vector<R>(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);
                    sq_pop(vm, 1);

                    push(vm, std::forward<std::vector<R>>(detail::callFunc<offset, DefaultArgs...>(vm, funcPtr)));
                    return 1;
                } catch (const std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };
        /* Function without a return value */
        template<int offset, typename... Args, typename... DefaultArgs>
        struct funcBinding<offset, void, DefaultArgumentsImpl<DefaultArgs...>, Args...> {
            static SQInteger call(HSQUIRRELVM vm) {
                try {
                    FuncPtr<void(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);
                    sq_pop(vm, 1);

                    detail::callFunc<offset, DefaultArgs...>(vm, funcPtr);
                    return 0;
                } catch (const std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };
        /* Function with a return value, signifying whether it has pushed returned data to the Squirrel stack */
        template<int offset, typename... Args, typename... DefaultArgs>
        struct funcBinding<offset, SQInteger, DefaultArgumentsImpl<DefaultArgs...>, Args...> {
            static SQInteger call(HSQUIRRELVM vm) {
                try {
                    FuncPtr<SQInteger(Args...)>* funcPtr;
                    sq_getuserdata(vm, -1, reinterpret_cast<void**>(&funcPtr), nullptr);
                    sq_pop(vm, 1);

                    return detail::callFunc<offset, DefaultArgs...>(vm, funcPtr);
                } catch (const std::exception& e) {
                    return sq_throwerror(vm, e.what());
                }
            }
        };


        template<typename T, typename... Args, typename... DefaultArgs>
        static Object addClass(HSQUIRRELVM vm, const char* name, const std::function<T*(Args...)>& allocator,
                               const DefaultArgumentsImpl<DefaultArgs...> defaultArgs, HSQOBJECT& base, bool release = true) {
            static_assert(std::is_base_of<ExposableClass, T>::value, "Exposed classes must inherit ssq::ExposableClass.");

            static const auto hashCode = typeid(T*).hash_code();
            constexpr std::size_t nparams = sizeof...(Args);
            constexpr std::size_t ndefparams = sizeof...(DefaultArgs);

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
            bindUserData(vm, defaultArgs);

            std::string params;
            paramPacker<T*, Args...>(params);

            if (release) {
                sq_newclosure(vm, &detail::classAllocatorBinding<T, DefaultArgumentsImpl<DefaultArgs...>, Args...>::call, ndefparams ? 2 : 1);
            } else {
                sq_newclosure(vm, &detail::classAllocatorNoReleaseBinding<T, DefaultArgumentsImpl<DefaultArgs...>, Args...>::call, ndefparams ? 2 : 1);
            }

            sq_setparamscheck(vm, nparams - ndefparams + 1, nparams + 1, params.c_str());

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


        template<typename R, typename... Args, typename... DefaultArgs>
        static void addFunc(HSQUIRRELVM vm, const char* name, const std::function<R(Args...)>& func,
                            const DefaultArgumentsImpl<DefaultArgs...>& defaultArgs) {
            constexpr std::size_t nparams = sizeof...(Args);
            constexpr std::size_t ndefparams = sizeof...(DefaultArgs);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            bindUserData(vm, defaultArgs);

            std::string params;
            paramPacker<void, Args...>(params);

            sq_newclosure(vm, &detail::funcBinding<1, R, DefaultArgumentsImpl<DefaultArgs...>, Args...>::call, ndefparams ? 2 : 1);
            sq_setparamscheck(vm, nparams - ndefparams + 1, nparams + 1, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }
        template<typename R, typename... Args, typename... DefaultArgs>
        static void addFunc(HSQUIRRELVM vm, const char* name, const std::function<R(HSQUIRRELVM, Args...)>& func,
                            const DefaultArgumentsImpl<DefaultArgs...>& defaultArgs) {
            constexpr std::size_t nparams = sizeof...(Args);
            constexpr std::size_t ndefparams = sizeof...(DefaultArgs);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            bindUserData(vm, defaultArgs);

            std::string params;
            paramPacker<void, Args...>(params);

            sq_newclosure(vm, &detail::funcBinding<0, R, DefaultArgumentsImpl<DefaultArgs...>, HSQUIRRELVM, Args...>::call, ndefparams ? 2 : 1);
            sq_setparamscheck(vm, nparams - ndefparams + 1, nparams + 1, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }

        template<typename R, typename... Args, typename... DefaultArgs>
        static void addMemberFunc(HSQUIRRELVM vm, const char* name, const std::function<R(Args...)>& func,
                                  const DefaultArgumentsImpl<DefaultArgs...>& defaultArgs, bool isStatic) {
            constexpr std::size_t nparams = sizeof...(Args);
            constexpr std::size_t ndefparams = sizeof...(DefaultArgs);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            bindUserData(vm, defaultArgs);

            std::string params;
            paramPacker<Args...>(params);

            sq_newclosure(vm, &detail::funcBinding<0, R, DefaultArgumentsImpl<DefaultArgs...>, Args...>::call, ndefparams ? 2 : 1);
            sq_setparamscheck(vm, nparams - ndefparams, nparams, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }
        template<typename R, typename... Args, typename... DefaultArgs>
        static void addMemberFunc(HSQUIRRELVM vm, const char* name, const std::function<R(HSQUIRRELVM, Args...)>& func,
                                  const DefaultArgumentsImpl<DefaultArgs...>& defaultArgs, bool isStatic) {
            constexpr std::size_t nparams = sizeof...(Args);
            constexpr std::size_t ndefparams = sizeof...(DefaultArgs);

            sq_pushstring(vm, name, strlen(name));

            bindUserData(vm, func);
            bindUserData(vm, defaultArgs);

            std::string params;
            paramPacker<Args...>(params);

            sq_newclosure(vm, &detail::funcBinding<-1, R, DefaultArgumentsImpl<DefaultArgs...>, HSQUIRRELVM, Args...>::call, ndefparams ? 2 : 1);
            sq_setparamscheck(vm, nparams - ndefparams, nparams, params.c_str());
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to bind function!");
            }
        }
    }
#endif
}
