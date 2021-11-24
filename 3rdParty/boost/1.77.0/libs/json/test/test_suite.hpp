//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef TEST_SUITE_HPP
#define TEST_SUITE_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <debugapi.h>
#endif

//  This is a derivative work
//  Copyright 2002-2018 Peter Dimov
//  Copyright (c) 2002, 2009, 2014 Peter Dimov
//  Copyright (2) Beman Dawes 2010, 2011
//  Copyright (3) Ion Gaztanaga 2013
//
//  Copyright 2018 Glen Joseph Fernandes
//  (glenjofe@gmail.com)

namespace test_suite {

//----------------------------------------------------------

#ifdef _MSC_VER

namespace detail {

template<
    class CharT, class Traits, class Allocator>
class debug_streambuf
    : public std::basic_stringbuf<
        CharT, Traits, Allocator>
{
    using ostream =
        std::basic_ostream<CharT, Traits>;

    ostream& os_;
    bool dbg_;

    template<class T>
    void write(T const*) = delete;

    void write(char const* s)
    {
        if(dbg_)
            ::OutputDebugStringA(s);
        os_ << s;
    }

    void write(wchar_t const* s)
    {
        if(dbg_)
            ::OutputDebugStringW(s);
        os_ << s;
    }

public:
    explicit
    debug_streambuf(ostream& os)
        : os_(os)
        , dbg_(::IsDebuggerPresent() != 0)
    {
    }

    ~debug_streambuf()
    {
        sync();
    }

    int
    sync() override
    {
        write(this->str().c_str());
        this->str("");
        return 0;
    }
};

/** std::ostream with Visual Studio IDE redirection.

    Instances of this stream wrap a specified `std::ostream`
    (such as `std::cout` or `std::cerr`). If the IDE debugger
    is attached when the stream is created, output will be
    additionally copied to the Visual Studio Output window.
*/
template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>
>
class basic_debug_stream
    : public std::basic_ostream<CharT, Traits>
{
    detail::debug_streambuf<
        CharT, Traits, Allocator> buf_;

public:
    /** Construct a stream.

        @param os The output stream to wrap.
    */
    explicit
    basic_debug_stream(std::ostream& os)
        : std::basic_ostream<CharT, Traits>(&buf_)
        , buf_(os)
    {
        if(os.flags() & std::ios::unitbuf)
            std::unitbuf(*this);
    }
};

} // detail

using debug_stream  = detail::basic_debug_stream<char>;
using debug_wstream = detail::basic_debug_stream<wchar_t>;

#else

using debug_stream  = std::ostream&;
using debug_wstream = std::wostream&;

#endif

//----------------------------------------------------------

namespace detail {

struct suite_info
{
    char const* name;
    void (*run)();
};

class runner;

inline
runner*&
current() noexcept;

inline
void
debug_break()
{
#ifdef _MSC_VER
    ::DebugBreak();
#else
    //?? profit?
#endif
}

//----------------------------------------------------------

struct checkpoint
{
    char const* const file;
    int const line;
    std::size_t const id;

    static
    checkpoint*&
    current() noexcept
    {
        static checkpoint* p = nullptr;
        return p;
    }

    checkpoint(
        char const* file_,
        int line_,
        std::size_t id_ = 0)
        : file(file_)
        , line(line_)
        , id([]
        {
            static std::size_t n = 0;
            return ++n;
        }())
        , prev_(current())
    {
        current() = this;
        if(id_ == id)
            debug_break();
    }

    ~checkpoint()
    {
        current() = prev_;
    }

private:
    checkpoint* prev_;
};

//----------------------------------------------------------

class runner
{
    runner* saved_;

public:
    runner() noexcept
        : saved_(current())
    {
        current() = this;
    }

    virtual
    ~runner()
    {
        current() = saved_;
    }

    virtual void on_begin(char const* name) = 0;
    virtual void on_end() = 0;

    virtual void note(char const* msg) = 0;
    virtual void pass(char const* expr, char const* file, int line, char const* func) = 0;
    virtual void fail(char const* expr, char const* file, int line, char const* func) = 0;

    template<class Bool
#if 0
        ,class = typename std::enable_if<
            std::is_convertible<bool, Bool>::Value>::type
#endif
    >
    bool
    maybe_fail(
        Bool cond,
        char const* expr,
        char const* file,
        int line,
        char const* func)
    {
        if(!!cond)
        {
            pass(expr, file, line, func);
            return true;
        }
        fail(expr, file, line, func);
        return false;
    }

