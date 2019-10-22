#ifndef BOOST_NUMERIC_SAFE_BASE_OPERATIONS_HPP
#define BOOST_NUMERIC_SAFE_BASE_OPERATIONS_HPP

//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <limits>
#include <type_traits> // is_base_of, is_same, is_floating_point, conditional
#include <algorithm>   // max
#include <cassert>

#include <boost/config.hpp>

#include <boost/core/enable_if.hpp> // lazy_enable_if
#include <boost/integer.hpp>
#include <boost/logic/tribool.hpp>

#include "checked_integer.hpp"
#include "checked_result.hpp"
#include "safe_base.hpp"

#include "interval.hpp"
#include "utility.hpp"

namespace boost {
namespace safe_numerics {

/////////////////////////////////////////////////////////////////
// validation

template<typename R, R Min, R Max, typename T, typename E>
struct validate_detail {
    using r_type = checked_result<R>;

    struct exception_possible {
        constexpr static R return_value(
            const T & t
        ){
            // INT08-C
            const r_type rx = heterogeneous_checked_operation<
                R,
                typename base_type<T>::type,
                dispatch_and_return<E, R>
            >::cast(t);
            const R r = rx.exception()
                ? static_cast<R>(t)
                : rx.m_r;
            return r;
        }
    };
    struct exception_not_possible {
        constexpr static R return_value(
            const T & t
        ){
            return static_cast<R>(base_value(t));
        }
    };

    constexpr static R return_value(const T & t){
        constexpr const interval<r_type> t_interval{
            checked::cast<R>(base_value(std::numeric_limits<T>::min())),
            checked::cast<R>(base_value(std::numeric_limits<T>::max()))
        };
        constexpr const interval<r_type> r_interval{r_type(Min), r_type(Max)};

        /*
        static_assert(
            true != r_interval.excludes(t_interval),
            "ranges don't overlap: can't cast"
        );
        */

        return std::conditional<
            static_cast<bool>(r_interval.includes(t_interval)),
            exception_not_possible,
            exception_possible
        >::type::return_value(t);
    }
};

template<class Stored, Stored Min, Stored Max, class P, class E>
template<class T>
constexpr Stored safe_base<Stored, Min, Max, P, E>::
validated_cast(const T & t) const {
    return validate_detail<Stored,Min,Max,T,E>::return_value(t);
}

template<class Stored, Stored Min, Stored Max, class P, class E>
template<typename T, T N, class P1, class E1>
constexpr Stored safe_base<Stored, Min, Max, P, E>::
validated_cast(const safe_literal_impl<T, N, P1, E1> &) const {
    constexpr const interval<Stored> this_interval{};
    // if static values don't overlap, the program can never function
    static_assert(
        this_interval.includes(N),
        "safe type cannot be constructed from this value"
    );
    return static_cast<Stored>(N);
}

/////////////////////////////////////////////////////////////////
// constructors

template<class Stored, Stored Min, Stored Max, class P, class E>
    constexpr /*explicit*/ safe_base<Stored, Min, Max, P, E>::safe_base(
    const Stored & rhs,
    skip_validation
) :
    m_t(rhs)
{}

template<class Stored, Stored Min, Stored Max, class P, class E>
constexpr /*explicit*/ safe_base<Stored, Min, Max, P, E>::safe_base(){
    dispatch<E, safe_numerics_error::uninitialized_value>(
        "safe values must be initialized"
    );
}

// construct an instance of a safe type
// from an instance of a convertible underlying type.
template<class Stored, Stored Min, Stored Max, class P, class E>
template<class T>
constexpr /*explicit*/ safe_base<Stored, Min, Max, P, E>::safe_base(
    const T & t,
    typename std::enable_if<
        is_safe<T>::value,
        bool
    >::type
) :
    m_t(validated_cast(t))
{}

template<class Stored, Stored Min, Stored Max, class P, class E>
template<class T>
constexpr /*explicit*/ safe_base<Stored, Min, Max, P, E>::safe_base(
    const T & t,
    typename std::enable_if<
        std::is_integral<T>::value,
        bool
    >::type
) :
    m_t(validated_cast(t))
{}

template<class Stored, Stored Min, Stored Max, class P, class E>
template<class T, T value>
constexpr /*explicit*/ safe_base<Stored, Min, Max, P, E>::safe_base(

    const std::integral_constant<T, value> &
) :
    m_t(validated_cast(value))
{}

/////////////////////////////////////////////////////////////////
// casting operators

// cast to a builtin type from a safe type
template< class Stored, Stored Min, Stored Max, class P, class E>
template<
    class R,
    typename std::enable_if<
        ! boost::safe_numerics::is_safe<R>::value,
        int
    >::type
>
constexpr safe_base<Stored, Min, Max, P, E>::
operator R () const {

    // if static values don't overlap, the program can never function
    #if 0
    constexpr const interval<R> r_interval;
    constexpr const interval<Stored> this_interval(Min, Max);
    static_assert(
        ! r_interval.excludes(this_interval),
        "safe type cannot be constructed with this type"
    );
    #endif
    
    return validate_detail<
        R,
        std::numeric_limits<R>::min(),
        std::numeric_limits<R>::max(),
        Stored,
        E
    >::return_value(m_t);

}

// cast to the underlying builtin type from a safe type
template< class Stored, Stored Min, Stored Max, class P, class E>
constexpr safe_base<Stored, Min, Max, P, E>::
operator Stored () const {
    return m_t;
}

/////////////////////////////////////////////////////////////////
// binary operators

template<class T, class U>
struct common_exception_policy {
    static_assert(is_safe<T>::value || is_safe<U>::value,
        "at least one type must be a safe type"
    );

