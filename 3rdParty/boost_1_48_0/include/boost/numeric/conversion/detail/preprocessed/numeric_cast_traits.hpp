//
//! Copyright (c) 2011
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
    
namespace boost { namespace numeric {

    template <>
    struct numeric_cast_traits
        <
            char
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            char
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int8_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint8_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int16_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint16_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int32_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint32_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::int64_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            boost::uint64_t
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            float
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            float
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            double
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            double
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , char
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<char> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::int8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::uint8_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint8_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::int16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::uint16_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint16_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::int32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::uint32_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint32_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::int64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::int64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , boost::uint64_t
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<boost::uint64_t> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , float
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<float> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<double> rounding_policy;
    }; 
    
    template <>
    struct numeric_cast_traits
        <
            long double
          , long double
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<long double> rounding_policy;
    }; 
       
}}
