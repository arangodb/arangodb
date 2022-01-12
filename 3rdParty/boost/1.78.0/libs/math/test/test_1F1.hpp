// Copyright John Maddock 2006.
// Copyright Paul A. Bristow 2007, 2009
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_MATH_OVERFLOW_ERROR_POLICY ignore_error

#include <boost/math/concepts/real_concept.hpp>
#include <boost/math/special_functions/math_fwd.hpp>
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>
#include <boost/math/tools/stats.hpp>
#include <boost/math/tools/test.hpp>
#include <boost/math/tools/big_constant.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <boost/array.hpp>
#include "functor.hpp"

#include "handle_test_result.hpp"
#include "table_type.hpp"

#include <boost/math/special_functions/hypergeometric_1F1.hpp>
#include <boost/math/quadrature/exp_sinh.hpp>

#ifdef _MSC_VER
#pragma warning(disable:4127)
#endif

template <class Real, class T>
void do_test_1F1(const T& data, const char* type_name, const char* test_name)
{
   typedef Real                   value_type;

   typedef value_type(*pg)(value_type, value_type, value_type);
#if defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
   pg funcp = boost::math::hypergeometric_0F1<value_type, value_type>;
#else
   pg funcp = boost::math::hypergeometric_1F1;
#endif

   boost::math::tools::test_result<value_type> result;

   std::cout << "Testing " << test_name << " with type " << type_name
      << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

   //
   // test hypergeometric_2F0 against data:
   //
   result = boost::math::tools::test_hetero<Real>(
      data,
      bind_func<Real>(funcp, 0, 1, 2),
      extract_result<Real>(3));
   handle_test_result(result, data[result.worst()], result.worst(), type_name, "hypergeometric_1F1", test_name);
   std::cout << std::endl;
}

#ifndef SC_
#define SC_(x) BOOST_MATH_BIG_CONSTANT(T, 1000000, x)
#endif

template <class T>
void test_spots1(T, const char* type_name)
{
#include "hypergeometric_1F1.ipp"

   do_test_1F1<T>(hypergeometric_1F1, type_name, "Integer a values");

#include "hypergeometric_1F1_small_random.ipp"

   do_test_1F1<T>(hypergeometric_1F1_small_random, type_name, "Small random values");
}

template <class T>
void test_spots2(T, const char* type_name)
{
#include "hypergeometric_1F1_big.ipp"

   do_test_1F1<T>(hypergeometric_1F1_big, type_name, "Large random values");
}

template <class T>
void test_spots3(T, const char* type_name)
{
#include "hypergeometric_1F1_big_double_limited.ipp"

   do_test_1F1<T>(hypergeometric_1F1_big_double_limited, type_name, "Large random values - double limited precision");
}

template <class T>
void test_spots4(T, const char* type_name)
{
#include "hypergeometric_1F1_big_unsolved.ipp"

   do_test_1F1<T>(hypergeometric_1F1_big, type_name, "Large random values - unsolved domains");
}

