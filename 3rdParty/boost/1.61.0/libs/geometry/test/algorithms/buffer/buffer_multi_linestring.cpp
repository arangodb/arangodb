// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <test_buffer.hpp>

static std::string const simplex = "MULTILINESTRING((0 0,4 5),(5 4,10 0))";
static std::string const two_bends = "MULTILINESTRING((0 0,4 5,7 4,10 6),(1 5,5 9,8 6))";
static std::string const turn_inside = "MULTILINESTRING((0 0,4 5,7 4,10 6),(1 5,5 9,8 6),(0 4,-2 6))";

static std::string const bend_near_start1 = "MULTILINESTRING((10 0,11 0,15 2),(9 0,8 0,4 2))";
static std::string const bend_near_start2 = "MULTILINESTRING((10 0,11 0,12 1.5,15 3),(9 0,8 0,7 1.5,4 3))";

static std::string const degenerate0 = "MULTILINESTRING()";
static std::string const degenerate1 = "MULTILINESTRING((5 5))";
static std::string const degenerate2 = "MULTILINESTRING((5 5),(9 9))";
static std::string const degenerate3 = "MULTILINESTRING((5 5),(9 9),(4 10))";
static std::string const degenerate4 = "MULTILINESTRING((5 5,5 5),(9 9,9 9,10 10,9 9,9 9,9 9),(4 10,4 10,3 11,4 12,3 11,4 10,4 10))";

static std::string const crossing = "MULTILINESTRING((0 0,10 10),(10 0,0 10))";

