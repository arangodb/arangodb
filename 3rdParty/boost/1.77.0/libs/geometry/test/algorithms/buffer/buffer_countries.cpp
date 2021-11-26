// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_buffer.hpp"


template <typename MultiPolygon>
std::string read_from_file(std::string const& filename)
{
    MultiPolygon mp;
    std::ifstream in(filename.c_str());
    while (in.good())
    {
        std::string line;
        std::getline(in, line);
        if (! line.empty())
        {
            typename boost::range_value<MultiPolygon>::type pol;
            bg::read_wkt(line, pol);
            mp.push_back(pol);
        }
    }
    std::ostringstream out;
    if (! mp.empty())
    {
        out << std::fixed << std::setprecision(19) << bg::wkt(mp);
    }

    BOOST_CHECK(! out.str().empty());

    return out.str();
}


/*

Area's from PostGIS:

query:
    with viewy as
    (
      select ST_GeomFromText((insert WKT here from data),0) as p1
    )
              select 10 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, 10.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, 10.0 * 1000, 25))from viewy
    union all select 20 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, 20.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, 20.0 * 1000, 25))from viewy
    union all select 50 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, 50.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, 50.0 * 1000, 25))from viewy
    union all select 100 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, 100.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, 100.0 * 1000, 25))from viewy
    union all select -10 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, -10.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, -10.0 * 1000, 25))from viewy
    union all select -20 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, -20.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, -20.0 * 1000, 25))from viewy
    union all select -50 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, -50.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, -50.0 * 1000, 25))from viewy
    union all select -100 as w,ST_NumGeometries(p1),ST_Area(ST_Buffer(p1, -100.0 * 1000, 25)),ST_NumGeometries(ST_Buffer(p1, -100.0 * 1000, 25))from viewy

Checked are 10,20,50,100 kilometer, inflate/deflate

But for many, in the unit tests below, distance of 1,2,5 k are kept too
because they could cause self-intersections in the past

Values are not identical. We might check area with less precision.

gr:
    336277774579
    442312549699
    680433051228
    910463608938

    139313936462
     96993175731
     31392586710
      2033070670

it:
     655017269701
     749348852219
    1018311961402
    1436442592714

    477946399339
    404696093915
    238752534654
     69771921799

nl:
    123407901256
    145045853638
    201203536326
    303295187184

    64669284652
    46684337476
    10245330928
    0

no:
    2102032030338
    2292165016326
    2725461758029
    3374949015149

    1361202945650
    1089854028969
     649632754053
     306749522531

uk:
    857680535981
    970483182932
   1247820319617
   1659854829029

    572557777232
    479260087245
    274834862993
     78209736228

*/

template <typename MP, typename P>
void test_one(std::string const& caseid, std::string const& wkt, double expected_area, double distance,
              ut_settings settings = ut_settings())
{
    bg::strategy::buffer::join_round join_round(100);
    bg::strategy::buffer::end_flat end_flat;

    // Test with a high tolerance, even a difference of 1000 is only ~1.0e-6%

    settings.tolerance = 10000.0;

#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    // in case robustness policies are changed, areas should be adapted
    settings.tolerance = boost::starts_with(caseid, "no") ? 200000.0 : 100000.0;
#endif

    test_one<MP, P>(caseid, wkt, join_round, end_flat,
        expected_area, distance * 1000.0, settings);
}


