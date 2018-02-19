//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/asio/streambuf.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <utility>
#include <vector>

namespace boost {
namespace beast {

class buffers_test : public beast::unit_test::suite
{
public:
    using size_type = std::uint64_t;

    class timer
    {
        using clock_type =
            std::chrono::system_clock;

        clock_type::time_point when_;

    public:
        using duration =
            clock_type::duration;

        timer()
            : when_(clock_type::now())
        {
        }

        duration
        elapsed() const
        {
            return clock_type::now() - when_;
        }
    };

    inline
    size_type
    throughput(std::chrono::duration<
        double> const& elapsed, size_type items)
    {
        using namespace std::chrono;
        return static_cast<size_type>(
            1 / (elapsed/items).count());
    }

    template<class MutableBufferSequence>
    static
    std::size_t
    fill(MutableBufferSequence const& buffers)
    {
        std::size_t n = 0;
        for(auto b : beast::detail::buffers_range(buffers))
        {
            std::fill(
                reinterpret_cast<char*>(b.data()),
                reinterpret_cast<char*>(b.data()) +
                    b.size(), '\0');
            n += b.size();
        }
        return n;
    }

    template<class DynamicBuffer>
    size_type
    do_prepares(std::size_t repeat,
        std::size_t count, std::size_t size)
    {
        timer t;
        size_type total = 0;
        for(auto i = repeat; i--;)
        {
            DynamicBuffer b;
            for(auto j = count; j--;)
            {
                auto const n = fill(b.prepare(size));
                b.commit(n);
                total += n;
            }
        }
        return throughput(t.elapsed(), total);
    }

    template<class DynamicBuffer>
    size_type
    do_hints(std::size_t repeat,
        std::size_t count, std::size_t size)
    {
        timer t;
        size_type total = 0;
        for(auto i = repeat; i--;)
        {
            DynamicBuffer b;
            for(auto j = count; j--;)
            {
                for(auto remain = size; remain;)
                {
                    auto const n = fill(b.prepare(
                        read_size(b, remain)));
                    b.commit(n);
                    remain -= n;
                    total += n;
                }
            }
        }
        return throughput(t.elapsed(), total);
    }

    template<class DynamicBuffer>
    size_type
    do_random(std::size_t repeat,
        std::size_t count, std::size_t size)
    {
        timer t;
        size_type total = 0;
        for(auto i = repeat; i--;)
        {
            DynamicBuffer b;
            for(auto j = count; j--;)
            {
                auto const n = fill(b.prepare(
                    1 + (rand()%(2*size))));
                b.commit(n);
                total += n;
            }
        }
        return throughput(t.elapsed(), total);
    }

    static
    inline
    void
    do_trials_1(bool)
    {
    }

    template<class F0, class... FN>
    void
    do_trials_1(bool print, F0&& f, FN... fn)
    {
        timer t;
        using namespace std::chrono;
        static size_type constexpr den = 1024 * 1024;
        if(print)
        {
            log << std::right << std::setw(10) <<
                ((f() + (den / 2)) / den) << " MB/s";
            log.flush();
        }
        else
        {
            f();
        }
        do_trials_1(print, fn...);
    }

    template<class F0, class... FN>
    void
    do_trials(string_view name,
        std::size_t trials, F0&& f0, FN... fn)
    {
        using namespace std::chrono;
        // warm-up
        do_trials_1(false, f0, fn...);
        do_trials_1(false, f0, fn...);
        while(trials--)
        {
            timer t;
            log << std::left << std::setw(24) << name << ":";
            log.flush();
            do_trials_1(true, f0, fn...);
            log << "   " <<
                duration_cast<milliseconds>(t.elapsed()).count() << "ms";
            log << std::endl;
        }
    }

    void
    run() override
    {
        static std::size_t constexpr trials = 1;
        static std::size_t constexpr repeat = 250;
        std::vector<std::pair<std::size_t, std::size_t>> params;
        params.emplace_back(1024, 1024);
        params.emplace_back(512, 4096);
        params.emplace_back(256, 32768);
        log << std::endl;
        for(auto const& param : params)
        {
            auto const count = param.first;
            auto const size = param.second;
            auto const s = std::string("count=") + std::to_string(count) +
                ", size=" + std::to_string(size);
            log << std::left << std::setw(24) << s << " " <<
                std::right << std::setw(15) << "prepare" <<
                std::right << std::setw(15) << "with hint" <<
                std::right << std::setw(15) << "random" <<
                std::endl;
            do_trials("multi_buffer", trials,
                 [&](){ return do_prepares<multi_buffer>(repeat, count, size); }
                ,[&](){ return do_hints   <multi_buffer>(repeat, count, size); }
                ,[&](){ return do_random  <multi_buffer>(repeat, count, size); }
            );
            do_trials("flat_buffer", trials,
                 [&](){ return do_prepares<flat_buffer>(repeat, count, size); }
                ,[&](){ return do_hints   <flat_buffer>(repeat, count, size); }
                ,[&](){ return do_random  <flat_buffer>(repeat, count, size); }
            );
            do_trials("boost::asio::streambuf", trials,
                 [&](){ return do_prepares<boost::asio::streambuf>(repeat, count, size); }
                ,[&](){ return do_hints   <boost::asio::streambuf>(repeat, count, size); }
                ,[&](){ return do_random  <boost::asio::streambuf>(repeat, count, size); }
            );
            log << std::endl;
        }
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,benchmarks,buffers);

} // beast
} // boost
