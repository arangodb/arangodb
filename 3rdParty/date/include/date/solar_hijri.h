#ifndef SOLAR_HIJRI_H
#define SOLAR_HIJRI_H

// The MIT License (MIT)
//
// Copyright (c) 2016 Howard Hinnant
// Copyright (c) 2019 Asad. Gharighi
//
// Calculations are based on:
// https://www.timeanddate.com/calendar/persian-calendar.html
// and follow <date.h> style
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Our apologies.  When the previous paragraph was written, lowercase had not yet
// been invented (that would involve another several millennia of evolution).
// We did not mean to shout.

#include "date.h"

namespace solar_hijri
{

namespace internal
{
static const auto epoch = static_cast<unsigned>(2121446);
static const auto days_in_era = static_cast<unsigned>(1029983);
static const auto years_in_era = static_cast<unsigned>(2820);
static const auto unix_time_shift = static_cast<unsigned>(2440588);
auto const years_in_first_cycle = static_cast<unsigned>(29);
auto const years_in_other_cycles = static_cast<int>(33);
auto const years_in_period = static_cast<int>(128);        // 29   + 3*33
auto const days_in_first_cycle = static_cast<unsigned>(10592);  // 28/4 + 29*365
auto const days_in_other_cycles = static_cast<unsigned>(12053); // 32/4 + 33*365
auto const days_in_period = static_cast<unsigned>(46751);       // days_in_first_cycle + 3*days_in_other_cycles;
}

// durations

using days = date::days;

using weeks = date::weeks;

using years = std::chrono::duration
              <int, date::detail::ratio_multiply<std::ratio<internal::days_in_era, internal::years_in_era>, days::period>>;

using months = std::chrono::duration
    <int, date::detail::ratio_divide<years::period, std::ratio<12>>>;

// time_point

using sys_days   = date::sys_days;
using local_days = date::local_days;

// types

struct last_spec
{
    explicit last_spec() = default;
};

class day;
class month;
class year;

class weekday;
class weekday_indexed;
class weekday_last;

class month_day;
class month_day_last;
class month_weekday;
class month_weekday_last;

class year_month;

class year_month_day;
class year_month_day_last;
class year_month_weekday;
class year_month_weekday_last;

// date composition operators

CONSTCD11 year_month operator/(const year& y, const month& m) NOEXCEPT;
CONSTCD11 year_month operator/(const year& y, int          m) NOEXCEPT;

CONSTCD11 month_day operator/(const day& d, const month& m) NOEXCEPT;
CONSTCD11 month_day operator/(const day& d, int          m) NOEXCEPT;
CONSTCD11 month_day operator/(const month& m, const day& d) NOEXCEPT;
CONSTCD11 month_day operator/(const month& m, int        d) NOEXCEPT;
CONSTCD11 month_day operator/(int          m, const day& d) NOEXCEPT;

CONSTCD11 month_day_last operator/(const month& m, last_spec) NOEXCEPT;
CONSTCD11 month_day_last operator/(int          m, last_spec) NOEXCEPT;
CONSTCD11 month_day_last operator/(last_spec, const month& m) NOEXCEPT;
CONSTCD11 month_day_last operator/(last_spec, int          m) NOEXCEPT;

CONSTCD11 month_weekday operator/(const month& m, const weekday_indexed& wdi) NOEXCEPT;
CONSTCD11 month_weekday operator/(int          m, const weekday_indexed& wdi) NOEXCEPT;
CONSTCD11 month_weekday operator/(const weekday_indexed& wdi, const month& m) NOEXCEPT;
CONSTCD11 month_weekday operator/(const weekday_indexed& wdi, int          m) NOEXCEPT;

CONSTCD11 month_weekday_last operator/(const month& m, const weekday_last& wdl) NOEXCEPT;
CONSTCD11 month_weekday_last operator/(int          m, const weekday_last& wdl) NOEXCEPT;
CONSTCD11 month_weekday_last operator/(const weekday_last& wdl, const month& m) NOEXCEPT;
CONSTCD11 month_weekday_last operator/(const weekday_last& wdl, int          m) NOEXCEPT;

CONSTCD11 year_month_day operator/(const year_month& ym, const day& d) NOEXCEPT;
CONSTCD11 year_month_day operator/(const year_month& ym, int        d) NOEXCEPT;
CONSTCD11 year_month_day operator/(const year& y, const month_day& md) NOEXCEPT;
CONSTCD11 year_month_day operator/(int         y, const month_day& md) NOEXCEPT;
CONSTCD11 year_month_day operator/(const month_day& md, const year& y) NOEXCEPT;
CONSTCD11 year_month_day operator/(const month_day& md, int         y) NOEXCEPT;

CONSTCD11
    year_month_day_last operator/(const year_month& ym,   last_spec) NOEXCEPT;
CONSTCD11
    year_month_day_last operator/(const year& y, const month_day_last& mdl) NOEXCEPT;
CONSTCD11
    year_month_day_last operator/(int         y, const month_day_last& mdl) NOEXCEPT;
CONSTCD11
    year_month_day_last operator/(const month_day_last& mdl, const year& y) NOEXCEPT;
CONSTCD11
    year_month_day_last operator/(const month_day_last& mdl, int         y) NOEXCEPT;

CONSTCD11
year_month_weekday
operator/(const year_month& ym, const weekday_indexed& wdi) NOEXCEPT;

CONSTCD11
year_month_weekday
operator/(const year&        y, const month_weekday&   mwd) NOEXCEPT;

CONSTCD11
year_month_weekday
operator/(int                y, const month_weekday&   mwd) NOEXCEPT;

CONSTCD11
year_month_weekday
operator/(const month_weekday& mwd, const year&          y) NOEXCEPT;

CONSTCD11
year_month_weekday
operator/(const month_weekday& mwd, int                  y) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator/(const year_month& ym, const weekday_last& wdl) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator/(const year& y, const month_weekday_last& mwdl) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator/(int         y, const month_weekday_last& mwdl) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator/(const month_weekday_last& mwdl, const year& y) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator/(const month_weekday_last& mwdl, int         y) NOEXCEPT;

// Detailed interface

// day

class day
{
    unsigned char d_;

public:
    day() = default;
    explicit CONSTCD11 day(unsigned d) NOEXCEPT;

    CONSTCD14 day& operator++()    NOEXCEPT;
    CONSTCD14 day  operator++(int) NOEXCEPT;
    CONSTCD14 day& operator--()    NOEXCEPT;
    CONSTCD14 day  operator--(int) NOEXCEPT;

    CONSTCD14 day& operator+=(const days& d) NOEXCEPT;
    CONSTCD14 day& operator-=(const days& d) NOEXCEPT;

    CONSTCD11 explicit operator unsigned() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const day& x, const day& y) NOEXCEPT;
CONSTCD11 bool operator!=(const day& x, const day& y) NOEXCEPT;
CONSTCD11 bool operator< (const day& x, const day& y) NOEXCEPT;
CONSTCD11 bool operator> (const day& x, const day& y) NOEXCEPT;
CONSTCD11 bool operator<=(const day& x, const day& y) NOEXCEPT;
CONSTCD11 bool operator>=(const day& x, const day& y) NOEXCEPT;

CONSTCD11 day  operator+(const day&  x, const days& y) NOEXCEPT;
CONSTCD11 day  operator+(const days& x, const day&  y) NOEXCEPT;
CONSTCD11 day  operator-(const day&  x, const days& y) NOEXCEPT;
CONSTCD11 days operator-(const day&  x, const day&  y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const day& d);

// month

class month
{
    unsigned char m_;

public:
    month() = default;
    explicit CONSTCD11 month(unsigned m) NOEXCEPT;

    CONSTCD14 month& operator++()    NOEXCEPT;
    CONSTCD14 month  operator++(int) NOEXCEPT;
    CONSTCD14 month& operator--()    NOEXCEPT;
    CONSTCD14 month  operator--(int) NOEXCEPT;

    CONSTCD14 month& operator+=(const months& m) NOEXCEPT;
    CONSTCD14 month& operator-=(const months& m) NOEXCEPT;

    CONSTCD11 explicit operator unsigned() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const month& x, const month& y) NOEXCEPT;
CONSTCD11 bool operator!=(const month& x, const month& y) NOEXCEPT;
CONSTCD11 bool operator< (const month& x, const month& y) NOEXCEPT;
CONSTCD11 bool operator> (const month& x, const month& y) NOEXCEPT;
CONSTCD11 bool operator<=(const month& x, const month& y) NOEXCEPT;
CONSTCD11 bool operator>=(const month& x, const month& y) NOEXCEPT;

CONSTCD14 month  operator+(const month&  x, const months& y) NOEXCEPT;
CONSTCD14 month  operator+(const months& x,  const month& y) NOEXCEPT;
CONSTCD14 month  operator-(const month&  x, const months& y) NOEXCEPT;
CONSTCD14 months operator-(const month&  x,  const month& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month& m);

// year

class year
{
    short y_;

public:
    year() = default;
    explicit CONSTCD11 year(int y) NOEXCEPT;

    CONSTCD14 year& operator++()    NOEXCEPT;
    CONSTCD14 year  operator++(int) NOEXCEPT;
    CONSTCD14 year& operator--()    NOEXCEPT;
    CONSTCD14 year  operator--(int) NOEXCEPT;

    CONSTCD14 year& operator+=(const years& y) NOEXCEPT;
    CONSTCD14 year& operator-=(const years& y) NOEXCEPT;

    CONSTCD14 bool is_leap() const NOEXCEPT;

    CONSTCD11 explicit operator int() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;

    static CONSTCD11 year min() NOEXCEPT;
    static CONSTCD11 year max() NOEXCEPT;
};

CONSTCD11 bool operator==(const year& x, const year& y) NOEXCEPT;
CONSTCD11 bool operator!=(const year& x, const year& y) NOEXCEPT;
CONSTCD11 bool operator< (const year& x, const year& y) NOEXCEPT;
CONSTCD11 bool operator> (const year& x, const year& y) NOEXCEPT;
CONSTCD11 bool operator<=(const year& x, const year& y) NOEXCEPT;
CONSTCD11 bool operator>=(const year& x, const year& y) NOEXCEPT;

CONSTCD11 year  operator+(const year&  x, const years& y) NOEXCEPT;
CONSTCD11 year  operator+(const years& x, const year&  y) NOEXCEPT;
CONSTCD11 year  operator-(const year&  x, const years& y) NOEXCEPT;
CONSTCD11 years operator-(const year&  x, const year&  y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year& y);

// weekday

class weekday
{
    unsigned char wd_;
public:
    weekday() = default;
    explicit CONSTCD11 weekday(unsigned wd) NOEXCEPT;
    explicit weekday(int) = delete;
    CONSTCD11 weekday(const sys_days& dp) NOEXCEPT;
    CONSTCD11 explicit weekday(const local_days& dp) NOEXCEPT;

