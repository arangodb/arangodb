// Copyright John Maddock 2006.
// Copyright Paul A. Bristow 2007, 2009
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#  pragma warning(disable : 4756) // overflow in constant arithmetic
// Constants are too big for float case, but this doesn't matter for test.
#endif

#include <boost/math/concepts/real_concept.hpp>
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/math/special_functions/math_fwd.hpp>
#include <boost/array.hpp>
#include "functor.hpp"

#include "handle_test_result.hpp"
#include "table_type.hpp"

#ifndef SC_
#define SC_(x) static_cast<typename table_type<T>::type>(BOOST_JOIN(x, L))
#endif

template <class Real, typename T>
void do_test_ellint_e2(const T& data, const char* type_name, const char* test)
{
#if !(defined(ERROR_REPORTING_MODE) && !defined(ELLINT_2_FUNCTION_TO_TEST))
   typedef Real                   value_type;

   std::cout << "Testing: " << test << std::endl;
#ifdef ELLINT_2_FUNCTION_TO_TEST
   value_type(*fp2)(value_type, value_type) = ELLINT_2_FUNCTION_TO_TEST;
#elif defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
    value_type (*fp2)(value_type, value_type) = boost::math::ellint_2<value_type, value_type>;
#else
    value_type (*fp2)(value_type, value_type) = boost::math::ellint_2;
#endif
    boost::math::tools::test_result<value_type> result;

    result = boost::math::tools::test_hetero<Real>(
      data,
      bind_func<Real>(fp2, 1, 0),
      extract_result<Real>(2));
   handle_test_result(result, data[result.worst()], result.worst(),
      type_name, "ellint_2", test);

   std::cout << std::endl;
#endif
}

template <class Real, typename T>
void do_test_ellint_e1(T& data, const char* type_name, const char* test)
{
#if !(defined(ERROR_REPORTING_MODE) && !defined(ELLINT_2C_FUNCTION_TO_TEST))
   typedef Real                   value_type;
    boost::math::tools::test_result<value_type> result;

   std::cout << "Testing: " << test << std::endl;

#ifdef ELLINT_2C_FUNCTION_TO_TEST
   value_type(*fp1)(value_type) = ELLINT_2C_FUNCTION_TO_TEST;
#elif defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
   value_type (*fp1)(value_type) = boost::math::ellint_2<value_type>;
#else
   value_type (*fp1)(value_type) = boost::math::ellint_2;
#endif
   result = boost::math::tools::test_hetero<Real>(
      data,
      bind_func<Real>(fp1, 0),
      extract_result<Real>(1));
   handle_test_result(result, data[result.worst()], result.worst(),
      type_name, "ellint_2 (complete)", test);

   std::cout << std::endl;
#endif
}

