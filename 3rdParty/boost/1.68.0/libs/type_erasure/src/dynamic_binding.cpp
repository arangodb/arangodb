// Boost.TypeErasure library
//
// Copyright 2015 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#define BOOST_TYPE_ERASURE_SOURCE

#include <boost/type_erasure/register_binding.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <map>
#include <utility>

namespace {
    
using ::boost::type_erasure::detail::key_type;
using ::boost::type_erasure::detail::value_type;

typedef ::std::map<key_type, void(*)()> map_type;
typedef ::boost::shared_mutex mutex_type;

// std::pair can have problems on older implementations
// when it tries to use the copy constructor of the mutex.
struct data_type
{
    map_type first;
    mutex_type second;
};

data_type * get_data() {
    static data_type result;
    return &result;
}

}

BOOST_TYPE_ERASURE_DECL void boost::type_erasure::detail::register_function_impl(const key_type& key, value_type fn) {
    ::data_type * data = ::get_data();
    ::boost::unique_lock<mutex_type> lock(data->second);
    data->first.insert(std::make_pair(key, fn));
}

BOOST_TYPE_ERASURE_DECL value_type boost::type_erasure::detail::lookup_function_impl(const key_type& key) {
    ::data_type * data = ::get_data();
    ::boost::shared_lock<mutex_type> lock(data->second);
    ::map_type::const_iterator pos = data->first.find(key);
    if(pos != data->first.end()) {
        return pos->second;
    } else {
        throw bad_any_cast();
    }
}
