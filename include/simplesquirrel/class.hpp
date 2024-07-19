#pragma once

#include <functional>
#include "function.hpp"
#include "binding.hpp"

namespace ssq {
    /**
    * @brief Squirrel class object
    * @ingroup simplesquirrel
    */
    class SSQ_API Class : public Object {
    public:
        /**
        * @brief Constructor helper class
        */
        template<class Signature>
        struct Ctor;

        template<class T, class... Args>
        struct Ctor<T(Args...)> {
            static T* allocate(Args&&... args) {
                return new T(std::forward<Args>(args)...);
            }
        };
		/**
        * @brief Creates an empty invalid class
        */
		Class();
        /**
        * @brief Destructor
        */
        virtual ~Class() override = default;
        /**
        * @brief Creates a new empty class
        */
        Class(HSQUIRRELVM vm);
        /**
        * @brief Converts Object to class object
        * @throws TypeException if the Object is not type of a class
        */
        explicit Class(const Object& object);
        /**
        * @brief Copy constructor
        */
        Class(const Class& other);
        /**
        * @brief Move constructor
        */
        Class(Class&& other) NOEXCEPT;
        /**
        * @brief Swaps two classes
        */
        void swap(Class& other) NOEXCEPT;
        /**
        * @brief Finds a function in this class
        * @throws RuntimeException if VM is invalid
        * @throws NotFoundException if function was not found
        * @throws TypeException if the object found is not a function
        */
        Function findFunc(const char* name) const;
        /**
        * @brief Adds a new function type to this class
        * @param name Name of the function to add
        * @param func std::function that contains "this" pointer to the class type followed
        * by any number of arguments with any type
        * @param defaultArgs A list of default values for last optional arguments
        * @param isStatic Determines whether the function is going to be static
        * @throws RuntimeException if VM is invalid
        * @returns Function object references the added function
        */
        template <typename Return, typename Object, typename... Args, typename... DefaultArgs>
        Function addFunc(const char* name, const std::function<Return(Object*, Args...)>& func, const DefaultArguments<DefaultArgs...> defaultArgs = {}, bool isStatic = false) {
            if (vm == nullptr) throw RuntimeException(nullptr, "VM is not initialised");
            Function ret(vm);
            sq_pushobject(vm, obj);
            detail::addMemberFunc(vm, name, func, defaultArgs, isStatic);
            sq_pop(vm, 1);
            return ret;
        }
        /**
        * @brief Adds a new function type to this class
        * @param name Name of the function to add
        * @param memfunc Pointer to member function
        * @param defaultArgs A list of default values for last optional arguments
        * @param isStatic Determines whether the function is going to be static
        * @throws RuntimeException if VM is invalid
        * @returns Function object references the added function
        */
        template <typename Return, typename Object, typename... Args, typename... DefaultArgs>
        Function addFunc(const char* name, Return(Object::*memfunc)(Args...), const DefaultArguments<DefaultArgs...> defaultArgs = {}, bool isStatic = false) {
            auto func = std::function<Return(Object*, Args...)>(std::mem_fn(memfunc));
            return addFunc(name, func, defaultArgs, isStatic);
        }
        /**
        * @brief Adds a new function type to this class
        * @param name Name of the function to add
        * @param memfunc Pointer to constant member function
        * @param defaultArgs A list of default values for last optional arguments
        * @param isStatic Determines whether the function is going to be static
        * @throws RuntimeException if VM is invalid
        * @returns Function object references the added function
        */
        template <typename Return, typename Object, typename... Args, typename... DefaultArgs>
        Function addFunc(const char* name, Return(Object::*memfunc)(Args...) const, const DefaultArguments<DefaultArgs...> defaultArgs = {}, bool isStatic = false) {
            auto func = std::function<Return(Object*, Args...)>(std::mem_fn(memfunc));
            return addFunc(name, func, defaultArgs, isStatic);
        }
        /**
        * @brief Adds a new function type to this class
        * @param name Name of the function to add
        * @param lambda Lambda function that contains "this" pointer to the class type followed
        * by any number of arguments with any type
        * @param defaultArgs A list of default values for last optional arguments
        * @param isStatic Determines whether the function is going to be static
        * @throws RuntimeException if VM is invalid
        * @returns Function object references the added function
        */
        template<typename F, typename... DefaultArgs>
        Function addFunc(const char* name, const F& lambda, const DefaultArguments<DefaultArgs...> defaultArgs = {}, bool isStatic = false) {
            return addFunc(name, detail::make_function(lambda), defaultArgs, isStatic);
        }
        template<typename T, typename V>
        void addVar(const std::string& name, V T::* ptr, bool isStatic = false) {
            bindVar<T, V>(name, ptr, tableGet.getRaw(), varGetStub<T, V>, isStatic);
            bindVar<T, V>(name, ptr, tableSet.getRaw(), varSetStub<T, V>, isStatic);
        }
        template<typename T, typename V>
        void addConstVar(const std::string& name, V T::* ptr, bool isStatic = false) {
            bindVar<T, V>(name, ptr, tableGet.getRaw(), varGetStub<T, V>, isStatic);
        }
        /**
        * @brief Copy assingment operator
        */
        Class& operator = (const Class& other);
        /**
        * @brief Move assingment operator
        */
        Class& operator = (Class&& other) NOEXCEPT;
    protected:
        void findTable(const char* name, Object& table, SQFUNCTION dlg) const;
        static SQInteger dlgGetStub(HSQUIRRELVM vm);
        static SQInteger dlgSetStub(HSQUIRRELVM vm);