static std::string const mikado1 = "MULTILINESTRING((-2 0,-17 -11,3.7142857142857144125969171000179 -2.4285714285714283811046243499732),(11.406143344709896325639419956133 0.75426621160409546007485914742574,12 1,11.403846153846153299582510953769 0.75),(4.25 -2.25,-19 -12,-1 0))";
static std::string const mikado2 = "MULTILINESTRING((-6.1176470588235289937983907293528 -12.696078431372548322997317882255,-6.116279069767442067018237139564 -12.697674418604652402109422837384),(-1.8000000000000007105427357601002 -1.6000000000000000888178419700125,-9.7619047619047627506461140001193 -1.238095238095238137532305700006),(-10.537366548042705005627794889733 -1.2028469750889678735461529868189,-10.567164179104477028658948256634 -1.201492537313432862333684170153),(1.9041095890410959512450972397346 3.4931506849315070439843111671507,-7 1,-7 -4),(-5.540540540540540348501963308081 -5.459459459459459651498036691919,-1.521739130434782261147574899951 -1.2173913043478261641894278000109),(1.2040816326530612290213184678578 2.530612244897959328682190971449,-6.288135593220339103481819620356 -4.711864406779660896518180379644),(-8.4018691588785046064913331065327 -6.755140186915888023122533923015,-8.4131455399061039202024403493851 -6.7660406885758996864410619309638),(-2 5,-12 12,-11.088000000000000966338120633736 0.14400000000000012789769243681803),(-10.720812182741116913575751823373 -0.80456852791878175068518430634867,-10.696969696969697238841945363674 -0.78787878787878784514475682954071),(-10.411764705882353254651206952985 -0.58823529411764707841570043456159,0.11913357400722013323957071406767 6.7833935018050537379963316197973),(-1.5283018867924527128820955113042 -1.2264150943396234794136034906842,1.1922525107604009519945975625888 6.1190817790530829256567812990397),(0.33490011750881265584212087560445 6.6498237367802577324482626863755,-11.548480463096961301516785169952 1.3335745296671497328588884556666))";
static std::string const mikado3 = "MULTILINESTRING((1 18,4.0559006211180124168436122999992 7.8136645962732922399140989000443),(6.7243816254416959310447055031545 -1.0812720848056533995418249105569,7 -2,7 -8,14 3.6666666666666669627261399000417),(15.297872340425531234586742357351 5.8297872340425538340014099958353,16 7,15.214285714285713524418497399893 5.8445378151260509724806979647838),(13.685863874345550073030608473346 3.5968586387434555717845796607435,-1 -18,-3.7900797165633304253162805252941 -11.117803365810452476125647081062),(-11.540540540540540348501963308081 8,-16 19,8 14),(1 -10,6.5999999999999996447286321199499 -1.200000000000000177635683940025),(11.5 6.5,15 12),(19 10,11.564231738035264385189293534495 6.4886649874055422060337150469422),(-13.438785504407443127661281323526 -5.3183153770812925387190261972137,-17 -7,-12.970074812967581578959652688354 -7.7556109725685784539450651209336),(-2.3532338308457703135445626685396 -9.7462686567164187323442092747428,-1 -10,12.285714285714286475581502600107 3.2857142857142864755815026001073),(14.90000000000000035527136788005 5.9000000000000003552713678800501,15 6,14.893004115226338157640384451952 5.9012345679012341292946075554937),(11.987804878048780921062643756159 3.2195121951219514144781896902714,-11 -18),(-12.210826210826210669324609625619 -11.703703703703702387883822666481,-15 -15,-11.463576158940396609864365018439 -15.589403973509934786534358863719),(-8.9189189189189193029960733838379 -16.013513513513512265262761502527,-3 -17,-7.0297239915074314353660156484693 -14.210191082802548834251865628175),(-12.450511945392491952588898129761 -10.457337883959045399251408525743,-16 -8,-12.923076923076923350208744523115 -8),(-0.52380952380952372493538859998807 -8,18 -8),(2 -19,-2.2961165048543685784920853620861 -9.6917475728155331182733789319173),(6.0463576158940393057150686217938 -1.7284768211920527036795647291001,7 -3,6.4482758620689653028534848999698 -1.3448275862068967967388744000345),(-1.3333333333333339254522798000835 8,4 16,2.9090909090909091716525836091023 8),(0.64705882352941168633719826175366 -6.8823529411764710062016092706472,-3 -16))";
static std::string const mikado4 = "MULTILINESTRING((-15 2,-15 -17,-6 11,-1.9358288770053475591481628725887 10.572192513368984023713892383967),(2.1545064377682408007785852532834 10.14163090128755406738036981551,6.87603305785123986026974307606 9.6446280991735537924114396446384),(8.4810810810810810522752944962122 9.475675675675674369813350494951,13 9),(-15 0,-8 9,-2.9850746268656713766631582984701 4.4865671641791049495395782287233),(-1.8235294117647056211239942058455 3.4411764705882355031008046353236,-1.1428571428571423496123315999284 2.8285714285714291804652020800859),(1.2307692307692308375521861307789 0.69230769230769229061195346730528,1.2857142857142858094476878250134 0.64285714285714290472384391250671,2 0,1.9459459459459460539676456392044 0.51351351351351348650808859019889),(1.908127208480565384363103476062 0.87279151943462895957281943992712,1.9078014184397162900097555393586 0.87588652482269502286271745106205),(1.4685990338164249813246442499803 5.0483091787439615671928550000302,0.63551401869158885560295857430901 12.962616822429906093816498469096,0 19,2.9565217391304345895264304999728 8.6521739130434784925682834000327),(0 19,3.4942528735632185643567027000245 6.770114942528735468840750399977),(4.75 2.375,5.2427184466019420838733822165523 0.65048543689320226235395239200443),(5.5384615384615383248956277384423 -0.38461538461538458122390693461057,5.7358490566037731994697423942853 -1.0754716981132084185901476303115),(5.9777777777777778567269706400111 -1.9222222222222207221875578397885,6.867052023121386739035187929403 -5.0346820809248553629799971531611,10 -16,-14 -19,-12 -12),(0 10,1.9476439790575916788384347455576 5.4554973821989527493769855936989),(-4 1,-4.2790697674418600726653494348284 0.16279069767441856075862460784265))";

static std::string const mysql_2015_04_10a = "MULTILINESTRING((-58 19, 61 88),(1.922421e+307 1.520384e+308, 15 42, 89 -93,-89 -22),(-63 -5, -262141 -536870908, -3 87, 77 -69))";
static std::string const mysql_2015_04_10b = "MULTILINESTRING((-58 19, 61 88),                                                     (-63 -5, -262141 -536870908, -3 87, 77 -69))";