template <class T>
void test_spots5(T, const char* type_name)
{
   std::cout << "Testing special cases for type " << type_name << std::endl;
   BOOST_MATH_STD_USING
      //
      // Special cases:
      //
      using boost::math::hypergeometric_1F1;
   T tol = boost::math::tools::epsilon<T>() * 200;
   if (std::numeric_limits<T>::digits > std::numeric_limits<double>::digits)
      tol *= 2;
   if (boost::is_class<T>::value)
      tol *= 4;
   // b = 2a
   T computed = hypergeometric_1F1(T(-12.25), T(2 * -12.25), T(6.75));
   T expected = boost::lexical_cast<T>("22.995348157760091167706081204212893687052775606591209203948675272473773725021024450870565197330528784707135828761");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(12.25), T(2 * 12.25), T(6.75));
   expected = boost::lexical_cast<T>("36.47281964229300610642392880149257389834650024065756742702265701321933782423217084029882132197130099355867287657");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-11), T(-12), T(6.75));
   expected = boost::lexical_cast<T>("376.3166426246459656334542608880377435064935064935064935064935064935064935064935064935064935064935064935064935064");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-2), T(-12), T(6.75));
   expected = boost::lexical_cast<T>("2.470170454545454545454545454545454545454545454545454545454545454545454545454545454545454545454545454545454545");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-224), T(-1205), T(6.75));
   expected = boost::lexical_cast<T>("3.497033449657595724636676193024114597507981035316405619832857546161530808157860391434240068189887198094611519953");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(0.5), T(-1205.5), T(-6.75));
   expected = boost::lexical_cast<T>("1.00281149043026925155096279505879868076290060374397866773878698584557482321961231721407215665017657501846692575");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-0.5), T(-1205.5), T(-6.75));
   expected = boost::lexical_cast<T>("0.99719639844965644594352920596780535220516138060108955206195178371227403775248888108818326220977962797312690");
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-12), T(16.25), T(1043.75));
   expected = boost::lexical_cast<T>("1.26527673505477678311707565502355407505496430400394171269315320194708537626079491650410923064978320042481912e20");
   BOOST_CHECK_CLOSE(computed, expected, tol * 3);

   computed = hypergeometric_1F1(T(3.5), T(3.5), T(36.25));
   expected = exp(T(36.25));
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-3.5), T(-3.5), T(36.25));
   expected = exp(T(36.25));
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(1), T(2), T(36.25));
   expected = boost::math::expm1(T(36.25)) / T(36.25);
   BOOST_CHECK_CLOSE(computed, expected, tol * 3);
   computed = hypergeometric_1F1(T(10.25), T(9.25), T(36.25));
   expected = exp(T(36.25)) * (T(9.25) + T(36.25)) / T(9.25);
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-10.25), T(-11.25), T(36.25));
   expected = exp(T(36.25)) * (T(-11.25) + T(36.25)) / T(-11.25);
   BOOST_CHECK_CLOSE(computed, expected, tol);
   computed = hypergeometric_1F1(T(-10.25), T(-11.25), T(-36.25));
   expected = exp(T(-36.25)) * (T(-11.25) + T(-36.25)) / T(-11.25);
   BOOST_CHECK_CLOSE(computed, expected, tol);
}

