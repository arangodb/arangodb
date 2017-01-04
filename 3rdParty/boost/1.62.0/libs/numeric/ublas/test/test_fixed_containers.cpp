#undef BOOST_UBLAS_NO_EXCEPTIONS
#include "common/testhelper.hpp"
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/assignment.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <string>
#include <sstream>
#include <complex>
#include <iomanip>
#include "utils.hpp"

#ifdef BOOST_UBLAS_CPP_GE_2011

using namespace boost::numeric::ublas;

using std::cout;
using std::endl;

template < class T >
bool test_vector( std::string type_name)
{
    std::stringstream stream;
    stream << "Testing for: " << type_name;
    BOOST_UBLAS_DEBUG_TRACE( stream.str() );

    bool pass = true;

    {
        typedef fixed_vector<T, 1> vec1;

        vec1 v1( 122.0 );

        pass &= ( v1(0) == (T)122 );

    }

    {
        typedef fixed_vector<T, 3> vec3;

        vec3 v1((T)0.0, (T)0.0, (T)0.0);

        pass &=(sizeof( vec3 )  == v1.size()*sizeof( T ) ) ;

        vector<T> v( 3, 0 ) ;

        pass &= compare( v1, v );

        v1 <<= 10.0, 10, 33;
        v  <<= 10.0, 10, 33;

        //cout << std::setprecision(20) << v1 << '\n' << v;

        pass &= compare( v1, v );


        vec3 v2;

        v2( 0 ) = 10.0; v2( 1 ) = 10; v2( 2 ) = 33;
        pass &= compare( v, v2 );

        v2 += v;

        pass &= compare( v2, 2*v );


        v1 = 2*v1 + v - 6*v2;
        pass &= compare( v1, (3-2*6)*v );


        vec3 v3{ (T)-90.0, (T)-90.0, (T)-297.0 };
        pass &= compare( v3, v1 );

        vec3 v4 =  { (T)-90.0, (T)-90.0, (T)-297.0 };
        pass &= compare( v4, v1 );

        vec3 v5( (T)-90.0, (T)-90.0, (T)-297.0 );
        pass &= compare( v5, v1 );

        vec3 v6((T) 5.0, (T)8.0, (T)9.0);

        matrix<T> M = outer_prod( v6, v6), L( 3, 3);

        L <<= 25, 40, 45, 40, 64, 72, 45, 72, 81;

        pass &= compare( M, L );

        L  <<= 1, 2, 3, 4, 5, 6, 7, 8, 9;
        v6 <<= 4, 5, 6;
        vec3 v7 ( (T)32.0, (T)77.0, (T)122.0 );

        pass &= compare( v7, prod(L, v6) );

        vec3 v8;
        noalias( v8 ) = prod(L, v6);

        pass &= compare( v7, v8 );

    }


    {
        const std::size_t N = 33;
        typedef fixed_vector<T, N> vec33;

        vec33 v1;
        vector<T> v( N );

        for ( std::size_t i = 0; i!= v1.size(); i++)
        {
            v1( i ) = 3.14159*i;
            v ( i ) = 3.14159*i;
        }

        pass &= compare( v1, v );


        auto ip = inner_prod( v, v);
        auto ip1 = inner_prod( v1, v1);

        pass &= (  ip == ip1 ) ;

        T c = 0;
        for (auto i = v1.begin(); i != v1.end(); i++)
        {
            *i = c;
            c = c + 1;
        }

        c = 0;
        for (auto i = v.begin(); i != v.end(); i++)
        {
            *i = c;
            c = c + 1;
        }

        pass &= compare( v1, v );

        // Check if bad index indeed works
        try {
            T a;
            a=v1( 100 );
            BOOST_UBLAS_NOT_USED( a );

        } catch ( bad_index &e) {
            std::cout << " Caught (GOOD): " << e.what() << endl;
            pass &= true;
        }


    }
    return pass;
}

