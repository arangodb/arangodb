////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for http request handlers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Richard Bruch
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_FUNCTOR_H
#define TRIAGENS_BASICS_FUNCTOR_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief null type
////////////////////////////////////////////////////////////////////////////////

    enum NullType {
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for functors
////////////////////////////////////////////////////////////////////////////////

    template <typename R = void, typename P1 = NullType, typename P2 = NullType>
    struct FunctionBase {
        virtual R call (P1 p1, P2 p2) = 0;
        virtual FunctionBase* clone () = 0;
        virtual ~FunctionBase () {}
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for functors
////////////////////////////////////////////////////////////////////////////////

    template <typename R>
    struct FunctionBase<R, NullType, NullType> {
        virtual R call () = 0;
        virtual FunctionBase* clone () = 0;
        virtual ~FunctionBase () {}
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for functors
////////////////////////////////////////////////////////////////////////////////

    template <typename R, typename P1>
    struct FunctionBase<R, P1, NullType> {
        virtual R call (P1 p1) = 0;
        virtual FunctionBase* clone () = 0;
        virtual ~FunctionBase () {}
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename F, typename R = void, typename P1 = NullType, typename P2 = NullType>
    struct InternalFunction : public FunctionBase<R, P1, P2> {
        typedef FunctionBase<R, P1, P2> BaseType;
        T& _ref;
        F _ptr;

        InternalFunction (T& ref, F ptr)
          : _ref(ref), _ptr(ptr) {
        }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief call with 0 arguments
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename R>
    struct Call0 {
        typedef R (T::*CALL) ();
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief call with 1 argument
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename R, typename P1>
    struct Call1 {
        typedef R (T::*CALL) (P1);
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief call with 2 arguments
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename R, typename P1, typename P2>
    struct Call2 {
        typedef R (T::*CALL) (P1, P2);
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief function with 0 arguments
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename R>
    struct Function0 : public InternalFunction<T, typename Call0<T, R>::CALL, R> {
        typedef typename Call0<T, R>::CALL CallType;
        typedef InternalFunction<T, CallType, R> BaseType;

        Function0 (T& ref, CallType call) :
            BaseType(ref, call) {
        }

        virtual typename BaseType::BaseType* clone () {
          return new Function0(BaseType::_ref, BaseType::_ptr);
        }
        virtual R call () {
          return (BaseType::_ref.*BaseType::_ptr)();
        }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief function with 1 argument
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename R, typename P1>
    struct Function1 : public InternalFunction<T, typename Call1<T, R, P1>::CALL, R, P1> {

        typedef typename Call1<T, R, P1>::CALL CallType;
        typedef InternalFunction<T, CallType, R, P1> BaseType;

        Function1 (T& ref, CallType call) :
            BaseType(ref, call) {
        }

        virtual typename BaseType::BaseType* clone () {
          return new Function1(BaseType::_ref, BaseType::_ptr);
        }
        virtual R call (P1 p1) {
          return (BaseType::_ref.*BaseType::_ptr)(p1);
        }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief function with 2 arguments
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename R, typename P1, typename P2>
    struct Function2 : public InternalFunction<T, typename Call2<T, R, P1, P2>::CALL, R, P1, P2> {

        typedef typename Call2<T, R, P1, P2>::CALL CallType;
        typedef InternalFunction<T, CallType, R, P1, P2> BaseType;

        Function2 (T& ref, CallType call) :
            BaseType(ref, call) {
        }

        virtual typename BaseType::BaseType* clone () {
          return new Function2(BaseType::_ref, BaseType::_ptr);
        }
        virtual R call (P1 p1, P2 p2) {
          return (BaseType::_ref.*BaseType::_ptr)(p1, p2);
        }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief functor
////////////////////////////////////////////////////////////////////////////////

    template <typename R, typename P1 = NullType, typename P2 = NullType>
    class Functor {
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        Functor () :
            _function(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        template <typename T>
        Functor (T& obj, R(T::*fptr) ()) {
          _function = new Function0<T, R>(obj, fptr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        template <typename T>
        Functor (T& obj, R(T::*fptr) (P1)) {
          _function = new Function1<T, R, P1>(obj, fptr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        template <typename T>
        Functor (T& obj, R(T::*fptr) (P1, P2)) {
          _function = new Function2<T, R, P1, P2>(obj, fptr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief copy constructor
////////////////////////////////////////////////////////////////////////////////

        Functor (const Functor& src)
          : _function(0) {
          copy(*this, src);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Functor () {
          if (_function) {
            delete _function;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assigment
////////////////////////////////////////////////////////////////////////////////

        Functor& operator= (const Functor& src) {
          copy(*this, src);
          return *this;
        }

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief call with no arguments
////////////////////////////////////////////////////////////////////////////////

        void operator() () {
          if (_function)
            _function->call();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief call with 1 argument
////////////////////////////////////////////////////////////////////////////////

        void operator() (P1 p1) {
          if (_function) {
            _function->call(p1);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief call with 2 argument
////////////////////////////////////////////////////////////////////////////////

        void operator() (P1 p1, P2 p2) {
          if (_function) {
            _function->call(p1, p2);
          }
        }

      private:
        FunctionBase<R, P1, P2>* _function;

        friend void copy (Functor& dst, const Functor& src) {
          if (dst._function) {
            delete dst._function;
          }

          if (src._function) {
            dst._function = src._function->clone();
          }
          else {
            dst._function = 0;
          }
        }
    };

    typedef Functor<void, NullType, NullType> Command;
  }
}

#endif