    CONSTCD14 weekday& operator++()    NOEXCEPT;
    CONSTCD14 weekday  operator++(int) NOEXCEPT;
    CONSTCD14 weekday& operator--()    NOEXCEPT;
    CONSTCD14 weekday  operator--(int) NOEXCEPT;

    CONSTCD14 weekday& operator+=(const days& d) NOEXCEPT;
    CONSTCD14 weekday& operator-=(const days& d) NOEXCEPT;

    CONSTCD11 explicit operator unsigned() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;

    CONSTCD11 weekday_indexed operator[](unsigned index) const NOEXCEPT;
    CONSTCD11 weekday_last    operator[](last_spec)      const NOEXCEPT;

private:
    static CONSTCD11 unsigned char weekday_from_days(int z) NOEXCEPT;
};

CONSTCD11 bool operator==(const weekday& x, const weekday& y) NOEXCEPT;
CONSTCD11 bool operator!=(const weekday& x, const weekday& y) NOEXCEPT;

CONSTCD14 weekday operator+(const weekday& x, const days&    y) NOEXCEPT;
CONSTCD14 weekday operator+(const days&    x, const weekday& y) NOEXCEPT;
CONSTCD14 weekday operator-(const weekday& x, const days&    y) NOEXCEPT;
CONSTCD14 days    operator-(const weekday& x, const weekday& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const weekday& wd);

// weekday_indexed

class weekday_indexed
{
    unsigned char wd_    : 4;
    unsigned char index_ : 4;

public:
    weekday_indexed() = default;
    CONSTCD11 weekday_indexed(const solar_hijri::weekday& wd, unsigned index) NOEXCEPT;

    CONSTCD11 solar_hijri::weekday weekday() const NOEXCEPT;
    CONSTCD11 unsigned index() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const weekday_indexed& x, const weekday_indexed& y) NOEXCEPT;
CONSTCD11 bool operator!=(const weekday_indexed& x, const weekday_indexed& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const weekday_indexed& wdi);

// weekday_last

class weekday_last
{
    solar_hijri::weekday wd_;

public:
    weekday_last() = default;
    explicit CONSTCD11 weekday_last(const solar_hijri::weekday& wd) NOEXCEPT;

    CONSTCD11 solar_hijri::weekday weekday() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const weekday_last& x, const weekday_last& y) NOEXCEPT;
CONSTCD11 bool operator!=(const weekday_last& x, const weekday_last& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const weekday_last& wdl);

// year_month

class year_month
{
    solar_hijri::year  y_;
    solar_hijri::month m_;

public:
    year_month() = default;
    CONSTCD11 year_month(const solar_hijri::year& y, const solar_hijri::month& m) NOEXCEPT;

    CONSTCD11 solar_hijri::year  year()  const NOEXCEPT;
    CONSTCD11 solar_hijri::month month() const NOEXCEPT;

    CONSTCD14 year_month& operator+=(const months& dm) NOEXCEPT;
    CONSTCD14 year_month& operator-=(const months& dm) NOEXCEPT;
    CONSTCD14 year_month& operator+=(const years& dy) NOEXCEPT;
    CONSTCD14 year_month& operator-=(const years& dy) NOEXCEPT;

    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const year_month& x, const year_month& y) NOEXCEPT;
CONSTCD11 bool operator!=(const year_month& x, const year_month& y) NOEXCEPT;
CONSTCD11 bool operator< (const year_month& x, const year_month& y) NOEXCEPT;
CONSTCD11 bool operator> (const year_month& x, const year_month& y) NOEXCEPT;
CONSTCD11 bool operator<=(const year_month& x, const year_month& y) NOEXCEPT;
CONSTCD11 bool operator>=(const year_month& x, const year_month& y) NOEXCEPT;

CONSTCD14 year_month operator+(const year_month& ym, const months& dm) NOEXCEPT;
CONSTCD14 year_month operator+(const months& dm, const year_month& ym) NOEXCEPT;
CONSTCD14 year_month operator-(const year_month& ym, const months& dm) NOEXCEPT;

CONSTCD11 months operator-(const year_month& x, const year_month& y) NOEXCEPT;
CONSTCD11 year_month operator+(const year_month& ym, const years& dy) NOEXCEPT;
CONSTCD11 year_month operator+(const years& dy, const year_month& ym) NOEXCEPT;
CONSTCD11 year_month operator-(const year_month& ym, const years& dy) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month& ym);

// month_day

class month_day
{
    solar_hijri::month m_;
    solar_hijri::day   d_;

public:
    month_day() = default;
    CONSTCD11 month_day(const solar_hijri::month& m, const solar_hijri::day& d) NOEXCEPT;

    CONSTCD11 solar_hijri::month month() const NOEXCEPT;
    CONSTCD11 solar_hijri::day   day() const NOEXCEPT;

    CONSTCD14 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const month_day& x, const month_day& y) NOEXCEPT;
CONSTCD11 bool operator!=(const month_day& x, const month_day& y) NOEXCEPT;
CONSTCD11 bool operator< (const month_day& x, const month_day& y) NOEXCEPT;
CONSTCD11 bool operator> (const month_day& x, const month_day& y) NOEXCEPT;
CONSTCD11 bool operator<=(const month_day& x, const month_day& y) NOEXCEPT;
CONSTCD11 bool operator>=(const month_day& x, const month_day& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_day& md);

// month_day_last

class month_day_last
{
    solar_hijri::month m_;

public:
    month_day_last() = default;
    CONSTCD11 explicit month_day_last(const solar_hijri::month& m) NOEXCEPT;

    CONSTCD11 solar_hijri::month month() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const month_day_last& x, const month_day_last& y) NOEXCEPT;
CONSTCD11 bool operator!=(const month_day_last& x, const month_day_last& y) NOEXCEPT;
CONSTCD11 bool operator< (const month_day_last& x, const month_day_last& y) NOEXCEPT;
CONSTCD11 bool operator> (const month_day_last& x, const month_day_last& y) NOEXCEPT;
CONSTCD11 bool operator<=(const month_day_last& x, const month_day_last& y) NOEXCEPT;
CONSTCD11 bool operator>=(const month_day_last& x, const month_day_last& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_day_last& mdl);

// month_weekday

class month_weekday
{
    solar_hijri::month           m_;
    solar_hijri::weekday_indexed wdi_;
public:
    month_weekday() = default;
    CONSTCD11 month_weekday(const solar_hijri::month& m,
                            const solar_hijri::weekday_indexed& wdi) NOEXCEPT;

    CONSTCD11 solar_hijri::month           month()           const NOEXCEPT;
    CONSTCD11 solar_hijri::weekday_indexed weekday_indexed() const NOEXCEPT;

    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11 bool operator==(const month_weekday& x, const month_weekday& y) NOEXCEPT;
CONSTCD11 bool operator!=(const month_weekday& x, const month_weekday& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_weekday& mwd);

// month_weekday_last

class month_weekday_last
{
    solar_hijri::month        m_;
    solar_hijri::weekday_last wdl_;

public:
    month_weekday_last() = default;
    CONSTCD11 month_weekday_last(const solar_hijri::month& m,
                                 const solar_hijri::weekday_last& wd) NOEXCEPT;

    CONSTCD11 solar_hijri::month        month()        const NOEXCEPT;
    CONSTCD11 solar_hijri::weekday_last weekday_last() const NOEXCEPT;

    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11
    bool operator==(const month_weekday_last& x, const month_weekday_last& y) NOEXCEPT;
CONSTCD11
    bool operator!=(const month_weekday_last& x, const month_weekday_last& y) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_weekday_last& mwdl);

// class year_month_day

class year_month_day
{
    solar_hijri::year  y_;
    solar_hijri::month m_;
    solar_hijri::day   d_;

public:
    year_month_day() = default;
    CONSTCD11 year_month_day(const solar_hijri::year& y, const solar_hijri::month& m,
                             const solar_hijri::day& d) NOEXCEPT;
    CONSTCD14 year_month_day(const year_month_day_last& ymdl) NOEXCEPT;

    CONSTCD14 year_month_day(sys_days dp) NOEXCEPT;
    CONSTCD14 explicit year_month_day(local_days dp) NOEXCEPT;

    CONSTCD14 year_month_day& operator+=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_day& operator-=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_day& operator+=(const years& y)  NOEXCEPT;
    CONSTCD14 year_month_day& operator-=(const years& y)  NOEXCEPT;

    CONSTCD11 solar_hijri::year  year()  const NOEXCEPT;
    CONSTCD11 solar_hijri::month month() const NOEXCEPT;
    CONSTCD11 solar_hijri::day   day()   const NOEXCEPT;

    CONSTCD14 operator sys_days() const NOEXCEPT;
    CONSTCD14 explicit operator local_days() const NOEXCEPT;
    CONSTCD14 bool ok() const NOEXCEPT;

private:
  static CONSTCD14 year_month_day from_days(days dp) NOEXCEPT;
  CONSTCD14 days to_days() const NOEXCEPT;
};

CONSTCD11 bool operator==(const year_month_day& x, const year_month_day& y) NOEXCEPT;
CONSTCD11 bool operator!=(const year_month_day& x, const year_month_day& y) NOEXCEPT;
CONSTCD11 bool operator< (const year_month_day& x, const year_month_day& y) NOEXCEPT;
CONSTCD11 bool operator> (const year_month_day& x, const year_month_day& y) NOEXCEPT;
CONSTCD11 bool operator<=(const year_month_day& x, const year_month_day& y) NOEXCEPT;
CONSTCD11 bool operator>=(const year_month_day& x, const year_month_day& y) NOEXCEPT;

CONSTCD14 year_month_day operator+(const year_month_day& ymd, const months& dm) NOEXCEPT;
CONSTCD14 year_month_day operator+(const months& dm, const year_month_day& ymd) NOEXCEPT;
CONSTCD14 year_month_day operator-(const year_month_day& ymd, const months& dm) NOEXCEPT;
CONSTCD11 year_month_day operator+(const year_month_day& ymd, const years& dy)  NOEXCEPT;
CONSTCD11 year_month_day operator+(const years& dy, const year_month_day& ymd)  NOEXCEPT;
CONSTCD11 year_month_day operator-(const year_month_day& ymd, const years& dy)  NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_day& ymd);

