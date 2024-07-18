#pragma once

#include "exceptions.hpp"
#include "table.hpp"
#include "script.hpp"
#include "args.hpp"
#include "class.hpp"
#include "instance.hpp"
#include "function.hpp"
#include "array.hpp"

#include <memory>

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif

namespace ssq {
    /**
     * @ingroup simplesquirrel
     */
    typedef void(*SqPrintFunc)(HSQUIRRELVM, const SQChar*, ...);
    /**
     * @ingroup simplesquirrel
     */
    typedef void(*SqErrorFunc)(HSQUIRRELVM, const SQChar*, ...);
    /**
     * @ingroup simplesquirrel
     */
    typedef SQInteger(*SqRuntimeErrorFunc)(HSQUIRRELVM);
    /**
     * @ingroup simplesquirrel
     */
    typedef void(*SqCompileErrorFunc)(HSQUIRRELVM, const SQChar*, const SQChar*, SQInteger, SQInteger);
    /**
     * @ingroup simplesquirrel
     */
    namespace Libs {
      enum Flag {
        NONE = 0x0000,
        IO = 0x0001,
        BLOB = 0x0002,
        MATH = 0x0004,
        SYSTEM = 0x0008,
        STRING = 0x0010,
        ALL = 0xFFFF
      };
    }

    /**
    * @brief Squirrel Virtual Machine object
    * @ingroup simplesquirrel
    */
    class SSQ_API VM: public Table {
    public:
        /**
        * @brief Obtains a pointer to the simplesquirrel VM instance of a Squirrel VM
        * @note The pointer may be NULL. Possible cause would be the thread not being created via simplesquirrel
        */
        static VM* get(HSQUIRRELVM vm);
        /**
        * @brief Obtains a reference to the main simplesquirrel VM instance from any Squirrel VM
        */
        static VM& getMain(HSQUIRRELVM vm);