template < class T >
bool test_matrix( std::string type_name)
{
    std::stringstream stream;
    stream << "Testing for: " << type_name;
    BOOST_UBLAS_DEBUG_TRACE( stream.str() );

    bool pass = true;

    typedef fixed_matrix<T, 3, 4> mat34;
    typedef fixed_matrix<T, 4, 3> mat43;
    typedef fixed_matrix<T, 3, 3> mat33;


    {
        typedef fixed_matrix<T, 1, 1> mat1;

        mat1 m1( 122.0 );

        pass &= ( m1(0, 0) == (T)122 );
    }


    {
        mat34 m1( 3.0 );

        pass &=(sizeof( mat34 )  == m1.size1()*m1.size2()*sizeof( T ) ) ;

        matrix<T> m( 3.0, 4.0, 3.0 ) ;

        pass &= compare( m1, m );

        cout << m1 << endl;
        cout << m << endl;


        m1 <<= 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;
        m  <<= 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;

        pass &= compare( m1, m );

        cout << m1 << endl;
        cout << m << endl;

        mat34 m2( 0.0 );

        T count = 1 ;
        for ( std::size_t i = 0; i != m2.size1(); i++)
        {
            for (std::size_t j = 0; j!= m2.size2(); j++)
            {
                m2( i, j ) = count;
                count = count + 1;
            }

        }
        pass &= compare( m2, m );
        cout << m2 << endl;

    }
    {
        mat34 m1 = { (T)1, (T)2, (T)3, (T)3, (T)3, (T)2, (T)5, (T)4, (T)2, (T)6, (T)5, (T)2 };
        mat43 m2 = { (T)4, (T)5, (T)6, (T)3, (T)2, (T)2, (T)1, (T)4, (T)2, (T)6, (T)5, (T)2 };

        mat33 m3 = prod(m1, m2);

        matrix<T> m(3, 3);
        m <<= 31,36,22,47,59,40,43,52,38;

        pass &= compare(m ,m3);

        mat33 m4;
        m4 <<= (T)1, (T)2, (T)1, (T)2, (T)1, (T)3, (T)1, (T)2, (T) 5;
        m3  = prod(m4, trans(m4));

        m<<=6,7,10,7,14,19,10,19,30;

        cout << m3 << endl;
        pass &= compare(m ,m3);

        m3 = 2 * m4 - 1 * m3;

        cout << m3;

        m <<= -4,-3,-8,-3,-12,-13,-8,-15,-20;

        pass &= compare(m, m3);

        m = m3;

        m3 = trans(m);

        pass &= compare(m3, trans(m));

        // Check if bad index indeed works
        try {
            T a;
            a=m1( 100, 100 );
            BOOST_UBLAS_NOT_USED( a );

        } catch ( bad_index &e) {
            std::cout << " Caught (GOOD): " << e.what() << endl;
            pass &= true;
        }

    }

    return pass;

}

BOOST_UBLAS_TEST_DEF (test_fixed) {

    BOOST_UBLAS_DEBUG_TRACE( "Starting fixed container tests" );

    BOOST_UBLAS_TEST_CHECK(  test_vector< double >( "double") );
    BOOST_UBLAS_TEST_CHECK(  test_vector< float >( "float") );
    BOOST_UBLAS_TEST_CHECK(  test_vector< int >( "int") );

    BOOST_UBLAS_TEST_CHECK(  test_vector< std::complex<double> >( "std::complex<double>") );
    BOOST_UBLAS_TEST_CHECK(  test_vector< std::complex<float> >( "std::complex<float>") );
    BOOST_UBLAS_TEST_CHECK(  test_vector< std::complex<int> >( "std::complex<int>") );

    BOOST_UBLAS_TEST_CHECK(  test_matrix< double >( "double") );
    BOOST_UBLAS_TEST_CHECK(  test_matrix< float >( "float") );
    BOOST_UBLAS_TEST_CHECK(  test_matrix< int >( "int") );

    BOOST_UBLAS_TEST_CHECK(  test_matrix< std::complex<double> >( "std::complex<double>") );
    BOOST_UBLAS_TEST_CHECK(  test_matrix< std::complex<float> >( "std::complex<float>") );
    BOOST_UBLAS_TEST_CHECK(  test_matrix< std::complex<int> >( "std::complex<int>") );
}


int main () {

    BOOST_UBLAS_TEST_BEGIN();

    BOOST_UBLAS_TEST_DO( test_fixed );

    BOOST_UBLAS_TEST_END();
    return EXIT_SUCCESS;

}

#else

int main () {

    BOOST_UBLAS_TEST_BEGIN();
    BOOST_UBLAS_TEST_END();

    return EXIT_SUCCESS;

}
#endif // BOOST_UBLAS_CPP_GE_2011
