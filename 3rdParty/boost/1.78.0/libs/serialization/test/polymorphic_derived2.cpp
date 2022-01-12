/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// polymorphic_derived2.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for updates, documentation, and revision history.

#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/extended_type_info_no_rtti.hpp>
#include <boost/serialization/export.hpp>

#include "polymorphic_base.hpp"
#include "polymorphic_derived2.hpp"

template<class Archive>
POLYMORPHIC_DERIVED2_DLL_DECL void polymorphic_derived2::serialize(
    Archive &ar,
    const unsigned int /* file_version */
){
    ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(polymorphic_base);
}

const char * polymorphic_derived2::get_key() const {
    return "polymorphic_derived2";
}
POLYMORPHIC_DERIVED2_DLL_DECL
polymorphic_derived2::polymorphic_derived2(){}
POLYMORPHIC_DERIVED2_DLL_DECL
polymorphic_derived2::~polymorphic_derived2(){}

#include <boost/archive/polymorphic_oarchive.hpp>
#include <boost/archive/polymorphic_iarchive.hpp>

template
POLYMORPHIC_DERIVED2_DLL_DECL void polymorphic_derived2::serialize(
    boost::archive::polymorphic_oarchive &,
    const unsigned int /* file_version */
);
template
POLYMORPHIC_DERIVED2_DLL_DECL void polymorphic_derived2::serialize(
    boost::archive::polymorphic_iarchive &,
    const unsigned int
);

// MWerks users can do this to make their code work
BOOST_SERIALIZATION_MWERKS_BASE_AND_DERIVED(polymorphic_base, polymorphic_derived2)

// note: export has to be AFTER #includes for all archive classes
BOOST_CLASS_EXPORT_IMPLEMENT(polymorphic_derived2)
