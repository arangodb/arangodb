//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_TO_DEVICE_HPP
#define BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_TO_DEVICE_HPP

#include <iterator>

#include <boost/utility/addressof.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/async/future.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>
#include <boost/compute/memory/svm_ptr.hpp>

namespace boost {
namespace compute {
namespace detail {

template<class HostIterator, class DeviceIterator>
inline DeviceIterator copy_to_device(HostIterator first,
                                     HostIterator last,
                                     DeviceIterator result,
                                     command_queue &queue)
{
    typedef typename
        std::iterator_traits<DeviceIterator>::value_type
        value_type;
    typedef typename
        std::iterator_traits<DeviceIterator>::difference_type
        difference_type;

    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    size_t offset = result.get_index();

    queue.enqueue_write_buffer(result.get_buffer(),
                               offset * sizeof(value_type),
                               count * sizeof(value_type),
                               ::boost::addressof(*first));

    return result + static_cast<difference_type>(count);
}

template<class HostIterator, class DeviceIterator>
inline future<DeviceIterator> copy_to_device_async(HostIterator first,
                                                   HostIterator last,
                                                   DeviceIterator result,
                                                   command_queue &queue)
{
    typedef typename
        std::iterator_traits<DeviceIterator>::value_type
        value_type;
    typedef typename
        std::iterator_traits<DeviceIterator>::difference_type
        difference_type;

    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return future<DeviceIterator>();
    }

    size_t offset = result.get_index();

    event event_ =
        queue.enqueue_write_buffer_async(result.get_buffer(),
                                         offset * sizeof(value_type),
                                         count * sizeof(value_type),
                                         ::boost::addressof(*first));

    return make_future(result + static_cast<difference_type>(count), event_);
}

#ifdef CL_VERSION_2_0
// copy_to_device() specialization for svm_ptr
template<class HostIterator, class T>
inline svm_ptr<T> copy_to_device(HostIterator first,
                                 HostIterator last,
                                 svm_ptr<T> result,
                                 command_queue &queue)
{
    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    queue.enqueue_svm_memcpy(
        result.get(), ::boost::addressof(*first), count * sizeof(T)
    );

    return result + count;
}

template<class HostIterator, class T>
inline future<svm_ptr<T> > copy_to_device_async(HostIterator first,
                                                HostIterator last,
                                                svm_ptr<T> result,
                                                command_queue &queue)
{
    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    event event_ = queue.enqueue_svm_memcpy_async(
        result.get(), ::boost::addressof(*first), count * sizeof(T)
    );

    return make_future(result + count, event_);
}
#endif // CL_VERSION_2_0

} // end detail namespace
} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_TO_DEVICE_HPP
