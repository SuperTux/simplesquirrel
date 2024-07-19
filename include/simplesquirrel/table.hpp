#pragma once

#include "class.hpp"

#include <string>
#include <vector>
#include <map>

namespace ssq {
    class Enum;
    /**
    * @brief Squirrel table object
    * @ingroup simplesquirrel
    */
    class SSQ_API Table: public Object {
    public:
        /**
        * @brief Creates empty table with null VM
        * @note This object will be unusable
        */
        Table();
        /**
        * @brief Destructor
        */
        virtual ~Table() override = default;
        /**
        * @brief Converts Object to Table
        * @throws TypeException if the Object is not type of a table
        */
        explicit Table(const Object& other);
        /**
        * @brief Creates empty table
        */
        explicit Table(HSQUIRRELVM vm);
        /**
        * @brief Copy constructor
        */
        Table(const Table& other);
        /**
        * @brief Move constructor
        */
        Table(Table&& other) NOEXCEPT;
        /**
        * @brief Finds a function in this table
        * @throws RuntimeException if VM is invalid
        * @throws NotFoundException if function was not found
        * @throws TypeException if the object found is not a function
        * @returns Function object references the found function
        */
        Function findFunc(const char* name) const;
        /**
        * @brief Finds a class in this table
        * @throws NotFoundException if function was not found
        * @throws TypeException if the object found is not a function
        * @returns Class object references the found class
        */
        Class findClass(const char* name) const;
        /**
        * @brief Finds a table in this table
        * @throws NotFoundException if table was not found
        * @throws TypeException if the object found is not a table
        * @returns Table object references the found class
        */
        Table findTable(const char* name) const;
        /**
        * @brief Adds a new class type, which could inherit another existing one, to this table
        * @returns Class object references the added class
        */
        template<typename T, typename... Args, typename... DefaultArgs>
        Class addClass(const char* name, const std::function<T*(Args...)>& allocator = std::bind(&detail::defaultClassAllocator<T>),
                       const DefaultArguments<DefaultArgs...> defaultArgs = {}, bool release = true, Class base = Class()) {
            sq_pushobject(vm, obj);
            Class cls(detail::addClass(vm, name, allocator, defaultArgs, base.getRaw(), release));
            sq_pop(vm, 1);
            return cls;
        }
        /**
        * @brief Adds a new class type, which could inherit another existing one, to this table
        * @returns Class object references the added class
        */
        template<typename T, typename... Args, typename... DefaultArgs>
        Class addClass(const char* name, const Class::Ctor<T(Args...)>& constructor,
                       const DefaultArguments<DefaultArgs...> defaultArgs = {}, bool release = true, Class base = Class()) {
            const std::function<T*(Args...)> func = &constructor.allocate;
            return addClass<T>(name, func, defaultArgs, release, std::move(base));
        }
        /**
        * @brief Adds a new class type, which could inherit another existing one, to this table
        * @returns Class object references the added class
        */
        template<typename F, typename... DefaultArgs>
        Class addClass(const char* name, const F& lambda, const DefaultArguments<DefaultArgs...> defaultArgs = {},
                       bool release = true, Class base = Class()) {
            return addClass(name, detail::make_function(lambda), defaultArgs, release, std::move(base));
        }
        /**
        * @brief Adds a new abstract class type, which could inherit another existing one, to this table
        * @returns Class object references the added class
        */
        template<typename T>
        Class addAbstractClass(const char* name, Class base = Class()) {
            sq_pushobject(vm, obj);
            Class cls(detail::addAbstractClass<T>(vm, name, base.getRaw()));
            sq_pop(vm, 1);
            return cls;
        }
        /**
        * @brief Adds a new function type to this table
        * @returns Function object references the added function
        */
        template<typename R, typename... Args, typename... DefaultArgs>
        Function addFunc(const char* name, const std::function<R(Args...)>& func, const DefaultArguments<DefaultArgs...> defaultArgs = {}){
            Function ret(vm);
            sq_pushobject(vm, obj);
            detail::addFunc(vm, name, func, defaultArgs);
            sq_pop(vm, 1);
            return ret;
        }
        /**
        * @brief Adds a new lambda type to this table
        * @returns Function object that references the added function
        */
        template<typename F, typename... DefaultArgs>
        Function addFunc(const char* name, const F& lambda, const DefaultArguments<DefaultArgs...> defaultArgs = {}) {
            return addFunc(name, detail::make_function(lambda), defaultArgs);
        }
        /**
         * @brief Adds a new key-value pair to this table
         */
        template<typename T>
        inline void set(const char* name, const T& value) {
            sq_pushobject(vm, obj);
            sq_pushstring(vm, name, strlen(name));
            detail::push<T>(vm, value);
            if (SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
                throw RuntimeException(vm, "Cannot add entry '" + std::string(name) + "' to table!");
            }
            sq_pop(vm,1); // pop table
        }
        /**
         * @brief Returns the value of an entry
         * @throws NotFoundException if an entry with the provided key does not exist
         */
        template<typename T>
        inline T get(const char* name) const {
            return find(name).to<T>();
        }
        /**
         * @brief Provides the value of an entry, if it exists
         * @returns Whether an entry with the provided key was found
         */
        template<typename T>
        inline bool get(const char* name, T& value) const {
            try {
                value = find(name).to<T>();
                return true;
            }
            catch (const NotFoundException&) {
                return false;
            }
        }
        /**
         * @brief Returns whether an entry with the provided key exists
         */
        bool hasEntry(const char* name) const;
        /**
         * @brief Changes the key of an entry with a new one
         * @throws RuntimeException if either deleting the old entry, or creating the new one, fails
         */
        void rename(const char* old_name, const char* new_name);
        /**
         * @brief Removes an entry from this table
         */
        void remove(const char* name);
        /**
         * @brief Removes all entries from this table
         */
        void clear();
        /**
         * @brief Sets a table as delegate for this table
         */
        void setDelegate(Table& table);
        /**
         * @brief Returns the size of this table
         */
        size_t size() const;
        /**
         * @brief Returns an array of all keys in this table
         */
        std::vector<std::string> getKeys() const;
        /**
         * @brief Converts this table to a map of key/value entries
         */
        std::map<std::string, ssq::Object> convertRaw() const;
        /**
         * @brief Converts this table to a map of key/value entries, where values are of specific type T
         */
        template<typename T>
        std::map<std::string, T> convert() const {
            const SQInteger old_top = sq_gettop(vm);
            sq_pushobject(vm, obj);

            std::map<std::string, T> map;

            sq_pushnull(vm); // push iterator
            while (SQ_SUCCEEDED(sq_next(vm, -2)))
            {
                // -1 is the value and -2 is the key
                const char* key;
                if (SQ_FAILED(sq_getstring(vm, -2, &key)))
                    throw RuntimeException(vm, "Cannot get string value for table entry key!");
                else
                    map.insert(key, detail::pop<T>(vm, -1));

                sq_pop(vm, 2); // pop key and value of this iteration
            }

            sq_settop(vm, old_top);
            return map;
        }
        /**
         * @brief Adds a new table to this table
         */
        Table addTable(const char* name);
        /**
         * @brief Returns a table from the provided key. If such doesn't exist, it's created
         * @throws TypeException if the entry exists, but the value is not a table
         */
        Table getOrCreateTable(const char* name);
        /**
        * @brief Copy assingment operator
        */
        Table& operator = (const Table& other);
        /**
        * @brief Move assingment operator
        */
        Table& operator = (Table&& other) NOEXCEPT;
    };
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
        template<>
        inline Table popValue(HSQUIRRELVM vm, SQInteger index){
            checkType(vm, index, OT_TABLE);
            Table val(vm);
            if (SQ_FAILED(sq_getstackobj(vm, index, &val.getRaw()))) throw RuntimeException(vm, "Could not get Table from Squirrel stack!");
            sq_addref(vm, &val.getRaw());
            return val;
        }
    }
#endif
}
