//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/detail/config.hpp>

#if defined(BOOST_JSON_USE_SSE2)
#  define RAPIDJSON_SSE2
#  define SSE2_ARCH_SUFFIX "/sse2"
#else
#  define SSE2_ARCH_SUFFIX ""
#endif

#include "lib/nlohmann/single_include/nlohmann/json.hpp"

#include "lib/rapidjson/include/rapidjson/rapidjson.h"
#include "lib/rapidjson/include/rapidjson/document.h"
#include "lib/rapidjson/include/rapidjson/writer.h"
#include "lib/rapidjson/include/rapidjson/stringbuffer.h"

#include <boost/json.hpp>
#include <boost/json/basic_parser_impl.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <cstdio>
#include <vector>

#include "test_suite.hpp"

/*  References

    https://github.com/nst/JSONTestSuite

    http://seriot.ch/parsing_json.php
*/

namespace boost {
namespace json {

using clock_type = std::chrono::steady_clock;

::test_suite::debug_stream dout(std::cerr);
std::stringstream strout;

#if defined(__clang__)
string_view toolset = "clang";
#elif defined(__GNUC__)
string_view toolset = "gcc";
#elif defined(_MSC_VER)
string_view toolset = "msvc";
#else
string_view toolset = "unknown";
#endif

#if BOOST_JSON_ARCH == 32
string_view arch = "x86" SSE2_ARCH_SUFFIX;
#elif BOOST_JSON_ARCH == 64
string_view arch = "x64" SSE2_ARCH_SUFFIX;
#else
#error Unknown architecture.
#endif

//----------------------------------------------------------

struct file_item
{
    string_view name;
    std::string text;
};

using file_list = std::vector<file_item>;

class any_impl
{
public:
    virtual ~any_impl() = default;
    virtual string_view name() const noexcept = 0;
    virtual void parse(string_view s, std::size_t repeat) const = 0;
    virtual void serialize(string_view s, std::size_t repeat) const = 0;
};

using impl_list = std::vector<
    std::unique_ptr<any_impl const>>;

std::string
load_file(char const* path)
{
    FILE* f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    auto const size = ftell(f);
    std::string s;
    s.resize(size);
    fseek(f, 0, SEEK_SET);
    auto const nread =
        fread(&s[0], 1, size, f);
    s.resize(nread);
    fclose(f);
    return s;
}

struct sample
{
    std::size_t calls;
    std::size_t millis;
    std::size_t mbs;
};

// Returns the number of invocations per second
template<
    class Rep,
    class Period,
    class F>
sample
run_for(
    std::chrono::duration<
        Rep, Period> interval,
    F&& f)
{
    using clock_type =
        std::chrono::high_resolution_clock;
    auto const when = clock_type::now();
    auto elapsed = clock_type::now() - when;
    std::size_t n = 0;
    do
    {
        f();
        elapsed = clock_type::now() - when;
        ++n;
    }
    while(elapsed < interval);
    return { n, static_cast<std::size_t>(
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                elapsed).count()), 0 };
}

void
bench(
    string_view verb,
    file_list const& vf,
    impl_list const& vi, std::size_t Trials)
{
    std::vector<sample> trial;
    for(unsigned i = 0; i < vf.size(); ++i)
    {
        for(unsigned j = 0; j < vi.size(); ++j)
        {
            trial.clear();
            std::size_t repeat = 1000;
            for(unsigned k = 0; k < Trials; ++k)
            {
                auto result = run_for(
                    std::chrono::seconds(5),
                    [&]
                    {
                        if(verb == "Parse")
                            vi[j]->parse(
                                vf[i].text,
                                repeat);
                        else if(verb == "Serialize")
                            vi[j]->serialize(
                                vf[i].text,
                                repeat);
                    });
                result.calls *= repeat;
                result.mbs = static_cast<
                    std::size_t>(( 0.5 + 1000.0 *
                        result.calls *
                        vf[i].text.size() /
                        result.millis / 1024 / 1024));
                dout <<
                    verb << " " << vf[i].name << "," <<
                    toolset << " " << arch << "," <<
                    vi[j]->name() << "," <<
                    result.calls << "," <<
                    result.millis << "," <<
                    result.mbs <<
                    "\n";
                trial.push_back(result);
                // adjust repeat to avoid overlong tests
                repeat = 250 * result.calls / result.millis;
            }

            // clean up the samples
            std::sort(
                trial.begin(),
                trial.end(),
                []( sample const& lhs,
                    sample const& rhs)
                {
                    return lhs.mbs < rhs.mbs;
                });
            if(Trials >= 6)
            {
                // discard worst 2
                trial.erase(
                    trial.begin(),
                    trial.begin() + 2);
                // discard best 1
                trial.resize(3);
            }
            else if(Trials > 3)
            {
                trial.erase(
                    trial.begin(),
                    trial.begin() + Trials - 3);
            }
            // average
            auto const calls =
                std::accumulate(
                trial.begin(), trial.end(),
                std::size_t{},
                []( std::size_t lhs,
                    sample const& rhs)
                {
                    return lhs + rhs.calls;
                });
            auto const millis =
                std::accumulate(
                trial.begin(), trial.end(),
                std::size_t{},
                []( std::size_t lhs,
                    sample const& rhs)
                {
                    return lhs + rhs.millis;
                });
            auto const mbs = static_cast<
                std::size_t>(( 0.5 + 1000.0 *
                calls * vf[i].text.size() /
                    millis / 1024 / 1024));
            strout <<
                verb << " " << vf[i].name << "," <<
                toolset << " " << arch << "," <<
                vi[j]->name() << "," <<
                mbs <<
                "\n";
        }
    }
}

//----------------------------------------------------------

class boost_default_impl : public any_impl
{
    std::string name_;

public:
    boost_default_impl(
        std::string const& branch)
    {
        name_ = "boost";
        if(! branch.empty())
            name_ += " " + branch;
    }

