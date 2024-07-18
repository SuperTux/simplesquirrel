#pragma once

#include "object.hpp"
#include "exceptions.hpp"
#include "args.hpp"

namespace ssq {
    /**
    * @brief Squirrel function
    * @ingroup simplesquirrel
    */
    class SSQ_API Function: public Object {
    public:
        /**
        * @brief Constructs empty function object
        */
        explicit Function(HSQUIRRELVM vm);
        /**
        * @brief Destructor
        */
        virtual ~Function() override = default;
        /**
        * @brief Converts Object to Function
        * @throws TypeException if the Object is not type of a function
        */
        explicit Function(const Object& object);
        /**
        * @brief Copy constructor
        */
        Function(const Function& other);
        /**
        * @brief Move constructor
        */
        Function(Function&& other) NOEXCEPT;
        /**
        * @brief Returns the number of minimum and maximum parameters allowed by the function
        * @note This ignores the "this" pointer
        */
        std::pair<unsigned int, unsigned int> getNumOfParams() const;
        /**
        * @brief Copy assingment operator
        */
        Function& operator = (const Function& other);
        /**
        * @brief Move assingment operator
        */
        Function& operator = (Function&& other) NOEXCEPT;
    };
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    namespace detail {
        template<>
        inline Function popValue(HSQUIRRELVM vm, SQInteger index){
            checkType(vm, index, OT_CLOSURE);
            Function val(vm);
            if (SQ_FAILED(sq_getstackobj(vm, index, &val.getRaw()))) throw RuntimeException(vm, "Could not get Table from Squirrel stack!");
            sq_addref(vm, &val.getRaw());
            return val;
        }
    }
#endif
}