template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise> pt;
    typedef bg::model::multi_polygon<pt> mpt;

    std::string base_folder = "data/";
    std::string gr = read_from_file<mpt>(base_folder + "gr.wkt");
    std::string it = read_from_file<mpt>(base_folder + "it.wkt");
    std::string nl = read_from_file<mpt>(base_folder + "nl.wkt");
    std::string no = read_from_file<mpt>(base_folder + "no.wkt");
    std::string uk = read_from_file<mpt>(base_folder + "uk.wkt");

    test_one<mpt, pt>("gr10", gr,    336279815682, 10);
    test_one<mpt, pt>("gr20", gr,    442317491749, 20);
    test_one<mpt, pt>("gr50", gr,    680442278645, 50);
    test_one<mpt, pt>("gr100", gr,   910474621215, 100);

    test_one<mpt, pt>("gr10", gr,    139313156846, -10);
    test_one<mpt, pt>("gr20", gr,     96991350242, -20);
    test_one<mpt, pt>("gr50", gr,     31391928002, -50);
    test_one<mpt, pt>("gr100", gr,     2035400805, -100);

    test_one<mpt, pt>("it1", it,     569862998347, 1);
    test_one<mpt, pt>("it2", it,     579239208963, 2);
    test_one<mpt, pt>("it5", it,     607625463736, 5);
    test_one<mpt, pt>("it10", it,    655018578530, 10);
    test_one<mpt, pt>("it20", it,    749353305743, 20);
    test_one<mpt, pt>("it50", it,   1018323115670, 50);
    test_one<mpt, pt>("it100", it,  1436451405439, 100);

    test_one<mpt, pt>("it1", it,     551474421881, -1);
    test_one<mpt, pt>("it2", it,     542617730624, -2);
    test_one<mpt, pt>("it5", it,     517402445790, -5);
    test_one<mpt, pt>("it10", it,    477945510429, -10);
    test_one<mpt, pt>("it20", it,    404693983797, -20);
    test_one<mpt, pt>("it50", it,    238748449624, -50);
    test_one<mpt, pt>("it100", it,    69768648896, -100);
    test_one<mpt, pt>("it200", it,              0, -200);

    test_one<mpt, pt>("nl1", nl,      97391170774, 1);
    test_one<mpt, pt>("nl2", nl,     100816707832, 2);
    test_one<mpt, pt>("nl5", nl,     110239801028, 5);
    test_one<mpt, pt>("nl10", nl,    123408274536, 10);
    test_one<mpt, pt>("nl20", nl,    145046915403, 20);
    test_one<mpt, pt>("nl50", nl,    201207309002, 50);
    test_one<mpt, pt>("nl100", nl,   303300936340, 100);

    test_one<mpt, pt>("nl1", nl,      90095050333, -1);
    test_one<mpt, pt>("nl2", nl,      86601861798, -2);
    test_one<mpt, pt>("nl5", nl,      77307843754, -5);
    test_one<mpt, pt>("nl10", nl,     64668870425, -10);
    test_one<mpt, pt>("nl20", nl,     46683531062, -20);
    test_one<mpt, pt>("nl50", nl,     10244523910, -50);
    test_one<mpt, pt>("nl100", nl,              0, -100);

    test_one<mpt, pt>("no1", no,    1819566570720, 1);
    test_one<mpt, pt>("no2", no,    1865041238129, 2, ut_settings::ignore_validity());
    test_one<mpt, pt>("no5", no,    1973615533600, 5);
    test_one<mpt, pt>("no10", no,   2102034240506, 10);
    test_one<mpt, pt>("no20", no,   2292171257647, 20);
    test_one<mpt, pt>("no50", no,   2725475403816, 50);
    test_one<mpt, pt>("no100", no,  3374987120112, 100);

    test_one<mpt, pt>("no1", no,    1725145487969, -1);
    test_one<mpt, pt>("no2", no,    1678942603503, -2);
    test_one<mpt, pt>("no5", no,    1547329249723, -5);
    test_one<mpt, pt>("no10", no,   1361198873951, -10);
    test_one<mpt, pt>("no20", no,   1089847815351, -20);
    test_one<mpt, pt>("no50", no,    649622162382, -50);
    test_one<mpt, pt>("no100", no,   306739133606, -100);

    test_one<mpt, pt>("uk1", uk,     733080790315, 1);
    test_one<mpt, pt>("uk2", uk,     749555939251, 2);
    test_one<mpt, pt>("uk5", uk,     793752660191, 5);
    test_one<mpt, pt>("uk10", uk,    857682286960, 10);
    test_one<mpt, pt>("uk20", uk,    970488082763, 20);
    test_one<mpt, pt>("uk50", uk,   1247830325401, 50);
    test_one<mpt, pt>("uk100", uk,  1659861958875, 100);

    test_one<mpt, pt>("uk1", uk,     699378146599, -1);
    test_one<mpt, pt>("uk2", uk,     683086442146, -2);
    test_one<mpt, pt>("uk5", uk,     637325279340, -5);
    test_one<mpt, pt>("uk10", uk,    572556625332, -10);
    test_one<mpt, pt>("uk20", uk,    479258129205, -20);
    test_one<mpt, pt>("uk50", uk,    274828071591, -50);
    test_one<mpt, pt>("uk100", uk,    78205461294, -100);
}

int test_main(int, char* [])
{
    BoostGeometryWriteTestConfiguration();

    test_all<true, bg::model::point<default_test_type, 2, bg::cs::cartesian> >();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_ORDER)
    test_all<false, bg::model::point<default_test_type, 2, bg::cs::cartesian> >();
#endif

#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    BoostGeometryWriteExpectedFailures(1, BG_NO_FAILURES, 2, BG_NO_FAILURES);
#endif

    return 0;
}