        /**
        * @brief Creates an empty VM object with null VM
        * @note This object won't be usable
        */
        VM();
        /**
        * @brief Creates a VM with a fixed stack size
        */
        VM(size_t stackSize, uint32_t flags = Libs::NONE);
        /**
        * @brief Creates a VM object for a thread VM
        * @note Meant for temporary usage for threads, which do not originate from simplesquirrel
        */
        VM(HSQUIRRELVM thread);
        /**
        * @brief Destroys the VM and all of this objects
        */
        void destroy();
        /**
        * @brief Destructor
        */
        virtual ~VM() override;
        /**
        * @brief Swaps the contents of this VM with another one
        */
        void swap(VM& other) NOEXCEPT;
        /**
        * @brief Disabled copy constructor
        */
        VM(const VM& other) = delete;
        /**
        * @brief Move constructor
        */
        VM(VM&& other) NOEXCEPT;
        /**
        * @brief Registers standard template libraries
        */
        void registerStdlib(uint32_t flags);
        /**
        * @brief Sets a table as the root table for the VM
        */
        void setRootTable(Table& table);
        /**
        * @brief Registers print and error functions
        */
        void setPrintFunc(SqPrintFunc printFunc, SqErrorFunc errorFunc);
        /**
        * @brief Registers runtime error function
        */
        void setRuntimeErrorFunc(SqRuntimeErrorFunc runtimeErrorFunc);
        /**
        * @brief Registers compilation error function
        */
        void setCompileErrorFunc(SqCompileErrorFunc compileErrorFunc);
        /**
        * @brief Registers runtime and compilation error functions from the Squirrel standard library
        */
        void setStdErrorFunc();
        /**
        * @brief Saves an arbitrary user-defined pointer
        */
        void setForeignPtr(void* ptr);
        /**
        * @brief Gets the saved arbitrary user-defined pointer
        */
        void* getForeignPtr() const;
        /**
        * @brief Gets the saved arbitrary user-defined pointer, casted to a provided type
        */
        template<typename T>
        T* getForeignPtr() const {
            return static_cast<T*>(foreignPtr);
        }
        /**
        * @brief Returns whether this VM is a thread
        */
        bool isThread() const;
        /**
        * @brief Returns the execution state of this VM
        */
        SQInteger getState() const;
        /**
        * @brief Returns the index of the top slot of the stack
        */
        SQInteger getTop() const;
        /**
        * @brief Returns the last compilation exception
        */
        /*
        const CompileException& getLastCompileException() const {
            return *compileException.get();
        }
        */
        /**
        * @brief Returns the last runtime exception
        */
        /*
        const RuntimeException& getLastRuntimeException() const {
            return *runtimeException.get();
        }
        */
        /**
        * @brief Compiles a script from memory
        * @details The script can be associated with a name as a second parameter.
        * This name is used during runtime error information.
        * @throws CompileException
        */
        Script compileSource(const char* source, const char* name = "buffer");
        /**
        * @brief Compiles a script from an input stream
        * @details The script can be associated with a name as a second parameter.
        * This name is used during runtime error information.
        * @throws CompileException
        */
        Script compileSource(std::istream& source, const char* name = "buffer");
        /**
        * @brief Compiles a script from a source file
        * @throws CompileException
        */
        Script compileFile(const char* path);
        /**
        * @brief Runs a script
        * @details When the script runs for the first time, the contens such as
        * class definitions are assigned to the root table (global table).
        * @throws RuntimeException
        */
        void run(const Script& script);
        /**
        * @brief Runs a script and returns its return value as an Object
        * @details When the script runs for the first time, the contens such as
        * class definitions are assigned to the root table (global table).
        * @returns A return value as an Object
        * @throws RuntimeException
        */
        Object runAndReturn(const Script& script);
        /**
        * @brief Calls a global function
        * @param func The instance of a function
        * @param args Any number of arguments
        * @throws RuntimeException if an exception is thrown or number of arguments
        * do not match
        * @throws TypeException if casting from Squirrel objects to C++ objects failed
        */
        template<class... Args>
        Object callFunc(const Function& func, const Object& env, Args&&... args) const {
            static const std::size_t params = sizeof...(Args);

            const std::pair<unsigned int, unsigned int> paramsRequired = func.getNumOfParams();
            if ((paramsRequired.first > 0 && params < paramsRequired.first) ||
                (paramsRequired.second > 0 && params > paramsRequired.second)) {
                throw RuntimeException(nullptr, "Incorrect number of arguments!");
            }

            auto top = sq_gettop(vm);
            sq_pushobject(vm, func.getRaw());
            sq_pushobject(vm, env.getRaw());

            pushArgs(std::forward<Args>(args)...);

            return callAndReturn(params, top);
        }
        /**
        * @brief Creates a new instance of class and call constructor with given arguments
        * @param cls The object of a class
        * @param args Any number of arguments
        * @throws RuntimeException
        */
        template<class... Args>
        Instance newInstance(const Class& cls, Args&&... args) const {
            Instance inst = newInstanceNoCtor(cls);
            Function ctor = cls.findFunc("constructor");
            callFunc(ctor, inst, std::forward<Args>(args)...);
            return inst;
        }
        /**
        * @brief Creates a new instance of class without calling a constructor
        * @param cls The object of a class
        * @throws RuntimeException
        */
        Instance newInstanceNoCtor(const Class& cls) const {
            Instance inst(vm);
            sq_pushobject(vm, cls.getRaw());
            if (SQ_FAILED(sq_createinstance(vm, -1)))
              throw RuntimeException(vm, "Cannot create instance.");
            sq_remove(vm, -2);
            sq_getstackobj(vm, -1, &inst.getRaw());
            sq_addref(vm, &inst.getRaw());
            sq_pop(vm, 1);
            return inst;
        }
        /**
        * @brief Creates a new instance of class from an existing C++ instance, through a pointer, in a provided table
        * @param table The Squirrel table to create a slot for the instance in
        * @param cls The object of a class
        * @param name The name of the instance
        * @param ptr The pointer to the class instance
        * @throws RuntimeException
        */
        Instance newInstancePtr(Table& table, const Class& cls, const char* name, ExposableClass* ptr) const {
            const SQInteger old_top = sq_gettop(vm);
            sq_pushobject(vm, table.getRaw());

            Instance inst(vm);
            sq_pushstring(vm, name, -1);
            sq_pushobject(vm, cls.getRaw());
            if (SQ_FAILED(sq_createinstance(vm, -1)) || SQ_FAILED(sq_setinstanceup(vm, -1, ptr)))
              throw RuntimeException(vm, "Cannot create instance.");
            sq_remove(vm, -2);
            sq_getstackobj(vm, -1, &inst.getRaw());
            sq_addref(vm, &inst.getRaw());

            if (SQ_FAILED(sq_createslot(vm, -3)))
              throw RuntimeException(vm, "Couldn't create table slot for instance.");

            sq_settop(vm, old_top);
            return inst;
        }
        /**
        * @brief Creates a new thread with a fixed stack size
        */
        VM newThread(size_t stackSize) const;
        /**
        * @brief Creates a new empty table
        */
        Table newTable() const {
            return Table(vm);
        }
        /**
        * @brief Creates a new empty array
        */
        Array newArray() const {
            return Array(vm);
        }
        /**
        * @brief Creates a new array
        */
        template<class T>
        Array newArray(const std::vector<T>& vector) const {
            return Array(vm, vector);
        }
        /**
         * @brief Adds a new enum to this table
         */
        Enum addEnum(const char* name);
        /**
         * @brief Adds a new constant key-value pair to this table
         */
        template<typename T>
        inline void setConst(const char* name, const T& value) {
            sq_pushconsttable(vm);
            sq_pushstring(vm, name, strlen(name));
            detail::push<T>(vm, value);
            if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Failed to add value '" + std::string(name) + "' to constant table!");
            }
            sq_pop(vm,1); // pop table
        }
        /**
        * @brief Prints stack objects
        */
        void debugStack() const;
        /**
        * @brief Add registered class object into the table of known classes
        */
        static void addClassObj(size_t hashCode, const HSQOBJECT& obj);
        /**
        * @brief Get registered class object from hash code
        */
        static const HSQOBJECT& getClassObj(size_t hashCode);
        /**
        * @brief Copy assingment operator
        */
        VM& operator = (const VM& other) = delete;
        /**
        * @brief Move assingment operator
        */
        VM& operator = (VM&& other) NOEXCEPT;
    private:
        static std::unordered_map<size_t, HSQOBJECT> classMap;

        HSQOBJECT threadObj;
        //std::unique_ptr<CompileException> compileException;
        //std::unique_ptr<RuntimeException> runtimeException;
        void* foreignPtr;

        /**
        * @brief Creates a VM object for a thread
        */
        VM(HSQOBJECT thread);

        static void pushArgs();

        template <class First, class... Rest> 
        void pushArgs(First&& first, Rest&&... rest) const {
            detail::push(vm, first);
            pushArgs(std::forward<Rest>(rest)...);
        }

        Object callAndReturn(SQUnsignedInteger nparams, SQInteger top) const;

        static void defaultPrintFunc(HSQUIRRELVM vm, const SQChar *s, ...);
        static void defaultErrorFunc(HSQUIRRELVM vm, const SQChar *s, ...);
        //static SQInteger defaultRuntimeErrorFunc(HSQUIRRELVM vm);
        //static void defaultCompilerErrorFunc(HSQUIRRELVM vm, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column);
    };
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