    using t_exception_policy = typename get_exception_policy<T>::type;
    using u_exception_policy = typename get_exception_policy<U>::type;

    static_assert(
        std::is_same<t_exception_policy, u_exception_policy>::value
        || std::is_same<t_exception_policy, void>::value
        || std::is_same<void, u_exception_policy>::value,
        "if the exception policies are different, one must be void!"
    );

    static_assert(
        ! (std::is_same<t_exception_policy, void>::value
        && std::is_same<void, u_exception_policy>::value),
        "at least one exception policy must not be void"
    );

    using type =
        typename std::conditional<
            !std::is_same<void, u_exception_policy>::value,
            u_exception_policy,
        typename std::conditional<
            !std::is_same<void, t_exception_policy>::value,
            t_exception_policy,
        //
            void
        >::type >::type;

    static_assert(
        !std::is_same<void, type>::value,
        "exception_policy is void"
    );
};

template<class T, class U>
struct common_promotion_policy {
    static_assert(is_safe<T>::value || is_safe<U>::value,
        "at least one type must be a safe type"
    );
    using t_promotion_policy = typename get_promotion_policy<T>::type;
    using u_promotion_policy = typename get_promotion_policy<U>::type;
    static_assert(
        std::is_same<t_promotion_policy, u_promotion_policy>::value
        ||std::is_same<t_promotion_policy, void>::value
        ||std::is_same<void, u_promotion_policy>::value,
        "if the promotion policies are different, one must be void!"
    );
    static_assert(
        ! (std::is_same<t_promotion_policy, void>::value
        && std::is_same<void, u_promotion_policy>::value),
        "at least one promotion policy must not be void"
    );

    using type =
        typename std::conditional<
            ! std::is_same<void, u_promotion_policy>::value,
            u_promotion_policy,
        typename std::conditional<
            ! std::is_same<void, t_promotion_policy>::value,
            t_promotion_policy,
        //
            void
        >::type >::type;