template <typename T>
void test_spots(T, const char* type_name)
{
    BOOST_MATH_STD_USING
    // Function values calculated on http://functions.wolfram.com/
    // Note that Mathematica's EllipticE accepts k^2 as the second parameter.
    static const boost::array<boost::array<T, 3>, 10> data1 = {{
        {{ SC_(0.0), SC_(0.0), SC_(0.0) }},
        {{ SC_(-10.0), SC_(0.0), SC_(-10.0) }},
        {{ SC_(-1.0), SC_(-1.0), SC_(-0.84147098480789650665250232163029899962256306079837) }},
        {{ SC_(-4.0), T(900) / 1024, SC_(-3.1756145986492562317862928524528520686391383168377) }},
        {{ SC_(8.0), T(-600) / 1024, SC_(7.2473147180505693037677015377802777959345489333465) }},
        {{ SC_(1e-05), T(800) / 1024, SC_(9.999999999898274739584436515967055859383969942432E-6) }},
        {{ SC_(1e+05), T(100) / 1024, SC_(99761.153306972066658135668386691227343323331995888) }},
        {{ SC_(1e+10), SC_(-0.5), SC_(9.3421545766487137036576748555295222252286528414669e9) }},
        {{ static_cast<T>(ldexp(T(1), 66)), T(400) / 1024, SC_(7.0886102721911705466476846969992069994308167515242e19) }},
        {{ static_cast<T>(ldexp(T(1), 166)), T(900) / 1024, SC_(7.1259011068364515942912094521783688927118026465790e49) }},
    }};

    do_test_ellint_e2<T>(data1, type_name, "Elliptic Integral E: Mathworld Data");

#include "ellint_e2_data.ipp"

    do_test_ellint_e2<T>(ellint_e2_data, type_name, "Elliptic Integral E: Random Data");

    // Function values calculated on http://functions.wolfram.com/
    // Note that Mathematica's EllipticE accepts k^2 as the second parameter.
    static const boost::array<boost::array<T, 2>, 10> data2 = {{
        {{ SC_(-1.0), SC_(1.0) }},
        {{ SC_(0.0), SC_(1.5707963267948966192313216916397514420985846996876) }},
        {{ T(100) / 1024, SC_(1.5670445330545086723323795143598956428788609133377) }},
        {{ T(200) / 1024, SC_(1.5557071588766556854463404816624361127847775545087) }},
        {{ T(300) / 1024, SC_(1.5365278991162754883035625322482669608948678755743) }},
        {{ T(400) / 1024, SC_(1.5090417763083482272165682786143770446401437564021) }},
        {{ SC_(-0.5), SC_(1.4674622093394271554597952669909161360253617523272) }},
        {{ T(-600) / 1024, SC_(1.4257538571071297192428217218834579920545946473778) }},
        {{ T(-800) / 1024, SC_(1.2927868476159125056958680222998765985004489572909) }},
        {{ T(-900) / 1024, SC_(1.1966864890248739524112920627353824133420353430982) }},
    }};

    do_test_ellint_e1<T>(data2, type_name, "Elliptic Integral E: Mathworld Data");

#include "ellint_e_data.ipp"

    do_test_ellint_e1<T>(ellint_e_data, type_name, "Elliptic Integral E: Random Data");

    static const boost::array<boost::array<T, 3>, 71> small_angles = { {
       {{ ldexp(T(1), -10), SC_(0.5), SC_(0.00097656246119489873806295171767681129) }}, {{ ldexp(T(1), -11), SC_(0.5), SC_(0.00048828124514936177847275804383491090) }}, {{ ldexp(T(1), -12), SC_(0.5), SC_(0.00024414062439367020469080959924292294) }}, {{ ldexp(T(1), -13), SC_(0.5), SC_(0.00012207031242420877503577978533579671) }}, {{ ldexp(T(1), -14), SC_(0.5), SC_(0.000061035156240526096862267116434822602) }}, {{ ldexp(T(1), -15), SC_(0.5), SC_(0.000030517578123815762107245722156263287) }}, {{ ldexp(T(1), -16), SC_(0.5), SC_(0.000015258789062351970263388913163340974) }}, {{ ldexp(T(1), -17), SC_(0.5), SC_(7.6293945312314962829230890795991109e-6) }}, {{ ldexp(T(1), -18), SC_(0.5), SC_(3.8146972656226870353653697266430603e-6) }}, {{ ldexp(T(1), -19), SC_(0.5), SC_(1.9073486328122108794206707030707941e-6) }}, {{ ldexp(T(1), -20), SC_(0.5), SC_(9.5367431640621385992758382186011213e-7) }}, {{ ldexp(T(1), -21), SC_(0.5), SC_(4.7683715820312048249094797723177223e-7) }}, {{ ldexp(T(1), -22), SC_(0.5), SC_(2.3841857910156193531136849713832335e-7) }}, {{ ldexp(T(1), -23), SC_(0.5), SC_(1.1920928955078117941392106214180141e-7) }}, {{ ldexp(T(1), -24), SC_(0.5), SC_(5.9604644775390616176740132767709895e-8) }}, {{ ldexp(T(1), -25), SC_(0.5), SC_(2.9802322387695311397092516595963259e-8) }}, {{ ldexp(T(1), -26), SC_(0.5), SC_(1.4901161193847656112136564574495392e-8) }}, {{ ldexp(T(1), -27), SC_(0.5), SC_(7.4505805969238281077670705718119236e-9) }}, {{ ldexp(T(1), -28), SC_(0.5), SC_(3.7252902984619140603458838214764904e-9) }}, {{ ldexp(T(1), -29), SC_(0.5), SC_(1.8626451492309570309807354776845613e-9) }}, {{ ldexp(T(1), -30), SC_(0.5), SC_(9.3132257461547851559134193471057016e-10) }}, {{ ldexp(T(1), -31), SC_(0.5), SC_(4.6566128730773925780829274183882127e-10) }}, {{ ldexp(T(1), -32), SC_(0.5), SC_(2.3283064365386962890572409272985266e-10) }}, {{ ldexp(T(1), -33), SC_(0.5), SC_(1.1641532182693481445305926159123158e-10) }}, {{ ldexp(T(1), -34), SC_(0.5), SC_(5.8207660913467407226554282698903948e-11) }}, {{ ldexp(T(1), -35), SC_(0.5), SC_(2.9103830456733703613280222837362993e-11) }}, {{ ldexp(T(1), -36), SC_(0.5), SC_(1.4551915228366851806640496604670374e-11) }}, {{ ldexp(T(1), -37), SC_(0.5), SC_(7.2759576141834259033202964505837968e-12) }}, {{ ldexp(T(1), -38), SC_(0.5), SC_(3.6379788070917129516601542438229746e-12) }}, {{ ldexp(T(1), -39), SC_(0.5), SC_(1.8189894035458564758300778742278718e-12) }}, {{ ldexp(T(1), -40), SC_(0.5), SC_(9.0949470177292823791503903115348398e-13) }}, {{ ldexp(T(1), -41), SC_(0.5), SC_(4.5474735088646411895751952733168550e-13) }}, {{ ldexp(T(1), -42), SC_(0.5), SC_(2.2737367544323205947875976513521069e-13) }}, {{ ldexp(T(1), -43), SC_(0.5), SC_(1.1368683772161602973937988275127634e-13) }}, {{ ldexp(T(1), -44), SC_(0.5), SC_(5.6843418860808014869689941398597042e-14) }}, {{ ldexp(T(1), -45), SC_(0.5), SC_(2.8421709430404007434844970702168380e-14) }}, {{ ldexp(T(1), -46), SC_(0.5), SC_(1.4210854715202003717422485351442923e-14) }}, {{ ldexp(T(1), -47), SC_(0.5), SC_(7.1054273576010018587112426757663028e-15) }}, {{ ldexp(T(1), -48), SC_(0.5), SC_(3.5527136788005009293556213378887566e-15) }}, {{ ldexp(T(1), -49), SC_(0.5), SC_(1.7763568394002504646778106689450790e-15) }}, {{ ldexp(T(1), -50), SC_(0.5), SC_(8.8817841970012523233890533447262706e-16) }}, {{ ldexp(T(1), -51), SC_(0.5), SC_(4.4408920985006261616945266723632448e-16) }}, {{ ldexp(T(1), -52), SC_(0.5), SC_(2.2204460492503130808472633361816361e-16) }}, {{ ldexp(T(1), -53), SC_(0.5), SC_(1.1102230246251565404236316680908197e-16) }}, {{ ldexp(T(1), -54), SC_(0.5), SC_(5.5511151231257827021181583404541008e-17) }}, {{ ldexp(T(1), -55), SC_(0.5), SC_(2.7755575615628913510590791702270507e-17) }}, {{ ldexp(T(1), -56), SC_(0.5), SC_(1.3877787807814456755295395851135254e-17) }}, {{ ldexp(T(1), -57), SC_(0.5), SC_(6.9388939039072283776476979255676269e-18) }}, {{ ldexp(T(1), -58), SC_(0.5), SC_(3.4694469519536141888238489627838135e-18) }}, {{ ldexp(T(1), -59), SC_(0.5), SC_(1.7347234759768070944119244813919067e-18) }}, {{ ldexp(T(1), -60), SC_(0.5), SC_(8.6736173798840354720596224069595337e-19) }}, {{ ldexp(T(1), -61), SC_(0.5), SC_(4.3368086899420177360298112034797668e-19) }}, {{ ldexp(T(1), -62), SC_(0.5), SC_(2.1684043449710088680149056017398834e-19) }}, {{ ldexp(T(1), -63), SC_(0.5), SC_(1.0842021724855044340074528008699417e-19) }}, {{ ldexp(T(1), -64), SC_(0.5), SC_(5.4210108624275221700372640043497086e-20) }}, {{ ldexp(T(1), -65), SC_(0.5), SC_(2.7105054312137610850186320021748543e-20) }}, {{ ldexp(T(1), -66), SC_(0.5), SC_(1.3552527156068805425093160010874271e-20) }}, {{ ldexp(T(1), -67), SC_(0.5), SC_(6.7762635780344027125465800054371357e-21) }}, {{ ldexp(T(1), -68), SC_(0.5), SC_(3.3881317890172013562732900027185678e-21) }}, {{ ldexp(T(1), -69), SC_(0.5), SC_(1.6940658945086006781366450013592839e-21) }}, {{ ldexp(T(1), -70), SC_(0.5), SC_(8.4703294725430033906832250067964196e-22) }}, {{ ldexp(T(1), -71), SC_(0.5), SC_(4.2351647362715016953416125033982098e-22) }}, {{ ldexp(T(1), -72), SC_(0.5), SC_(2.1175823681357508476708062516991049e-22) }}, {{ ldexp(T(1), -73), SC_(0.5), SC_(1.0587911840678754238354031258495525e-22) }}, {{ ldexp(T(1), -74), SC_(0.5), SC_(5.2939559203393771191770156292477623e-23) }}, {{ ldexp(T(1), -75), SC_(0.5), SC_(2.6469779601696885595885078146238811e-23) }}, {{ ldexp(T(1), -76), SC_(0.5), SC_(1.3234889800848442797942539073119406e-23) }}, {{ ldexp(T(1), -77), SC_(0.5), SC_(6.6174449004242213989712695365597028e-24) }}, {{ ldexp(T(1), -78), SC_(0.5), SC_(3.3087224502121106994856347682798514e-24) }}, {{ ldexp(T(1), -79), SC_(0.5), SC_(1.6543612251060553497428173841399257e-24) }}, {{ ldexp(T(1), -80), SC_(0.5), SC_(8.2718061255302767487140869206996285e-25) }}
       } };
    do_test_ellint_e2<T>(small_angles, type_name, "Elliptic Integral E: Small Angles");
}

