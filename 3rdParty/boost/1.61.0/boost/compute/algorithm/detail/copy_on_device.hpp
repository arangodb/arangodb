//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_ON_DEVICE_HPP
#define BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_ON_DEVICE_HPP

#include <iterator>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/async/future.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>
#include <boost/compute/iterator/discard_iterator.hpp>
#include <boost/compute/memory/svm_ptr.hpp>
#include <boost/compute/detail/iterator_range_size.hpp>
#include <boost/compute/detail/meta_kernel.hpp>
#include <boost/compute/detail/parameter_cache.hpp>
#include <boost/compute/detail/work_size.hpp>

namespace boost {
namespace compute {
namespace detail {

inline size_t pick_copy_work_group_size(size_t n, const device &device)
{
    (void) device;

    if(n % 32 == 0) return 32;
    else if(n % 16 == 0) return 16;
    else if(n % 8 == 0) return 8;
    else if(n % 4 == 0) return 4;
    else if(n % 2 == 0) return 2;
    else return 1;
}

template<class InputIterator, class OutputIterator>
class copy_kernel : public meta_kernel
{
public:
    copy_kernel(const device &device)
        : meta_kernel("copy")
    {
        m_count = 0;

        typedef typename std::iterator_traits<InputIterator>::value_type input_type;

        boost::shared_ptr<parameter_cache> parameters =
            detail::parameter_cache::get_global_cache(device);

        std::string cache_key =
            "__boost_copy_kernel_" + boost::lexical_cast<std::string>(sizeof(input_type));

        m_vpt = parameters->get(cache_key, "vpt", 4);
        m_tpb = parameters->get(cache_key, "tpb", 128);
    }

    void set_range(InputIterator first,
                   InputIterator last,
                   OutputIterator result)
    {
        m_count_arg = add_arg<uint_>("count");

        *this <<
            "uint index = get_local_id(0) + " <<
               "(" << m_vpt * m_tpb << " * get_group_id(0));\n" <<
            "for(uint i = 0; i < " << m_vpt << "; i++){\n" <<
            "    if(index < count){\n" <<
                     result[expr<uint_>("index")] << '=' <<
                         first[expr<uint_>("index")] << ";\n" <<
            "        index += " << m_tpb << ";\n"
            "    }\n"
            "}\n";

        m_count = detail::iterator_range_size(first, last);
    }

    event exec(command_queue &queue)
    {
        if(m_count == 0){
            // nothing to do
            return event();
        }

        size_t global_work_size = calculate_work_size(m_count, m_vpt, m_tpb);

        set_arg(m_count_arg, uint_(m_count));

        return exec_1d(queue, 0, global_work_size, m_tpb);
    }

private:
    size_t m_count;
    size_t m_count_arg;
    uint_ m_vpt;
    uint_ m_tpb;
};

template<class InputIterator, class OutputIterator>
inline OutputIterator copy_on_device(InputIterator first,
                                     InputIterator last,
                                     OutputIterator result,
                                     command_queue &queue)
{
    const device &device = queue.get_device();

    copy_kernel<InputIterator, OutputIterator> kernel(device);

    kernel.set_range(first, last, result);
    kernel.exec(queue);

    return result + std::distance(first, last);
}

template<class InputIterator>
inline discard_iterator copy_on_device(InputIterator first,
                                       InputIterator last,
                                       discard_iterator result,
                                       command_queue &queue)
{
    (void) queue;

    return result + std::distance(first, last);
}

template<class InputIterator, class OutputIterator>
inline future<OutputIterator> copy_on_device_async(InputIterator first,
                                                   InputIterator last,
                                                   OutputIterator result,
                                                   command_queue &queue)
{
    const device &device = queue.get_device();

    copy_kernel<InputIterator, OutputIterator> kernel(device);

    kernel.set_range(first, last, result);
    event event_ = kernel.exec(queue);

    return make_future(result + std::distance(first, last), event_);
}

#ifdef CL_VERSION_2_0
// copy_on_device() specialization for svm_ptr
template<class T>
inline svm_ptr<T> copy_on_device(svm_ptr<T> first,
                                 svm_ptr<T> last,
                                 svm_ptr<T> result,
                                 command_queue &queue)
{
    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    queue.enqueue_svm_memcpy(
        result.get(), first.get(), count * sizeof(T)
    );

    return result + count;
}

template<class T>
inline future<svm_ptr<T> > copy_on_device_async(svm_ptr<T> first,
                                                svm_ptr<T> last,
                                                svm_ptr<T> result,
                                                command_queue &queue)
{
    size_t count = iterator_range_size(first, last);
    if(count == 0){
        return result;
    }

    event event_ = queue.enqueue_svm_memcpy_async(
        result.get(), first.get(), count * sizeof(T)
    );

    return make_future(result + count, event_);
}
#endif // CL_VERSION_2_0

} // end detail namespace
} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_ALGORITHM_DETAIL_COPY_ON_DEVICE_HPP