template <class T>
void test_spots6(T, const char* type_name)
{
   static const boost::array<boost::array<T, 4>, 91> hypergeometric_1F1_bugs = { {
        { { static_cast<double>(17955.561660766602), static_cast<double>(9.6968994205831605e-09), static_cast<double>(-82.406154185533524), SC_(6.98056008378736714088730927132364938220428678e-11) }},
        { { static_cast<double>(17955.561660766602), static_cast<double>(-9.6968994205831605e-09), static_cast<double>(-82.406154185533524), SC_(-6.98055306629610746072607353939306734740549551e-11) }},
        { { static_cast<double>(-17955.561660766602), static_cast<double>(-9.6968994205831605e-09), static_cast<double>(82.406154185533524), SC_(-42897094853118832762870100.8669248353530950866) }} ,
        { { static_cast<double>(17955.561660766602), static_cast<double>(17956.061660766602), static_cast<double>(82.406154185533524), SC_(613117565438499794408370861624072730.553215432) }},
        { { static_cast<double>(2.9127331452327709e-07), static_cast<double>(-0.99999970872668542), static_cast<double>(0.15018942760070786), SC_(0.987526018990506843793601092932108059727149508) }},
        { { static_cast<double>(-2.9127331452327709e-07), static_cast<double>(-1.0000002912733146), static_cast<double>(0.15018942760070786), SC_(0.987526120661366412484942089372497015837368389) }},
        { { static_cast<double>(6.7191087900739423e-13), static_cast<double>(-0.99999999999932809), static_cast<double>(0.0011913633891253994), SC_(0.999999289758605006762757201699750974296453229) }},
        { { static_cast<double>(6.7191087900739423e-13), static_cast<double>(-0.99999999999932809), static_cast<double>(-0.0011913633891253994), SC_(0.999999290885918468326416221021126912154021802) }},
        { { static_cast<double>(-6.7191087900739423e-13), static_cast<double>(-1.0000000000006719), static_cast<double>(0.0011913633891253994), SC_(0.999999289758606609651292394510404091049823243) }},
        { { static_cast<double>(-6.7191087900739423e-13), static_cast<double>(-1.0000000000006719), static_cast<double>(-0.0011913633891253994), SC_(0.999999290885916869252591036674587894145399498) }},
        { { static_cast<double>(1.2860067365774887e-17), static_cast<double>(6.2442285664031425e-16), static_cast<double>(-2539.60133934021), SC_(0.979404874070484696999110600576068012417904384) }},
        { { static_cast<double>(1.2860067365774887e-17), static_cast<double>(-6.2442285664031425e-16), static_cast<double>(-2539.60133934021), SC_(1.0205951259295150865252112924093487321207727) }},
        { { static_cast<double>(-1.2860067365774887e-17), static_cast<double>(6.2442285664031425e-16), static_cast<double>(-2539.60133934021), SC_(1.02059512592951530745923325071510441026202975) }},
        { { static_cast<double>(-1.2860067365774887e-17), static_cast<double>(-6.2442285664031425e-16), static_cast<double>(-2539.60133934021), SC_(0.979404874070484909016444856299500644331897735) }},
        { { static_cast<double>(1.2860067365774887e-17), static_cast<double>(1), static_cast<double>(-2539.60133934021), SC_(0.999999999999999891757095137551552220860540801) }},
        { { static_cast<double>(-1.2860067365774887e-17), static_cast<double>(1), static_cast<double>(-2539.60133934021), SC_(1.00000000000000010824290486244845922375479178) }},
        { { static_cast<double>(1.2860067365774887e-17), static_cast<double>(0.5), static_cast<double>(-2539.60133934021), SC_(0.999999999999999873931788919689096760455570214) }},
        { { static_cast<double>(-1.2860067365774887e-17), static_cast<double>(0.5), static_cast<double>(-2539.60133934021), SC_(1.0000000000000001260682110803109183167444166) }},
        { { static_cast<double>(1.2860067365774887e-17), static_cast<double>(-0.5), static_cast<double>(-2539.60133934021), SC_(0.999999999999999899656990458526368219886894767) }},
        { { static_cast<double>(-1.2860067365774887e-17), static_cast<double>(-0.5), static_cast<double>(-2539.60133934021), SC_(1.00000000000000010034300954147364037131355735) }},
        { { static_cast<double>(1.9561377367172441e-13), static_cast<double>(-0.99999999999980438), static_cast<double>(0.53720525559037924), SC_(0.791950585963666119273677451162365759080483409) }},
        { { static_cast<double>(1.9561377367172441e-13), static_cast<double>(-0.99999999999980438), static_cast<double>(-0.53720525559037924), SC_(0.898314630992769591673208399706587643905527327) }},
        { { static_cast<double>(-1.9561377367172441e-13), static_cast<double>(-1.0000000000001956), static_cast<double>(0.53720525559037924), SC_(0.791950585964025761367113514279915403442035074) }},
        { { static_cast<double>(-1.9561377367172441e-13), static_cast<double>(-1.0000000000001956), static_cast<double>(-0.53720525559037924), SC_(0.898314630992646771749564140770704893561753597) }},
        { { static_cast<double>(5.1851756946064858e-12), static_cast<double>(-0.99999999999481481), static_cast<double>(-774.06985878944397), SC_(1.91306610467163858324476828831735612399803649e-06) }},
        { { static_cast<double>(-5.1851756946064858e-12), static_cast<double>(-1.0000000000051852), static_cast<double>(-774.06985878944397), SC_(1.91306610479516297551035931150910859922270467e-06) }},

        {{ static_cast<double>(4.782769898853794e-15), static_cast<double>(1.0000000000000049), static_cast<double>(43.289540141820908), SC_(715.678254892476818206948251991084031658534788) }},
        { { static_cast<double>(-4.782769898853794e-15), static_cast<double>(0.99999999999999523), static_cast<double>(43.289540141820908), SC_(-713.67825489247727251051792450091274703212426) }},
        { { static_cast<double>(4.782769898853794e-15), static_cast<double>(0.50000000000000477), static_cast<double>(43.289540141820908), SC_(8235.578376364917373771471380274179857713986) }},
        { { static_cast<double>(-4.782769898853794e-15), static_cast<double>(0.49999999999999523), static_cast<double>(43.289540141820908), SC_(-8233.57837636502669085205930058992320862281194) }},
        { { static_cast<double>(4.782769898853794e-15), static_cast<double>(-0.49999999999999523), static_cast<double>(43.289540141820908), SC_(-696269.800378137841948029488304613132151506346) }},
        { { static_cast<double>(-4.782769898853794e-15), static_cast<double>(-0.50000000000000477), static_cast<double>(43.289540141820908), SC_(696271.8003781336298001417038674968935893361) }},
        { { static_cast<double>(8.1104991963343309e-05), static_cast<double>(-0.99991889500803666), static_cast<double>(-289.12455415725708), SC_(7.89625448009377635153307897651433007437615965e-124) }},
        { { static_cast<double>(-8.1104991963343309e-05), static_cast<double>(-1.0000811049919633), static_cast<double>(-289.12455415725708), SC_(7.8949781467741574268884621364833028722017032e-124) }},

         {{ static_cast<double>(-1.98018241448205767), static_cast<double>(1.98450573845762079), static_cast<double>(54.4977916804564302), SC_(2972026581564772.790187123046255523239732028) }},

         { { static_cast<double>(-7.8238229420435346e-05), static_cast<double>(-0.50007823822942044), static_cast<double>(-1896.0561106204987), SC_(1.00058778866237037053151236215058095904086972) }},

         // Unexpected high error : 2.48268e+91 Found : -9.61305e+268 Expected : -1.74382e+193
         { { static_cast<double>(5.9981750131794866e-15), static_cast<double>(-230.70702263712883), static_cast<double>(240.42092034220695), SC_(-1.74381782591884817018404492963109914357365958e+193) }},
         // Unexpected high error : 1.79769313486231570814527423731704356798070568e+308 Found : -9.61305077326281580724540507004198316499661687e+268 Expected : 1.74381782591870724567837900957146707932623893e+193
         { { static_cast<double>(-5.9981750131794866e-15), static_cast<double>(-230.70702263712883), static_cast<double>(240.42092034220695), SC_(1.74381782591870734495565763481520223752372107e+193) }},
         //Unexpected exception : Error in function boost::math::hypergeometric_pFq<long double> : Cancellation is so severe that no bits in the reuslt are correct, last result was - 13497.312248525042
         { { static_cast<double>(-0.00023636552788275367), static_cast<double>(0.49976363447211725), static_cast<double>(-55.448519088327885), SC_(1.00141219419064760011631555641142295011268795) }},
         // Unexpected exception: Error in function boost::math::hypergeometric_pFq<long double>: Cancellation is so severe that no bits in the reuslt are correct, last result was -13497.312248525042
         {{ static_cast<double>(-0.00023636552788275367), static_cast<double>(-0.50023636552788275), static_cast<double>(-55.448519088327885), SC_(1.00093463146763986302362749764017215184711625) }},
         // Unexpected exception : Error in function boost::math::hypergeometric_pFq<long double> : Cancellation is so severe that no bits in the reuslt are correct, last result was - 1.3871133003351527e+47
         { { static_cast<double>(-1.6548533913638905e-10), static_cast<double>(0.49999999983451465), static_cast<double>(-169.20843148231506), SC_(1.00000000117356793527360151094991866549128017) }},
         // Unexpected exception: Error in function boost::math::hypergeometric_pFq<long double>: Cancellation is so severe that no bits in the reuslt are correct, last result was -1.3871133003351527e+47
         {{ static_cast<double>(-1.6548533913638905e-10), static_cast<double>(-0.50000000016548529), static_cast<double>(-169.20843148231506), SC_(1.00000000084161045914716192484600809610013447) }},
         // Unexpected high error : 17825.7893791562892147339880466461181640625 Found : -0.000253525216373273569459012577453904668800532818 Expected : -0.000253525216374277052779756536082800266740377992
         { { static_cast<double>(-2.0211181797563725e-14), static_cast<double>(-1.0000000000000202), static_cast<double>(-25.653068032115698), SC_(-0.000253525216374277055047768086884693917115210113) }},
         // Unexpected high error: 1.79769e+308 Found: -inf Expected: -2.63233e-197
         {{ static_cast<double>(235.44106131792068), static_cast<double>(-2.250966744069919e-13), static_cast<double>(-974.28781914710999), SC_(-2.63233018990922939037251029961929844581862228e-197) }},
         // Unexpected high error : 1.79769313486231570814527423731704356798070568e+308 Found : -inf Expected : -3.53316570137147325345279499243339692001224196e+226
         { { static_cast<double>(-235.44106131792068), static_cast<double>(-2.250966744069919e-13), static_cast<double>(974.28781914710999), SC_(-3.53316570137147343919975579872097464691424847e+226) }},
         // Unexpected high error : 2.48268e+91 Found : -9.61305e+268 Expected : -1.74382e+193
         { { static_cast<double>(5.9981750131794866e-15), static_cast<double>(-230.70702263712883), static_cast<double>(240.42092034220695), SC_(-1.74381782591884817018404492963109914357365958e+193) }},
         // Unexpected high error : 1.79769313486231570814527423731704356798070568e+308 Found : -9.61305077326281580724540507004198316499661687e+268 Expected : 1.74381782591870724567837900957146707932623893e+193
         { { static_cast<double>(-5.9981750131794866e-15), static_cast<double>(-230.70702263712883), static_cast<double>(240.42092034220695), SC_(1.74381782591870734495565763481520223752372107e+193) }},
         // Unexpected exception : Error in function boost::math::hypergeometric_pFq<long double> : Cancellation is so severe that no bits in the reuslt are correct, last result was 3.0871891698197084e+73
         { { static_cast<double>(-5.9981750131794866e-15), static_cast<double>(0.499999999999994), static_cast<double>(-240.42092034220695), SC_(1.00000000000004464930530925572237133417488137) }},
         // Unexpected exception : Error in function boost::math::hypergeometric_pFq<long double> : Cancellation is so severe that no bits in the reuslt are correct, last result was 3.0871891698197084e+73
         { { static_cast<double>(-5.9981750131794866e-15), static_cast<double>(-0.500000000000006), static_cast<double>(-240.42092034220695), SC_(1.00000000000003262784934420226963147689063665) }},
         // Unexpected high error : 18466.4373304979599197395145893096923828125 Found : 1.32865406167486480872551302123696359558380209e-08 Expected : 1.3286540616694168317751162703647255236560909e-08
         { { static_cast<double>(6.772927684190258e-10), static_cast<double>(-0.99999999932270722), static_cast<double>(-483.69576895236969), SC_(1.32865406166941679958876322759721528297325713e-08) }},
         // Unexpected high error: 1.79769e+308 Found: -nan(ind) Expected: 5.31173e-38
         {{ static_cast<double>(6763.4877452850342), static_cast<double>(3.6834977949762315e-08), static_cast<double>(-210.20976513624191), SC_(5.31173132667573457976877380237496445775181141e-38) }},
         // Unexpected high error : 1.79769313486231570814527423731704356798070568e+308 Found : -nan(ind) Expected : 1.04274264437409856500364465136386556989276338e+54
         { { static_cast<double>(-6763.4877452850342), static_cast<double>(3.6834977949762315e-08), static_cast<double>(210.20976513624191), SC_(1.04274264437409861991447530452939035771734596e+54) }},
         // Unexpected high error: 3.17219226436543247206316287281668161679098192e+185 Found: 1.00012411189051491970538746574092795078602734e+201 Expected: 14198882672502154063215954231296
         {{ static_cast<double>(76763.042617797852), static_cast<double>(-21.722407214343548), static_cast<double>(-0.60326536209322512), SC_(14198882672502153010712531896984.8126667697959) }},

         // Unexpected high error: 1.79769313486231570814527423731704356798070568e+308 Found: -2.39521645877904927856730119998375850409649219e+124 Expected: 2.3952164587795095929135248712964248422934629e+124
         {{ static_cast<double>(-1.8857404964801872e-09), static_cast<double>(-226.52341184020042), static_cast<double>(160.86221924424171), SC_(2.39521645877950946848639784331327651190093595e+124) }},
         // Unexpected high error : 73027.246763920571538619697093963623046875 Found : 0.000111810625893715248580992382976262433658121154 Expected : 0.000111810625895528292111404111697225971511215903
         { { static_cast<double>(-7.5220323642510769e-13), static_cast<double>(-1.0000000000007523), static_cast<double>(-17.948102783411741), SC_(0.00011181062589552829441403260197223627311023229) }},
         // Unexpected high error: 111726.15160028330865316092967987060546875 Found: 0.00856985181006919560786627698689699172973632813 Expected: 0.00856985180985659310282098743982714950107038021
         {{ static_cast<double>(5.6136137469239618e-15), static_cast<double>(-0.99999999999999434), static_cast<double>(-1989.8742001056671), SC_(0.00856985180985659334965068576732515544478559175) }},
         // Unexpected high error : 10431.000000023717802832834422588348388671875 Found : 0.99999999999772626324556767940521240234375 Expected : 1.00000000000004241051954068097984418272972107
         { { static_cast<double>(-5.6136137469239618e-15), static_cast<double>(-0.50000000000000566), static_cast<double>(-1989.8742001056671), SC_(1.00000000000004243096226509338784935080089269) }},
         // And more from error rate testing:
         {{ (T)std::ldexp((double)-17079780487168000, -44), (T)std::ldexp((double)9462273998848000, -46), (T)std::ldexp((double)9928190459904000, -48), SC_(7.7358754011357422722746277257633664799903803239195e-72) }},
         {{ (T)std::ldexp((double)-16238757384192000, -44), (T)std::ldexp((double)17248812490752000, -44), (T)std::ldexp((double)12549255331840000, -49), SC_(4.7354970214088286546733909450191631190700414608975e-10) }},
         {{ static_cast<double>(-6.8543290253728628), static_cast<double>(607.72073245607316), static_cast<double>(253.26409819535911), SC_(0.024418741483258497441042709681531519387974841769189) }},
         {{ (T)std::ldexp((double)-15569844699136000, -52), (T)std::ldexp((double)12855440629760000, -44), (T)std::ldexp((double)12563412279296000, -45), SC_(0.097879401070280078654536987721507669872679020399179) }},
         {{ (T)std::ldexp((double)-13521484578816000, -48), (T)std::ldexp((double)11813014388736000, -46), (T)std::ldexp((double)12736881098752000, -48), SC_(9.1262751214688536871555425535678062558805718157237e-08) }},
         {{ (T)std::ldexp((double)-13125670141952000, -44), (T)std::ldexp((double)16524914262016000, -44), (T)std::ldexp((double)12270166867968000, -49), SC_(2.0809215788388623809065210261671764534436583442155e-08) }},
         {{ (T)std::ldexp((double)-9012443406336000, -45), (T)std::ldexp((double)12293411340288000, -46), (T)std::ldexp((double)15162862993408000, -52), SC_(0.00634911418172408957356631082162378669273898042) }},
         {{ (T)std::ldexp((double)10907252916224000, -46), (T)std::ldexp((double)10872033234944000, -44), (T)std::ldexp((double)14845267734528000, -44), SC_(3.35597139167246486559762237420776458756928282e+152) }},
         {{ (T)std::ldexp((double)10206210322432000, -44), (T)std::ldexp((double)-16798514331648000, -45), (T)std::ldexp((double)21261284909056000, -48), SC_(3.8172723666678171743099642722909945977624468e+207) }},
         //{{ (T)std::ldexp((double)9125305942016000, -46), (T)std::ldexp((double)-15115828240384000, -45), (T)std::ldexp((double)9662868946944000, -47), SC_(4175579218962.24466854749118518544065513059142) }},
         //
         // These next few are the result of probing the boundary cases in hypergeometric_1F1_negative_b_recurrence_region
         //
         {{ (T)std::ldexp((double)10860755407856640, -40), (T)std::ldexp((double)-15992550230222440, -47), (T)std::ldexp((double)11953621172224000, -51), SC_(1.77767974631716859575450750736407296713916302e+278) }},
         {{ (T)std::ldexp((double)10788477424245760, -40), (T)std::ldexp((double)-17098099940288104, -45), (T)std::ldexp((double)9309879533568000, -50), SC_(3.30879597828065234949261835734767876076477669e+268) }},
         {{ (T)std::ldexp((double)10938221827471360, -40), (T)std::ldexp((double)-13207828614139084, -46), (T)std::ldexp((double)14276471291904000, -57), SC_(0.00563892736925233243283328398477659041011689599) }},
         { { (T)std::ldexp((double)10886339790484480, -40), (T)std::ldexp((double)-15267677514969908, -46), (T)std::ldexp((double)11568125313024000, -56), SC_(0.000743168361387021436166355590813648069510383979) } },
         { { (T)std::ldexp((double)10486036094119936, -40), (T)std::ldexp((double)-15535492710109184, -41), (T)std::ldexp((double)17014293405696000, -45), SC_(-2.61817515260939017621443182916266462279292638e+230) } },
         { { (T)std::ldexp((double)10485257266971648, -40), (T)std::ldexp((double)-17826711054409018, -35), (T)std::ldexp((double)17138334978048000, -44), SC_(1.70138735099219741672706572460585684251928784e-08) } },
         { { (T)std::ldexp((double)10485122560373760, -40), (T)std::ldexp((double)-11098279821997376, -39), (T)std::ldexp((double)16925852270592000, -45), SC_(9.77378642649349178995585980824930703376759021e-98) } },
         { { (T)std::ldexp((double)10485292967829248, -40), (T)std::ldexp((double)-14859721380002656, -35), (T)std::ldexp((double)13729956970496000, -44), SC_(3.41094899910311302761937103011397882987669395e-08) } },
         { { (T)std::ldexp((double)10485037389193216, -40), (T)std::ldexp((double)-10840488483391544, -35), (T)std::ldexp((double)17577061875712000, -45), SC_(2.8030884395368690164859926372380406504460219e-07) } },
           //
           // Negative a and b worst cases:
        { { (T)std::ldexp((double)-9281686323200000, -44), (T)std::ldexp((double)-14062138056704000, -44), (T)std::ldexp((double)13563284652032000, -44), SC_(2.8338102961174890442403751063892055396228341374378e+265) } },
        { { (T)std::ldexp((double)-17049048150016000, -44), (T)std::ldexp((double)-16971363917824000, -45), (T)std::ldexp((double)11759598960640000, -49), SC_(4636596575297708282.1539119952275597833292408543916) }},
        { { (T)std::ldexp((double)-14233964060672000, -45), (T)std::ldexp((double)-12648356216832000, -47), (T)std::ldexp((double)9597206757376000, -46), SC_(-1.2995296554447445191533190670521132012426135496934e+104) }},
        { { (T)std::ldexp((double)-16705334214656000, -45), (T)std::ldexp((double)-15447756718080000, -46), (T)std::ldexp((double)16395884134400000, -47), SC_(5.4068014134661635929301319845768046995946557071618e+113) }},
        { { (T)std::ldexp((double)-13991530405888000, -45), (T)std::ldexp((double)-10196587347968000, -46), (T)std::ldexp((double)13331347734528000, -46), SC_(1.2861275297661534781908508971693782447411136476694e+138) }},
        { { (T)std::ldexp((double)-15134950760448000, -45), (T)std::ldexp((double)-14587193786368000, -48), (T)std::ldexp((double)17022855921664000, -46), SC_(-8.8168904087758007346518546320759101059296394741359e+115) }},
        { { (T)std::ldexp((double)-14854672039936000, -45), (T)std::ldexp((double)-10436558200832000, -45), (T)std::ldexp((double)11370918969344000, -47), SC_(8.8553524727253411744552846056891456360191660433059e+54) } },
        { { (T)std::ldexp((double)-16711069286400000, -46), (T)std::ldexp((double)-14809815056384000, -46), (T)std::ldexp((double)10469312954368000, -47), SC_(50343352353398198766339890377687038177.388095267191) } },
        { { (T)std::ldexp((double)-15026786402304000, -45), (T)std::ldexp((double)-16687356968960000, -46), (T)std::ldexp((double)14895621603328000, -47), SC_(2.8532956042460265690059969666558072704044483623242e+95) } },
        { { (T)std::ldexp((double)-15519073435648000, -45), (T)std::ldexp((double)-14162009718784000, -45), (T)std::ldexp((double)9997818855424000, -48), SC_(95767987018108517.763999577428194082282035178055037) } },
        { { (T)std::ldexp((double)-15317481275392000, -46), (T)std::ldexp((double)-16531865931776000, -44), (T)std::ldexp((double)17586268880896000, -45), SC_(1.4701248047083724279783071194324315286789986738882e+104) }},
        { { (T)std::ldexp((double)-11335669673984000, -44), (T)std::ldexp((double)-13146047094784000, -44), (T)std::ldexp((double)13671437864960000, -44), SC_(-2.1887607284987089539904337941443591993019781369247e+288) }},
        { { (T)std::ldexp((double)-16877985234944000, -46), (T)std::ldexp((double)-14384006086656000, -46), (T)std::ldexp((double)9074349342720000, -47), SC_(15376193613462463541358751744530105.412429016705833) }},
        { { (T)std::ldexp((double)-9751199809536000, -45), (T)std::ldexp((double)-17654191685632000, -47), (T)std::ldexp((double)10587451850752000, -47), SC_(-1.9601415510439595625538337964298353914980331018955e+68) }},
        { { (T)std::ldexp((double)-15233620754432000, -45), (T)std::ldexp((double)-12708283072512000, -46), (T)std::ldexp((double)10255461007360000, -46), SC_(-5.4344106361679075861859567858016187271235441673635e+125) }},
        { { (T)std::ldexp((double)-11241354149888000, -45), (T)std::ldexp((double)-9580579905536000, -45), (T)std::ldexp((double)12224976846848000, -47), SC_(12046856548470067405870726490464935201150430438.035) }},
   } };
   static const boost::array<boost::array<T, 4>, 2> hypergeometric_1F1_big_bugs = { {
#if DBL_MAX_EXP == LDBL_MAX_EXP
         {{ static_cast<double>(7.8238229420435346e-05), static_cast<double>(-5485.3222503662109), static_cast<double>(1896.0561106204987), BOOST_MATH_HUGE_CONSTANT(T, 1000, 4.33129800901478785957996719992774682013355926e+668) }},
         {{ static_cast<double>(-7.8238229420435346e-05), static_cast<double>(-5485.3222503662109), static_cast<double>(1896.0561106204987), BOOST_MATH_HUGE_CONSTANT(T, 1000, -4.3248750673398590673783317624407455467680038e+668) }},
#else
   { { static_cast<double>(7.8238229420435346e-05), static_cast<double>(-5485.3222503662109), static_cast<double>(1896.0561106204987), SC_(4.33129800901478785957996719992774682013355926e+668) } },
   { { static_cast<double>(-7.8238229420435346e-05), static_cast<double>(-5485.3222503662109), static_cast<double>(1896.0561106204987), SC_(-4.3248750673398590673783317624407455467680038e+668) } },
#endif
   } };
   do_test_1F1<T>(hypergeometric_1F1_bugs, type_name, "Bug cases");
   if(std::numeric_limits<T>::max_exponent10 > 800)
      do_test_1F1<T>(hypergeometric_1F1_big_bugs, type_name, "Bug cases - oversized");
   else
   {
      for (unsigned i = 0; i < hypergeometric_1F1_big_bugs.size(); ++i)
      {
         T val = boost::math::hypergeometric_1F1(hypergeometric_1F1_big_bugs[i][0], hypergeometric_1F1_big_bugs[i][1], hypergeometric_1F1_big_bugs[i][2]);
         BOOST_CHECK((boost::math::isinf)(val));
      }
   }
}

