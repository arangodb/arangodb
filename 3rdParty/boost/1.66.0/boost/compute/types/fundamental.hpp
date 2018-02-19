//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_TYPES_FUNDAMENTAL_HPP
#define BOOST_COMPUTE_TYPES_FUNDAMENTAL_HPP

#include <cstring>
#include <ostream>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comma.hpp>
#include <boost/preprocessor/repetition.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <boost/compute/cl.hpp>

namespace boost {
namespace compute {

// scalar data types
typedef cl_char char_;
typedef cl_uchar uchar_;
typedef cl_short short_;
typedef cl_ushort ushort_;
typedef cl_int int_;
typedef cl_uint uint_;
typedef cl_long long_;
typedef cl_ulong ulong_;
typedef cl_float float_;
typedef cl_double double_;

// converts uchar to ::boost::compute::uchar_
#define BOOST_COMPUTE_MAKE_SCALAR_TYPE(scalar) \
    BOOST_PP_CAT(::boost::compute::scalar, _)

// converts float, 4 to ::boost::compute::float4_
#define BOOST_COMPUTE_MAKE_VECTOR_TYPE(scalar, size) \
    BOOST_PP_CAT(BOOST_PP_CAT(::boost::compute::scalar, size), _)

// vector data types
template<class Scalar, size_t N>
class vector_type
{
public:
    typedef Scalar scalar_type;

    vector_type()
    {

    }

    explicit vector_type(const Scalar scalar)
    {
        for(size_t i = 0; i < N; i++)
            m_value[i] = scalar;
    }

    vector_type(const vector_type<Scalar, N> &other)
    {
        std::memcpy(m_value, other.m_value, sizeof(m_value));
    }

    vector_type<Scalar, N>&
    operator=(const vector_type<Scalar, N> &other)
    {
        std::memcpy(m_value, other.m_value, sizeof(m_value));
        return *this;
    }

    size_t size() const
    {
        return N;
    }

    Scalar& operator[](size_t i)
    {
        return m_value[i];
    }

    Scalar operator[](size_t i) const
    {
        return m_value[i];
    }

    bool operator==(const vector_type<Scalar, N> &other) const
    {
        return std::memcmp(m_value, other.m_value, sizeof(m_value)) == 0;
    }

    bool operator!=(const vector_type<Scalar, N> &other) const
    {
        return !(*this == other);
    }

protected:
    scalar_type m_value[N];
};

#define BOOST_COMPUTE_VECTOR_TYPE_CTOR_ARG_FUNCTION(z, i, _) \
    BOOST_PP_COMMA_IF(i) scalar_type BOOST_PP_CAT(arg, i)
#define BOOST_COMPUTE_VECTOR_TYPE_DECLARE_CTOR_ARGS(scalar, size) \
    BOOST_PP_REPEAT(size, BOOST_COMPUTE_VECTOR_TYPE_CTOR_ARG_FUNCTION, _)
#define BOOST_COMPUTE_VECTOR_TYPE_ASSIGN_CTOR_ARG(z, i, _) \
    m_value[i] = BOOST_PP_CAT(arg, i);
#define BOOST_COMPUTE_VECTOR_TYPE_ASSIGN_CTOR_SINGLE_ARG(z, i, _) \
    m_value[i] = arg;

#define BOOST_COMPUTE_DECLARE_VECTOR_TYPE_CLASS(cl_scalar, size, class_name) \
    class class_name : public vector_type<cl_scalar, size> \
    { \
    public: \
        class_name() { } \
        explicit class_name( scalar_type arg ) \
        { \
            BOOST_PP_REPEAT(size, BOOST_COMPUTE_VECTOR_TYPE_ASSIGN_CTOR_SINGLE_ARG, _) \
        } \
        class_name( \
            BOOST_PP_REPEAT(size, BOOST_COMPUTE_VECTOR_TYPE_CTOR_ARG_FUNCTION, _) \
        ) \
        { \
          BOOST_PP_REPEAT(size, BOOST_COMPUTE_VECTOR_TYPE_ASSIGN_CTOR_ARG, _) \
        } \
    };

#define BOOST_COMPUTE_DECLARE_VECTOR_TYPE(scalar, size) \
    BOOST_COMPUTE_DECLARE_VECTOR_TYPE_CLASS(BOOST_PP_CAT(cl_, scalar), \
                                            size, \
                                            BOOST_PP_CAT(BOOST_PP_CAT(scalar, size), _)) \
    \
    inline std::ostream& operator<<( \
        std::ostream &s, \
        const BOOST_COMPUTE_MAKE_VECTOR_TYPE(scalar, size) &v) \
    { \
        s << BOOST_PP_STRINGIZE(BOOST_PP_CAT(scalar, size)) << "("; \
        for(size_t i = 0; i < size; i++){\
            s << v[i]; \
            if(i != size - 1){\
                s << ", "; \
            } \
        } \
        s << ")"; \
        return s; \
    }

#define BOOST_COMPUTE_DECLARE_VECTOR_TYPES(scalar) \
    BOOST_COMPUTE_DECLARE_VECTOR_TYPE(scalar, 2) \
    BOOST_COMPUTE_DECLARE_VECTOR_TYPE(scalar, 4) \
    BOOST_COMPUTE_DECLARE_VECTOR_TYPE(scalar, 8) \
    BOOST_COMPUTE_DECLARE_VECTOR_TYPE(scalar, 16) \

BOOST_COMPUTE_DECLARE_VECTOR_TYPES(char)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(uchar)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(short)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(ushort)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(int)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(uint)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(long)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(ulong)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(float)
BOOST_COMPUTE_DECLARE_VECTOR_TYPES(double)

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_TYPES_FUNDAMENTAL_HPP