    static_assert(
        ! std::is_same<void, type>::value,
        "promotion_policy is void"
    );

};

// give the resultant base type, figure out what the final result
// type will be.  Note we currently need this because we support
// return of only safe integer types. Someday ..., we'll support
// all other safe types including float and user defined ones.
//

// helper - cast arguments to binary operators to a specified
// result type

template<class EP, class R, class T, class U>
std::pair<R, R>
constexpr static casting_helper(const T & t, const U & u){
    using r_type = checked_result<R>;
    const r_type tx = heterogeneous_checked_operation<
        R,
        typename base_type<T>::type,
        dispatch_and_return<EP, R>
    >::cast(base_value(t));
    const R tr = tx.exception()
        ? static_cast<R>(t)
        : tx.m_r;

    const r_type ux = heterogeneous_checked_operation<
        R,
        typename base_type<U>::type,
        dispatch_and_return<EP, R>
    >::cast(base_value(u));
    const R ur = ux.exception()
        ? static_cast<R>(u)
        : ux.m_r;
    return std::pair<R, R>(tr, ur);
}

// Note: the following global operators will be found via
// argument dependent lookup.

/////////////////////////////////////////////////////////////////
// addition

template<class T, class U>
struct addition_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template addition_result<T,U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            + static_cast<result_base_type>(base_value(u));
    }

    // if exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;

    using r_type = checked_result<result_base_type>;

    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        const r_type rx = checked_operation<
            result_base_type,
            dispatch_and_return<exception_policy, result_base_type>
        >::add(r.first, r.second);

        return
            rx.exception()
            ? r.first + r.second
            : rx.m_r;
    }

    using r_type_interval_t = interval<r_type>;

    constexpr static const r_type_interval_t get_r_type_interval(){
        constexpr const r_type_interval_t t_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };
        constexpr const r_type_interval_t u_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };
        return t_interval + u_interval;
    }
    constexpr static const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        if(r_type_interval.l.exception())
            return true;
        if(r_type_interval.u.exception())
            return true;
        if(! return_interval.includes(r_type_interval))
            return true;
        return false;
    }

    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

public:
    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    addition_result<T, U>
>::type
constexpr operator+(const T & t, const U & u){
    return addition_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator+=(T & t, const U & u){
    t = static_cast<T>(t + u);
    return t;
}

/////////////////////////////////////////////////////////////////
// subtraction

template<class T, class U>
struct subtraction_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template subtraction_result<T, U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            - static_cast<result_base_type>(base_value(u));
    }

    // if exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;

    using r_type = checked_result<result_base_type>;

    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        const r_type rx = checked_operation<
            result_base_type,
            dispatch_and_return<exception_policy, result_base_type>
        >::subtract(r.first, r.second);

        return
            rx.exception()
            ? r.first + r.second
            : rx.m_r;
    }
    using r_type_interval_t = interval<r_type>;

    constexpr static const r_type_interval_t get_r_type_interval(){
        constexpr const r_type_interval_t t_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };

        constexpr const r_type_interval_t u_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };

        return t_interval - u_interval;
    }
    static constexpr const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        if(r_type_interval.l.exception())
            return true;
        if(r_type_interval.u.exception())
            return true;
        if(! return_interval.includes(r_type_interval))
            return true;
        return false;
    }

public:
    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    subtraction_result<T, U>
>::type
constexpr operator-(const T & t, const U & u){
    return subtraction_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator-=(T & t, const U & u){
    t = static_cast<T>(t - u);
    return t;
}

/////////////////////////////////////////////////////////////////
// multiplication

template<class T, class U>
struct multiplication_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template multiplication_result<T, U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            * static_cast<result_base_type>(base_value(u));
    }

    // if exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;

    using r_type = checked_result<result_base_type>;
    
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        const r_type rx = checked_operation<
            result_base_type,
            dispatch_and_return<exception_policy, result_base_type>
        >::multiply(r.first, r.second);

        return
            rx.exception()
            ? r.first * r.second
            : rx.m_r;
    }

    using r_type_interval_t = interval<r_type>;

    constexpr static r_type_interval_t get_r_type_interval(){
        constexpr const r_type_interval_t t_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };

        constexpr const r_type_interval_t u_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };

        return t_interval * u_interval;
    }

    static constexpr const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        if(r_type_interval.l.exception())
            return true;
        if(r_type_interval.u.exception())
            return true;
        if(! return_interval.includes(r_type_interval))
            return true;
        return false;
    }

    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

public:
    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    multiplication_result<T, U>
