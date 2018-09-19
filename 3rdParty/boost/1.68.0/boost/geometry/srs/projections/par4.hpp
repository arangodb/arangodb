// Boost.Geometry

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_SRS_PROJECTIONS_PAR4_HPP
#define BOOST_GEOMETRY_SRS_PROJECTIONS_PAR4_HPP


#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/srs/sphere.hpp>
#include <boost/geometry/srs/spheroid.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/if.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_void.hpp>


namespace boost { namespace geometry { namespace srs { namespace par4
{
    
// proj
// defined in projections' implementation files

// ellps
struct MERIT {};
struct SGS85 {};
struct GRS80 {};
struct IAU76 {};
struct airy {};
struct APL4_9 {};
struct NWL9D {};
struct mod_airy {};
struct andrae {};
struct aust_SA {};
struct GRS67 {};
struct bessel {};
struct bess_nam {};
struct clrk66 {};
struct clrk80 {};
struct clrk80ign {};
struct CPM {};
struct delmbr {};
struct engelis {};
struct evrst30 {};
struct evrst48 {};
struct evrst56 {};
struct evrst69 {};
struct evrstSS {};
struct fschr60 {};
struct fschr60m {};
struct fschr68 {};
struct helmert {};
struct hough {};
struct intl {};
struct krass {};
struct kaula {};
struct lerch {};
struct mprts {};
struct new_intl {};
struct plessis {};
struct SEasia {};
struct walbeck {};
struct WGS60 {};
struct WGS66 {};
struct WGS72 {};
struct WGS84 {};
struct sphere {};

// datum
//struct WGS84 {}; // already defined above
struct GGRS87 {};
struct NAD83 {};
struct NAD27 {};
struct potsdam {};
struct carthage {};
struct hermannskogel {};
struct ire65 {};
struct nzgd49 {};
struct OSGB36 {};

template <typename P>
struct proj
{
    typedef P type;
};

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

template
<
    typename E,
    typename Tag = typename geometry::tag<E>::type
>
struct ellps_impl
    : private E // empty base optimization
{
    typedef E type;
    
    ellps_impl() : E() {}
    explicit ellps_impl(E const& e) : E(e) {}

    E const& model() const { return *this; }
};

template <typename E>
struct ellps_impl<E, void>
{
    typedef E type;
};

} // namespace detail
#endif // DOXYGEN_NO_DETAIL

template<typename E>
struct ellps
    : par4::detail::ellps_impl<E>
{
    ellps() {}
    explicit ellps(E const& e)
        : par4::detail::ellps_impl<E>(e)
    {}
};

template <typename D>
struct datum
{
    typedef D type;
};

template <typename P>
struct o_proj
{
    typedef P type;
};

struct guam {};

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

inline double b_from_a_rf(double a, double rf)
{
    return a * (1.0 - 1.0 / rf);
}

template
<
    typename Ellps,
    typename Tag = typename geometry::tag<typename Ellps::type>::type
>
struct ellps_traits
{
    typedef typename Ellps::type model_type;
    static model_type model(Ellps const& e) { return e.model(); }
};

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B(NAME, A, B) \
template <> \
struct ellps_traits<ellps<par4::NAME>, void> \
{ \
    typedef srs::spheroid<double> model_type; \
    static model_type model(ellps<par4::NAME> const&) { return model_type(A, B); } \
};

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(NAME, A, RF) \
template <> \
struct ellps_traits<ellps<par4::NAME>, void> \
{ \
    typedef srs::spheroid<double> model_type; \
    static model_type model(ellps<par4::NAME> const&) { return model_type(A, b_from_a_rf(A, RF)); } \
};

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_SPHERE(NAME, R) \
template <> \
struct ellps_traits<ellps<par4::NAME>, void> \
{ \
    typedef srs::sphere<double> model_type; \
    static model_type model(ellps<par4::NAME> const&) { return model_type(R); } \
};

BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(MERIT, 6378137.0, 298.257)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(SGS85, 6378136.0, 298.257)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(GRS80, 6378137.0, 298.257222101)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(IAU76, 6378140.0, 298.257)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (airy, 6377563.396, 6356256.910)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(APL4_9, 6378137.0, 298.25)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(NWL9D, 6378145.0, 298.25)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (mod_airy, 6377340.189, 6356034.446)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(andrae, 6377104.43, 300.0)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(aust_SA, 6378160.0, 298.25)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(GRS67, 6378160.0, 298.2471674270)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(bessel, 6377397.155, 299.1528128)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(bess_nam, 6377483.865, 299.1528128)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (clrk66, 6378206.4, 6356583.8)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(clrk80, 6378249.145, 293.4663)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(clrk80ign, 6378249.2, 293.4660212936269)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(CPM, 6375738.7, 334.29)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(delmbr, 6376428.0, 311.5)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(engelis, 6378136.05, 298.2566)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(evrst30, 6377276.345, 300.8017)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(evrst48, 6377304.063, 300.8017)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(evrst56, 6377301.243, 300.8017)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(evrst69, 6377295.664, 300.8017)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(evrstSS, 6377298.556, 300.8017)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(fschr60, 6378166.0, 298.3)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(fschr60m, 6378155.0, 298.3)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(fschr68, 6378150.0, 298.3)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(helmert, 6378200.0, 298.3)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(hough, 6378270.0, 297.0)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(intl, 6378388.0, 297.0)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(krass, 6378245.0, 298.3)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(kaula, 6378163.0, 298.24)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(lerch, 6378139.0, 298.257)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(mprts, 6397300.0, 191.0)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (new_intl, 6378157.5, 6356772.2)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (plessis, 6376523.0, 6355863.0)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (SEasia, 6378155.0, 6356773.3205)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_B (walbeck, 6376896.0, 6355834.8467)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(WGS60, 6378165.0, 298.3)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(WGS66, 6378145.0, 298.25)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(WGS72, 6378135.0, 298.26)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_ELLPS_A_RF(WGS84, 6378137.0, 298.257223563)
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_SPHERE    (sphere, 6370997.0)


template <typename D>
struct datum_traits
{
    typedef void ellps_type;
    static std::string id() { return ""; }
    static std::string def_n() { return ""; }
    static std::string def_v() { return ""; }
};

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(NAME, ID, ELLPS, DEF_N, DEF_V) \
template <> \
struct datum_traits< datum<par4::NAME> > \
{ \
    typedef par4::ellps<par4::ELLPS> ellps_type; \
    static std::string id() { return ID; } \
    static std::string def_n() { return DEF_N; } \
    static std::string def_v() { return DEF_V; } \
};

BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(WGS84, "WGS84", WGS84, "towgs84", "0,0,0")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(GGRS87, "GGRS87", GRS80, "towgs84", "-199.87,74.79,246.62")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(NAD83, "NAD83", GRS80, "towgs84", "0,0,0")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(NAD27, "NAD27", clrk66, "nadgrids", "@conus,@alaska,@ntv2_0.gsb,@ntv1_can.dat")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(potsdam, "potsdam", bessel, "towgs84", "598.1,73.7,418.2,0.202,0.045,-2.455,6.7")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(carthage, "carthage", clrk80ign, "towgs84", "-263.0,6.0,431.0")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(hermannskogel, "hermannskogel", bessel, "towgs84", "577.326,90.129,463.919,5.137,1.474,5.297,2.4232")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(ire65, "ire65", mod_airy, "towgs84", "482.530,-130.596,564.557,-1.042,-0.214,-0.631,8.15")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(nzgd49, "nzgd49", intl, "towgs84", "59.47,-5.04,187.44,0.47,-0.1,1.024,-4.5993")
BOOST_GEOMETRY_PROJECTIONS_DETAIL_REGISTER_DATUM(OSGB36, "OSGB36", airy, "towgs84", "446.448,-125.157,542.060,0.1502,0.2470,0.8421,-20.4894")


template
<
    typename Tuple,
    template <typename> class IsSamePred,
    int I = 0,
    int N = boost::tuples::length<Tuple>::value
>
struct tuples_find_index_if
    : boost::mpl::if_c
        <
            IsSamePred<typename boost::tuples::element<I, Tuple>::type>::value,
            boost::integral_constant<int, I>,
            typename tuples_find_index_if<Tuple, IsSamePred, I+1, N>::type
        >::type
{};

template
<
    typename Tuple,
    template <typename> class IsSamePred,
    int N
>
struct tuples_find_index_if<Tuple, IsSamePred, N, N>
    : boost::integral_constant<int, N>
{};

template
<
    typename Tuple,
    template <typename> class IsSamePred,
    int I = tuples_find_index_if<Tuple, IsSamePred>::value,
    int N = boost::tuples::length<Tuple>::value
>
struct tuples_find_if
    : boost::tuples::element<I, Tuple>
{};

template
<
    typename Tuple,
    template <typename> class IsSamePred,
    int N
>
struct tuples_find_if<Tuple, IsSamePred, N, N>
{
    typedef void type;
};

/*template <typename Param>
struct is_param
{
    template <typename T, int D = 0>
    struct is_same_impl : boost::false_type {};
    template <int D>
    struct is_same_impl<Param, D> : boost::true_type {};

    template <typename T>
    struct is_same : is_same_impl<T> {};
};

template <template <typename> class Param>
struct is_param_t
{
    template <typename T>
    struct is_same : boost::false_type {};
    template <typename T>
    struct is_same<Param<T> > : boost::true_type {};
};*/

// NOTE: The following metafunctions are implemented one for each parameter
// because mingw-gcc-4.1.2 is unable to compile a solution based on template
// template parameter and member struct template partial specialization
// (see above).

/*template <typename T>
struct is_proj : boost::false_type {};
template <typename T>
struct is_proj<proj<T> > : boost::true_type {};

template <typename T>
struct is_ellps : boost::false_type {};
template <typename T>
struct is_ellps<ellps<T> > : boost::true_type {};

template <typename T>
struct is_datum : boost::false_type {};
template <typename T>
struct is_datum<datum<T> > : boost::true_type {};

template <typename T>
struct is_o_proj : boost::false_type {};
template <typename T>
struct is_o_proj<o_proj<T> > : boost::true_type {};

template <typename T>
struct is_guam : boost::false_type {};
template <>
struct is_guam<guam> : boost::true_type {};*/

// NOTE: The following implementation seems to work as well.

// TEST

template <typename T, template <typename> class Param>
struct is_same_t : boost::false_type {};
template <typename T, template <typename> class Param>
struct is_same_t<Param<T>, Param> : boost::true_type {};

template <typename Param>
struct is_param
{
    template <typename T>
    struct pred : boost::is_same<T, Param> {};
};

template <template <typename> class Param>
struct is_param_t
{
    template <typename T>
    struct pred : is_same_t<T, Param> {};
};

// pick proj static name

template <typename Tuple>
struct pick_proj_tag
{
    typedef typename tuples_find_if
        <
            Tuple,
            // is_proj
            is_param_t<proj>::pred
        >::type proj_type;