static std::string const mysql_2015_09_08a = "MULTILINESTRING((7 -4, -3 -5), (72057594037927936 15, 72057594037927940 70368744177660, 32771 36028797018963964, 8589934589 2305843009213693953, 7 2, 9.300367e+307 9.649737e+307, -4092 -274877906946, 5 10, -3 4))";
static std::string const mysql_2015_09_08b = "MULTILINESTRING((-9 -10, 0 -1, 5 -10, -6 7, -7 7, 5.041061e+307 9.926906e+307, 6.870356e+307 1.064454e+307, 35184372088830 288230376151711743, 183673728842483250000000000000000000000.000000 244323751784861950000000000000000000000.000000), (-23530 -7131, -6 1, 1 1, 2 -6, 32766 -4194302, -4 -6), (134217725 0, 50336782742294697000000000000000000000.000000 36696596077212901000000000000000000000.000000, 7434 16486, 3.025467e+307 8.926790e+307), (2147483646 67108868, 71328904281592545000000000000000000000.000000 225041650340452780000000000000000000000.000000, -7 4, 1.667154e+307 3.990414e+307))"; 

template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::linestring<P> linestring;
    typedef bg::model::multi_linestring<linestring> multi_linestring_type;
    typedef bg::model::polygon<P, Clockwise> polygon;

    bg::strategy::buffer::join_miter join_miter;
    bg::strategy::buffer::join_round join_round(100);
    bg::strategy::buffer::join_round_by_divide join_round_by_divide(4);
    bg::strategy::buffer::end_flat end_flat;
    bg::strategy::buffer::end_round end_round(100);

    bg::strategy::buffer::end_round end_round32(32);
    bg::strategy::buffer::join_round join_round32(32);

    // Round joins / round ends
    test_one<multi_linestring_type, polygon>("simplex", simplex, join_round, end_round, 49.0217, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("two_bends", two_bends, join_round, end_round, 74.73, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("turn_inside", turn_inside, join_round, end_round, 86.3313, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("two_bends_asym", two_bends, join_round, end_round, 58.3395, 1.5, 0.75);

    // Round joins / flat ends:
    test_one<multi_linestring_type, polygon>("simplex", simplex, join_round, end_flat, 38.2623, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("two_bends", two_bends, join_round, end_flat, 64.6217, 1.5, 1.5);

    test_one<multi_linestring_type, polygon>("bend_near_start1", bend_near_start1, join_round, end_flat, 202.5910, 9.0, 9.0);
    test_one<multi_linestring_type, polygon>("bend_near_start2", bend_near_start2, join_round, end_flat, 231.4988, 9.0, 9.0);

    // TODO this should be fixed test_one<multi_linestring_type, polygon>("turn_inside", turn_inside, join_round, end_flat, 99, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("two_bends_asym", two_bends, join_round, end_flat, 52.3793, 1.5, 0.75);

    // This one is far from done:
    // test_one<multi_linestring_type, polygon>("turn_inside_asym_neg", turn_inside, join_round, end_flat, 99, +1.5, -1.0);

    // Miter / divide joins, various ends
    test_one<multi_linestring_type, polygon>("two_bends", two_bends, join_round_by_divide, end_flat, 64.6217, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("two_bends", two_bends, join_miter, end_flat, 65.1834, 1.5, 1.5);
    test_one<multi_linestring_type, polygon>("two_bends", two_bends, join_miter, end_round, 75.2917, 1.5, 1.5);

    test_one<multi_linestring_type, polygon>("degenerate0", degenerate0, join_round, end_round, 0.0, 3.0, 3.0);
    test_one<multi_linestring_type, polygon>("degenerate1", degenerate1, join_round, end_round, 28.2503, 3.0, 3.0);
    test_one<multi_linestring_type, polygon>("degenerate2", degenerate2, join_round, end_round, 56.0457, 3.0, 3.0);
    test_one<multi_linestring_type, polygon>("degenerate3", degenerate3, join_round, end_round, 80.4531, 3.0, 3.0);
    test_one<multi_linestring_type, polygon>("degenerate4", degenerate4, join_round, end_round, 104.3142, 3.0, 3.0);

    test_one<multi_linestring_type, polygon>("crossing", crossing, join_round32, end_flat, 2628.4272, 50.0);
    test_one<multi_linestring_type, polygon>("crossing", crossing, join_round32, end_round32,  9893.764, 50.0);

    // Cases formly causing a segmentation fault because a generated side was skipped
    // (The expected area for large distances is about R*R*PI where R is distance)
    // Note that for large distances the flat ends (not tested here) still give weird effects
    {
        // The results can differ between compilers and platforms
#if defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
        double mikado_tolerance = 40.0;
#else
        double mikado_tolerance = 30.0;
#endif

        test_one<multi_linestring_type, polygon>("mikado1_large", mikado1, join_round32, end_round32, 5455052125, 41751.0, same_distance, true, mikado_tolerance);
        test_one<multi_linestring_type, polygon>("mikado1_small", mikado1, join_round32, end_round32, 1057.37, 10.0);
        test_one<multi_linestring_type, polygon>("mikado1_small", mikado1, join_round32, end_flat, 874.590, 10.0);

        test_one<multi_linestring_type, polygon>("mikado2_large", mikado2, join_round32, end_round32, 19878812253, 79610.0, same_distance, true, 10 * mikado_tolerance);
        test_one<multi_linestring_type, polygon>("mikado2_small", mikado2, join_round32, end_round32, 1082.470, 10.0);
        test_one<multi_linestring_type, polygon>("mikado2_small", mikado2, join_round32, end_flat, 711.678, 10.0);

        // BSD         29151950588
        // msvc        29151950611
        // clang/linux 29151950612
        // mingw       29151950711
        test_one<multi_linestring_type, polygon>("mikado3_large", mikado3, join_round32, end_round32, 29151950650, 96375.0, same_distance, true, 10 * mikado_tolerance);
        test_one<multi_linestring_type, polygon>("mikado3_small", mikado3, join_round32, end_round32, 2533.285, 10.0);
        test_one<multi_linestring_type, polygon>("mikado3_small", mikado3, join_round32, end_flat, 2136.236, 10.0);

        test_one<multi_linestring_type, polygon>("mikado4_large", mikado4, join_round32, end_round32, 11212832169, 59772.0, same_distance, true, mikado_tolerance);
        test_one<multi_linestring_type, polygon>("mikado4_small", mikado4, join_round32, end_round32, 2103.686, 10.0);
        test_one<multi_linestring_type, polygon>("mikado4_small", mikado4, join_round32, end_flat, 1930.785, 10.0);
    }

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    // Coordinates in one linestring vary so much that
    // length = geometry::math::sqrt(dx * dx + dy * dy); returns a value of inf for length
    // That geometry is skipped for the buffer
    // SQL Server reports area 2117753600 (for b)
    // PostGIS    reports      2087335072 (for b)
    // BG (2)     reports       794569660 (for a/b) which apparently misses parts
    // old value was            927681870 (for a/b) which also misses parts
    // (2: since selecting other IP at end points or when segment b is smaller than a)
    test_one<multi_linestring_type, polygon>("mysql_2015_04_10a", mysql_2015_04_10a, join_round32, end_round32, 1063005187.214, 0.98);
    test_one<multi_linestring_type, polygon>("mysql_2015_04_10b", mysql_2015_04_10b, join_round32, end_round32, 1063005187.214, 0.98);
#endif

    // Two other cases with inf for length calculation (tolerance quite high
    // because the output area is quite high and varies between gcc/clang)
    test_one<multi_linestring_type, polygon>("mysql_2015_09_08a",
            mysql_2015_09_08a, join_round32, end_round32,
            5.12436196736438764e+19, 4051744443.0,
            same_distance, true, 1.0e12);
    test_one<multi_linestring_type, polygon>("mysql_2015_09_08b",
            mysql_2015_09_08b, join_round32, end_round32,
            1.32832149026508268e+19, 2061380362.0,
            same_distance, true, 1.0e12);
}


int test_main(int, char* [])
{
    test_all<true, bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all<false, bg::model::point<double, 2, bg::cs::cartesian> >();

    return 0;
}