    string_view
    name() const noexcept override
    {
        return name_;
    }

    void
    parse(
        string_view s,
        std::size_t repeat) const override
    {
        stream_parser p;
        while(repeat--)
        {
            p.reset();
            error_code ec;
            p.write(s.data(), s.size(), ec);
            if(! ec)
                p.finish(ec);
            if(! ec)
                auto jv = p.release();
        }
    }

    void
    serialize(
        string_view s,
        std::size_t repeat) const override
    {
        auto jv = json::parse(s);
        serializer sr;
        string out;
        out.reserve(512);
        while(repeat--)
        {
            sr.reset(&jv);
            out.clear();
            for(;;)
            {
                out.grow(sr.read(
                    out.end(),
                    out.capacity() -
                        out.size()).size());
                if(sr.done())
                    break;
                out.reserve(
                    out.capacity() + 1);
            }
        }
    }
};

//----------------------------------------------------------

class boost_pool_impl : public any_impl
{
    std::string name_;

public:
    boost_pool_impl(
        std::string const& branch)
    {
        name_ = "boost (pool)";
        if(! branch.empty())
            name_ += " " + branch;
    }

    string_view
    name() const noexcept override
    {
        return name_;
    }

    void
    parse(
        string_view s,
        std::size_t repeat) const override
    {
        stream_parser p;
        while(repeat--)
        {
            monotonic_resource mr;
            p.reset(&mr);
            error_code ec;
            p.write(s.data(), s.size(), ec);
            if(! ec)
                p.finish(ec);
            if(! ec)
                auto jv = p.release();
        }
    }

    void
    serialize(
        string_view s,
        std::size_t repeat) const override
    {
        monotonic_resource mr;
        auto jv = json::parse(s, &mr);
        serializer sr;
        string out;
        out.reserve(512);
        while(repeat--)
        {
            sr.reset(&jv);
            out.clear();
            for(;;)
            {
                out.grow(sr.read(
                    out.end(),
                    out.capacity() -
                        out.size()).size());
                if(sr.done())
                    break;
                out.reserve(
                    out.capacity() + 1);
            }
        }
    }
};

//----------------------------------------------------------

class boost_null_impl : public any_impl
{
    struct null_parser
    {
        struct handler
        {
            constexpr static std::size_t max_object_size = std::size_t(-1);
            constexpr static std::size_t max_array_size = std::size_t(-1);
            constexpr static std::size_t max_key_size = std::size_t(-1);
            constexpr static std::size_t max_string_size = std::size_t(-1);

