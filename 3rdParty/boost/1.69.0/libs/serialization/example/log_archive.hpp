#ifndef LOG_ARCHIVE_HPP
#define LOG_ARCHIVE_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER)
# pragma once
#endif

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// log_archive.hpp

// (C) Copyright 2010 Robert Ramey - http://www.rrsd.com . 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for updates, documentation, and revision history.

#include <boost/archive/xml_oarchive.hpp>

namespace boost {
    namespace archive {
        namespace detail {
            template<class Archive> class interface_oarchive;
        } // namespace detail
    } // namespace archive
} // boost

/////////////////////////////////////////////////////////////////////////
// log data to an output stream.  This illustrates a simpler implemenation
// of text output which is useful for getting a formatted display of
// any serializable class.  Intended to be useful as a debugging aid.
class log_archive :
    /* protected ? */ 
    public boost::archive::xml_oarchive_impl<log_archive>
{
    typedef boost::archive::xml_oarchive_impl<log_archive> base;
    // give serialization implementation access to this clas
    friend class boost::archive::detail::interface_oarchive<log_archive>;
    friend class boost::archive::basic_xml_oarchive<log_archive>;
    friend class boost::archive::save_access;

    /////////////////////////////////////////////////////////////////////
    // Override functions defined in basic_xml_oarchive

    // Anything not an attribute and not a name-value pair is an
    // error and should be trapped here.
    template<class T>
    void save_override(T & t){
        // make it a name-value pair and pass it on.
        // this permits this to be used even with data types which
        // are not wrapped with the name
        base::save_override(boost::serialization::make_nvp(NULL, t));
    }
    template<class T>
    void save_override(const boost::serialization::nvp< T > & t){
        // this is here to remove the "const" requirement.  Since
        // this class is to be used only for output, it's not required.
        base::save_override(t);
    }
    // specific overrides for attributes - not name value pairs so we
    // want to trap them before the above "fall through"
    // since we don't want to see these in the output - make them no-ops.
    void save_override(const boost::archive::object_id_type & t){}
    void save_override(const boost::archive::object_reference_type & t){}
    void save_override(const boost::archive::version_type & t){}
    void save_override(const boost::archive::class_id_type & t){}
    void save_override(const boost::archive::class_id_optional_type & t){}
    void save_override(const boost::archive::class_id_reference_type & t){}
    void save_override(const boost::archive::class_name_type & t){}
    void save_override(const boost::archive::tracking_type & t){}
public:
    log_archive(std::ostream & os, unsigned int flags = 0) :
        boost::archive::xml_oarchive_impl<log_archive>(
            os, 
            flags | boost::archive::no_header
        )
    {}
};

#endif // LOG_ARCHIVE_HPP
