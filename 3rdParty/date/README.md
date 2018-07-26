[![Join the chat at https://gitter.im/HowardHinnant/date](https://badges.gitter.im/HowardHinnant/date.svg)](https://gitter.im/HowardHinnant/date?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
<hr/>

**[Try it out on wandbox!](https://wandbox.org/permlink/vqwMyTphHJv5iXX7)**

This is actually several separate C++11/C++14 libraries:

1.  `"date.h"` is a header-only library which builds upon `<chrono>`.  It adds some new `duration` types, and new `time_point` types.  It also adds "field" types such as `year_month_day` which is a struct `{year, month, day}`.  And it provides convenient means to convert between the "field" types and the `time_point` types.

    * Documentation: http://howardhinnant.github.io/date/date.html
    * Video: https://www.youtube.com/watch?v=tzyGjOm8AKo
    * Slides: http://schd.ws/hosted_files/cppcon2015/43/hinnant_dates.pdf

2. `"tz.h"` / `"tz.cpp"`  are a timezone library built on top of the `"date.h"` library.  This timezone library is a complete parser of the IANA timezone database.  It provides for an easy way to access all of the data in this database, using the types from `"date.h"` and `<chrono>`.  The IANA database also includes data on leap seconds, and this library provides utilities to compute with that information as well.

    * Documentation: http://howardhinnant.github.io/date/tz.html
    * Video: https://www.youtube.com/watch?v=Vwd3pduVGKY
    * Slides: http://schd.ws/hosted_files/cppcon2016/0f/Welcome%20To%20The%20Time%20Zone%20-%20Howard%20Hinnant%20-%20CppCon%202016.pdf

3. `"iso_week.h"` is a header-only library built on top of the `"date.h"` library which implements the ISO week date calendar.  

    * Documentation: http://howardhinnant.github.io/date/iso_week.html

4. `"julian.h"` is a header-only library built on top of the `"date.h"` library which implements a proleptic Julian calendar which is fully interoperable with everything above.

    * Documentation: http://howardhinnant.github.io/date/julian.html

5. `"islamic.h"` is a header-only library built on top of the `"date.h"` library which implements a proleptic Islamic calendar which is fully interoperable with everything above.

    * Documentation: http://howardhinnant.github.io/date/islamic.html

`"date.h"` and `"tz.h"` are now being proposed for standardization:

   * Current proposal:  http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0355r4.html
   * Draft revision:  http://howardhinnant.github.io/date/d0355r5.html
   * Status:  The proposal was approved by LEWG and forwarded to LWG with the recommendation of targeting C++20 at the Albuquerque, NM meeting in Nov. 2017.

List of projects using this library:

   * www.safe.com
   * www.webtoolkit.eu/wt

If you would like your project (or product) on this list, just let me know.
