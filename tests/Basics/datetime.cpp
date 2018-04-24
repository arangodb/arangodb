
#include "Basics/datetime.h"
#include "catch.hpp"
#include <date/date.h>

using namespace arangodb;
using namespace arangodb::basics;

SCENARIO("testing", "[datetime]") {
    using namespace std::chrono;
    using namespace date;
    
    tp_sys_clock_ms tp;

    std::vector<std::string> dates{"2017", "2017-11", "2017-11-12"};
    std::vector<std::string> times{"", "T12:34",
                                       "T12:34+10:22",
                                       "T12:34-10:22",

                                       "T12:34:56",
                                       "T12:34:56+10:22",
                                       "T12:34:56-10:22",

                                       "T12:34:56.789",
                                       "T12:34:56.789+10:22",
                                       "T12:34:56.789-10:22"};

    std::vector<std::string> datesToTest{};

    for (auto const& d : dates) {
        for (auto const& t : times) {
            datesToTest.push_back(d+t);
        }
    }

    std::vector<std::string> datesToFail{"2017-01-01-12",
    "2017-01-01:12:34", "2017-01-01:12:34Z+10:20", "2017-01-01:12:34Z-10:20"};
    
    for (auto const& dateTime : datesToTest) {
        GIVEN(dateTime) {
            bool ret = parse_dateTime(dateTime, tp);
            
            THEN(dateTime) { REQUIRE(ret == true); }
        }
    }

    for (auto const& dateTime : datesToFail) {
        GIVEN(dateTime) {
            bool ret = parse_dateTime(dateTime, tp);
            
            THEN(dateTime) { REQUIRE(ret == false); }
        }
    }

}