        template<typename T, typename V>
        void bindVar(const std::string& name, V T::* ptr, HSQOBJECT& table, SQFUNCTION stub, bool isStatic) {
            auto rst = sq_gettop(vm);

            sq_pushobject(vm, table);
            sq_pushstring(vm, name.c_str(), name.size());

            auto vp = sq_newuserdata(vm, sizeof(ptr));
            std::memcpy(vp, &ptr, sizeof(ptr));

            sq_newclosure(vm, stub, 1);

            if (SQ_FAILED(sq_newslot(vm, -3, isStatic))) {
                throw RuntimeException(vm, "Failed to bind member variable to class!");
            }

            sq_pop(vm, 1);
            sq_settop(vm, rst);
        }

        template<typename T, typename V>
        static SQInteger varGetStub(HSQUIRRELVM vm_) {
            T* ptr;
            sq_getinstanceup(vm_, 1, reinterpret_cast<SQUserPointer*>(&ptr), nullptr, SQTrue);

            typedef V T::*M;
            M* memberPtr = nullptr;
            sq_getuserdata(vm_, -1, reinterpret_cast<SQUserPointer*>(&memberPtr), nullptr);
            M member = *memberPtr;

            detail::push(vm_, ptr->*member);
            return 1;
        }

        template<typename T, typename V>
        static SQInteger varSetStub(HSQUIRRELVM vm_) {
            T* ptr;
            sq_getinstanceup(vm_, 1, reinterpret_cast<SQUserPointer*>(&ptr), nullptr, SQTrue);

            typedef V T::*M;
            M* memberPtr = nullptr;
            sq_getuserdata(vm_, -1, reinterpret_cast<SQUserPointer*>(&memberPtr), nullptr);
            M member = *memberPtr;

            ptr->*member = detail::pop<V>(vm_, 2);
            return 0;
        }

        Object tableSet;
        Object tableGet;
    };

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
        template<>
        inline Class popValue(HSQUIRRELVM vm, SQInteger index){
            checkType(vm, index, OT_CLASS);
            Class val(vm);
            if (SQ_FAILED(sq_getstackobj(vm, index, &val.getRaw()))) throw RuntimeException(vm, "Could not get Class from Squirrel stack!");
            sq_addref(vm, &val.getRaw());
            return val;
        }
    }
#endif
}
