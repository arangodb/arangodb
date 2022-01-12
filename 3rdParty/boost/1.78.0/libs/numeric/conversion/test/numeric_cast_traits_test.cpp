//
//! Copyright (c) 2011
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/operators.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/cstdint.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/static_assert.hpp>

//! Define a simple custom number
struct Double
{
    Double()
        : v(0)
    {}

    template <typename T>
    explicit Double( T v )
        : v(static_cast<double>(v))
    {}

    template <typename T>
    Double& operator= ( T t )
    {
        v = static_cast<double>(t);
        return *this;
    }

    bool operator < ( const Double& rhs ) const
    {
        return v < rhs.v;
    }

    template <typename T>
    bool operator < ( T rhs ) const
    {
        return v < static_cast<double>(rhs);
    }

    template <typename LHS>
    friend bool operator < ( const LHS& lhs, const Double& rhs )
    {
        return lhs < rhs.v;
    }

    bool operator > ( const Double& rhs ) const
    {
        return v > rhs.v;
    }

    template <typename LHS>
    friend bool operator > ( const LHS& lhs, const Double& rhs )
    {
        return lhs > rhs.v;
    }
    
    template <typename T>
    bool operator > ( T rhs ) const
    {
        return v > static_cast<double>(rhs);
    }
    
    bool operator == ( const Double& rhs ) const
    {
        return v == rhs.v;
    }
    
    template <typename T>
    bool operator == ( T rhs ) const
    {
        return v == static_cast<double>(rhs);
    }

    template <typename LHS>
    friend bool operator == ( const LHS& lhs, const Double& rhs )
    {
        return lhs == rhs.v;
    }
    
    bool operator !() const
    {
        return v == 0; 
    }
    
    Double operator -() const
    {
        return Double(-v);
    }
    
    Double& operator +=( const Double& t )
    {
        v += t.v;
        return *this;
    }
    
    template <typename T>
    Double& operator +=( T t )
    {
        v += static_cast<double>(t);
        return *this;
    }
    
    Double& operator -=( const Double& t )
    {
        v -= t.v;
        return *this;
    }
    
    template <typename T>
    Double& operator -=( T t )
    {
        v -= static_cast<double>(t);
        return *this;
    }
    
    Double& operator *= ( const Double& factor )
    {
        v *= factor.v;
        return *this;
    }
    
    template <typename T>
    Double& operator *=( T t )
    {
        v *= static_cast<double>(t);
        return *this;
    }

    Double& operator /= (const Double& divisor)
    {
        v /= divisor.v;
        return *this;
    }
    
    template <typename T>
    Double& operator /=( T t )
    {
         v /= static_cast<double>(t);
        return (*this);       
    }
    
    double v;
};

//! Define numeric_limits for the custom type.
namespace std
{
    template<>
    class numeric_limits< Double > : public numeric_limits<double>
    {
    public:

        //! Limit our Double to a range of +/- 100.0
        static Double (min)()
        {            
            return Double(1.e-2);
        }

        static Double (max)()
        {
            return Double(1.e2);
        }

        static Double epsilon()
        {
            return Double( std::numeric_limits<double>::epsilon() );
        }
    };
}

//! Define range checking and overflow policies.
namespace custom
{
    //! Define a custom range checker
    template<typename Traits, typename OverFlowHandler>
    struct range_checker
    {
        typedef typename Traits::argument_type argument_type ;
        typedef typename Traits::source_type S;
        typedef typename Traits::target_type T;
        
        //! Check range of integral types.
        static boost::numeric::range_check_result out_of_range( argument_type s )
        {
            using namespace boost::numeric;
            if( s > bounds<T>::highest() )
                return cPosOverflow;
            else if( s < bounds<T>::lowest() )
                return cNegOverflow;
            else
                return cInRange;
        }

        static void validate_range ( argument_type s )
        {
            BOOST_STATIC_ASSERT( std::numeric_limits<T>::is_bounded );
            OverFlowHandler()( out_of_range(s) );
        }
    };

    //! Overflow handler
    struct positive_overflow{};
    struct negative_overflow{};

    struct overflow_handler
    {
        void operator() ( boost::numeric::range_check_result r )
        {
            using namespace boost::numeric;
            if( r == cNegOverflow )
                throw negative_overflow() ;
            else if( r == cPosOverflow )
                throw positive_overflow() ;
        }
    };

    //! Define a rounding policy and specialize on the custom type.
    template<class S>
    struct Ceil : boost::numeric::Ceil<S>{};

