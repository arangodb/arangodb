//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_TO_HOST_HPP
#define BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_TO_HOST_HPP

#include <iterator>

#include <boost/utility/addressof.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/async/future.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>
#include <boost/compute/memory/svm_ptr.hpp>
#include <boost/compute/detail/iterator_plus_distance.hpp>

namespace boost {
namespace compute {
namespace detail {

template<class DeviceIterator, class HostIterator>
inline HostIterator copy_to_host(DeviceIterator first,
                                 DeviceIterator last,
                                 HostIterator result,
                                 command_queue &queue)
{
    typedef typename
        std::iterator_traits<DeviceIterator>::value_type
        value_type;

    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    const buffer &buffer = first.get_buffer();
    size_t offset = first.get_index();

    queue.enqueue_read_buffer(buffer,
                              offset * sizeof(value_type),
                              count * sizeof(value_type),
                              ::boost::addressof(*result));

    return iterator_plus_distance(result, count);
}

// copy_to_host() specialization for std::vector<bool>
template<class DeviceIterator>
inline std::vector<bool>::iterator
copy_to_host(DeviceIterator first,
             DeviceIterator last,
             std::vector<bool>::iterator result,
             command_queue &queue)
{
    std::vector<uint8_t> temp(std::distance(first, last));
    copy_to_host(first, last, temp.begin(), queue);
    return std::copy(temp.begin(), temp.end(), result);
}

template<class DeviceIterator, class HostIterator>
inline future<HostIterator> copy_to_host_async(DeviceIterator first,
                                               DeviceIterator last,
                                               HostIterator result,
                                               command_queue &queue)
{
    typedef typename
        std::iterator_traits<DeviceIterator>::value_type
        value_type;

    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return future<HostIterator>();
    }

    const buffer &buffer = first.get_buffer();
    size_t offset = first.get_index();

    event event_ =
        queue.enqueue_read_buffer_async(buffer,
                                        offset * sizeof(value_type),
                                        count * sizeof(value_type),
                                        ::boost::addressof(*result));

    return make_future(iterator_plus_distance(result, count), event_);
}

#ifdef CL_VERSION_2_0
// copy_to_host() specialization for svm_ptr
template<class T, class HostIterator>
inline HostIterator copy_to_host(svm_ptr<T> first,
                                 svm_ptr<T> last,
                                 HostIterator result,
                                 command_queue &queue)
{
    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    queue.enqueue_svm_memcpy(
        ::boost::addressof(*result), first.get(), count * sizeof(T)
    );

    return result + count;
}

template<class T, class HostIterator>
inline future<HostIterator> copy_to_host_async(svm_ptr<T> first,
                                               svm_ptr<T> last,
                                               HostIterator result,
                                               command_queue &queue)
{
    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    event event_ = queue.enqueue_svm_memcpy_async(
        ::boost::addressof(*result), first.get(), count * sizeof(T)
    );

    return make_future(iterator_plus_distance(result, count), event_);
}
#endif // CL_VERSION_2_0

} // end detail namespace
} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_TO_HOST_HPP
