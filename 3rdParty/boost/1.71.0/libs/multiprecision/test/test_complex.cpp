
///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2016.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

#include <boost/lexical_cast.hpp>
#ifdef TEST_MPC
#include <boost/multiprecision/mpc.hpp>
#endif
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/complex_adaptor.hpp>
#ifdef BOOST_HAS_FLOAT128
#include <boost/multiprecision/complex128.hpp>
#endif

#include "test.hpp"

namespace local
{
   template<typename complex_type>
   void test()
   {

      typedef typename complex_type::value_type float_type;

      const std::string str_tol("0."
         + std::string(std::size_t(std::numeric_limits<float_type>::digits10 - 2), char('0'))
         + std::string(std::size_t(2U), char('9')));

      const float_type tol = boost::lexical_cast<float_type>(str_tol.c_str());

      std::cout << "Testing with tolerance: " << tol << std::endl;

      const complex_type z1(float_type(12U) / 10U, float_type(34U) / 10U);
      const complex_type z2(float_type(56U) / 10U, float_type(78U) / 10U);
      const complex_type  i(float_type(0U), float_type(1U));

      // See also, for example, numerical evaluation at Wolfram's Alpha.

      const complex_type result_01 = z1 / z2;                        // N[((12/10) + ((34 I)/10)) / ((56/10) + ((78 I)/10)), 100]
      const complex_type result_02 = complex_type(z1) /= z2;         // Same as above.
      const complex_type result_03 = z1 / (i * z2);                  // N[((12/10) + ((34 I)/10)) / ((-78/10) + ((56 I)/10)), 100]
      const complex_type result_04 = complex_type(z1) /= (i * z2);   // Same as above.
      const complex_type result_05 = z1.real() / z2;                 // N[((12/10) / ((56/10) + ((78 I)/10)), 100]
      const complex_type result_06 = z1.real() / (i * z2);           // N[((12/10) / ((-78/10) + ((56 I)/10)), 100]
      const complex_type result_07 = sqrt(z1);                     // N[Sqrt[(12/10) + ((34 I)/10)], 100]
      const complex_type result_08 = sqrt(-z1);                     // N[Sqrt[(-12/10) - ((34 I)/10)], 100]
      const complex_type result_09 = sin(z1);                     // N[Sin[(12/10) + ((34 I)/10)], 100]
      const complex_type result_10 = sinh(z1);                     // N[Sinh[(12/10) + ((34 I)/10)], 100]
      const complex_type result_11 = cosh(z1);                     // N[Cosh[(12/10) + ((34 I)/10)], 100]
      const complex_type result_12 = log(z1);                     // N[Log[(12/10) + ((34 I)/10)], 100]
      const complex_type result_13 = asin(z1);                     // N[ArcSin[(12/10) + ((34 I)/10)], 100]
      const complex_type result_14 = acos(z1);                     // N[ArcCos[(12/10) + ((34 I)/10)], 100]
      const complex_type result_15 = atan(z1);                     // N[ArcTan[(12/10) + ((34 I)/10)], 100]
      const complex_type result_16 = acosh(z1);                     // N[ArcCosh[(12/10) + ((34 I)/10)], 100]
      const complex_type result_17 = atanh(z1);                     // N[ArcTanh[(12/10) + ((34 I)/10)], 100]
      const complex_type result_18 = exp(z1);                     // N[Exp[(12/10) + ((34 I)/10)], 100]
      const complex_type result_19 = pow(z1, 5);                  // N[((12/10) + ((34 I)/10)) ^ 5, 100]
      const complex_type result_20 = pow(z1, z2);                 // N[((12/10) + ((34 I)/10)) ^ ((56/10) + ((78 I)/10)), 100]
      const complex_type result_21 = pow(z1.real(), z2);          // N[(12/10)^((56/10) + ((78 I)/10)), 100]
      const complex_type result_22 = cos(z1);                     
      const complex_type result_23 = asinh(z1);                     
      const complex_type result_24 = tanh(z1);                     
      const complex_type result_25 = log10(z1);                     
      const complex_type result_26 = tan(z1);                     

      const complex_type control_01(boost::lexical_cast<float_type>("+0.3605206073752711496746203904555314533622559652928416485900216919739696312364425162689804772234273319"), boost::lexical_cast<float_type>("+0.1049891540130151843817787418655097613882863340563991323210412147505422993492407809110629067245119306"));
      const complex_type control_02(boost::lexical_cast<float_type>("+0.3605206073752711496746203904555314533622559652928416485900216919739696312364425162689804772234273319"), boost::lexical_cast<float_type>("+0.1049891540130151843817787418655097613882863340563991323210412147505422993492407809110629067245119306"));
      const complex_type control_03(boost::lexical_cast<float_type>("+0.1049891540130151843817787418655097613882863340563991323210412147505422993492407809110629067245119306"), boost::lexical_cast<float_type>("-0.3605206073752711496746203904555314533622559652928416485900216919739696312364425162689804772234273319"));
      const complex_type control_04(boost::lexical_cast<float_type>("+0.1049891540130151843817787418655097613882863340563991323210412147505422993492407809110629067245119306"), boost::lexical_cast<float_type>("-0.3605206073752711496746203904555314533622559652928416485900216919739696312364425162689804772234273319"));
      const complex_type control_05(boost::lexical_cast<float_type>("+0.07288503253796095444685466377440347071583514099783080260303687635574837310195227765726681127982646421"), boost::lexical_cast<float_type>("-0.10151843817787418655097613882863340563991323210412147505422993492407809110629067245119305856832971800"));
      const complex_type control_06(boost::lexical_cast<float_type>("-0.10151843817787418655097613882863340563991323210412147505422993492407809110629067245119305856832971800"), boost::lexical_cast<float_type>("-0.07288503253796095444685466377440347071583514099783080260303687635574837310195227765726681127982646421"));
      const complex_type control_07(boost::lexical_cast<float_type>("+1.5500889128472581416161256546038815669761567486848749301860666965618993040312647033986371788677357208"), boost::lexical_cast<float_type>("+1.096711282759503047577277387056220643003106823143745046422869808875853261131777962620301480493467395"));
      const complex_type control_08(boost::lexical_cast<float_type>("+1.096711282759503047577277387056220643003106823143745046422869808875853261131777962620301480493467395"), boost::lexical_cast<float_type>("-1.550088912847258141616125654603881566976156748684874930186066696561899304031264703398637178867735721"));
      const complex_type control_09(boost::lexical_cast<float_type>("+13.97940880601799793712580492576613541257396172944193599059708688128463118206190268215536541838594224"), boost::lexical_cast<float_type>("+5.42281547246340124509840716106599160358961329374827042575715571177243361237429170135167564889390308"));
      const complex_type control_10(boost::lexical_cast<float_type>("-1.459344510181031985739679928789446132188487461323488604725673812272622166868694452733557505015403343"), boost::lexical_cast<float_type>("-0.462696919065088203665190427736980818788403809123239459086853242811288735966522197819049006036217659"));
      const complex_type control_11(boost::lexical_cast<float_type>("-1.750538529873144139045226521462954860931070703406867443705575327698120127693949444003491179539803540"), boost::lexical_cast<float_type>("-0.385729418228941114585783287542904778761684113049496885765699906882071623161614865310950661528173196"));
      const complex_type control_12(boost::lexical_cast<float_type>("+1.282474678730768368026743720782659302402633972380103558209522755331732333662205089699787331720244744"), boost::lexical_cast<float_type>("+1.231503712340851938048420309342408065643217837171236736591653326549432606404929552637127722999523972"));
      const complex_type control_13(boost::lexical_cast<float_type>("+0.327743052014525194927829972510958755346463574500092232271394201982853487105798907461836716106793827"), boost::lexical_cast<float_type>("+1.990465064891068704855135027843677587369707826516050430927052768488360486375325411568355926052994795"));
      const complex_type control_14(boost::lexical_cast<float_type>("+1.243053274780371424303491719128792686752121125187460678216078094171054716037305591852180696564264707"), boost::lexical_cast<float_type>("-1.990465064891068704855135027843677587369707826516050430927052768488360486375325411568355926052994795"));
      const complex_type control_15(boost::lexical_cast<float_type>("+1.472098546869956240046296809042356295374792147793626859728627826033391218153982606447668498652414512"), boost::lexical_cast<float_type>("+0.265217990171315665670057272294610940896446740868740866767584213220467425714221740314551620202361000"));
      const complex_type control_16(boost::lexical_cast<float_type>("+1.990465064891068704855135027843677587369707826516050430927052768488360486375325411568355926052994795"), boost::lexical_cast<float_type>("+1.243053274780371424303491719128792686752121125187460678216078094171054716037305591852180696564264707"));
      const complex_type control_17(boost::lexical_cast<float_type>("+0.0865690591794584441708728351688739957204743888691393886743981396542994588169512931672375066385325971"), boost::lexical_cast<float_type>("+1.3130218230654070842957152910299990808349983340931830855923498117237087784015805687823303378344863815"));
      const complex_type control_18(boost::lexical_cast<float_type>("-3.209883040054176124784906450252400993119558164730356048431249139970742294562643896737048684555206883"), boost::lexical_cast<float_type>("-0.848426337294029318250973715279885597550087922172736344852553149693360359128137063129999667564390855"));
      const complex_type control_19(boost::lexical_cast<float_type>("+604.5331200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"), boost::lexical_cast<float_type>("-76.3721600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"));
      const complex_type control_20(boost::lexical_cast<float_type>("-0.03277613870122620601990858385164868868755535963372013573012556184586155243067581560513902047571175876"), boost::lexical_cast<float_type>("-0.08229096285844296094766104540456274850393339107196281307901532754610012233461478959682645571793968423"));
      const complex_type control_21(boost::lexical_cast<float_type>("+0.411234943477464115466545795217592784968613499972731227382657362718492707513495252941835813107553442"), boost::lexical_cast<float_type>("+2.745341999926603737618437066482640101524732307796305942046035072295581269096378050886721641340275877"));
      const complex_type control_22(boost::lexical_cast<float_type>("+5.434908535625768828356755041961059164496984604872689362797531800548498933607521437636349622508613554091586385714"), boost::lexical_cast<float_type>("-13.94830361398843812562310928749334812612948167559995042721295554487075295893311479161719058895913730856339351903"));
      const complex_type control_23(boost::lexical_cast<float_type>("1.960545624274756532757863147926614306606344023611895252748744677291286163242757958621205801565261867742512464966"), boost::lexical_cast<float_type>("1.218868916639890129907167289780557894460959308403278313426586722860313100564201336738382323265397208095855244598"));
      const complex_type control_24(boost::lexical_cast<float_type>("0.850596957549373767077286649475619401136709177565015571227494818854489310955671601843121766190907188858336389856"), boost::lexical_cast<float_type>("0.0768887100657045933256083080177585250084258780189064853743639639730289473609169028321092839605973770873615254969"));
      const complex_type control_25(boost::lexical_cast<float_type>("0.556971676153418384603252578971164215414864594193534135900595487498776545815097120403823727129449829836488977743"), boost::lexical_cast<float_type>("0.534835266713001566463607491752731752251883431441322590506193641481222966948925488019132991641807563958917938106"));
      const complex_type control_26(boost::lexical_cast<float_type>("0.001507101875805783093387404217890750536099188330196632424994845319829605048223792684300657269522163036773069001022"), boost::lexical_cast<float_type>("1.001642796989141044331404504473463720289288872859072985897399123485040632782581390249339681901758734269277005380"));
      
      BOOST_CHECK_CLOSE_FRACTION(result_01.real(), control_01.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_01.imag(), control_01.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_02.real(), control_02.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_02.imag(), control_02.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_03.real(), control_03.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_03.imag(), control_03.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_04.real(), control_04.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_04.imag(), control_04.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_05.real(), control_05.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_05.imag(), control_05.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_06.real(), control_06.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_06.imag(), control_06.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_07.real(), control_07.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_07.imag(), control_07.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_08.real(), control_08.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_08.imag(), control_08.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_09.real(), control_09.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_09.imag(), control_09.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_10.real(), control_10.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_10.imag(), control_10.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_11.real(), control_11.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_11.imag(), control_11.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_12.real(), control_12.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_12.imag(), control_12.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_13.real(), control_13.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_13.imag(), control_13.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_14.real(), control_14.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_14.imag(), control_14.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_15.real(), control_15.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_15.imag(), control_15.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_16.real(), control_16.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_16.imag(), control_16.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_17.real(), control_17.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_17.imag(), control_17.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_18.real(), control_18.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_18.imag(), control_18.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_19.real(), control_19.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_19.imag(), control_19.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_20.real(), control_20.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_20.imag(), control_20.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_21.real(), control_21.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_21.imag(), control_21.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_22.real(), control_22.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_22.imag(), control_22.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_23.real(), control_23.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_23.imag(), control_23.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_24.real(), control_24.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_24.imag(), control_24.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_25.real(), control_25.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_25.imag(), control_25.imag(), tol);
      BOOST_CHECK_CLOSE_FRACTION(result_26.real(), control_26.real(), tol); BOOST_CHECK_CLOSE_FRACTION(result_26.imag(), control_26.imag(), tol);

      BOOST_CHECK_CLOSE_FRACTION(abs(z1), boost::lexical_cast<float_type>("3.60555127546398929311922126747049594625129657384524621271045305622716694829301044520461908201849071767351418202406"), tol)
   }
}

int main()
{
   //local::test<std::complex<double> >();
#ifdef TEST_MPC
   local::test<boost::multiprecision::mpc_complex_50>();
   local::test<boost::multiprecision::mpc_complex_100>();
#endif
   local::test<boost::multiprecision::number<boost::multiprecision::complex_adaptor<boost::multiprecision::cpp_bin_float<50> >, boost::multiprecision::et_on> >();
   local::test<boost::multiprecision::number<boost::multiprecision::complex_adaptor<boost::multiprecision::cpp_bin_float<50> >, boost::multiprecision::et_off> >();
#ifdef BOOST_HAS_FLOAT128
   local::test<boost::multiprecision::complex128>();
#endif
   return boost::report_errors();
}