>::type
constexpr operator*(const T & t, const U & u){
    // argument dependent lookup should guarentee that we only get here
    return multiplication_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator*=(T & t, const U & u){
    t = static_cast<T>(t * u);
    return t;
}

/////////////////////////////////////////////////////////////////
// division

// key idea here - result will never be larger than T
template<class T, class U>
struct division_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template division_result<T, U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            / static_cast<result_base_type>(base_value(u));
    }

    // if exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;

    constexpr static int bits = std::min(
        std::numeric_limits<std::uintmax_t>::digits,
        std::max(std::initializer_list<int>{
            std::numeric_limits<result_base_type>::digits,
            std::numeric_limits<typename base_type<T>::type>::digits,
            std::numeric_limits<typename base_type<U>::type>::digits
        }) + (std::numeric_limits<result_base_type>::is_signed ? 1 : 0)
    );

    using r_type = checked_result<result_base_type>;

    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        using temp_base = typename std::conditional<
            std::numeric_limits<result_base_type>::is_signed,
            typename boost::int_t<bits>::least,
            typename boost::uint_t<bits>::least
        >::type;
        using t_type = checked_result<temp_base>;

        const std::pair<t_type, t_type> r = casting_helper<
            exception_policy,
            temp_base
        >(t, u);

        const t_type rx = checked_operation<
            temp_base,
            dispatch_and_return<exception_policy, temp_base>
        >::divide(r.first, r.second);

        return
            rx.exception()
            ? r.first / r.second
            : rx;
    }
    using r_type_interval_t = interval<r_type>;

    constexpr static r_type_interval_t t_interval(){
        return r_type_interval_t{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };
    };

    constexpr static r_type_interval_t u_interval(){
        return r_type_interval_t{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };
    };

    constexpr static r_type_interval_t get_r_type_interval(){
        constexpr const r_type_interval_t t = t_interval();
        constexpr const r_type_interval_t u = u_interval();

        if(u.u < r_type(0) || u.l > r_type(0))
            return t / u;
        return utility::minmax(
            std::initializer_list<r_type> {
                t.l / u.l,
                t.l / r_type(-1),
                t.l / r_type(1),
                t.l / u.u,
                t.u / u.l,
                t.u / r_type(-1),
                t.u / r_type(1),
                t.u / u.u,
            }
        );
    }

    static constexpr const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        constexpr const r_type_interval_t ri = get_r_type_interval();
        constexpr const r_type_interval_t ui = u_interval();
        return
            static_cast<bool>(ui.includes(r_type(0)))
            || ri.l.exception()
            || ri.u.exception();
    }

    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

public:
    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    division_result<T, U>
>::type
constexpr operator/(const T & t, const U & u){
    return division_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator/=(T & t, const U & u){
    t = static_cast<T>(t / u);
    return t;
}

/////////////////////////////////////////////////////////////////
// modulus

template<class T, class U>
struct modulus_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type = typename promotion_policy::template modulus_result<T, U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            % static_cast<result_base_type>(base_value(u));
    }

    // if exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;

    constexpr static int bits = std::min(
        std::numeric_limits<std::uintmax_t>::digits,
        std::max(std::initializer_list<int>{
            std::numeric_limits<result_base_type>::digits,
            std::numeric_limits<typename base_type<T>::type>::digits,
            std::numeric_limits<typename base_type<U>::type>::digits
        }) + (std::numeric_limits<result_base_type>::is_signed ? 1 : 0)
    );

    using r_type = checked_result<result_base_type>;

    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        using temp_base = typename std::conditional<
            std::numeric_limits<result_base_type>::is_signed,
            typename boost::int_t<bits>::least,
            typename boost::uint_t<bits>::least
        >::type;
        using t_type = checked_result<temp_base>;
        
        const std::pair<t_type, t_type> r = casting_helper<
            exception_policy,
            temp_base
        >(t, u);

        const t_type rx = checked_operation<
            temp_base,
            dispatch_and_return<exception_policy, temp_base>
        >::modulus(r.first, r.second);

        return
            rx.exception()
            ? r.first % r.second
            : rx;
    }

    using r_type_interval_t = interval<r_type>;

    constexpr static const r_type_interval_t t_interval(){
        return r_type_interval_t{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };
    };

    constexpr static const r_type_interval_t u_interval(){
        return r_type_interval_t{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };
    };

    constexpr static const r_type_interval_t get_r_type_interval(){
        constexpr const r_type_interval_t t = t_interval();
        constexpr const r_type_interval_t u = u_interval();

        if(u.u < r_type(0)
        || u.l > r_type(0))
            return t % u;
        return utility::minmax(
            std::initializer_list<r_type> {
                t.l % u.l,
                t.l % r_type(-1),
                t.l % r_type(1),
                t.l % u.u,
                t.u % u.l,
                t.u % r_type(-1),
                t.u % r_type(1),
                t.u % u.u,
            }
        );
    }

    static constexpr const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        constexpr const r_type_interval_t ri = get_r_type_interval();
        constexpr const r_type_interval_t ui = u_interval();
        return
            static_cast<bool>(ui.includes(r_type(0)))
            || ri.l.exception()
            || ri.u.exception();
    }

    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