    static const bool is_non_void = ! boost::is_void<proj_type>::value;

    BOOST_MPL_ASSERT_MSG((is_non_void), PROJECTION_NOT_NAMED, (Tuple));

    typedef typename proj_type::type type;
};


template <typename Ellps, typename Datum, int EllpsIndex>
struct pick_ellps_impl
{
    typedef Ellps type;
    typedef typename ellps_traits<Ellps>::model_type model_type;
    template <typename Tuple>
    static model_type model(Tuple const& tup)
    {
        return ellps_traits<Ellps>::model(boost::get<EllpsIndex>(tup));
    }
};

template <typename Ellps, int EllpsIndex>
struct pick_ellps_impl<Ellps, void, EllpsIndex>
{
    typedef Ellps type;
    typedef typename ellps_traits<Ellps>::model_type model_type;
    template <typename Tuple>
    static model_type model(Tuple const& tup)
    {
        return ellps_traits<Ellps>::model(boost::get<EllpsIndex>(tup));
    }
};

template <typename Datum, int EllpsIndex>
struct pick_ellps_impl<void, Datum, EllpsIndex>
{
    typedef typename datum_traits<Datum>::ellps_type type;

    static const bool is_datum_known = ! boost::is_void<type>::value;
    BOOST_MPL_ASSERT_MSG((is_datum_known), UNKNOWN_DATUM, (types<Datum>));