// year_month_day_last

class year_month_day_last
{
    solar_hijri::year           y_;
    solar_hijri::month_day_last mdl_;

public:
    year_month_day_last() = default;
    CONSTCD11 year_month_day_last(const solar_hijri::year& y,
                                  const solar_hijri::month_day_last& mdl) NOEXCEPT;

    CONSTCD14 year_month_day_last& operator+=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_day_last& operator-=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_day_last& operator+=(const years& y)  NOEXCEPT;
    CONSTCD14 year_month_day_last& operator-=(const years& y)  NOEXCEPT;

    CONSTCD11 solar_hijri::year           year()           const NOEXCEPT;
    CONSTCD11 solar_hijri::month          month()          const NOEXCEPT;
    CONSTCD11 solar_hijri::month_day_last month_day_last() const NOEXCEPT;
    CONSTCD14 solar_hijri::day            day()            const NOEXCEPT;

    CONSTCD14 operator sys_days() const NOEXCEPT;
    CONSTCD14 explicit operator local_days() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;
};

CONSTCD11
    bool operator==(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT;
CONSTCD11
    bool operator!=(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT;
CONSTCD11
    bool operator< (const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT;
CONSTCD11
    bool operator> (const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT;
CONSTCD11
    bool operator<=(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT;
CONSTCD11
    bool operator>=(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT;

CONSTCD14
year_month_day_last
operator+(const year_month_day_last& ymdl, const months& dm) NOEXCEPT;

CONSTCD14
year_month_day_last
operator+(const months& dm, const year_month_day_last& ymdl) NOEXCEPT;

CONSTCD11
year_month_day_last
operator+(const year_month_day_last& ymdl, const years& dy) NOEXCEPT;

CONSTCD11
year_month_day_last
operator+(const years& dy, const year_month_day_last& ymdl) NOEXCEPT;

CONSTCD14
year_month_day_last
operator-(const year_month_day_last& ymdl, const months& dm) NOEXCEPT;

CONSTCD11
year_month_day_last
operator-(const year_month_day_last& ymdl, const years& dy) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_day_last& ymdl);

// year_month_weekday

class year_month_weekday
{
    solar_hijri::year            y_;
    solar_hijri::month           m_;
    solar_hijri::weekday_indexed wdi_;

public:
    year_month_weekday() = default;
    CONSTCD11 year_month_weekday(const solar_hijri::year& y, const solar_hijri::month& m,
                                   const solar_hijri::weekday_indexed& wdi) NOEXCEPT;
    CONSTCD14 year_month_weekday(const sys_days& dp) NOEXCEPT;
    CONSTCD14 explicit year_month_weekday(const local_days& dp) NOEXCEPT;

    CONSTCD14 year_month_weekday& operator+=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_weekday& operator-=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_weekday& operator+=(const years& y)  NOEXCEPT;
    CONSTCD14 year_month_weekday& operator-=(const years& y)  NOEXCEPT;

    CONSTCD11 solar_hijri::year year() const NOEXCEPT;
    CONSTCD11 solar_hijri::month month() const NOEXCEPT;
    CONSTCD11 solar_hijri::weekday weekday() const NOEXCEPT;
    CONSTCD11 unsigned index() const NOEXCEPT;
    CONSTCD11 solar_hijri::weekday_indexed weekday_indexed() const NOEXCEPT;

    CONSTCD14 operator sys_days() const NOEXCEPT;
    CONSTCD14 explicit operator local_days() const NOEXCEPT;
    CONSTCD14 bool ok() const NOEXCEPT;

private:
    static CONSTCD14 year_month_weekday from_days(days dp) NOEXCEPT;
    CONSTCD14 days to_days() const NOEXCEPT;
};

CONSTCD11
    bool operator==(const year_month_weekday& x, const year_month_weekday& y) NOEXCEPT;
CONSTCD11
    bool operator!=(const year_month_weekday& x, const year_month_weekday& y) NOEXCEPT;

CONSTCD14
year_month_weekday
operator+(const year_month_weekday& ymwd, const months& dm) NOEXCEPT;

CONSTCD14
year_month_weekday
operator+(const months& dm, const year_month_weekday& ymwd) NOEXCEPT;

CONSTCD11
year_month_weekday
operator+(const year_month_weekday& ymwd, const years& dy) NOEXCEPT;

CONSTCD11
year_month_weekday
operator+(const years& dy, const year_month_weekday& ymwd) NOEXCEPT;

CONSTCD14
year_month_weekday
operator-(const year_month_weekday& ymwd, const months& dm) NOEXCEPT;

CONSTCD11
year_month_weekday
operator-(const year_month_weekday& ymwd, const years& dy) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_weekday& ymwdi);

// year_month_weekday_last

class year_month_weekday_last
{
    solar_hijri::year y_;
    solar_hijri::month m_;
    solar_hijri::weekday_last wdl_;

public:
    year_month_weekday_last() = default;
    CONSTCD11 year_month_weekday_last(const solar_hijri::year& y, const solar_hijri::month& m,
                                      const solar_hijri::weekday_last& wdl) NOEXCEPT;

    CONSTCD14 year_month_weekday_last& operator+=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_weekday_last& operator-=(const months& m) NOEXCEPT;
    CONSTCD14 year_month_weekday_last& operator+=(const years& y) NOEXCEPT;
    CONSTCD14 year_month_weekday_last& operator-=(const years& y) NOEXCEPT;

    CONSTCD11 solar_hijri::year year() const NOEXCEPT;
    CONSTCD11 solar_hijri::month month() const NOEXCEPT;
    CONSTCD11 solar_hijri::weekday weekday() const NOEXCEPT;
    CONSTCD11 solar_hijri::weekday_last weekday_last() const NOEXCEPT;

    CONSTCD14 operator sys_days() const NOEXCEPT;
    CONSTCD14 explicit operator local_days() const NOEXCEPT;
    CONSTCD11 bool ok() const NOEXCEPT;

private:
    CONSTCD14 days to_days() const NOEXCEPT;
};

CONSTCD11
bool
operator==(const year_month_weekday_last& x, const year_month_weekday_last& y) NOEXCEPT;

CONSTCD11
bool
operator!=(const year_month_weekday_last& x, const year_month_weekday_last& y) NOEXCEPT;

CONSTCD14
year_month_weekday_last
operator+(const year_month_weekday_last& ymwdl, const months& dm) NOEXCEPT;

CONSTCD14
year_month_weekday_last
operator+(const months& dm, const year_month_weekday_last& ymwdl) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator+(const year_month_weekday_last& ymwdl, const years& dy) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator+(const years& dy, const year_month_weekday_last& ymwdl) NOEXCEPT;

CONSTCD14
year_month_weekday_last
operator-(const year_month_weekday_last& ymwdl, const months& dm) NOEXCEPT;

CONSTCD11
year_month_weekday_last
operator-(const year_month_weekday_last& ymwdl, const years& dy) NOEXCEPT;

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_weekday_last& ymwdl);

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
inline namespace literals
{

CONSTCD11 solar_hijri::day  operator "" _d(unsigned long long d) NOEXCEPT;
CONSTCD11 solar_hijri::year operator "" _y(unsigned long long y) NOEXCEPT;

}  // inline namespace literals
#endif // !defined(_MSC_VER) || (_MSC_VER >= 1900)

//----------------+
// Implementation |
//----------------+

// day

CONSTCD11 inline day::day(unsigned d) NOEXCEPT : d_(static_cast<unsigned char>(d)) {}
CONSTCD14 inline day& day::operator++() NOEXCEPT {++d_; return *this;}
CONSTCD14 inline day day::operator++(int) NOEXCEPT {auto tmp(*this); ++(*this); return tmp;}
CONSTCD14 inline day& day::operator--() NOEXCEPT {--d_; return *this;}
CONSTCD14 inline day day::operator--(int) NOEXCEPT {auto tmp(*this); --(*this); return tmp;}
CONSTCD14 inline day& day::operator+=(const days& d) NOEXCEPT {*this = *this + d; return *this;}
CONSTCD14 inline day& day::operator-=(const days& d) NOEXCEPT {*this = *this - d; return *this;}
CONSTCD11 inline day::operator unsigned() const NOEXCEPT {return d_;}
CONSTCD11 inline bool day::ok() const NOEXCEPT {return 1 <= d_ && d_ <= 30;}

CONSTCD11
inline
bool
operator==(const day& x, const day& y) NOEXCEPT
{
    return static_cast<unsigned>(x) == static_cast<unsigned>(y);
}

CONSTCD11
inline
bool
operator!=(const day& x, const day& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const day& x, const day& y) NOEXCEPT
{
    return static_cast<unsigned>(x) < static_cast<unsigned>(y);
}

CONSTCD11
inline
bool
operator>(const day& x, const day& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const day& x, const day& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const day& x, const day& y) NOEXCEPT
{
    return !(x < y);
}

CONSTCD11
inline
days
operator-(const day& x, const day& y) NOEXCEPT
{
    return days{static_cast<days::rep>(static_cast<unsigned>(x)
                                     - static_cast<unsigned>(y))};
}

CONSTCD11
inline
day
operator+(const day& x, const days& y) NOEXCEPT
{
    return day{static_cast<unsigned>(x) + static_cast<unsigned>(y.count())};
}

CONSTCD11
inline
day
operator+(const days& x, const day& y) NOEXCEPT
{
    return y + x;
}

CONSTCD11
inline
day
operator-(const day& x, const days& y) NOEXCEPT
{
    return x + -y;
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const day& d)
{
    date::detail::save_ostream<CharT, Traits> _(os);
    os.fill('0');
    os.flags(std::ios::dec | std::ios::right);
    os.width(2);
    os << static_cast<unsigned>(d);
    return os;
}

// month

CONSTCD11 inline month::month(unsigned m) NOEXCEPT : m_(static_cast<decltype(m_)>(m)) {}
CONSTCD14 inline month& month::operator++() NOEXCEPT {if (++m_ == 13) m_ = 1; return *this;}
CONSTCD14 inline month month::operator++(int) NOEXCEPT {auto tmp(*this); ++(*this); return tmp;}
CONSTCD14 inline month& month::operator--() NOEXCEPT {if (--m_ == 0) m_ = 12; return *this;}
CONSTCD14 inline month month::operator--(int) NOEXCEPT {auto tmp(*this); --(*this); return tmp;}

CONSTCD14
inline
month&
month::operator+=(const months& m) NOEXCEPT
{
    *this = *this + m;
    return *this;
}

CONSTCD14
inline
month&
month::operator-=(const months& m) NOEXCEPT
{
    *this = *this - m;
    return *this;
}

CONSTCD11 inline month::operator unsigned() const NOEXCEPT {return m_;}
CONSTCD11 inline bool month::ok() const NOEXCEPT {return 1 <= m_ && m_ <= 12;}

CONSTCD11
inline
bool
operator==(const month& x, const month& y) NOEXCEPT
{
    return static_cast<unsigned>(x) == static_cast<unsigned>(y);
}

CONSTCD11
inline
bool
operator!=(const month& x, const month& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const month& x, const month& y) NOEXCEPT
{
    return static_cast<unsigned>(x) < static_cast<unsigned>(y);
}

CONSTCD11
inline
bool
operator>(const month& x, const month& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const month& x, const month& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const month& x, const month& y) NOEXCEPT
{
    return !(x < y);
}

CONSTCD14
inline
months
operator-(const month& x, const month& y) NOEXCEPT
{
    auto const d = static_cast<unsigned>(x) - static_cast<unsigned>(y);
    return months(d <= 11 ? d : d + 12);
}

CONSTCD14
inline
month
operator+(const month& x, const months& y) NOEXCEPT
{
    auto const mu = static_cast<long long>(static_cast<unsigned>(x)) - 1 + y.count();
    auto const yr = (mu >= 0 ? mu : mu-11) / 12;
    return month{static_cast<unsigned>(mu - yr * 12 + 1)};
}

CONSTCD14
inline
month
operator+(const months& x, const month& y) NOEXCEPT
{
    return y + x;
}

CONSTCD14
inline
month
operator-(const month& x, const months& y) NOEXCEPT
{
    return x + -y;
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month& m)
{
    switch (static_cast<unsigned>(m))
    {
    case 1:
        os << "Farvardin";
        break;
    case 2:
        os << "Ordibehesht";
        break;
    case 3:
        os << "Khordad";
        break;
    case 4:
        os << "Tir";
        break;
    case 5:
        os << "Mordad";
        break;
    case 6:
        os << "Shahrivar";
        break;
    case 7:
        os << "Mehr";
        break;
    case 8:
        os << "Aban";
        break;
    case 9:
        os << "Azar";
        break;
    case 10:
        os << "Dey";
        break;
    case 11:
        os << "Bahman";
        break;
    case 12:
        os << "Esfand";
        break;
    default:
        os << static_cast<unsigned>(m) << " is not a valid month";
        break;
    }
    return os;
}

// year

CONSTCD11 inline year::year(int y) NOEXCEPT : y_(static_cast<decltype(y_)>(y)) {}
CONSTCD14 inline year& year::operator++() NOEXCEPT {++y_; return *this;}
CONSTCD14 inline year year::operator++(int) NOEXCEPT {auto tmp(*this); ++(*this); return tmp;}
CONSTCD14 inline year& year::operator--() NOEXCEPT {--y_; return *this;}
CONSTCD14 inline year year::operator--(int) NOEXCEPT {auto tmp(*this); --(*this); return tmp;}
CONSTCD14 inline year& year::operator+=(const years& y) NOEXCEPT {*this = *this + y; return *this;}
CONSTCD14 inline year& year::operator-=(const years& y) NOEXCEPT {*this = *this - y; return *this;}

CONSTCD14
inline
bool
year::is_leap() const NOEXCEPT
{
  using namespace internal;
  auto const y = static_cast<int>(y_)-475;
  auto const era_d = static_cast<int>(y >= 0 ? y : y-years_in_era+1) / static_cast<double>(years_in_era);
  auto const era = static_cast<int>(era_d);
  auto const yoe = static_cast<unsigned>(y - era * years_in_era);

  // Reference: https://www.timeanddate.com/date/iran-leap-year.html
  // 29 + 33 + 33 + 33 = 128
  // 22 * 128 + 4
  auto const yoc = (yoe < (22 * 128)) ? ((yoe%128) < 29 ? yoe%128 : (yoe%128 - 29)%33) : yoe - (22 * 128) + 33;
  return (yoc != 0 && (yoc%4)==0);
}

CONSTCD11 inline year::operator int() const NOEXCEPT {return y_;}
CONSTCD11 inline bool year::ok() const NOEXCEPT {return true;}

CONSTCD11
inline
year
year::min() NOEXCEPT
{
    return year{std::numeric_limits<short>::min()};
}

CONSTCD11
inline
year
year::max() NOEXCEPT
{
    return year{std::numeric_limits<short>::max()};
}

CONSTCD11
inline
bool
operator==(const year& x, const year& y) NOEXCEPT
{
    return static_cast<int>(x) == static_cast<int>(y);
}

CONSTCD11
inline
bool
operator!=(const year& x, const year& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const year& x, const year& y) NOEXCEPT
{
    return static_cast<int>(x) < static_cast<int>(y);
}

CONSTCD11
inline
bool
operator>(const year& x, const year& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const year& x, const year& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const year& x, const year& y) NOEXCEPT
{
    return !(x < y);
}

CONSTCD11
inline
years
operator-(const year& x, const year& y) NOEXCEPT
{
    return years{static_cast<int>(x) - static_cast<int>(y)};
}

CONSTCD11
inline
year
operator+(const year& x, const years& y) NOEXCEPT
{
    return year{static_cast<int>(x) + y.count()};
}

CONSTCD11
inline
year
operator+(const years& x, const year& y) NOEXCEPT
{
    return y + x;
}

CONSTCD11
inline
year
operator-(const year& x, const years& y) NOEXCEPT
{
    return year{static_cast<int>(x) - y.count()};
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year& y)
{
    date::detail::save_ostream<CharT, Traits> _(os);
    os.fill('0');
    os.flags(std::ios::dec | std::ios::internal);
    os.width(4 + (y < year{0}));
    os << static_cast<int>(y);
    return os;
}

// weekday

CONSTCD11
inline
unsigned char
weekday::weekday_from_days(int z) NOEXCEPT
{
  auto u = static_cast<unsigned>(z);
  return static_cast<unsigned char>(z >= -4 ? (u+4) % 7 : u % 7);
}

CONSTCD11
inline
weekday::weekday(unsigned wd) NOEXCEPT
    : wd_(static_cast<decltype(wd_)>(wd != 7 ? wd : 0))
    {}

CONSTCD11
inline
weekday::weekday(const sys_days& dp) NOEXCEPT
    : wd_(weekday_from_days(dp.time_since_epoch().count()))
    {}

CONSTCD11
inline
weekday::weekday(const local_days& dp) NOEXCEPT
    : wd_(weekday_from_days(dp.time_since_epoch().count()))
    {}

CONSTCD14 inline weekday& weekday::operator++() NOEXCEPT {if (++wd_ == 7) wd_ = 0; return *this;}
CONSTCD14 inline weekday weekday::operator++(int) NOEXCEPT {auto tmp(*this); ++(*this); return tmp;}
CONSTCD14 inline weekday& weekday::operator--() NOEXCEPT {if (wd_-- == 0) wd_ = 6; return *this;}
CONSTCD14 inline weekday weekday::operator--(int) NOEXCEPT {auto tmp(*this); --(*this); return tmp;}

CONSTCD14
inline
weekday&
weekday::operator+=(const days& d) NOEXCEPT
{
    *this = *this + d;
    return *this;
}

CONSTCD14
inline
weekday&
weekday::operator-=(const days& d) NOEXCEPT
{
    *this = *this - d;
    return *this;
}

CONSTCD11
inline
weekday::operator unsigned() const NOEXCEPT
{
    return static_cast<unsigned>(wd_);
}

CONSTCD11 inline bool weekday::ok() const NOEXCEPT {return wd_ <= 6;}

CONSTCD11
inline
bool
operator==(const weekday& x, const weekday& y) NOEXCEPT
{
    return static_cast<unsigned>(x) == static_cast<unsigned>(y);
}

CONSTCD11
inline
bool
operator!=(const weekday& x, const weekday& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD14
inline
days
operator-(const weekday& x, const weekday& y) NOEXCEPT
{
    auto const diff = static_cast<unsigned>(x) - static_cast<unsigned>(y);
    return days{diff <= 6 ? diff : diff + 7};
}

CONSTCD14
inline
weekday
operator+(const weekday& x, const days& y) NOEXCEPT
{
    auto const wdu = static_cast<long long>(static_cast<unsigned>(x)) + y.count();
    auto const wk = (wdu >= 0 ? wdu : wdu-6) / 7;
    return weekday{static_cast<unsigned>(wdu - wk * 7)};
}

CONSTCD14
inline
weekday
operator+(const days& x, const weekday& y) NOEXCEPT
{
    return y + x;
}

CONSTCD14
inline
weekday
operator-(const weekday& x, const days& y) NOEXCEPT
{
    return x + -y;
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const weekday& wd)
{
    switch (static_cast<unsigned>(wd))
    {
    case 0:
        os << "Yekshanbe";
        break;
    case 1:
        os << "Doshanbe";
        break;
    case 2:
        os << "Seshanbe";
        break;
    case 3:
        os << "Chaharshanbe";
        break;
    case 4:
        os << "Panjshanbe";
        break;
    case 5:
        os << "Adine";
        break;
    case 6:
        os << "Shanbe";
        break;
    default:
        os << static_cast<unsigned>(wd) << " is not a valid weekday";
        break;
    }
    return os;
}

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
inline namespace literals
{

CONSTCD11
inline
solar_hijri::day
operator "" _d(unsigned long long d) NOEXCEPT
{
    return solar_hijri::day{static_cast<unsigned>(d)};
}

CONSTCD11
inline
solar_hijri::year
operator "" _y(unsigned long long y) NOEXCEPT
{
    return solar_hijri::year(static_cast<int>(y));
}
#endif  // !defined(_MSC_VER) || (_MSC_VER >= 1900)

CONSTDATA solar_hijri::last_spec last{};

CONSTDATA solar_hijri::month far            {1};
CONSTDATA solar_hijri::month ord            {2};
CONSTDATA solar_hijri::month kho            {3};
CONSTDATA solar_hijri::month tir            {4};
CONSTDATA solar_hijri::month mor            {5};
CONSTDATA solar_hijri::month sha            {6};
CONSTDATA solar_hijri::month meh            {7};
CONSTDATA solar_hijri::month aba            {8};
CONSTDATA solar_hijri::month aza            {9};
CONSTDATA solar_hijri::month dey            {10};
CONSTDATA solar_hijri::month bah            {11};
CONSTDATA solar_hijri::month esf            {12};

CONSTDATA solar_hijri::month Farvardin      {1};
CONSTDATA solar_hijri::month Ordibehesht    {2};
CONSTDATA solar_hijri::month Khordad        {3};
CONSTDATA solar_hijri::month Tir            {4};
CONSTDATA solar_hijri::month Mordad         {5};
CONSTDATA solar_hijri::month Shahrivar      {6};
CONSTDATA solar_hijri::month Mehr           {7};
CONSTDATA solar_hijri::month Aban           {8};
CONSTDATA solar_hijri::month Azar           {9};
CONSTDATA solar_hijri::month Dey            {10};
CONSTDATA solar_hijri::month Bahman         {11};
CONSTDATA solar_hijri::month Esfand         {12};

CONSTDATA solar_hijri::weekday yek          {0u};
CONSTDATA solar_hijri::weekday dos          {1u};
CONSTDATA solar_hijri::weekday ses          {2u};
CONSTDATA solar_hijri::weekday cha          {3u};
CONSTDATA solar_hijri::weekday pan          {4u};
CONSTDATA solar_hijri::weekday adi          {5u};
CONSTDATA solar_hijri::weekday shn          {6u};

CONSTDATA solar_hijri::weekday Yekshanbe    {0u};
CONSTDATA solar_hijri::weekday Doshanbe     {1u};
CONSTDATA solar_hijri::weekday Seshanbe     {2u};
CONSTDATA solar_hijri::weekday Chaharshanbe {3u};
CONSTDATA solar_hijri::weekday Panjshanbe   {4u};
CONSTDATA solar_hijri::weekday Adine        {5u};
CONSTDATA solar_hijri::weekday Shanbe       {6u};

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
}  // inline namespace literals
#endif

// weekday_indexed

CONSTCD11
inline
weekday
weekday_indexed::weekday() const NOEXCEPT
{
    return solar_hijri::weekday{static_cast<unsigned>(wd_)};
}

CONSTCD11 inline unsigned weekday_indexed::index() const NOEXCEPT {return index_;}

CONSTCD11
inline
bool
weekday_indexed::ok() const NOEXCEPT
{
    return weekday().ok() && 1 <= index_ && index_ <= 5;
}

CONSTCD11
inline
weekday_indexed::weekday_indexed(const solar_hijri::weekday& wd, unsigned index) NOEXCEPT
    : wd_(static_cast<decltype(wd_)>(static_cast<unsigned>(wd)))
    , index_(static_cast<decltype(index_)>(index))
    {}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const weekday_indexed& wdi)
{
    return os << wdi.weekday() << '[' << wdi.index() << ']';
}

CONSTCD11
inline
weekday_indexed
weekday::operator[](unsigned index) const NOEXCEPT
{
    return {*this, index};
}

CONSTCD11
inline
bool
operator==(const weekday_indexed& x, const weekday_indexed& y) NOEXCEPT
{
    return x.weekday() == y.weekday() && x.index() == y.index();
}

CONSTCD11
inline
bool
operator!=(const weekday_indexed& x, const weekday_indexed& y) NOEXCEPT
{
    return !(x == y);
}

// weekday_last

CONSTCD11 inline solar_hijri::weekday weekday_last::weekday() const NOEXCEPT {return wd_;}
CONSTCD11 inline bool weekday_last::ok() const NOEXCEPT {return wd_.ok();}
CONSTCD11 inline weekday_last::weekday_last(const solar_hijri::weekday& wd) NOEXCEPT : wd_(wd) {}

CONSTCD11
inline
bool
operator==(const weekday_last& x, const weekday_last& y) NOEXCEPT
{
    return x.weekday() == y.weekday();
}

CONSTCD11
inline
bool
operator!=(const weekday_last& x, const weekday_last& y) NOEXCEPT
{
    return !(x == y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const weekday_last& wdl)
{
    return os << wdl.weekday() << "[last]";
}

CONSTCD11
inline
weekday_last
weekday::operator[](last_spec) const NOEXCEPT
{
    return weekday_last{*this};
}

// year_month

CONSTCD11
inline
year_month::year_month(const solar_hijri::year& y, const solar_hijri::month& m) NOEXCEPT
    : y_(y)
    , m_(m)
    {}

CONSTCD11 inline year year_month::year() const NOEXCEPT {return y_;}
CONSTCD11 inline month year_month::month() const NOEXCEPT {return m_;}
CONSTCD11 inline bool year_month::ok() const NOEXCEPT {return y_.ok() && m_.ok();}

CONSTCD14
inline
year_month&
year_month::operator+=(const months& dm) NOEXCEPT
{
    *this = *this + dm;
    return *this;
}

CONSTCD14
inline
year_month&
year_month::operator-=(const months& dm) NOEXCEPT
{
    *this = *this - dm;
    return *this;
}

CONSTCD14
inline
year_month&
year_month::operator+=(const years& dy) NOEXCEPT
{
    *this = *this + dy;
    return *this;
}

CONSTCD14
inline
year_month&
year_month::operator-=(const years& dy) NOEXCEPT
{
    *this = *this - dy;
    return *this;
}

CONSTCD11
inline
bool
operator==(const year_month& x, const year_month& y) NOEXCEPT
{
    return x.year() == y.year() && x.month() == y.month();
}

CONSTCD11
inline
bool
operator!=(const year_month& x, const year_month& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const year_month& x, const year_month& y) NOEXCEPT
{
    return x.year() < y.year() ? true
        : (x.year() > y.year() ? false
        : (x.month() < y.month()));
}

CONSTCD11
inline
bool
operator>(const year_month& x, const year_month& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const year_month& x, const year_month& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const year_month& x, const year_month& y) NOEXCEPT
{
    return !(x < y);
}

CONSTCD14
inline
year_month
operator+(const year_month& ym, const months& dm) NOEXCEPT
{
    auto dmi = static_cast<int>(static_cast<unsigned>(ym.month())) - 1 + dm.count();
    auto dy = (dmi >= 0 ? dmi : dmi-11) / 12;
    dmi = dmi - dy * 12 + 1;
    return (ym.year() + years(dy)) / month(static_cast<unsigned>(dmi));
}

CONSTCD14
inline
year_month
operator+(const months& dm, const year_month& ym) NOEXCEPT
{
    return ym + dm;
}

CONSTCD14
inline
year_month
operator-(const year_month& ym, const months& dm) NOEXCEPT
{
    return ym + -dm;
}

CONSTCD11
inline
months
operator-(const year_month& x, const year_month& y) NOEXCEPT
{
    return (x.year() - y.year()) +
            months(static_cast<unsigned>(x.month()) - static_cast<unsigned>(y.month()));
}

CONSTCD11
inline
year_month
operator+(const year_month& ym, const years& dy) NOEXCEPT
{
    return (ym.year() + dy) / ym.month();
}

CONSTCD11
inline
year_month
operator+(const years& dy, const year_month& ym) NOEXCEPT
{
    return ym + dy;
}

CONSTCD11
inline
year_month
operator-(const year_month& ym, const years& dy) NOEXCEPT
{
    return ym + -dy;
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month& ym)
{
    return os << ym.year() << '/' << ym.month();
}

// month_day

CONSTCD11
inline
month_day::month_day(const solar_hijri::month& m, const solar_hijri::day& d) NOEXCEPT
    : m_(m)
    , d_(d)
    {}

CONSTCD11 inline solar_hijri::month month_day::month() const NOEXCEPT {return m_;}
CONSTCD11 inline solar_hijri::day month_day::day() const NOEXCEPT {return d_;}

CONSTCD14
inline
bool
month_day::ok() const NOEXCEPT
{
    CONSTDATA solar_hijri::day d[] =
        {31_d, 31_d, 31_d, 31_d, 31_d, 31_d, 30_d, 30_d, 30_d, 30_d, 30_d, 30_d};
    return m_.ok() && 1_d <= d_ && d_ <= d[static_cast<unsigned>(m_)-1];
}

CONSTCD11
inline
bool
operator==(const month_day& x, const month_day& y) NOEXCEPT
{
    return x.month() == y.month() && x.day() == y.day();
}

CONSTCD11
inline
bool
operator!=(const month_day& x, const month_day& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const month_day& x, const month_day& y) NOEXCEPT
{
    return x.month() < y.month() ? true
        : (x.month() > y.month() ? false
        : (x.day() < y.day()));
}

CONSTCD11
inline
bool
operator>(const month_day& x, const month_day& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const month_day& x, const month_day& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const month_day& x, const month_day& y) NOEXCEPT
{
    return !(x < y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_day& md)
{
    return os << md.month() << '/' << md.day();
}

// month_day_last

CONSTCD11 inline month month_day_last::month() const NOEXCEPT {return m_;}
CONSTCD11 inline bool month_day_last::ok() const NOEXCEPT {return m_.ok();}
CONSTCD11 inline month_day_last::month_day_last(const solar_hijri::month& m) NOEXCEPT : m_(m) {}

CONSTCD11
inline
bool
operator==(const month_day_last& x, const month_day_last& y) NOEXCEPT
{
    return x.month() == y.month();
}

CONSTCD11
inline
bool
operator!=(const month_day_last& x, const month_day_last& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const month_day_last& x, const month_day_last& y) NOEXCEPT
{
    return x.month() < y.month();
}

CONSTCD11
inline
bool
operator>(const month_day_last& x, const month_day_last& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const month_day_last& x, const month_day_last& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const month_day_last& x, const month_day_last& y) NOEXCEPT
{
    return !(x < y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_day_last& mdl)
{
    return os << mdl.month() << "/last";
}

// month_weekday

CONSTCD11
inline
month_weekday::month_weekday(const solar_hijri::month& m,
                             const solar_hijri::weekday_indexed& wdi) NOEXCEPT
    : m_(m)
    , wdi_(wdi)
    {}

CONSTCD11 inline month month_weekday::month() const NOEXCEPT {return m_;}

CONSTCD11
inline
weekday_indexed
month_weekday::weekday_indexed() const NOEXCEPT
{
    return wdi_;
}

CONSTCD11
inline
bool
month_weekday::ok() const NOEXCEPT
{
    return m_.ok() && wdi_.ok();
}

CONSTCD11
inline
bool
operator==(const month_weekday& x, const month_weekday& y) NOEXCEPT
{
    return x.month() == y.month() && x.weekday_indexed() == y.weekday_indexed();
}

CONSTCD11
inline
bool
operator!=(const month_weekday& x, const month_weekday& y) NOEXCEPT
{
    return !(x == y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_weekday& mwd)
{
    return os << mwd.month() << '/' << mwd.weekday_indexed();
}

// month_weekday_last

CONSTCD11
inline
month_weekday_last::month_weekday_last(const solar_hijri::month& m,
                                       const solar_hijri::weekday_last& wdl) NOEXCEPT
    : m_(m)
    , wdl_(wdl)
    {}

CONSTCD11 inline month month_weekday_last::month() const NOEXCEPT {return m_;}

CONSTCD11
inline
weekday_last
month_weekday_last::weekday_last() const NOEXCEPT
{
    return wdl_;
}

CONSTCD11
inline
bool
month_weekday_last::ok() const NOEXCEPT
{
    return m_.ok() && wdl_.ok();
}

CONSTCD11
inline
bool
operator==(const month_weekday_last& x, const month_weekday_last& y) NOEXCEPT
{
    return x.month() == y.month() && x.weekday_last() == y.weekday_last();
}

CONSTCD11
inline
bool
operator!=(const month_weekday_last& x, const month_weekday_last& y) NOEXCEPT
{
    return !(x == y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const month_weekday_last& mwdl)
{
    return os << mwdl.month() << '/' << mwdl.weekday_last();
}

// year_month_day_last

CONSTCD11
inline
year_month_day_last::year_month_day_last(const solar_hijri::year& y,
                                         const solar_hijri::month_day_last& mdl) NOEXCEPT
    : y_(y)
    , mdl_(mdl)
    {}

CONSTCD14
inline
year_month_day_last&
year_month_day_last::operator+=(const months& m) NOEXCEPT
{
    *this = *this + m;
    return *this;
}

CONSTCD14
inline
year_month_day_last&
year_month_day_last::operator-=(const months& m) NOEXCEPT
{
    *this = *this - m;
    return *this;
}

CONSTCD14
inline
year_month_day_last&
year_month_day_last::operator+=(const years& y) NOEXCEPT
{
    *this = *this + y;
    return *this;
}

CONSTCD14
inline
year_month_day_last&
year_month_day_last::operator-=(const years& y) NOEXCEPT
{
    *this = *this - y;
    return *this;
}

CONSTCD11 inline year year_month_day_last::year() const NOEXCEPT {return y_;}
CONSTCD11 inline month year_month_day_last::month() const NOEXCEPT {return mdl_.month();}

CONSTCD11
inline
month_day_last
year_month_day_last::month_day_last() const NOEXCEPT
{
    return mdl_;
}

CONSTCD14
inline
day
year_month_day_last::day() const NOEXCEPT
{
    CONSTDATA solar_hijri::day d[] =
        {31_d, 31_d, 31_d, 31_d, 31_d, 31_d, 30_d, 30_d, 30_d, 30_d, 30_d, 29_d};
    return month() != esf || !y_.is_leap() ?
        d[static_cast<unsigned>(month())-1] : 30_d;
}

CONSTCD14
inline
year_month_day_last::operator sys_days() const NOEXCEPT
{
    return sys_days(year()/month()/day());
}

CONSTCD14
inline
year_month_day_last::operator local_days() const NOEXCEPT
{
    return local_days(year()/month()/day());
}

CONSTCD11
inline
bool
year_month_day_last::ok() const NOEXCEPT
{
    return y_.ok() && mdl_.ok();
}

CONSTCD11
inline
bool
operator==(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT
{
    return x.year() == y.year() && x.month_day_last() == y.month_day_last();
}

CONSTCD11
inline
bool
operator!=(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT
{
    return x.year() < y.year() ? true
        : (x.year() > y.year() ? false
        : (x.month_day_last() < y.month_day_last()));
}

CONSTCD11
inline
bool
operator>(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const year_month_day_last& x, const year_month_day_last& y) NOEXCEPT
{
    return !(x < y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_day_last& ymdl)
{
    return os << ymdl.year() << '/' << ymdl.month_day_last();
}

CONSTCD14
inline
year_month_day_last
operator+(const year_month_day_last& ymdl, const months& dm) NOEXCEPT
{
    return (ymdl.year() / ymdl.month() + dm) / last;
}

CONSTCD14
inline
year_month_day_last
operator+(const months& dm, const year_month_day_last& ymdl) NOEXCEPT
{
    return ymdl + dm;
}

CONSTCD14
inline
year_month_day_last
operator-(const year_month_day_last& ymdl, const months& dm) NOEXCEPT
{
    return ymdl + (-dm);
}

CONSTCD11
inline
year_month_day_last
operator+(const year_month_day_last& ymdl, const years& dy) NOEXCEPT
{
    return {ymdl.year()+dy, ymdl.month_day_last()};
}

CONSTCD11
inline
year_month_day_last
operator+(const years& dy, const year_month_day_last& ymdl) NOEXCEPT
{
    return ymdl + dy;
}

CONSTCD11
inline
year_month_day_last
operator-(const year_month_day_last& ymdl, const years& dy) NOEXCEPT
{
    return ymdl + (-dy);
}

// year_month_day

CONSTCD11
inline
year_month_day::year_month_day(const solar_hijri::year& y, const solar_hijri::month& m,
                               const solar_hijri::day& d) NOEXCEPT
    : y_(y)
    , m_(m)
    , d_(d)
    {}

CONSTCD14
inline
year_month_day::year_month_day(const year_month_day_last& ymdl) NOEXCEPT
    : y_(ymdl.year())
    , m_(ymdl.month())
    , d_(ymdl.day())
    {}

CONSTCD14
inline
year_month_day::year_month_day(sys_days dp) NOEXCEPT
    : year_month_day(from_days(dp.time_since_epoch()))
    {}

CONSTCD14
inline
year_month_day::year_month_day(local_days dp) NOEXCEPT
    : year_month_day(from_days(dp.time_since_epoch()))
    {}

CONSTCD11 inline year year_month_day::year() const NOEXCEPT {return y_;}
CONSTCD11 inline month year_month_day::month() const NOEXCEPT {return m_;}
CONSTCD11 inline day year_month_day::day() const NOEXCEPT {return d_;}

CONSTCD14
inline
year_month_day&
year_month_day::operator+=(const months& m) NOEXCEPT
{
    *this = *this + m;
    return *this;
}

CONSTCD14
inline
year_month_day&
year_month_day::operator-=(const months& m) NOEXCEPT
{
    *this = *this - m;
    return *this;
}

CONSTCD14
inline
year_month_day&
year_month_day::operator+=(const years& y) NOEXCEPT
{
    *this = *this + y;
    return *this;
}

CONSTCD14
inline
year_month_day&
year_month_day::operator-=(const years& y) NOEXCEPT
{
    *this = *this - y;
    return *this;
}

CONSTCD14
inline
days
year_month_day::to_days() const NOEXCEPT
{
    static_assert(std::numeric_limits<unsigned>::digits >= 18,
             "This algorithm has not been ported to a 16 bit unsigned integer");
    static_assert(std::numeric_limits<int>::digits >= 20,
             "This algorithm has not been ported to a 16 bit signed integer");

    using namespace internal;
    auto const y = static_cast<int>(y_) - 475;
    auto const m = static_cast<unsigned>(m_);
    auto const d = static_cast<unsigned>(d_);
    auto const era_d = static_cast<int>(y >= 0 ? y : y-years_in_era+1) / static_cast<double>(years_in_era);
    auto const era = static_cast<int>(era_d);
    auto const fdoe = static_cast<int>(epoch + era * days_in_era);
    auto const yoe = static_cast<int>(y - era * years_in_era);

    auto const period_d = static_cast<double>(yoe/years_in_period);
    auto const period = static_cast<unsigned>(period_d);
    auto const yop = yoe%years_in_period;
    auto const fdop = period*days_in_period;
    auto const cycle = yop < 29 ? 0 : static_cast<unsigned>((yop-29)/years_in_other_cycles + 1);
    auto const yoc = yop < 29 ? yop : (yop-29)%years_in_other_cycles;
    auto const fdoc = cycle > 0 ? days_in_first_cycle + (cycle-1)*days_in_other_cycles : 0;
    auto const group = yoc < 1 ? 0 : static_cast<unsigned>((yoc-1) / 4);
    auto const yog = static_cast<int>(yoc < 1 ? -1 : (yoc-1) % 4);
    auto const fdoyog = group*1461 + (yog+1)*365;
    auto const fdoyoe = fdop + fdoc + fdoyog;

    auto const doy = 30*(m-1) + ((m > 6) ? 6 : m-1) + d-1;                     // [0, 365]
    auto const doe = fdoe + fdoyoe + doy;
    return days{doe - unix_time_shift};
}

CONSTCD14
inline
year_month_day::operator sys_days() const NOEXCEPT
{
    return sys_days{to_days()};
}

CONSTCD14
inline
year_month_day::operator local_days() const NOEXCEPT
{
    return local_days{to_days()};
}

CONSTCD14
inline
bool
year_month_day::ok() const NOEXCEPT
{
    if (!(y_.ok() && m_.ok()))
        return false;
    return 1_d <= d_ && d_ <= (y_/m_/last).day();
}

CONSTCD11
inline
bool
operator==(const year_month_day& x, const year_month_day& y) NOEXCEPT
{
    return x.year() == y.year() && x.month() == y.month() && x.day() == y.day();
}

CONSTCD11
inline
bool
operator!=(const year_month_day& x, const year_month_day& y) NOEXCEPT
{
    return !(x == y);
}

CONSTCD11
inline
bool
operator<(const year_month_day& x, const year_month_day& y) NOEXCEPT
{
    return x.year() < y.year() ? true
        : (x.year() > y.year() ? false
        : (x.month() < y.month() ? true
        : (x.month() > y.month() ? false
        : (x.day() < y.day()))));
}

CONSTCD11
inline
bool
operator>(const year_month_day& x, const year_month_day& y) NOEXCEPT
{
    return y < x;
}

CONSTCD11
inline
bool
operator<=(const year_month_day& x, const year_month_day& y) NOEXCEPT
{
    return !(y < x);
}

CONSTCD11
inline
bool
operator>=(const year_month_day& x, const year_month_day& y) NOEXCEPT
{
    return !(x < y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_day& ymd)
{
    date::detail::save_ostream<CharT, Traits> _(os);
    os.fill('0');
    os.flags(std::ios::dec | std::ios::right);
    os << ymd.year() << '-';
    os.width(2);
    os << static_cast<unsigned>(ymd.month()) << '-';
    os << ymd.day();
    return os;
}

CONSTCD14
inline
year_month_day
year_month_day::from_days(days dp) NOEXCEPT
{
    static_assert(std::numeric_limits<unsigned>::digits >= 18,
             "This algorithm has not been ported to a 16 bit unsigned integer");
    static_assert(std::numeric_limits<int>::digits >= 20,
             "This algorithm has not been ported to a 16 bit signed integer");

    using namespace internal;
    auto const z = dp.count() + unix_time_shift;
    auto const delta = static_cast<int>(z - epoch);
    auto const era = static_cast<int>(delta >= 0 ? delta : delta-days_in_era+1) / static_cast<double>(days_in_era);
    auto const era_i = static_cast<int>(era);
    auto const fdoe = static_cast<int>(epoch + static_cast<int>(era_i * days_in_era));

    auto const doe_fdoe = z - fdoe;
    auto const period = static_cast<unsigned>(doe_fdoe < 22*days_in_period ? doe_fdoe / days_in_period : 22);
    auto const dop = doe_fdoe % days_in_period;
    auto const cycle = dop < days_in_first_cycle ? 0 : (dop-days_in_first_cycle) / days_in_other_cycles + 1;
    auto const doc = dop < days_in_first_cycle ? dop : (dop-days_in_first_cycle) % days_in_other_cycles;
    auto const group = doc < 365 && period != 22 ? -1 : static_cast<int>(((doc < 365 ? 365 : doc)-365)/1461);
    auto const yog =   doc < 365 && period != 22 ? -1 : static_cast<int>( (period != 22 ? ((doc-365 )%1461) : doc)/365);
    auto const yoc = group == -1 ?  0 : (period != 22 ? 1 : 0) + group*4 + (yog == 4 ? 3 : yog);
    auto const doy = group == -1 ? doc : (period != 22 ? ((yoc-1)%4 == 0 ? (group >= 0 ? (doe_fdoe -
                                                                                         (period*days_in_period) -
                                                                                         (cycle > 0 ? days_in_first_cycle + (cycle-1)*days_in_other_cycles : 0) -
                                                                                         (group*1461 + ((yog == 4 ? 3 : yog)+1)*365))
                                                                                      : 365)
                                                                         : doe_fdoe -
                                                                           (period*days_in_period) -
                                                                           (cycle > 0 ? days_in_first_cycle + (cycle-1)*days_in_other_cycles : 0) -
                                                                           (group*1461 + ((yog == 4 ? 3 : yog)+1)*365))
                                                       : (yog == 4 ? 365 : doe_fdoe - (period*days_in_period) - yog*365));
    auto const yoe = period != 22 ? period*years_in_period +
                                    (cycle > 0 ? years_in_first_cycle +
                                                (cycle-1)*years_in_other_cycles
                                              : 0) +
                                    yoc
                                  : 22*years_in_period + ((yog == 4) ? 3 : yog);
    auto const y = static_cast<int>(static_cast<sys_days::rep>(yoe) + 475 + era_i * years_in_era);
    auto const m = doy < 186 ? doy/31 + 1 : (doy-186)/30 + 7;           // [1, 12]
    auto const d = doy - (30*(m-1) + ((m > 6) ? 6 : m-1) - 1);          // [1, 31]

    return year_month_day{solar_hijri::year(y), solar_hijri::month(m), solar_hijri::day(d)};
}

CONSTCD14
inline
year_month_day
operator+(const year_month_day& ymd, const months& dm) NOEXCEPT
{
    return (ymd.year() / ymd.month() + dm) / ymd.day();
}

CONSTCD14
inline
year_month_day
operator+(const months& dm, const year_month_day& ymd) NOEXCEPT
{
    return ymd + dm;
}

CONSTCD14
inline
year_month_day
operator-(const year_month_day& ymd, const months& dm) NOEXCEPT
{
    return ymd + (-dm);
}

CONSTCD11
inline
year_month_day
operator+(const year_month_day& ymd, const years& dy) NOEXCEPT
{
    return (ymd.year() + dy) / ymd.month() / ymd.day();
}

CONSTCD11
inline
year_month_day
operator+(const years& dy, const year_month_day& ymd) NOEXCEPT
{
    return ymd + dy;
}

CONSTCD11
inline
year_month_day
operator-(const year_month_day& ymd, const years& dy) NOEXCEPT
{
    return ymd + (-dy);
}

// year_month_weekday

CONSTCD11
inline
year_month_weekday::year_month_weekday(const solar_hijri::year& y, const solar_hijri::month& m,
                                       const solar_hijri::weekday_indexed& wdi)
        NOEXCEPT
    : y_(y)
    , m_(m)
    , wdi_(wdi)
    {}

CONSTCD14
inline
year_month_weekday::year_month_weekday(const sys_days& dp) NOEXCEPT
    : year_month_weekday(from_days(dp.time_since_epoch()))
    {}

CONSTCD14
inline
year_month_weekday::year_month_weekday(const local_days& dp) NOEXCEPT
    : year_month_weekday(from_days(dp.time_since_epoch()))
    {}

CONSTCD14
inline
year_month_weekday&
year_month_weekday::operator+=(const months& m) NOEXCEPT
{
    *this = *this + m;
    return *this;
}

CONSTCD14
inline
year_month_weekday&
year_month_weekday::operator-=(const months& m) NOEXCEPT
{
    *this = *this - m;
    return *this;
}

CONSTCD14
inline
year_month_weekday&
year_month_weekday::operator+=(const years& y) NOEXCEPT
{
    *this = *this + y;
    return *this;
}

CONSTCD14
inline
year_month_weekday&
year_month_weekday::operator-=(const years& y) NOEXCEPT
{
    *this = *this - y;
    return *this;
}

CONSTCD11 inline year year_month_weekday::year() const NOEXCEPT {return y_;}
CONSTCD11 inline month year_month_weekday::month() const NOEXCEPT {return m_;}

CONSTCD11
inline
weekday
year_month_weekday::weekday() const NOEXCEPT
{
    return wdi_.weekday();
}

CONSTCD11
inline
unsigned
year_month_weekday::index() const NOEXCEPT
{
    return wdi_.index();
}

CONSTCD11
inline
weekday_indexed
year_month_weekday::weekday_indexed() const NOEXCEPT
{
    return wdi_;
}

CONSTCD14
inline
year_month_weekday::operator sys_days() const NOEXCEPT
{
    return sys_days{to_days()};
}

CONSTCD14
inline
year_month_weekday::operator local_days() const NOEXCEPT
{
    return local_days{to_days()};
}

CONSTCD14
inline
bool
year_month_weekday::ok() const NOEXCEPT
{
    if (!y_.ok() || !m_.ok() || !wdi_.weekday().ok() || wdi_.index() < 1)
        return false;
    if (wdi_.index() <= 4)
        return true;
    auto d2 = wdi_.weekday() - solar_hijri::weekday(y_/m_/1) + days((wdi_.index()-1)*7 + 1);
    return static_cast<unsigned>(d2.count()) <= static_cast<unsigned>((y_/m_/last).day());
}

CONSTCD14
inline
year_month_weekday
year_month_weekday::from_days(days d) NOEXCEPT
{
    sys_days dp{d};
    auto const wd = solar_hijri::weekday(dp);
    auto const ymd = year_month_day(dp);
    return {ymd.year(), ymd.month(), wd[(static_cast<unsigned>(ymd.day())-1)/7+1]};
}

CONSTCD14
inline
days
year_month_weekday::to_days() const NOEXCEPT
{
    auto d = sys_days(y_/m_/1);
    return (d + (wdi_.weekday() - solar_hijri::weekday(d) + days{(wdi_.index()-1)*7})
           ).time_since_epoch();
}

CONSTCD11
inline
bool
operator==(const year_month_weekday& x, const year_month_weekday& y) NOEXCEPT
{
    return x.year() == y.year() && x.month() == y.month() &&
           x.weekday_indexed() == y.weekday_indexed();
}

CONSTCD11
inline
bool
operator!=(const year_month_weekday& x, const year_month_weekday& y) NOEXCEPT
{
    return !(x == y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_weekday& ymwdi)
{
    return os << ymwdi.year() << '/' << ymwdi.month()
              << '/' << ymwdi.weekday_indexed();
}

CONSTCD14
inline
year_month_weekday
operator+(const year_month_weekday& ymwd, const months& dm) NOEXCEPT
{
    return (ymwd.year() / ymwd.month() + dm) / ymwd.weekday_indexed();
}

CONSTCD14
inline
year_month_weekday
operator+(const months& dm, const year_month_weekday& ymwd) NOEXCEPT
{
    return ymwd + dm;
}

CONSTCD14
inline
year_month_weekday
operator-(const year_month_weekday& ymwd, const months& dm) NOEXCEPT
{
    return ymwd + (-dm);
}

CONSTCD11
inline
year_month_weekday
operator+(const year_month_weekday& ymwd, const years& dy) NOEXCEPT
{
    return {ymwd.year()+dy, ymwd.month(), ymwd.weekday_indexed()};
}

CONSTCD11
inline
year_month_weekday
operator+(const years& dy, const year_month_weekday& ymwd) NOEXCEPT
{
    return ymwd + dy;
}

CONSTCD11
inline
year_month_weekday
operator-(const year_month_weekday& ymwd, const years& dy) NOEXCEPT
{
    return ymwd + (-dy);
}

// year_month_weekday_last

CONSTCD11
inline
year_month_weekday_last::year_month_weekday_last(const solar_hijri::year& y,
                                                 const solar_hijri::month& m,
                                                 const solar_hijri::weekday_last& wdl) NOEXCEPT
    : y_(y)
    , m_(m)
    , wdl_(wdl)
    {}

CONSTCD14
inline
year_month_weekday_last&
year_month_weekday_last::operator+=(const months& m) NOEXCEPT
{
    *this = *this + m;
    return *this;
}

CONSTCD14
inline
year_month_weekday_last&
year_month_weekday_last::operator-=(const months& m) NOEXCEPT
{
    *this = *this - m;
    return *this;
}

CONSTCD14
inline
year_month_weekday_last&
year_month_weekday_last::operator+=(const years& y) NOEXCEPT
{
    *this = *this + y;
    return *this;
}

CONSTCD14
inline
year_month_weekday_last&
year_month_weekday_last::operator-=(const years& y) NOEXCEPT
{
    *this = *this - y;
    return *this;
}

CONSTCD11 inline year year_month_weekday_last::year() const NOEXCEPT {return y_;}
CONSTCD11 inline month year_month_weekday_last::month() const NOEXCEPT {return m_;}

CONSTCD11
inline
weekday
year_month_weekday_last::weekday() const NOEXCEPT
{
    return wdl_.weekday();
}

CONSTCD11
inline
weekday_last
year_month_weekday_last::weekday_last() const NOEXCEPT
{
    return wdl_;
}

CONSTCD14
inline
year_month_weekday_last::operator sys_days() const NOEXCEPT
{
    return sys_days{to_days()};
}

CONSTCD14
inline
year_month_weekday_last::operator local_days() const NOEXCEPT
{
    return local_days{to_days()};
}

CONSTCD11
inline
bool
year_month_weekday_last::ok() const NOEXCEPT
{
    return y_.ok() && m_.ok() && wdl_.ok();
}

CONSTCD14
inline
days
year_month_weekday_last::to_days() const NOEXCEPT
{
    auto const d = sys_days(y_/m_/last);
    return (d - (solar_hijri::weekday{d} - wdl_.weekday())).time_since_epoch();
}

CONSTCD11
inline
bool
operator==(const year_month_weekday_last& x, const year_month_weekday_last& y) NOEXCEPT
{
    return x.year() == y.year() && x.month() == y.month() &&
           x.weekday_last() == y.weekday_last();
}

CONSTCD11
inline
bool
operator!=(const year_month_weekday_last& x, const year_month_weekday_last& y) NOEXCEPT
{
    return !(x == y);
}

template<class CharT, class Traits>
inline
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os, const year_month_weekday_last& ymwdl)
{
    return os << ymwdl.year() << '/' << ymwdl.month() << '/' << ymwdl.weekday_last();
}

CONSTCD14
inline
year_month_weekday_last
operator+(const year_month_weekday_last& ymwdl, const months& dm) NOEXCEPT
{
    return (ymwdl.year() / ymwdl.month() + dm) / ymwdl.weekday_last();
}

CONSTCD14
inline
year_month_weekday_last
operator+(const months& dm, const year_month_weekday_last& ymwdl) NOEXCEPT
{
    return ymwdl + dm;
}

CONSTCD14
inline
year_month_weekday_last
operator-(const year_month_weekday_last& ymwdl, const months& dm) NOEXCEPT
{
    return ymwdl + (-dm);
}

CONSTCD11
inline
year_month_weekday_last
operator+(const year_month_weekday_last& ymwdl, const years& dy) NOEXCEPT
{
    return {ymwdl.year()+dy, ymwdl.month(), ymwdl.weekday_last()};
}

CONSTCD11
inline
year_month_weekday_last
operator+(const years& dy, const year_month_weekday_last& ymwdl) NOEXCEPT
{
    return ymwdl + dy;
}

CONSTCD11
inline
year_month_weekday_last
operator-(const year_month_weekday_last& ymwdl, const years& dy) NOEXCEPT
{
    return ymwdl + (-dy);
}

// year_month from operator/()

CONSTCD11
inline
year_month
operator/(const year& y, const month& m) NOEXCEPT
{
    return {y, m};
}

CONSTCD11
inline
year_month
operator/(const year& y, int   m) NOEXCEPT
{
    return y / month(static_cast<unsigned>(m));
}

// month_day from operator/()

CONSTCD11
inline
month_day
operator/(const month& m, const day& d) NOEXCEPT
{
    return {m, d};
}

CONSTCD11
inline
month_day
operator/(const day& d, const month& m) NOEXCEPT
{
    return m / d;
}

CONSTCD11
inline
month_day
operator/(const month& m, int d) NOEXCEPT
{
    return m / day(static_cast<unsigned>(d));
}

CONSTCD11
inline
month_day
operator/(int m, const day& d) NOEXCEPT
{
    return month(static_cast<unsigned>(m)) / d;
}

CONSTCD11 inline month_day operator/(const day& d, int m) NOEXCEPT {return m / d;}

// month_day_last from operator/()

CONSTCD11
inline
month_day_last
operator/(const month& m, last_spec) NOEXCEPT
{
    return month_day_last{m};
}

CONSTCD11
inline
month_day_last
operator/(last_spec, const month& m) NOEXCEPT
{
    return m/last;
}

CONSTCD11
inline
month_day_last
operator/(int m, last_spec) NOEXCEPT
{
    return month(static_cast<unsigned>(m))/last;
}

CONSTCD11
inline
month_day_last
operator/(last_spec, int m) NOEXCEPT
{
    return m/last;
}

// month_weekday from operator/()

CONSTCD11
inline
month_weekday
operator/(const month& m, const weekday_indexed& wdi) NOEXCEPT
{
    return {m, wdi};
}

CONSTCD11
inline
month_weekday
operator/(const weekday_indexed& wdi, const month& m) NOEXCEPT
{
    return m / wdi;
}

CONSTCD11
inline
month_weekday
operator/(int m, const weekday_indexed& wdi) NOEXCEPT
{
    return month(static_cast<unsigned>(m)) / wdi;
}

CONSTCD11
inline
month_weekday
operator/(const weekday_indexed& wdi, int m) NOEXCEPT
{
    return m / wdi;
}

// month_weekday_last from operator/()

CONSTCD11
inline
month_weekday_last
operator/(const month& m, const weekday_last& wdl) NOEXCEPT
{
    return {m, wdl};
}

CONSTCD11
inline
month_weekday_last
operator/(const weekday_last& wdl, const month& m) NOEXCEPT
{
    return m / wdl;
}

CONSTCD11
inline
month_weekday_last
operator/(int m, const weekday_last& wdl) NOEXCEPT
{
    return month(static_cast<unsigned>(m)) / wdl;
}

CONSTCD11
inline
month_weekday_last
operator/(const weekday_last& wdl, int m) NOEXCEPT
{
    return m / wdl;
}

// year_month_day from operator/()

CONSTCD11
inline
year_month_day
operator/(const year_month& ym, const day& d) NOEXCEPT
{
    return {ym.year(), ym.month(), d};
}

CONSTCD11
inline
year_month_day
operator/(const year_month& ym, int d)  NOEXCEPT
{
    return ym / day(static_cast<unsigned>(d));
}

CONSTCD11
inline
year_month_day
operator/(const year& y, const month_day& md) NOEXCEPT
{
    return y / md.month() / md.day();
}

CONSTCD11
inline
year_month_day
operator/(int y, const month_day& md) NOEXCEPT
{
    return year(y) / md;
}

CONSTCD11
inline
year_month_day
operator/(const month_day& md, const year& y)  NOEXCEPT
{
    return y / md;
}

CONSTCD11
inline
year_month_day
operator/(const month_day& md, int y) NOEXCEPT
{
    return year(y) / md;
}

// year_month_day_last from operator/()

CONSTCD11
inline
year_month_day_last
operator/(const year_month& ym, last_spec) NOEXCEPT
{
    return {ym.year(), month_day_last{ym.month()}};
}

CONSTCD11
inline
year_month_day_last
operator/(const year& y, const month_day_last& mdl) NOEXCEPT
{
    return {y, mdl};
}

CONSTCD11
inline
year_month_day_last
operator/(int y, const month_day_last& mdl) NOEXCEPT
{
    return year(y) / mdl;
}

CONSTCD11
inline
year_month_day_last
operator/(const month_day_last& mdl, const year& y) NOEXCEPT
{
    return y / mdl;
}

CONSTCD11
inline
year_month_day_last
operator/(const month_day_last& mdl, int y) NOEXCEPT
{
    return year(y) / mdl;
}

// year_month_weekday from operator/()

CONSTCD11
inline
year_month_weekday
operator/(const year_month& ym, const weekday_indexed& wdi) NOEXCEPT
{
    return {ym.year(), ym.month(), wdi};
}

CONSTCD11
inline
year_month_weekday
operator/(const year& y, const month_weekday& mwd) NOEXCEPT
{
    return {y, mwd.month(), mwd.weekday_indexed()};
}

CONSTCD11
inline
year_month_weekday
operator/(int y, const month_weekday& mwd) NOEXCEPT
{
    return year(y) / mwd;
}

CONSTCD11
inline
year_month_weekday
operator/(const month_weekday& mwd, const year& y) NOEXCEPT
{
    return y / mwd;
}

CONSTCD11
inline
year_month_weekday
operator/(const month_weekday& mwd, int y) NOEXCEPT
{
    return year(y) / mwd;
}

// year_month_weekday_last from operator/()

CONSTCD11
inline
year_month_weekday_last
operator/(const year_month& ym, const weekday_last& wdl) NOEXCEPT
{
    return {ym.year(), ym.month(), wdl};
}

CONSTCD11
inline
year_month_weekday_last
operator/(const year& y, const month_weekday_last& mwdl) NOEXCEPT
{
    return {y, mwdl.month(), mwdl.weekday_last()};
}

CONSTCD11
inline
year_month_weekday_last
operator/(int y, const month_weekday_last& mwdl) NOEXCEPT
{
    return year(y) / mwdl;
}

CONSTCD11
inline
year_month_weekday_last
operator/(const month_weekday_last& mwdl, const year& y) NOEXCEPT
{
    return y / mwdl;
}

CONSTCD11
inline
year_month_weekday_last
operator/(const month_weekday_last& mwdl, int y) NOEXCEPT
{
    return year(y) / mwdl;
}

}  // namespace solar_hijri

#endif  // SOLAR_HIJRI_H