public:
    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
   is_safe<T>::value || is_safe<U>::value,
    modulus_result<T, U>
>::type
constexpr operator%(const T & t, const U & u){
    // see https://en.wikipedia.org/wiki/Modulo_operation
    return modulus_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator%=(T & t, const U & u){
    t = static_cast<T>(t % u);
    return t;
}

/////////////////////////////////////////////////////////////////
// comparison

// less than

template<class T, class U>
struct less_than_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;

    using result_base_type =
        typename promotion_policy::template comparison_result<T, U>::type;

    // if exception not possible
    constexpr static bool
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            < static_cast<result_base_type>(base_value(u));
    }

    using exception_policy = typename common_exception_policy<T, U>::type;

    using r_type = checked_result<result_base_type>;

    // if exception possible
    constexpr static bool
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        return safe_compare::less_than(r.first, r.second);
    }

    using r_type_interval_t = interval<r_type>;

    constexpr static bool interval_open(const r_type_interval_t & t){
        return t.l.exception() || t.u.exception();
    }

public:
    constexpr static bool
    return_value(const T & t, const U & u){
        constexpr const r_type_interval_t t_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };
        constexpr const r_type_interval_t u_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };

        if(t_interval < u_interval)
            return true;
        if(t_interval > u_interval)
            return false;

        constexpr bool exception_possible
            = interval_open(t_interval) || interval_open(u_interval);

        return return_value(
            t,
            u,
            std::integral_constant<bool, exception_possible>()
        );
    }
};

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    bool
>::type
constexpr operator<(const T & lhs, const U & rhs) {
    return less_than_result<T, U>::return_value(lhs, rhs);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    bool
>::type
constexpr operator>(const T & lhs, const U & rhs) {
    return rhs < lhs;
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    bool
>::type
constexpr operator>=(const T & lhs, const U & rhs) {
    return ! ( lhs < rhs );
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    bool
>::type
constexpr operator<=(const T & lhs, const U & rhs) {
    return ! ( lhs > rhs );
}

// equal

template<class T, class U>
struct equal_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;

    using result_base_type =
        typename promotion_policy::template comparison_result<T, U>::type;

    // if exception not possible
    constexpr static bool
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            == static_cast<result_base_type>(base_value(u));
    }

    using exception_policy = typename common_exception_policy<T, U>::type;

    using r_type = checked_result<result_base_type>;

    // exception possible
    constexpr static bool
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        return safe_compare::equal(r.first, r.second);
    }

    using r_type_interval = interval<r_type>;

    constexpr static bool interval_open(const r_type_interval & t){
        return t.l.exception() || t.u.exception();
    }

public:
    constexpr static bool
    return_value(const T & t, const U & u){
        constexpr const r_type_interval t_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };

        constexpr const r_type_interval u_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };

        if(! intersect(t_interval, u_interval))
            return false;

        constexpr bool exception_possible
            = interval_open(t_interval) || interval_open(u_interval);

        return return_value(
            t,
            u,
            std::integral_constant<bool, exception_possible>()
        );
    }
};

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    bool
>::type
constexpr operator==(const T & lhs, const U & rhs) {
    return equal_result<T, U>::return_value(lhs, rhs);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    bool
>::type
constexpr operator!=(const T & lhs, const U & rhs) {
    return ! (lhs == rhs);
}

/////////////////////////////////////////////////////////////////
// shift operators

// left shift
template<class T, class U>
struct left_shift_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template left_shift_result<T, U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            << static_cast<result_base_type>(base_value(u));
    }

    // exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;
    
    using r_type = checked_result<result_base_type>;

    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        const r_type rx = checked_operation<
            result_base_type,
            dispatch_and_return<exception_policy, result_base_type>
        >::left_shift(r.first, r.second);

        return
            rx.exception()
            ? r.first << r.second
            : rx.m_r;
    }

    using r_type_interval_t = interval<r_type>;

    constexpr static r_type_interval_t get_r_type_interval(){
        constexpr const r_type_interval_t t_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        };

        constexpr const r_type_interval_t u_interval{
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        };
        return (t_interval << u_interval);
    }

    static constexpr const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        if(r_type_interval.l.exception())
            return true;
        if(r_type_interval.u.exception())
            return true;
        if(! return_interval.includes(r_type_interval))
            return true;
        return false;
    }

    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

