//
// Boost.Pointer Container
//
//  Copyright Thorsten Ottosen 2009. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/ptr_container/
//

#include <boost/ptr_container/ptr_container.hpp>

// force instantiation of all members

template class boost::ptr_array<const int, 42>;
template class boost::ptr_array<boost::nullable<const int>, 42>;

template class boost::ptr_deque<const int>;
template class boost::ptr_deque< boost::nullable<const int> >;

template class boost::ptr_list<const int>;
template class boost::ptr_list< boost::nullable<const int> >;

template class boost::ptr_map<int, const int>;
template class boost::ptr_map<int, boost::nullable<const int> >;

template class boost::ptr_vector<const int>;
template class boost::ptr_vector< boost::nullable<const int> >;

//@todo problem with constructor forwarding
//
//template class boost::ptr_unordered_map<int,T>;

// @todo: there seems to be some problems with
//        argument passing in circular_buffer
//
//boost::ptr_circular_buffer<T> buffer(32);
//buffer.push_back( new int(42)  );

template class boost::ptr_set<const int>;

// @todo: problem with constructor forwarding
//
//template class boost::ptr_unordered_set<T>;
