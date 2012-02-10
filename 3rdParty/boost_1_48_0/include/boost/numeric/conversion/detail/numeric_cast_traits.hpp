//
//! Copyright (c) 2011
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#if !defined(BOOST_NUMERIC_CONVERSION_DONT_USE_PREPROCESSED_FILES)
    #include <boost/numeric/conversion/detail/preprocessed/numeric_cast_traits.hpp>
#else
#if !BOOST_PP_IS_ITERATING

    #include <boost/preprocessor/iteration/iterate.hpp>
    #include <boost/preprocessor/seq/elem.hpp>
    #include <boost/preprocessor/seq/size.hpp>

    #if defined(__WAVE__) && defined(BOOST_NUMERIC_CONVERSION_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 2, line: 0, output: "preprocessed/numeric_cast_traits.hpp")
    #endif

//
//! Copyright (c) 2011
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
    #if defined(__WAVE__) && defined(BOOST_NUMERIC_CONVERSION_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
    #endif

    //! Generate the specializations for the built-in types.
    #if !defined( BOOST_NO_INT64_T )    
        #define BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES() \
                (char)                                   \
                (boost::int8_t)                          \
                (boost::uint8_t)                         \
                (boost::int16_t)                         \
                (boost::uint16_t)                        \
                (boost::int32_t)                         \
                (boost::uint32_t)                        \
                (boost::int64_t)                         \
                (boost::uint64_t)                        \
                (float)                                  \
                (double)                                 \
                (long double)                            \
        /***/
    #else
        #define BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES() \
                (char)                                   \
                (boost::int8_t)                          \
                (boost::uint8_t)                         \
                (boost::int16_t)                         \
                (boost::uint16_t)                        \
                (boost::int32_t)                         \
                (boost::uint32_t)                        \
                (float)                                  \
                (double)                                 \
                (long double)                            \
        /***/
    #endif

namespace boost { namespace numeric {

    #define BOOST_PP_ITERATION_PARAMS_1 (3, (0, BOOST_PP_DEC(BOOST_PP_SEQ_SIZE(BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES())), <boost/numeric/conversion/detail/numeric_cast_traits.hpp>))
    #include BOOST_PP_ITERATE()    

}}//namespace boost::numeric;

    #if defined(__WAVE__) && defined(BOOST_NUMERIC_CONVERSION_CREATE_PREPROCESSED_FILES)
        #pragma wave option(output: null)
    #endif   

    #undef BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES

#elif BOOST_PP_ITERATION_DEPTH() == 1

   #define BOOST_PP_ITERATION_PARAMS_2 (3, (0, BOOST_PP_DEC(BOOST_PP_SEQ_SIZE(BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES())), <boost/numeric/conversion/detail/numeric_cast_traits.hpp>))
   #include BOOST_PP_ITERATE()
       
#elif BOOST_PP_ITERATION_DEPTH() == 2

    //! Generate default traits for the specified source and target.
    #define BOOST_NUMERIC_CONVERSION_A BOOST_PP_FRAME_ITERATION(1)
    #define BOOST_NUMERIC_CONVERSION_B BOOST_PP_FRAME_ITERATION(2)

    template <>
    struct numeric_cast_traits
        <
            BOOST_PP_SEQ_ELEM(BOOST_NUMERIC_CONVERSION_A, BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES())
          , BOOST_PP_SEQ_ELEM(BOOST_NUMERIC_CONVERSION_B, BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES())
        >
    {
        typedef def_overflow_handler overflow_policy;
        typedef UseInternalRangeChecker range_checking_policy;
        typedef Trunc<BOOST_PP_SEQ_ELEM(BOOST_NUMERIC_CONVERSION_B, BOOST_NUMERIC_CONVERSION_BUILTIN_TYPES())> rounding_policy;
    };     

    #undef BOOST_NUMERIC_CONVERSION_A
    #undef BOOST_NUMERIC_CONVERSION_B

#endif//! Depth 2.
#endif// BOOST_NUMERIC_CONVERSION_DONT_USE_PREPROCESSED_FILES