template <class T>
void test_spots(T z, const char* type_name)
{
   test_spots1(z, type_name);
   test_spots2(z, type_name);
   //
   // Test ranges that are limited to double precision, these contain test cases
   // which require full double precision for the inputs, so we don't test
   // at float precision as well as higher precisions:
   //
   if (std::numeric_limits<T>::digits10 == std::numeric_limits<double>::digits10)
      test_spots3(z, type_name);
#ifdef TEST_UNSOLVED
   test_spots4(z, type_name);
#endif
   test_spots5(z, type_name);
   //
   // Try as we might, we can't get better than quad precision on some of these:
   //
   if(std::numeric_limits<T>::digits >= std::numeric_limits<double>::digits && std::numeric_limits<T>::digits <= 128)
      test_spots6(z, type_name);
}


// Tests the Mellin transform formula given here: https://dlmf.nist.gov/13.10, Equation 13.10.10
template <class Real>
void test_hypergeometric_mellin_transform()
{
   using boost::math::hypergeometric_1F1;
   using boost::math::quadrature::exp_sinh;
   using boost::math::tgamma;
   using std::pow;

   // Constraint: 0 < lambda < a.
   Real lambda = 0.5;
   Real a = 1;
   Real b = 3;
   auto f = [&](Real t)->Real { return pow(t, lambda - 1)*hypergeometric_1F1(a, b, -t); };

   auto integrator = exp_sinh<double>();
   Real computed = integrator.integrate(f, boost::math::tools::epsilon<Real>());
   Real expected = tgamma(b)*tgamma(lambda)*tgamma(a - lambda) / (tgamma(a)*tgamma(b - lambda));

   Real tol = boost::math::tools::epsilon<Real>() * 5;
   BOOST_CHECK_CLOSE_FRACTION(computed, expected, tol);
}


// Tests the Laplace transform formula given here: https://dlmf.nist.gov/13.10, Equation 13.10.4
template <class Real>
void test_hypergeometric_laplace_transform()
{
   using boost::math::hypergeometric_1F1;
   using boost::math::quadrature::exp_sinh;
   using boost::math::tgamma;
   using std::pow;
   using std::exp;

   // Set a = 1 blows up for some reason . . .
   Real a = -1;
   Real b = 3;
   Real z = 1.5;
   auto f = [&](Real t)->Real { return exp(-z * t)*pow(t, b - 1)*hypergeometric_1F1(a, b, t); };

   auto integrator = exp_sinh<double>();
   Real computed = integrator.integrate(f, boost::math::tools::epsilon<Real>());
   Real expected = tgamma(b) / (pow(z, b)*pow(1 - 1 / z, a));

   Real tol = boost::math::tools::epsilon<Real>() * 200;
   BOOST_CHECK_CLOSE(computed, expected, tol);
}