    void
    run(suite_info const& si)
    {
        on_begin(si.name);
        si.run();
        on_end();
    }
};

runner*&
current() noexcept
{
    static runner* p = nullptr;
    return p;
}

//----------------------------------------------------------

using clock_type =
    std::chrono::steady_clock;

struct elapsed
{
    clock_type::duration d;
};

inline
std::ostream&
operator<<(
    std::ostream& os,
    elapsed const& e)
{
    using namespace std::chrono;
    auto const ms = duration_cast<
        milliseconds>(e.d);
    if(ms < seconds{1})
    {
        os << ms.count() << "ms";
    }
    else
    {
        std::streamsize width{
            os.width()};
        std::streamsize precision{
            os.precision()};
        os << std::fixed <<
            std::setprecision(1) <<
            (ms.count() / 1000.0) << "s";
        os.precision(precision);
        os.width(width);
    }
    return os;
}

class simple_runner : public runner
{
    struct summary
    {
        char const* name;
        clock_type::time_point start;
        clock_type::duration elapsed;
        std::atomic<std::size_t> failed;
        std::atomic<std::size_t> total;

        summary(summary const& other) noexcept
            : name(other.name)
            , start(other.start)
            , elapsed(other.elapsed)
            , failed(other.failed.load())
            , total(other.total.load())
        {
        }

        explicit
        summary(char const* name_) noexcept
            : name(name_)
            , start(clock_type::now())
            , failed(0)
            , total(0)
        {
        }
    };

    summary all_;
    std::ostream& log_;
    std::vector<summary> v_;

public:
    explicit
    simple_runner(
        std::ostream& log)
        : all_("(all)")
        , log_(log)
    {
        std::unitbuf(log_);
        v_.reserve(256);
    }

    ~simple_runner()
    {
        log_ <<
            elapsed{clock_type::now() -
                all_.start } << ", " <<
            v_.size() << " suites, " <<
            all_.failed.load() << " failures, " <<
            all_.total.load() << " total." <<
                std::endl;
    }

    // true if no failures
    bool
    success() const noexcept
    {
        return all_.failed.load() == 0;
    }

    void
    on_begin(char const* name) override
    {
        v_.emplace_back(name);
        log_ << name << "\n";
    }

    void
    on_end() override
    {
        v_.back().elapsed =
            clock_type::now() -
            v_.back().start;
    }

    void
    note(char const* msg) override
    {
        log_ << msg << "\n";
    }

    char const*
    filename(
        char const* file)
    {
        auto const p0 = file;
        auto p = p0 + std::strlen(file);
        while(p-- != p0)
        {
        #ifdef _MSC_VER
            if(*p == '\\')
        #else
            if(*p == '/')
        #endif
                break;
        }
        return p + 1;
    }

    void
    pass(
        char const*,
        char const*,
        int,
        char const*) override
    {
        ++all_.total;
        ++v_.back().total;
    }

    void
    fail(
        char const* expr,
        char const* file,
        int line,
        char const*) override
    {
        ++all_.failed;
        ++v_.back().total;
        ++v_.back().failed;
        auto const id = ++all_.total;
        auto const cp =
            checkpoint::current();
        if(cp)
            log_ <<
                "case " << cp->id <<
                "(#" << id << ") " <<
                filename(cp->file) <<
                "(" << cp->line << ") "
                "failed: " << expr << "\n";
        else
            log_ <<
                "#" << id <<
                " " << filename(file) <<
                "(" << line << ") "
                "failed: " << expr << "\n";
    }
};

//----------------------------------------------------------

template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>
>
class log_ostream
    : public std::basic_ostream<CharT, Traits>
{
    struct buffer_type :
        std::basic_stringbuf<
            CharT, Traits, Allocator>
    {
        ~buffer_type()
        {
            sync();
        }

        int
        sync() override
        {
            auto const& s = this->str();
            if(s.size() > 0)
                current()->note(s.c_str());
            this->str("");
            return 0;
        }
    };

    buffer_type buf_;

public:
    log_ostream()
        : std::basic_ostream<
            CharT, Traits>(&buf_)
    {
    }
};

//----------------------------------------------------------

class suite_list
{
    static std::size_t const max_size_ = 256;
    std::size_t count_ = 0;
    suite_info data_[max_size_];

public:
    void
    insert(suite_info inf)
    {
    #ifndef BOOST_NO_EXCEPTIONS
        if(count_ == max_size_)
            throw std::length_error(
                "too many test suites");
    #endif
        data_[count_++] = inf;
    }

    suite_info const*
    begin() const noexcept
    {
        return data_;
    }

    suite_info const*
    end() const noexcept
    {
        return begin() + count_;
    }

    void
    sort()
    {
        std::sort(
            data_,
            data_ + count_,
            []( detail::suite_info const& lhs,
                detail::suite_info const& rhs)
            {
                return std::lexicographical_compare(
                    lhs.name, lhs.name + std::strlen(lhs.name),
                    rhs.name, rhs.name + std::strlen(rhs.name));
            });
    }
};

inline
suite_list&
suites()
{
    static suite_list list;
    return list;
}

//----------------------------------------------------------

template<class T>
struct instance
{
    explicit
    instance(
        char const* name) noexcept
    {
        suites().insert(suite_info{
            name, &instance::run });
    }