public:
    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    // handle safe<T> << int, int << safe<U>, safe<T> << safe<U>
    // exclude std::ostream << ...
    (! std::is_base_of<std::ios_base, T>::value)
    && (is_safe<T>::value || is_safe<U>::value),
    left_shift_result<T, U>
>::type
constexpr operator<<(const T & t, const U & u){
    // INT13-CPP
    // C++ standards document N4618 & 5.8.2
    static_assert(
        std::numeric_limits<T>::is_integer, "shifted value must be an integer"
    );
    static_assert(
        std::numeric_limits<U>::is_integer, "shift amount must be an integer"
    );
    return left_shift_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator<<=(T & t, const U & u){
    t = static_cast<T>(t << u);
    return t;
}

// right shift
template<class T, class U>
struct right_shift_result {
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template right_shift_result<T, U>::type;

    // if exception not possible
    constexpr static result_base_type
    return_value(const T & t, const U & u, std::false_type){
        return
            static_cast<result_base_type>(base_value(t))
            >> static_cast<result_base_type>(base_value(u));
    }

    // exception possible
    using exception_policy = typename common_exception_policy<T, U>::type;

    using r_type = checked_result<result_base_type>;

    constexpr static result_base_type
    return_value(const T & t, const U & u, std::true_type){
        const std::pair<result_base_type, result_base_type> r = casting_helper<
            exception_policy,
            result_base_type
        >(t, u);

        const r_type rx = checked_operation<
            result_base_type,
            dispatch_and_return<exception_policy, result_base_type>
        >::right_shift(r.first, r.second);

        return
            rx.exception()
            ? r.first >> r.second
            : rx.m_r;
    }

    using r_type_interval_t = interval<r_type>;

    constexpr static r_type_interval_t t_interval(){
        return r_type_interval_t(
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<T>::max()))
        );
    };

    constexpr static r_type_interval_t u_interval(){
        return r_type_interval_t(
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::min())),
            checked::cast<result_base_type>(base_value(std::numeric_limits<U>::max()))
        );
    }
    constexpr static r_type_interval_t get_r_type_interval(){;
        return (t_interval() >> u_interval());
    }

    static constexpr const r_type_interval_t r_type_interval = get_r_type_interval();

    constexpr static const interval<result_base_type> return_interval{
        r_type_interval.l.exception()
            ? std::numeric_limits<result_base_type>::min()
            : static_cast<result_base_type>(r_type_interval.l),
        r_type_interval.u.exception()
            ? std::numeric_limits<result_base_type>::max()
            : static_cast<result_base_type>(r_type_interval.u)
    };

    constexpr static bool exception_possible(){
        constexpr const r_type_interval_t ri = r_type_interval;
        constexpr const r_type_interval_t ti = t_interval();
        constexpr const r_type_interval_t ui = u_interval();
        return static_cast<bool>(
            // note undesirable coupling with checked::shift right here !
            ui.u > checked_result<result_base_type>(
                std::numeric_limits<result_base_type>::digits
            )
            || ti.l < checked_result<result_base_type>(0)
            || ui.l < checked_result<result_base_type>(0)
            || ri.l.exception()
            || ri.u.exception()
        );
    }

    constexpr static auto rl = return_interval.l;
    constexpr static auto ru = return_interval.u;