    template<>
    struct Ceil<Double>
    {
      typedef Double source_type;

      typedef Double const& argument_type;

      static source_type nearbyint ( argument_type s )
      {
#if !defined(BOOST_NO_STDC_NAMESPACE)
          using std::ceil ;
#endif
          return Double( ceil(s.v) );
      }

      typedef boost::mpl::integral_c< std::float_round_style, std::round_toward_infinity> round_style;
    };

    //! Define a rounding policy and specialize on the custom type.
    template<class S>
    struct Trunc: boost::numeric::Trunc<S>{};

    template<>
    struct Trunc<Double>
    {
      typedef Double source_type;

      typedef Double const& argument_type;

      static source_type nearbyint ( argument_type s )
      {
#if !defined(BOOST_NO_STDC_NAMESPACE)
          using std::floor;
#endif
          return Double( floor(s.v) );
      }

      typedef boost::mpl::integral_c< std::float_round_style, std::round_toward_zero> round_style;
    };
}//namespace custom;

namespace boost { namespace numeric {

    //! Define the numeric_cast_traits specializations on the custom type.
    template <typename S>
    struct numeric_cast_traits<Double, S>
    {
        typedef custom::overflow_handler                         overflow_policy;
        typedef custom::range_checker
                <
                    boost::numeric::conversion_traits<Double, S>
                  , overflow_policy
                >                                                range_checking_policy;
        typedef boost::numeric::Trunc<S>                         rounding_policy;
    };
    
    template <typename T>
    struct numeric_cast_traits<T, Double>
    {
        typedef custom::overflow_handler                         overflow_policy;
        typedef custom::range_checker
                <
                    boost::numeric::conversion_traits<T, Double>
                  , overflow_policy
                >                                                range_checking_policy;
        typedef custom::Trunc<Double>                            rounding_policy;
    };

    //! Define the conversion from the custom type to built-in types and vice-versa.
    template<typename T>
    struct raw_converter< conversion_traits< T, Double > >
    {
        static T low_level_convert ( const Double& n )
        {
            return static_cast<T>( n.v ); 
        }
    };

    template<typename S>
    struct raw_converter< conversion_traits< Double, S > >
    {
        static Double low_level_convert ( const S& n )
        {
            return Double(n); 
        }
    };
}}//namespace boost::numeric;

#define BOOST_TEST_CATCH_CUSTOM_POSITIVE_OVERFLOW( CastCode ) \
    BOOST_TEST_THROWS( CastCode, custom::positive_overflow )
/***/

#define BOOST_TEST_CATCH_CUSTOM_NEGATIVE_OVERFLOW( CastCode ) \
    BOOST_TEST_THROWS( CastCode, custom::negative_overflow )
/***/

struct test_cast_traits
{
    template <typename T>
    void operator()(T) const
    {
        Double d = boost::numeric_cast<Double>( static_cast<T>(50) );
        BOOST_TEST( d.v == 50. );
        T v = boost::numeric_cast<T>( d );
        BOOST_TEST( v == 50 );
    }
};

void test_numeric_cast_traits()
{
    typedef boost::mpl::vector
        <
            boost::int8_t
          , boost::uint8_t
          , boost::int16_t
          , boost::uint16_t
          , boost::int32_t
          , boost::uint32_t
#if !defined( BOOST_NO_INT64_T )
          , boost::int64_t
          , boost::uint64_t
#endif
          , float
          , double
          , long double 
        > types;
    boost::mpl::for_each<types>( test_cast_traits() );
        
    //! Check overflow handler.
    Double d( 56.0 );
    BOOST_TEST_CATCH_CUSTOM_POSITIVE_OVERFLOW( d = boost::numeric_cast<Double>( 101 ) );
    BOOST_TEST( d.v == 56. );
    BOOST_TEST_CATCH_CUSTOM_NEGATIVE_OVERFLOW( d = boost::numeric_cast<Double>( -101 ) );
    BOOST_TEST( d.v == 56.);

    //! Check custom round policy.
    d = 5.9;
    int five = boost::numeric_cast<int>( d );
    BOOST_TEST( five == 5 );
}

int main()
{
    test_numeric_cast_traits();
    return boost::report_errors();
}

#undef BOOST_TEST_CATCH_CUSTOM_POSITIVE_OVERFLOW
#undef BOOST_TEST_CATCH_CUSTOM_NEGATIVE_OVERFLOW