    static
    void
    run()
    {
        auto const saved =
            current();
        //current()->os_
        T().run();
        current() = saved;
    }
};

//----------------------------------------------------------

inline
void
current_function_helper()
{
#if  defined(__GNUC__) || \
    (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || \
    (defined(__ICC) && (__ICC >= 600)) || \
        defined(__ghs__) || \
        defined(__clang__)
# define TEST_SUITE_FUNCTION __PRETTY_FUNCTION__

#elif defined(__DMC__) && (__DMC__ >= 0x810)
# define TEST_SUITE_FUNCTION __PRETTY_FUNCTION__

#elif defined(__FUNCSIG__)
# define TEST_SUITE_FUNCTION __FUNCSIG__

#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || \
        (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
# define TEST_SUITE_FUNCTION __FUNCTION__

#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
# define TEST_SUITE_FUNCTION __FUNC__

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
# define TEST_SUITE_FUNCTION __func__

#elif defined(__cplusplus) && (__cplusplus >= 201103)
# define TEST_SUITE_FUNCTION __func__

#else
# define TEST_SUITE_FUNCTION "(unknown)"

#endif
}

} // detail

//----------------------------------------------------------

/** The type of log for output to the currently running suite.
*/
using log_type = detail::log_ostream<char>;

#define BOOST_TEST_CHECKPOINT(...) \
    ::test_suite::detail::checkpoint \
        _BOOST_TEST_CHECKPOINT ## __LINE__ ( \
            __FILE__, __LINE__, __VA_ARGS__ + 0)

#define BOOST_TEST(expr) \
    ::test_suite::detail::current()->maybe_fail( \
        (expr), #expr, __FILE__, __LINE__, TEST_SUITE_FUNCTION)

#define BOOST_ERROR(msg) \
    ::test_suite::detail::current()->fail( \
        msg, __FILE__, __LINE__, TEST_SUITE_FUNCTION)

#define BOOST_TEST_PASS() \
    ::test_suite::detail::current()->pass( \
        "", __FILE__, __LINE__, TEST_SUITE_FUNCTION)

#define BOOST_TEST_FAIL() \
    ::test_suite::detail::current()->fail( \
        "", __FILE__, __LINE__, TEST_SUITE_FUNCTION)

#define BOOST_TEST_NOT(expr) BOOST_TEST(!(expr))

#ifndef BOOST_NO_EXCEPTIONS
# define BOOST_TEST_THROWS( expr, except ) \
    try { \
        (void)(expr); \
        ::test_suite::detail::current()->fail ( \
            #except, __FILE__, __LINE__, \
            TEST_SUITE_FUNCTION); \
    } \
    catch(except const&) { \
        ::test_suite::detail::current()->pass( \
            #except, __FILE__, __LINE__, \
            TEST_SUITE_FUNCTION); \
    } \
    catch(...) { \
        ::test_suite::detail::current()->fail( \
            #except, __FILE__, __LINE__, \
            TEST_SUITE_FUNCTION); \
    }

#else
   #define BOOST_TEST_THROWS( expr, except )
#endif

#define TEST_SUITE(type, name) \
    static ::test_suite::detail::instance<type> type##_(name)

inline
int
run(std::ostream& log,
    int argc, char const* const* argv)
{
    if(argc == 2)
    {
        std::string arg(argv[1]);
        if(arg == "-h" || arg == "--help")
        {
            log <<
                "Usage:\n"
                "  " << argv[0] << ": { <suite-name>... }" <<
                std::endl;
            return EXIT_SUCCESS;
        }
    }

    using namespace ::test_suite;

    detail::simple_runner runner(log);
    detail::suites().sort();
    if(argc == 1)
    {
        for(auto const& e : detail::suites())
            runner.run(e);
    }
    else
    {
        std::vector<std::string> args;
        args.reserve(argc - 1);
        for(int i = 0; i < argc - 1; ++i)
            args.push_back(argv[i + 1]);
        for(auto const& e : detail::suites())
        {
            std::string s(e.name);
            if(std::find_if(
                args.begin(), args.end(),
                [&](std::string const& arg)
                {
                    if(arg.size() > s.size())
                        return false;
                    return s.compare(
                        s.size() - arg.size(),
                        arg.size(),
                        arg.data(),
                        arg.size()) == 0;
                }) != args.end())
            {
                runner.run(e);
            }
        }
    }
    return runner.success() ?
        EXIT_SUCCESS : EXIT_FAILURE;
}

} // test_suite

#endif