            bool on_document_begin(error_code&) { return true; }
            bool on_document_end(error_code&) { return true; }
            bool on_object_begin(error_code&) { return true; }
            bool on_object_end(std::size_t, error_code&) { return true; }
            bool on_array_begin(error_code&) { return true; }
            bool on_array_end(std::size_t, error_code&) { return true; }
            bool on_key_part(string_view, std::size_t, error_code&) { return true; }
            bool on_key( string_view, std::size_t, error_code&) { return true; }
            bool on_string_part(string_view, std::size_t, error_code&) { return true; }
            bool on_string(string_view, std::size_t, error_code&) { return true; }
            bool on_number_part(string_view, error_code&) { return true; }
            bool on_int64(std::int64_t, string_view, error_code&) { return true; }
            bool on_uint64(std::uint64_t, string_view, error_code&) { return true; }
            bool on_double(double, string_view, error_code&) { return true; }
            bool on_bool(bool, error_code&) { return true; }
            bool on_null(error_code&) { return true; }
            bool on_comment_part(string_view, error_code&) { return true; }
            bool on_comment(string_view, error_code&) { return true; }
        };

        basic_parser<handler> p_;

        null_parser()
            : p_(json::parse_options())
        {
        }

        void
        reset()
        {
            p_.reset();
        }

        std::size_t
        write(
            char const* data,
            std::size_t size,
            error_code& ec)
        {
            auto const n = p_.write_some(
                false, data, size, ec);
            if(! ec && n < size)
                ec = error::extra_data;
            return n;
        }
    };

    std::string name_;

public:
    boost_null_impl(
        std::string const& branch)
    {
        name_ = "boost (null)";
        if(! branch.empty())
            name_ += " " + branch;
    }

    string_view
    name() const noexcept override
    {
        return name_;
    }

    void
    parse(
        string_view s,
        std::size_t repeat) const override
    {
        null_parser p;
        while(repeat--)
        {
            p.reset();
            error_code ec;
            p.write(s.data(), s.size(), ec);
            BOOST_ASSERT(! ec);
        }
    }

    void
    serialize(
        string_view, std::size_t) const override
    {
    }
};

//----------------------------------------------------------

struct rapidjson_crt_impl : public any_impl
{
    string_view
    name() const noexcept override
    {
        return "rapidjson";
    }

    void
    parse(
        string_view s, std::size_t repeat) const override
    {
        using namespace rapidjson;
        while(repeat--)
        {
            CrtAllocator alloc;
            GenericDocument<
                UTF8<>, CrtAllocator> d(&alloc);
            d.Parse(s.data(), s.size());
        }
    }

    void
    serialize(string_view s, std::size_t repeat) const override
    {
        using namespace rapidjson;
        CrtAllocator alloc;
        GenericDocument<
            UTF8<>, CrtAllocator> d(&alloc);
        d.Parse(s.data(), s.size());
        rapidjson::StringBuffer st;
        while(repeat--)
        {
            st.Clear();
            rapidjson::Writer<
                rapidjson::StringBuffer> wr(st);
            d.Accept(wr);
        }
    }
};

struct rapidjson_memory_impl : public any_impl
{
    string_view
    name() const noexcept override
    {
        return "rapidjson (pool)";
    }

    void
    parse(
        string_view s, std::size_t repeat) const override
    {
        while(repeat--)
        {
            rapidjson::Document d;
            d.Parse(s.data(), s.size());
        }
    }

    void
    serialize(string_view s, std::size_t repeat) const override
    {
        rapidjson::Document d;
        d.Parse(s.data(), s.size());
        rapidjson::StringBuffer st;
        while(repeat--)
        {
            st.Clear();
            rapidjson::Writer<
                rapidjson::StringBuffer> wr(st);
            d.Accept(wr);
        }
    }
};

//----------------------------------------------------------

struct nlohmann_impl : public any_impl
{
    string_view
    name() const noexcept override
    {
        return "nlohmann";
    }