    typedef typename ellps_traits<type>::model_type model_type;
    template <typename Tuple>
    static model_type model(Tuple const& )
    {
        return ellps_traits<type>::model(type());
    }
};

template <int EllpsIndex>
struct pick_ellps_impl<void, void, EllpsIndex>
{
    // default ellipsoid
    typedef ellps<WGS84> type;
    typedef typename ellps_traits<type>::model_type model_type;
    template <typename Tuple>
    static model_type model(Tuple const& )
    {
        return ellps_traits<type>::model(type());
    }
};

// Pick spheroid/sphere model from ellps or datum
// mimic pj_init() calling pj_datum_set() and pj_ell_set()
template <typename Tuple>
struct pick_ellps
    : pick_ellps_impl
        <
            typename tuples_find_if<Tuple, /*is_ellps*/is_param_t<ellps>::pred>::type,
            typename tuples_find_if<Tuple, /*is_datum*/is_param_t<datum>::pred>::type,
            tuples_find_index_if<Tuple, /*is_ellps*/is_param_t<ellps>::pred>::value
        >
{};


template <typename Tuple>
struct pick_o_proj_tag
{
    typedef typename tuples_find_if
        <
            Tuple,
            //is_o_proj
            is_param_t<o_proj>::pred
        >::type proj_type;

    static const bool is_non_void = ! boost::is_void<proj_type>::value;

    BOOST_MPL_ASSERT_MSG((is_non_void), NO_O_PROJ_PARAMETER, (Tuple));

    typedef typename proj_type::type type;
};


} // namespace detail
#endif // DOXYGEN_NO_DETAIL


}}}} // namespace boost::geometry::srs::par4


#endif // BOOST_GEOMETRY_SRS_PROJECTIONS_PAR4_HPP