public:
    using type =
        safe_base<
            result_base_type,
            rl,
            ru,
            promotion_policy,
            exception_policy
        >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            return_value(
                t,
                u,
                std::integral_constant<bool, exception_possible()>()
            ),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    (! std::is_base_of<std::ios_base, T>::value)
    && (is_safe<T>::value || is_safe<U>::value),
    right_shift_result<T, U>
>::type
constexpr operator>>(const T & t, const U & u){
    // INT13-CPP
    static_assert(
        std::numeric_limits<T>::is_integer, "shifted value must be an integer"
    );
    static_assert(
        std::numeric_limits<U>::is_integer, "shift amount must be an integer"
    );
    return right_shift_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator>>=(T & t, const U & u){
    t = static_cast<T>(t >> u);
    return t;
}

/////////////////////////////////////////////////////////////////
// bitwise operators

// operator |
template<class T, class U>
struct bitwise_or_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template bitwise_or_result<T, U>::type;

    // according to the C++ standard, the bitwise operators are executed as if
    // the operands are consider a logical array of bits.  That is, there is no
    // sense that these are signed numbers.

    using r_type = typename std::make_unsigned<result_base_type>::type;
    using r_type_interval_t = interval<r_type>;

    #if 0
    // breaks compilation for earlier versions of clant
    constexpr static const r_type_interval_t r_interval{
        r_type(0),
        utility::round_out(
            std::max(
                static_cast<r_type>(base_value(std::numeric_limits<T>::max())),
                static_cast<r_type>(base_value(std::numeric_limits<U>::max()))
            )
        )
    };
    #endif

    using exception_policy = typename common_exception_policy<T, U>::type;

public:
    // lazy_enable_if_c depends on this
    using type = safe_base<
        result_base_type,
        //r_interval.l,
        r_type(0),
        //r_interval.u,
        utility::round_out(
            std::max(
                static_cast<r_type>(base_value(std::numeric_limits<T>::max())),
                static_cast<r_type>(base_value(std::numeric_limits<U>::max()))
            )
        ),
        promotion_policy,
        exception_policy
    >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            static_cast<result_base_type>(base_value(t))
            | static_cast<result_base_type>(base_value(u)),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    bitwise_or_result<T, U>
>::type
constexpr operator|(const T & t, const U & u){
    return bitwise_or_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator|=(T & t, const U & u){
    t = static_cast<T>(t | u);
    return t;
}

// operator &
template<class T, class U>
struct bitwise_and_result {
private:
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template bitwise_and_result<T, U>::type;

    // according to the C++ standard, the bitwise operators are executed as if
    // the operands are consider a logical array of bits.  That is, there is no
    // sense that these are signed numbers.

    using r_type = typename std::make_unsigned<result_base_type>::type;
    using r_type_interval_t = interval<r_type>;

    #if 0
    // breaks compilation for earlier versions of clant
    constexpr static const r_type_interval_t r_interval{
        r_type(0),
        utility::round_out(
            std::min(
                static_cast<r_type>(base_value(std::numeric_limits<T>::max())),
                static_cast<r_type>(base_value(std::numeric_limits<U>::max()))
            )
        )
    };
    #endif
    using exception_policy = typename common_exception_policy<T, U>::type;

public:
    // lazy_enable_if_c depends on this
    using type = safe_base<
        result_base_type,
        //r_interval.l,
        r_type(0),
        //r_interval.u,
        utility::round_out(
            std::min(
                static_cast<r_type>(base_value(std::numeric_limits<T>::max())),
                static_cast<r_type>(base_value(std::numeric_limits<U>::max()))
            )
        ),
        promotion_policy,
        exception_policy
    >;
    
    constexpr static type return_value(const T & t, const U & u){
        return type(
            static_cast<result_base_type>(base_value(t))
            & static_cast<result_base_type>(base_value(u)),
            typename type::skip_validation()
        );
    }
};
    
template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    bitwise_and_result<T, U>
>::type
constexpr operator&(const T & t, const U & u){
    return bitwise_and_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator&=(T & t, const U & u){
    t = static_cast<T>(t & u);
    return t;
}

// operator ^
template<class T, class U>
struct bitwise_xor_result {
    using promotion_policy = typename common_promotion_policy<T, U>::type;
    using result_base_type =
        typename promotion_policy::template bitwise_xor_result<T, U>::type;

    // according to the C++ standard, the bitwise operators are executed as if
    // the operands are consider a logical array of bits.  That is, there is no
    // sense that these are signed numbers.

    using r_type = typename std::make_unsigned<result_base_type>::type;
    using r_type_interval_t = interval<r_type>;

    #if 0
    // breaks compilation for earlier versions of clant
    constexpr static const r_type_interval_t r_interval{
        r_type(0),
        utility::round_out(
            std::max(
                static_cast<r_type>(base_value(std::numeric_limits<T>::max())),
                static_cast<r_type>(base_value(std::numeric_limits<U>::max()))
            )
        )
    };
    #endif

    using exception_policy = typename common_exception_policy<T, U>::type;

public:
    // lazy_enable_if_c depends on this
    using type = safe_base<
        result_base_type,
        //r_interval.l,
        r_type(0),
        //r_interval.u,
        utility::round_out(
            std::max(
                static_cast<r_type>(base_value(std::numeric_limits<T>::max())),
                static_cast<r_type>(base_value(std::numeric_limits<U>::max()))
            )
        ),
        promotion_policy,
        exception_policy
    >;

    constexpr static type return_value(const T & t, const U & u){
        return type(
            static_cast<result_base_type>(base_value(t))
            ^ static_cast<result_base_type>(base_value(u)),
            typename type::skip_validation()
        );
    }
};

template<class T, class U>
typename boost::lazy_enable_if_c<
    is_safe<T>::value || is_safe<U>::value,
    bitwise_xor_result<T, U>
>::type
constexpr operator^(const T & t, const U & u){
    return bitwise_xor_result<T, U>::return_value(t, u);
}

template<class T, class U>
typename std::enable_if<
    is_safe<T>::value || is_safe<U>::value,
    T
>::type
constexpr operator^=(T & t, const U & u){
    t = static_cast<T>(t ^ u);
    return t;
}

/////////////////////////////////////////////////////////////////
// stream helpers

template<
    class T,
    T Min,
    T Max,
    class P, // promotion polic
    class E  // exception policy
>
template<
    class CharT,
    class Traits
>
void safe_base<T, Min, Max, P, E>::output(
    std::basic_ostream<CharT, Traits> & os
) const {
    os << (
        (std::is_same<T, signed char>::value
        || std::is_same<T, unsigned char>::value
        || std::is_same<T, wchar_t>::value
        ) ?
            static_cast<int>(m_t)
        :
            m_t
    );
}

template<
    class T,
    T Min,
    T Max,
    class P, // promotion polic
    class E  // exception policy
>
template<
    class CharT,
    class Traits
>
void safe_base<T, Min, Max, P, E>::input(
    std::basic_istream<CharT, Traits> & is
){
    if(std::is_same<T, signed char>::value
    || std::is_same<T, unsigned char>::value
    || std::is_same<T, wchar_t>::value
    ){
        int x;
        is >> x;
        m_t = validated_cast(x);
    }
    else{
        is >> m_t;
        validated_cast(m_t);
    }
    if(is.fail()){
        boost::safe_numerics::dispatch<
            E,
            boost::safe_numerics::safe_numerics_error::domain_error
        >(
            "error in file input"
        );
    }
}

} // safe_numerics
} // boost

#endif // BOOST_NUMERIC_SAFE_BASE_OPERATIONS_HPP