    void
    parse(string_view s, std::size_t repeat) const override
    {
        while(repeat--)
        {
            auto jv = nlohmann::json::parse(
                s.begin(), s.end());
        }
    }

    void
    serialize(string_view s, std::size_t repeat) const override
    {
        auto jv = nlohmann::json::parse(
            s.begin(), s.end());
        while(repeat--)
            auto st = jv.dump();

    }
};

//----------------------------------------------------------

} // json
} // boost

//

using namespace boost::json;

std::string s_tests = "ps";
std::string s_impls = "bdrcn";
std::size_t s_trials = 6;
std::string s_branch = "";

static bool parse_option( char const * s )
{
    if( *s == 0 )
    {
        return false;
    }

    char opt = *s++;

    if( *s++ != ':' )
    {
        return false;
    }

    switch( opt )
    {
    case 't':

        s_tests = s;
        break;

    case 'i':

        s_impls = s;
        break;

    case 'n':

        {
            int k = std::atoi( s );

            if( k > 0 )
            {
                s_trials = k;
            }
            else
            {
                return false;
            }
        }

        break;

    case 'b':
        s_branch = s;
        break;
    }

    return true;
}

static bool add_impl( impl_list & vi, char impl )
{
    switch( impl )
    {
    case 'b':

        vi.emplace_back(new boost_pool_impl(s_branch));
        break;

    case 'd':

        vi.emplace_back(new boost_default_impl(s_branch));
        break;

    case 'u':

        vi.emplace_back(new boost_null_impl(s_branch));
        break;

    case 'r':

        vi.emplace_back(new rapidjson_memory_impl);
        break;

    case 'c':

        vi.emplace_back(new rapidjson_crt_impl);
        break;

    case 'n':

        vi.emplace_back(new nlohmann_impl);
        break;

    default:

        std::cerr << "Unknown implementation: '" << impl << "'\n";
        return false;
    }

    return true;
}

static bool do_test( file_list const & vf, impl_list const & vi, char test )
{
    switch( test )
    {
    case 'p':

        bench("Parse", vf, vi, s_trials);
        break;

    case 's':

        bench("Serialize", vf, vi, s_trials);
        break;

    default:

        std::cerr << "Unknown test type: '" << test << "'\n";
        return false;
    }

    return true;
}

int
main(
    int const argc,
    char const* const* const argv)
{
    if( argc < 2 )
    {
        std::cerr <<
            "Usage: bench [options...] <file>...\n"
            "\n"
            "Options:  -t:[p][s]            Test parsing, serialization or both\n"
			"                                 (default both)\n"
            "          -i:[b][d][r][c][n]   Test the specified implementations\n"
            "                                 (b: Boost.JSON, pool storage)\n"
            "                                 (d: Boost.JSON, default storage)\n"
            "                                 (u: Boost.JSON, null parser)\n"
            "                                 (r: RapidJSON, memory storage)\n"
            "                                 (c: RapidJSON, CRT storage)\n"
            "                                 (n: nlohmann/json)\n"
			"                                 (default all)\n"
            "          -n:<number>          Number of trials (default 6)\n"
            "          -b:<branch>          Branch label for boost implementations\n"
        ;

        return 4;
    }

    file_list vf;

    for( int i = 1; i < argc; ++i )
    {
        char const * s = argv[ i ];

        if( *s == '-' )
        {
            if( !parse_option( s+1 ) )
            {
                std::cerr << "Unrecognized or incorrect option: '" << s << "'\n";
            }
        }
        else
        {
            vf.emplace_back( file_item{ argv[i], load_file( s ) } );
        }
    }

    try
    {
/*
        strings.json integers-32.json integers-64.json twitter.json small.json array.json random.json citm_catalog.json canada.json
*/
        impl_list vi;

        for( char ch: s_impls )
        {
            add_impl( vi, ch );
        }

        for( char ch: s_tests )
        {
            do_test( vf, vi, ch );
        }

        dout << "\n" << strout.str();
    }
    catch(system_error const& se)
    {
        dout << se.what() << std::endl;
    }

    return 0;
}
