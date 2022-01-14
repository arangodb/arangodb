/*=============================================================================
    Copyright (c) 2016 Lee Clagett

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/fusion/sequence/comparison.hpp>
#include <boost/mpl/identity.hpp>

namespace test_detail
{
    struct convertible
    {
        convertible() : value_() {}
        convertible(int value) : value_(value) {}
        int value_;
    };

    bool operator==(convertible const& lhs, convertible const& rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    bool operator!=(convertible const& lhs, convertible const& rhs)
    {
        return lhs.value_ != rhs.value_;
    }

    // Testing conversion at function call allows for testing mutable lvalue,
    // const lvalue, and rvalue as the source. mpl::identity prevents deduction
    template <typename T>
    T implicit_construct(typename boost::mpl::identity<T>::type source)
    {
        return source;
    }

    template <typename F, typename Source, typename Expected>
    bool run(Source const& source, Expected const& expected)
    {
        return F()(source, expected);
    }

    template <typename F, typename Source>
    bool run(Source const& source)
    {
        return run<F>(source, source);
    }

    template <typename T>
    struct can_rvalue_implicit_construct
    {
        template<typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return expected == implicit_construct<T>(implicit_construct<Source>(source));
        }
    };

    template <typename T>
    struct can_lvalue_implicit_construct    
    {
        template <typename Source, typename Expected>
        bool operator()(Source source, Expected const& expected) const
        {
            return expected == implicit_construct<T>(source);
        }
    };

    template <typename T>
    struct can_const_lvalue_implicit_construct
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return expected == implicit_construct<T>(source);
        }
    };

    template <typename T>
    struct can_implicit_construct
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return
                run< can_rvalue_implicit_construct<T> >(source, expected) &&
                run< can_lvalue_implicit_construct<T> >(source, expected) &&
                run< can_const_lvalue_implicit_construct<T> >(source, expected);
        }
    };

    template <typename T>
    struct can_rvalue_construct
    {
        template<typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return expected == T(implicit_construct<Source>(source));
        }
    };

    template <typename T>
    struct can_lvalue_construct    
    {
        template <typename Source, typename Expected>
        bool operator()(Source source, Expected const& expected) const
        {
            return expected == T(source);
        }
    };

    template <typename T>
    struct can_const_lvalue_construct
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return expected == T(source);
        }
    };

    template <typename T>
    struct can_construct
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return
                run< can_rvalue_construct<T> >(source, expected) &&
                run< can_lvalue_construct<T> >(source, expected) &&
                run< can_const_lvalue_construct<T> >(source, expected);
        }
    };

    template <typename T>
    struct can_rvalue_assign
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            bool result = true;
            {
                T seq;
                result &= (seq == expected || seq != expected);

                seq = implicit_construct<Source>(source);
                result &= (seq == expected);
            }
            return result;
        }
    };

    template <typename T>    
    struct can_lvalue_assign
    {

        template <typename Source, typename Expected>
        bool operator()(Source source, Expected const& expected) const
        {
            bool result = true;
            {
                T seq;
                result &= (seq == expected || seq != expected);

                seq = source;
                result &= (seq == expected);
            }
            return result;
        }
    };

    template <typename T>
    struct can_const_lvalue_assign
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            bool result = true;
            {
                T seq;
                result &= (seq == expected || seq != expected);

                seq = source;
                result &= (seq == expected);
            }
            return result;
        }
    };
    
    template <typename T>
    struct can_assign
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return 
                run< can_rvalue_assign<T> >(source, expected) &&
                run< can_lvalue_assign<T> >(source, expected) &&
                run< can_const_lvalue_assign<T> >(source, expected);
        }
    };

    template <typename T>
    struct can_copy
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected) const
        {
            return
                run< can_construct<T> >(source, expected) &&
                run< can_implicit_construct<T> >(source, expected) &&
                run< can_assign<T> >(source, expected);
        }
    }; 
} // test_detail
