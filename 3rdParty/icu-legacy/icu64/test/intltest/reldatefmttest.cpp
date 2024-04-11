// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File RELDATEFMTTEST.CPP
*
*******************************************************************************
*/
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_BREAK_ITERATION

#include "unicode/localpointer.h"
#include "unicode/numfmt.h"
#include "unicode/reldatefmt.h"
#include "unicode/rbnf.h"
#include "cmemory.h"
#include "itformat.h"

static const char *DirectionStr(UDateDirection direction);
static const char *RelativeUnitStr(UDateRelativeUnit unit);
static const char *RelativeDateTimeUnitStr(URelativeDateTimeUnit unit);
static const char *AbsoluteUnitStr(UDateAbsoluteUnit unit);

typedef struct WithQuantityExpected {
    double value;
    UDateDirection direction;
    UDateRelativeUnit unit;
    const char *expected;
} WithQuantityExpected;

typedef struct WithoutQuantityExpected {
    UDateDirection direction;
    UDateAbsoluteUnit unit;
    const char *expected;
} WithoutQuantityExpected;

static WithQuantityExpected kEnglish[] = {
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 0 seconds"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 0.5 seconds"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 1 second"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 2 seconds"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 0 minutes"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 0.5 minutes"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 1 minute"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 2 minutes"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 0 hours"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 0.5 hours"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 1 hour"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 2 hours"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 0 days"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 0.5 days"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 1 day"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 2 days"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 0 weeks"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 0.5 weeks"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 1 week"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 2 weeks"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 0 months"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 0.5 months"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 1 month"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 2 months"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 0 years"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 0.5 years"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 1 year"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 2 years"},
                
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "0 seconds ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "0.5 seconds ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "1 second ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "2 seconds ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "0 minutes ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "0.5 minutes ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "1 minute ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "2 minutes ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "0 hours ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "0.5 hours ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "1 hour ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "2 hours ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "0 days ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "0.5 days ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "1 day ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "2 days ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "0 weeks ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "0.5 weeks ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "1 week ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "2 weeks ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "0 months ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "0.5 months ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "1 month ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "2 months ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "0 years ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "0.5 years ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "1 year ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "2 years ago"} 
};

static WithQuantityExpected kEnglishCaps[] = {
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "In 0 seconds"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "In 0.5 seconds"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "In 1 second"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "In 2 seconds"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "In 0 minutes"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "In 0.5 minutes"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "In 1 minute"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "In 2 minutes"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "In 0 hours"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "In 0.5 hours"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "In 1 hour"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "In 2 hours"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "In 0 days"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "In 0.5 days"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "In 1 day"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "In 2 days"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "In 0 weeks"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "In 0.5 weeks"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "In 1 week"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "In 2 weeks"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "In 0 months"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "In 0.5 months"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "In 1 month"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "In 2 months"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "In 0 years"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "In 0.5 years"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "In 1 year"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "In 2 years"},
                
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "0 seconds ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "0.5 seconds ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "1 second ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "2 seconds ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "0 minutes ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "0.5 minutes ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "1 minute ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "2 minutes ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "0 hours ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "0.5 hours ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "1 hour ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "2 hours ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "0 days ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "0.5 days ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "1 day ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "2 days ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "0 weeks ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "0.5 weeks ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "1 week ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "2 weeks ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "0 months ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "0.5 months ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "1 month ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "2 months ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "0 years ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "0.5 years ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "1 year ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "2 years ago"} 
};

static WithQuantityExpected kEnglishShort[] = {
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 0 sec."},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 0.5 sec."},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 1 sec."},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 2 sec."},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 0 min."},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 0.5 min."},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 1 min."},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MINUTES, "in 2 min."},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 0 hr."},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 0.5 hr."},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 1 hr."},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_HOURS, "in 2 hr."},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 0 days"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 0.5 days"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 1 day"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_DAYS, "in 2 days"},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 0 wk."},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 0.5 wk."},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 1 wk."},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_WEEKS, "in 2 wk."},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 0 mo."},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 0.5 mo."},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 1 mo."},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "in 2 mo."},
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 0 yr."},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 0.5 yr."},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 1 yr."},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_YEARS, "in 2 yr."},
                
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "0 sec. ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "0.5 sec. ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "1 sec. ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_SECONDS, "2 sec. ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "0 min. ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "0.5 min. ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "1 min. ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MINUTES, "2 min. ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "0 hr. ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "0.5 hr. ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "1 hr. ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, "2 hr. ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "0 days ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "0.5 days ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "1 day ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, "2 days ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "0 wk. ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "0.5 wk. ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "1 wk. ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_WEEKS, "2 wk. ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "0 mo. ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "0.5 mo. ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "1 mo. ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_MONTHS, "2 mo. ago"},
        {0.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "0 yr. ago"},
        {0.5, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "0.5 yr. ago"},
        {1.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "1 yr. ago"},
        {2.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_YEARS, "2 yr. ago"} 
};

static WithQuantityExpected kEnglishDecimal[] = {
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 0.0 seconds"},
        {0.5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 0.5 seconds"},
        {1.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 1.0 seconds"},
        {2.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, "in 2.0 seconds"}
};

static WithQuantityExpected kSerbian[] = {
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "\\u0437\\u0430 0 \\u043c\\u0435\\u0441\\u0435\\u0446\\u0438"},
        {1.2, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "\\u0437\\u0430 1,2 \\u043c\\u0435\\u0441\\u0435\\u0446\\u0430"},
        {21.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "\\u0437\\u0430 21 \\u043c\\u0435\\u0441\\u0435\\u0446"}
};

static WithQuantityExpected kSerbianNarrow[] = {
        {0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "\\u0437\\u0430 0 \\u043c."},
        {1.2, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "\\u0437\\u0430 1,2 \\u043c."},
        {21.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, "\\u0437\\u0430 21 \\u043c."}
};

static WithoutQuantityExpected kEnglishNoQuantity[] = {
        {UDAT_DIRECTION_NEXT_2, UDAT_ABSOLUTE_DAY, ""},
                
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_DAY, "tomorrow"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEEK, "next week"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONTH, "next month"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_QUARTER, "next quarter"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_YEAR, "next year"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONDAY, "next Monday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_TUESDAY, "next Tuesday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEDNESDAY, "next Wednesday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_THURSDAY, "next Thursday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_FRIDAY, "next Friday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SATURDAY, "next Saturday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SUNDAY, "next Sunday"},
        
        {UDAT_DIRECTION_LAST_2, UDAT_ABSOLUTE_DAY, ""},
        
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_DAY, "yesterday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEEK, "last week"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONTH, "last month"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_QUARTER, "last quarter"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_YEAR, "last year"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONDAY, "last Monday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_TUESDAY, "last Tuesday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEDNESDAY, "last Wednesday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_THURSDAY, "last Thursday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_FRIDAY, "last Friday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SATURDAY, "last Saturday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SUNDAY, "last Sunday"},
         
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_DAY, "today"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEEK, "this week"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONTH, "this month"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_QUARTER, "this quarter"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_YEAR, "this year"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONDAY, "this Monday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_TUESDAY, "this Tuesday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEDNESDAY, "this Wednesday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_THURSDAY, "this Thursday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_FRIDAY, "this Friday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SATURDAY, "this Saturday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SUNDAY, "this Sunday"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_DAY, "day"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEEK, "week"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONTH, "month"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_QUARTER, "quarter"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_YEAR, "year"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONDAY, "Monday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_TUESDAY, "Tuesday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEDNESDAY, "Wednesday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_THURSDAY, "Thursday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_FRIDAY, "Friday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SATURDAY, "Saturday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SUNDAY, "Sunday"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_NOW, "now"}
};

static WithoutQuantityExpected kEnglishNoQuantityCaps[] = {
        {UDAT_DIRECTION_NEXT_2, UDAT_ABSOLUTE_DAY, ""},
                
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_DAY, "Tomorrow"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEEK, "Next week"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONTH, "Next month"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_QUARTER, "Next quarter"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_YEAR, "Next year"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONDAY, "Next Monday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_TUESDAY, "Next Tuesday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEDNESDAY, "Next Wednesday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_THURSDAY, "Next Thursday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_FRIDAY, "Next Friday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SATURDAY, "Next Saturday"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SUNDAY, "Next Sunday"},
        
        {UDAT_DIRECTION_LAST_2, UDAT_ABSOLUTE_DAY, ""},
        
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_DAY, "Yesterday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEEK, "Last week"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONTH, "Last month"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_QUARTER, "Last quarter"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_YEAR, "Last year"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONDAY, "Last Monday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_TUESDAY, "Last Tuesday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEDNESDAY, "Last Wednesday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_THURSDAY, "Last Thursday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_FRIDAY, "Last Friday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SATURDAY, "Last Saturday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SUNDAY, "Last Sunday"},
         
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_DAY, "Today"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEEK, "This week"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONTH, "This month"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_QUARTER, "This quarter"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_YEAR, "This year"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONDAY, "This Monday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_TUESDAY, "This Tuesday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEDNESDAY, "This Wednesday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_THURSDAY, "This Thursday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_FRIDAY, "This Friday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SATURDAY, "This Saturday"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SUNDAY, "This Sunday"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_DAY, "Day"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEEK, "Week"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONTH, "Month"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_QUARTER, "Quarter"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_YEAR, "Year"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONDAY, "Monday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_TUESDAY, "Tuesday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEDNESDAY, "Wednesday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_THURSDAY, "Thursday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_FRIDAY, "Friday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SATURDAY, "Saturday"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SUNDAY, "Sunday"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_NOW, "Now"}
};

static WithoutQuantityExpected kEnglishNoQuantityShort[] = {
        {UDAT_DIRECTION_NEXT_2, UDAT_ABSOLUTE_DAY, ""},
                
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_DAY, "tomorrow"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEEK, "next wk."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONTH, "next mo."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_QUARTER, "next qtr."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_YEAR, "next yr."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONDAY, "next Mon."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_TUESDAY, "next Tue."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEDNESDAY, "next Wed."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_THURSDAY, "next Thu."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_FRIDAY, "next Fri."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SATURDAY, "next Sat."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SUNDAY, "next Sun."},
        
        {UDAT_DIRECTION_LAST_2, UDAT_ABSOLUTE_DAY, ""},
        
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_DAY, "yesterday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEEK, "last wk."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONTH, "last mo."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_QUARTER, "last qtr."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_YEAR, "last yr."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONDAY, "last Mon."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_TUESDAY, "last Tue."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEDNESDAY, "last Wed."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_THURSDAY, "last Thu."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_FRIDAY, "last Fri."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SATURDAY, "last Sat."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SUNDAY, "last Sun."},
         
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_DAY, "today"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEEK, "this wk."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONTH, "this mo."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_QUARTER, "this qtr."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_YEAR, "this yr."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONDAY, "this Mon."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_TUESDAY, "this Tue."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEDNESDAY, "this Wed."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_THURSDAY, "this Thu."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_FRIDAY, "this Fri."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SATURDAY, "this Sat."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SUNDAY, "this Sun."},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_DAY, "day"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEEK, "wk."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONTH, "mo."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_QUARTER, "qtr."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_YEAR, "yr."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONDAY, "Mo"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_TUESDAY, "Tu"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEDNESDAY, "We"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_THURSDAY, "Th"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_FRIDAY, "Fr"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SATURDAY, "Sa"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SUNDAY, "Su"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_NOW, "now"}
};

static WithoutQuantityExpected kEnglishNoQuantityNarrow[] = {
        {UDAT_DIRECTION_NEXT_2, UDAT_ABSOLUTE_DAY, ""},
                
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_DAY, "tomorrow"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEEK, "next wk."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONTH, "next mo."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_QUARTER, "next qtr."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_YEAR, "next yr."},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONDAY, "next M"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_TUESDAY, "next Tu"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_WEDNESDAY, "next W"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_THURSDAY, "next Th"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_FRIDAY, "next F"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SATURDAY, "next Sa"},
        {UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_SUNDAY, "next Su"},
        
        {UDAT_DIRECTION_LAST_2, UDAT_ABSOLUTE_DAY, ""},
        
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_DAY, "yesterday"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEEK, "last wk."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONTH, "last mo."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_QUARTER, "last qtr."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_YEAR, "last yr."},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_MONDAY, "last M"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_TUESDAY, "last Tu"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_WEDNESDAY, "last W"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_THURSDAY, "last Th"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_FRIDAY, "last F"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SATURDAY, "last Sa"},
        {UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_SUNDAY, "last Su"},
         
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_DAY, "today"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEEK, "this wk."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONTH, "this mo."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_QUARTER, "this qtr."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_YEAR, "this yr."},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_MONDAY, "this M"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_TUESDAY, "this Tu"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_WEDNESDAY, "this W"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_THURSDAY, "this Th"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_FRIDAY, "this F"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SATURDAY, "this Sa"},
        {UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_SUNDAY, "this Su"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_DAY, "day"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEEK, "wk."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONTH, "mo."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_QUARTER, "qtr."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_YEAR, "yr."},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_MONDAY, "M"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_TUESDAY, "T"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_WEDNESDAY, "W"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_THURSDAY, "T"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_FRIDAY, "F"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SATURDAY, "S"},
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_SUNDAY, "S"},
        
        {UDAT_DIRECTION_PLAIN, UDAT_ABSOLUTE_NOW, "now"}
};

static WithoutQuantityExpected kSpanishNoQuantity[] = {
        {UDAT_DIRECTION_NEXT_2, UDAT_ABSOLUTE_DAY, "pasado ma\\u00F1ana"},
        {UDAT_DIRECTION_LAST_2, UDAT_ABSOLUTE_DAY, "anteayer"}
};

typedef struct WithQuantityExpectedRelativeDateTimeUnit {
    double value;
    URelativeDateTimeUnit unit;
    const char *expected;
} WithQuantityExpectedRelativeDateTimeUnit;

static WithQuantityExpectedRelativeDateTimeUnit kEnglishFormatNumeric[] = {
        {0.0, UDAT_REL_UNIT_SECOND, "in 0 seconds"},
        {0.5, UDAT_REL_UNIT_SECOND, "in 0.5 seconds"},
        {1.0, UDAT_REL_UNIT_SECOND, "in 1 second"},
        {2.0, UDAT_REL_UNIT_SECOND, "in 2 seconds"},
        {0.0, UDAT_REL_UNIT_MINUTE, "in 0 minutes"},
        {0.5, UDAT_REL_UNIT_MINUTE, "in 0.5 minutes"},
        {1.0, UDAT_REL_UNIT_MINUTE, "in 1 minute"},
        {2.0, UDAT_REL_UNIT_MINUTE, "in 2 minutes"},
        {0.0, UDAT_REL_UNIT_HOUR, "in 0 hours"},
        {0.5, UDAT_REL_UNIT_HOUR, "in 0.5 hours"},
        {1.0, UDAT_REL_UNIT_HOUR, "in 1 hour"},
        {2.0, UDAT_REL_UNIT_HOUR, "in 2 hours"},
        {0.0, UDAT_REL_UNIT_DAY, "in 0 days"},
        {0.5, UDAT_REL_UNIT_DAY, "in 0.5 days"},
        {1.0, UDAT_REL_UNIT_DAY, "in 1 day"},
        {2.0, UDAT_REL_UNIT_DAY, "in 2 days"},
        {0.0, UDAT_REL_UNIT_WEEK, "in 0 weeks"},
        {0.5, UDAT_REL_UNIT_WEEK, "in 0.5 weeks"},
        {1.0, UDAT_REL_UNIT_WEEK, "in 1 week"},
        {2.0, UDAT_REL_UNIT_WEEK, "in 2 weeks"},
        {0.0, UDAT_REL_UNIT_MONTH, "in 0 months"},
        {0.5, UDAT_REL_UNIT_MONTH, "in 0.5 months"},
        {1.0, UDAT_REL_UNIT_MONTH, "in 1 month"},
        {2.0, UDAT_REL_UNIT_MONTH, "in 2 months"},
        {0.0, UDAT_REL_UNIT_QUARTER, "in 0 quarters"},
        {0.5, UDAT_REL_UNIT_QUARTER, "in 0.5 quarters"},
        {1.0, UDAT_REL_UNIT_QUARTER, "in 1 quarter"},
        {2.0, UDAT_REL_UNIT_QUARTER, "in 2 quarters"},
        {0.0, UDAT_REL_UNIT_YEAR, "in 0 years"},
        {0.5, UDAT_REL_UNIT_YEAR, "in 0.5 years"},
        {1.0, UDAT_REL_UNIT_YEAR, "in 1 year"},
        {2.0, UDAT_REL_UNIT_YEAR, "in 2 years"},
        {0.0, UDAT_REL_UNIT_SUNDAY, "in 0 Sundays"},
        {0.5, UDAT_REL_UNIT_SUNDAY, "in 0.5 Sundays"},
        {1.0, UDAT_REL_UNIT_SUNDAY, "in 1 Sunday"},
        {2.0, UDAT_REL_UNIT_SUNDAY, "in 2 Sundays"},
        {0.0, UDAT_REL_UNIT_MONDAY, "in 0 Mondays"},
        {0.5, UDAT_REL_UNIT_MONDAY, "in 0.5 Mondays"},
        {1.0, UDAT_REL_UNIT_MONDAY, "in 1 Monday"},
        {2.0, UDAT_REL_UNIT_MONDAY, "in 2 Mondays"},
        {0.0, UDAT_REL_UNIT_TUESDAY, "in 0 Tuesdays"},
        {0.5, UDAT_REL_UNIT_TUESDAY, "in 0.5 Tuesdays"},
        {1.0, UDAT_REL_UNIT_TUESDAY, "in 1 Tuesday"},
        {2.0, UDAT_REL_UNIT_TUESDAY, "in 2 Tuesdays"},
        {0.0, UDAT_REL_UNIT_WEDNESDAY, "in 0 Wednesdays"},
        {0.5, UDAT_REL_UNIT_WEDNESDAY, "in 0.5 Wednesdays"},
        {1.0, UDAT_REL_UNIT_WEDNESDAY, "in 1 Wednesday"},
        {2.0, UDAT_REL_UNIT_WEDNESDAY, "in 2 Wednesdays"},
        {0.0, UDAT_REL_UNIT_THURSDAY, "in 0 Thursdays"},
        {0.5, UDAT_REL_UNIT_THURSDAY, "in 0.5 Thursdays"},
        {1.0, UDAT_REL_UNIT_THURSDAY, "in 1 Thursday"},
        {2.0, UDAT_REL_UNIT_THURSDAY, "in 2 Thursdays"},
        {0.0, UDAT_REL_UNIT_FRIDAY, "in 0 Fridays"},
        {0.5, UDAT_REL_UNIT_FRIDAY, "in 0.5 Fridays"},
        {1.0, UDAT_REL_UNIT_FRIDAY, "in 1 Friday"},
        {2.0, UDAT_REL_UNIT_FRIDAY, "in 2 Fridays"},
        {0.0, UDAT_REL_UNIT_SATURDAY, "in 0 Saturdays"},
        {0.5, UDAT_REL_UNIT_SATURDAY, "in 0.5 Saturdays"},
        {1.0, UDAT_REL_UNIT_SATURDAY, "in 1 Saturday"},
        {2.0, UDAT_REL_UNIT_SATURDAY, "in 2 Saturdays"},

        {-0.0, UDAT_REL_UNIT_SECOND, "0 seconds ago"},
        {-0.5, UDAT_REL_UNIT_SECOND, "0.5 seconds ago"},
        {-1.0, UDAT_REL_UNIT_SECOND, "1 second ago"},
        {-2.0, UDAT_REL_UNIT_SECOND, "2 seconds ago"},
        {-0.0, UDAT_REL_UNIT_MINUTE, "0 minutes ago"},
        {-0.5, UDAT_REL_UNIT_MINUTE, "0.5 minutes ago"},
        {-1.0, UDAT_REL_UNIT_MINUTE, "1 minute ago"},
        {-2.0, UDAT_REL_UNIT_MINUTE, "2 minutes ago"},
        {-0.0, UDAT_REL_UNIT_HOUR, "0 hours ago"},
        {-0.5, UDAT_REL_UNIT_HOUR, "0.5 hours ago"},
        {-1.0, UDAT_REL_UNIT_HOUR, "1 hour ago"},
        {-2.0, UDAT_REL_UNIT_HOUR, "2 hours ago"},
        {-0.0, UDAT_REL_UNIT_DAY, "0 days ago"},
        {-0.5, UDAT_REL_UNIT_DAY, "0.5 days ago"},
        {-1.0, UDAT_REL_UNIT_DAY, "1 day ago"},
        {-2.0, UDAT_REL_UNIT_DAY, "2 days ago"},
        {-0.0, UDAT_REL_UNIT_WEEK, "0 weeks ago"},
        {-0.5, UDAT_REL_UNIT_WEEK, "0.5 weeks ago"},
        {-1.0, UDAT_REL_UNIT_WEEK, "1 week ago"},
        {-2.0, UDAT_REL_UNIT_WEEK, "2 weeks ago"},
        {-0.0, UDAT_REL_UNIT_MONTH, "0 months ago"},
        {-0.5, UDAT_REL_UNIT_MONTH, "0.5 months ago"},
        {-1.0, UDAT_REL_UNIT_MONTH, "1 month ago"},
        {-2.0, UDAT_REL_UNIT_MONTH, "2 months ago"},
        {-0.0, UDAT_REL_UNIT_QUARTER, "0 quarters ago"},
        {-0.5, UDAT_REL_UNIT_QUARTER, "0.5 quarters ago"},
        {-1.0, UDAT_REL_UNIT_QUARTER, "1 quarter ago"},
        {-2.0, UDAT_REL_UNIT_QUARTER, "2 quarters ago"},
        {-0.0, UDAT_REL_UNIT_YEAR, "0 years ago"},
        {-0.5, UDAT_REL_UNIT_YEAR, "0.5 years ago"},
        {-1.0, UDAT_REL_UNIT_YEAR, "1 year ago"},
        {-2.0, UDAT_REL_UNIT_YEAR, "2 years ago"},
        {-0.0, UDAT_REL_UNIT_SUNDAY, "0 Sundays ago"},
        {-0.5, UDAT_REL_UNIT_SUNDAY, "0.5 Sundays ago"},
        {-1.0, UDAT_REL_UNIT_SUNDAY, "1 Sunday ago"},
        {-2.0, UDAT_REL_UNIT_SUNDAY, "2 Sundays ago"},
        {-0.0, UDAT_REL_UNIT_MONDAY, "0 Mondays ago"},
        {-0.5, UDAT_REL_UNIT_MONDAY, "0.5 Mondays ago"},
        {-1.0, UDAT_REL_UNIT_MONDAY, "1 Monday ago"},
        {-2.0, UDAT_REL_UNIT_MONDAY, "2 Mondays ago"},
        {-0.0, UDAT_REL_UNIT_TUESDAY, "0 Tuesdays ago"},
        {-0.5, UDAT_REL_UNIT_TUESDAY, "0.5 Tuesdays ago"},
        {-1.0, UDAT_REL_UNIT_TUESDAY, "1 Tuesday ago"},
        {-2.0, UDAT_REL_UNIT_TUESDAY, "2 Tuesdays ago"},
        {-0.0, UDAT_REL_UNIT_WEDNESDAY, "0 Wednesdays ago"},
        {-0.5, UDAT_REL_UNIT_WEDNESDAY, "0.5 Wednesdays ago"},
        {-1.0, UDAT_REL_UNIT_WEDNESDAY, "1 Wednesday ago"},
        {-2.0, UDAT_REL_UNIT_WEDNESDAY, "2 Wednesdays ago"},
        {-0.0, UDAT_REL_UNIT_THURSDAY, "0 Thursdays ago"},
        {-0.5, UDAT_REL_UNIT_THURSDAY, "0.5 Thursdays ago"},
        {-1.0, UDAT_REL_UNIT_THURSDAY, "1 Thursday ago"},
        {-2.0, UDAT_REL_UNIT_THURSDAY, "2 Thursdays ago"},
        {-0.0, UDAT_REL_UNIT_FRIDAY, "0 Fridays ago"},
        {-0.5, UDAT_REL_UNIT_FRIDAY, "0.5 Fridays ago"},
        {-1.0, UDAT_REL_UNIT_FRIDAY, "1 Friday ago"},
        {-2.0, UDAT_REL_UNIT_FRIDAY, "2 Fridays ago"},
        {-0.0, UDAT_REL_UNIT_SATURDAY, "0 Saturdays ago"},
        {-0.5, UDAT_REL_UNIT_SATURDAY, "0.5 Saturdays ago"},
        {-1.0, UDAT_REL_UNIT_SATURDAY, "1 Saturday ago"},
        {-2.0, UDAT_REL_UNIT_SATURDAY, "2 Saturdays ago"}
};

static WithQuantityExpectedRelativeDateTimeUnit kEnglishFormat[] = {
        {0.0, UDAT_REL_UNIT_SECOND, "now"},
        {0.5, UDAT_REL_UNIT_SECOND, "in 0.5 seconds"},
        {1.0, UDAT_REL_UNIT_SECOND, "in 1 second"},
        {2.0, UDAT_REL_UNIT_SECOND, "in 2 seconds"},
        {0.0, UDAT_REL_UNIT_MINUTE, "in 0 minutes"},
        {0.5, UDAT_REL_UNIT_MINUTE, "in 0.5 minutes"},
        {1.0, UDAT_REL_UNIT_MINUTE, "in 1 minute"},
        {2.0, UDAT_REL_UNIT_MINUTE, "in 2 minutes"},
        {0.0, UDAT_REL_UNIT_HOUR, "in 0 hours"},
        {0.5, UDAT_REL_UNIT_HOUR, "in 0.5 hours"},
        {1.0, UDAT_REL_UNIT_HOUR, "in 1 hour"},
        {2.0, UDAT_REL_UNIT_HOUR, "in 2 hours"},
        {0.0, UDAT_REL_UNIT_DAY, "today"},
        {0.5, UDAT_REL_UNIT_DAY, "in 0.5 days"},
        {1.0, UDAT_REL_UNIT_DAY, "tomorrow"},
        {2.0, UDAT_REL_UNIT_DAY, "in 2 days"},
        {0.0, UDAT_REL_UNIT_WEEK, "this week"},
        {0.5, UDAT_REL_UNIT_WEEK, "in 0.5 weeks"},
        {1.0, UDAT_REL_UNIT_WEEK, "next week"},
        {2.0, UDAT_REL_UNIT_WEEK, "in 2 weeks"},
        {0.0, UDAT_REL_UNIT_MONTH, "this month"},
        {0.5, UDAT_REL_UNIT_MONTH, "in 0.5 months"},
        {1.0, UDAT_REL_UNIT_MONTH, "next month"},
        {2.0, UDAT_REL_UNIT_MONTH, "in 2 months"},
        {0.0, UDAT_REL_UNIT_QUARTER, "this quarter"},
        {0.5, UDAT_REL_UNIT_QUARTER, "in 0.5 quarters"},
        {1.0, UDAT_REL_UNIT_QUARTER, "next quarter"},
        {2.0, UDAT_REL_UNIT_QUARTER, "in 2 quarters"},
        {0.0, UDAT_REL_UNIT_YEAR, "this year"},
        {0.5, UDAT_REL_UNIT_YEAR, "in 0.5 years"},
        {1.0, UDAT_REL_UNIT_YEAR, "next year"},
        {2.0, UDAT_REL_UNIT_YEAR, "in 2 years"},
        {0.0, UDAT_REL_UNIT_SUNDAY, "this Sunday"},
        {0.5, UDAT_REL_UNIT_SUNDAY, "in 0.5 Sundays"},
        {1.0, UDAT_REL_UNIT_SUNDAY, "next Sunday"},
        {2.0, UDAT_REL_UNIT_SUNDAY, "in 2 Sundays"},
        {0.0, UDAT_REL_UNIT_MONDAY, "this Monday"},
        {0.5, UDAT_REL_UNIT_MONDAY, "in 0.5 Mondays"},
        {1.0, UDAT_REL_UNIT_MONDAY, "next Monday"},
        {2.0, UDAT_REL_UNIT_MONDAY, "in 2 Mondays"},
        {0.0, UDAT_REL_UNIT_TUESDAY, "this Tuesday"},
        {0.5, UDAT_REL_UNIT_TUESDAY, "in 0.5 Tuesdays"},
        {1.0, UDAT_REL_UNIT_TUESDAY, "next Tuesday"},
        {2.0, UDAT_REL_UNIT_TUESDAY, "in 2 Tuesdays"},
        {0.0, UDAT_REL_UNIT_WEDNESDAY, "this Wednesday"},
        {0.5, UDAT_REL_UNIT_WEDNESDAY, "in 0.5 Wednesdays"},
        {1.0, UDAT_REL_UNIT_WEDNESDAY, "next Wednesday"},
        {2.0, UDAT_REL_UNIT_WEDNESDAY, "in 2 Wednesdays"},
        {0.0, UDAT_REL_UNIT_THURSDAY, "this Thursday"},
        {0.5, UDAT_REL_UNIT_THURSDAY, "in 0.5 Thursdays"},
        {1.0, UDAT_REL_UNIT_THURSDAY, "next Thursday"},
        {2.0, UDAT_REL_UNIT_THURSDAY, "in 2 Thursdays"},
        {0.0, UDAT_REL_UNIT_FRIDAY, "this Friday"},
        {0.5, UDAT_REL_UNIT_FRIDAY, "in 0.5 Fridays"},
        {1.0, UDAT_REL_UNIT_FRIDAY, "next Friday"},
        {2.0, UDAT_REL_UNIT_FRIDAY, "in 2 Fridays"},
        {0.0, UDAT_REL_UNIT_SATURDAY, "this Saturday"},
        {0.5, UDAT_REL_UNIT_SATURDAY, "in 0.5 Saturdays"},
        {1.0, UDAT_REL_UNIT_SATURDAY, "next Saturday"},
        {2.0, UDAT_REL_UNIT_SATURDAY, "in 2 Saturdays"},

        {-0.0, UDAT_REL_UNIT_SECOND, "now"},
        {-0.5, UDAT_REL_UNIT_SECOND, "0.5 seconds ago"},
        {-1.0, UDAT_REL_UNIT_SECOND, "1 second ago"},
        {-2.0, UDAT_REL_UNIT_SECOND, "2 seconds ago"},
        {-0.0, UDAT_REL_UNIT_MINUTE, "0 minutes ago"},
        {-0.5, UDAT_REL_UNIT_MINUTE, "0.5 minutes ago"},
        {-1.0, UDAT_REL_UNIT_MINUTE, "1 minute ago"},
        {-2.0, UDAT_REL_UNIT_MINUTE, "2 minutes ago"},
        {-0.0, UDAT_REL_UNIT_HOUR, "0 hours ago"},
        {-0.5, UDAT_REL_UNIT_HOUR, "0.5 hours ago"},
        {-1.0, UDAT_REL_UNIT_HOUR, "1 hour ago"},
        {-2.0, UDAT_REL_UNIT_HOUR, "2 hours ago"},
        {-0.0, UDAT_REL_UNIT_DAY, "today"},
        {-0.5, UDAT_REL_UNIT_DAY, "0.5 days ago"},
        {-1.0, UDAT_REL_UNIT_DAY, "yesterday"},
        {-2.0, UDAT_REL_UNIT_DAY, "2 days ago"},
        {-0.0, UDAT_REL_UNIT_WEEK, "this week"},
        {-0.5, UDAT_REL_UNIT_WEEK, "0.5 weeks ago"},
        {-1.0, UDAT_REL_UNIT_WEEK, "last week"},
        {-2.0, UDAT_REL_UNIT_WEEK, "2 weeks ago"},
        {-0.0, UDAT_REL_UNIT_MONTH, "this month"},
        {-0.5, UDAT_REL_UNIT_MONTH, "0.5 months ago"},
        {-1.0, UDAT_REL_UNIT_MONTH, "last month"},
        {-2.0, UDAT_REL_UNIT_MONTH, "2 months ago"},
        {-0.0, UDAT_REL_UNIT_QUARTER, "this quarter"},
        {-0.5, UDAT_REL_UNIT_QUARTER, "0.5 quarters ago"},
        {-1.0, UDAT_REL_UNIT_QUARTER, "last quarter"},
        {-2.0, UDAT_REL_UNIT_QUARTER, "2 quarters ago"},
        {-0.0, UDAT_REL_UNIT_YEAR, "this year"},
        {-0.5, UDAT_REL_UNIT_YEAR, "0.5 years ago"},
        {-1.0, UDAT_REL_UNIT_YEAR, "last year"},
        {-2.0, UDAT_REL_UNIT_YEAR, "2 years ago"},
        {-0.0, UDAT_REL_UNIT_SUNDAY, "this Sunday"},
        {-0.5, UDAT_REL_UNIT_SUNDAY, "0.5 Sundays ago"},
        {-1.0, UDAT_REL_UNIT_SUNDAY, "last Sunday"},
        {-2.0, UDAT_REL_UNIT_SUNDAY, "2 Sundays ago"},
        {-0.0, UDAT_REL_UNIT_MONDAY, "this Monday"},
        {-0.5, UDAT_REL_UNIT_MONDAY, "0.5 Mondays ago"},
        {-1.0, UDAT_REL_UNIT_MONDAY, "last Monday"},
        {-2.0, UDAT_REL_UNIT_MONDAY, "2 Mondays ago"},
        {-0.0, UDAT_REL_UNIT_TUESDAY, "this Tuesday"},
        {-0.5, UDAT_REL_UNIT_TUESDAY, "0.5 Tuesdays ago"},
        {-1.0, UDAT_REL_UNIT_TUESDAY, "last Tuesday"},
        {-2.0, UDAT_REL_UNIT_TUESDAY, "2 Tuesdays ago"},
        {-0.0, UDAT_REL_UNIT_WEDNESDAY, "this Wednesday"},
        {-0.5, UDAT_REL_UNIT_WEDNESDAY, "0.5 Wednesdays ago"},
        {-1.0, UDAT_REL_UNIT_WEDNESDAY, "last Wednesday"},
        {-2.0, UDAT_REL_UNIT_WEDNESDAY, "2 Wednesdays ago"},
        {-0.0, UDAT_REL_UNIT_THURSDAY, "this Thursday"},
        {-0.5, UDAT_REL_UNIT_THURSDAY, "0.5 Thursdays ago"},
        {-1.0, UDAT_REL_UNIT_THURSDAY, "last Thursday"},
        {-2.0, UDAT_REL_UNIT_THURSDAY, "2 Thursdays ago"},
        {-0.0, UDAT_REL_UNIT_FRIDAY, "this Friday"},
        {-0.5, UDAT_REL_UNIT_FRIDAY, "0.5 Fridays ago"},
        {-1.0, UDAT_REL_UNIT_FRIDAY, "last Friday"},
        {-2.0, UDAT_REL_UNIT_FRIDAY, "2 Fridays ago"},
        {-0.0, UDAT_REL_UNIT_SATURDAY, "this Saturday"},
        {-0.5, UDAT_REL_UNIT_SATURDAY, "0.5 Saturdays ago"},
        {-1.0, UDAT_REL_UNIT_SATURDAY, "last Saturday"},
        {-2.0, UDAT_REL_UNIT_SATURDAY, "2 Saturdays ago"}
};


class RelativeDateTimeFormatterTest : public IntlTestWithFieldPosition {
public:
    RelativeDateTimeFormatterTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestEnglish();
    void TestEnglishCaps();
    void TestEnglishShort();
    void TestEnglishNarrow();
    void TestSerbian();
    void TestSerbianFallback();
    void TestEnglishNoQuantity();
    void TestEnglishNoQuantityCaps();
    void TestEnglishNoQuantityShort();
    void TestEnglishNoQuantityNarrow();
    void TestSpanishNoQuantity();
    void TestFormatWithQuantityIllegalArgument();
    void TestFormatWithoutQuantityIllegalArgument();
    void TestCustomNumberFormat();
    void TestGetters();
    void TestCombineDateAndTime();
    void TestBadDisplayContext();
    void TestFormat();
    void TestFormatNumeric();
    void TestLocales();
    void TestFields();
    void TestRBNF();

    void RunTest(
            const Locale& locale,
            const WithQuantityExpected* expectedResults,
            int32_t expectedResultLength);
    void RunTest(
            const Locale& locale,
            const WithQuantityExpectedRelativeDateTimeUnit* expectedResults,
            int32_t expectedResultLength,
            bool numeric);
    void RunTest(
            const Locale& locale,
            UDateRelativeDateTimeFormatterStyle style,
            const WithQuantityExpected* expectedResults,
            int32_t expectedResultLength);
    void RunTest(
            const Locale& locale,
            const WithoutQuantityExpected* expectedResults,
            int32_t expectedResultLength);
    void RunTest(
            const Locale& locale,
            UDateRelativeDateTimeFormatterStyle style,
            const WithoutQuantityExpected* expectedResults,
            int32_t expectedResultLength);
    void RunTest(
            const RelativeDateTimeFormatter& fmt,
            const WithQuantityExpected* expectedResults,
            int32_t expectedResultLength,
            const char *description);
    void RunTest(
            const RelativeDateTimeFormatter& fmt,
            const WithQuantityExpectedRelativeDateTimeUnit* expectedResults,
            int32_t expectedResultLength,
            const char *description,
            bool numeric);
    void RunTest(
            const RelativeDateTimeFormatter& fmt,
            const WithoutQuantityExpected* expectedResults,
            int32_t expectedResultLength,
            const char *description);
    void CheckExpectedResult(
            const RelativeDateTimeFormatter& fmt,
            const WithQuantityExpected& expectedResult,
            const char* description);
    void CheckExpectedResult(
            const RelativeDateTimeFormatter& fmt,
            const WithQuantityExpectedRelativeDateTimeUnit& expectedResults,
            const char* description,
            bool numeric);
    void CheckExpectedResult(
            const RelativeDateTimeFormatter& fmt,
            const WithoutQuantityExpected& expectedResult,
            const char* description);
    void VerifyIllegalArgument(
            const RelativeDateTimeFormatter& fmt,
            UDateDirection direction,
            UDateRelativeUnit unit);
    void VerifyIllegalArgument(
            const RelativeDateTimeFormatter& fmt,
            UDateDirection direction,
            UDateAbsoluteUnit unit);
    void TestSidewaysDataLoading(void);
};

void RelativeDateTimeFormatterTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite RelativeDateTimeFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestEnglish);
    TESTCASE_AUTO(TestEnglishCaps);
    TESTCASE_AUTO(TestEnglishShort);
    TESTCASE_AUTO(TestEnglishNarrow);
    TESTCASE_AUTO(TestSerbian);
    TESTCASE_AUTO(TestSerbianFallback);
    TESTCASE_AUTO(TestEnglishNoQuantity);
    TESTCASE_AUTO(TestEnglishNoQuantityCaps);
    TESTCASE_AUTO(TestEnglishNoQuantityShort);
    TESTCASE_AUTO(TestEnglishNoQuantityNarrow);
    TESTCASE_AUTO(TestSpanishNoQuantity);
    TESTCASE_AUTO(TestFormatWithQuantityIllegalArgument);
    TESTCASE_AUTO(TestFormatWithoutQuantityIllegalArgument);
    TESTCASE_AUTO(TestCustomNumberFormat);
    TESTCASE_AUTO(TestGetters);
    TESTCASE_AUTO(TestCombineDateAndTime);
    TESTCASE_AUTO(TestBadDisplayContext);
    TESTCASE_AUTO(TestSidewaysDataLoading);
    TESTCASE_AUTO(TestFormat);
    TESTCASE_AUTO(TestFormatNumeric);
    TESTCASE_AUTO(TestLocales);
    TESTCASE_AUTO(TestFields);
    TESTCASE_AUTO(TestRBNF);
    TESTCASE_AUTO_END;
}

void RelativeDateTimeFormatterTest::TestEnglish() {
    RunTest("en", kEnglish, UPRV_LENGTHOF(kEnglish));
}

void RelativeDateTimeFormatterTest::TestEnglishCaps() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(
            "en",
            NULL,
            UDAT_STYLE_LONG,
            UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
            status);
    if (U_FAILURE(status)) {
        dataerrln("Failed call to RelativeDateTimeFormatter(\"en\", NULL, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, status); : %s", u_errorName(status));
        return;
    }
    RelativeDateTimeFormatter fmt3(status);

    // Test assignment and copy constructor with capitalization on.
    RelativeDateTimeFormatter fmt2(fmt);
    fmt3 = fmt2;
    assertSuccess("", status);
    RunTest(fmt3, kEnglishCaps, UPRV_LENGTHOF(kEnglishCaps), "en caps");
}

void RelativeDateTimeFormatterTest::TestEnglishShort() {
    RunTest("en", UDAT_STYLE_SHORT, kEnglishShort, UPRV_LENGTHOF(kEnglishShort));
}

void RelativeDateTimeFormatterTest::TestEnglishNarrow() {
    RunTest("en", UDAT_STYLE_NARROW, kEnglishShort, UPRV_LENGTHOF(kEnglishShort));
}

void RelativeDateTimeFormatterTest::TestSerbian() {
    RunTest("sr", kSerbian, UPRV_LENGTHOF(kSerbian));
}

void RelativeDateTimeFormatterTest::TestSerbianFallback() {
    RunTest("sr", UDAT_STYLE_NARROW, kSerbianNarrow, UPRV_LENGTHOF(kSerbianNarrow));
}

void RelativeDateTimeFormatterTest::TestEnglishNoQuantity() {
    RunTest("en", kEnglishNoQuantity, UPRV_LENGTHOF(kEnglishNoQuantity));
}

void RelativeDateTimeFormatterTest::TestEnglishNoQuantityCaps() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(
            "en",
            NULL,
            UDAT_STYLE_LONG,
            UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
            status);
    if (assertSuccess("RelativeDateTimeFormatter", status, TRUE) == FALSE) {
        return;
    }
    RunTest(
            fmt,
            kEnglishNoQuantityCaps,
            UPRV_LENGTHOF(kEnglishNoQuantityCaps),
            "en caps no quantity");
}

void RelativeDateTimeFormatterTest::TestEnglishNoQuantityShort() {
    RunTest(
            "en",
            UDAT_STYLE_SHORT,
            kEnglishNoQuantityShort,
            UPRV_LENGTHOF(kEnglishNoQuantityShort));
}

void RelativeDateTimeFormatterTest::TestEnglishNoQuantityNarrow() {
    RunTest(
            "en",
            UDAT_STYLE_NARROW,
            kEnglishNoQuantityNarrow,
            UPRV_LENGTHOF(kEnglishNoQuantityNarrow));
}

void RelativeDateTimeFormatterTest::TestSpanishNoQuantity() {
    RunTest("es", kSpanishNoQuantity, UPRV_LENGTHOF(kSpanishNoQuantity));
}

void RelativeDateTimeFormatterTest::TestFormatWithQuantityIllegalArgument() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt("en", status);
    if (U_FAILURE(status)) {
        dataerrln("Failure creating format object - %s", u_errorName(status));
        return;
    }
    VerifyIllegalArgument(fmt, UDAT_DIRECTION_PLAIN, UDAT_RELATIVE_DAYS);
    VerifyIllegalArgument(fmt, UDAT_DIRECTION_THIS, UDAT_RELATIVE_DAYS);
}

void RelativeDateTimeFormatterTest::TestFormatWithoutQuantityIllegalArgument() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt("en", status);
    if (U_FAILURE(status)) {
        dataerrln("Failure creating format object - %s", u_errorName(status));
        return;
    }
    VerifyIllegalArgument(fmt, UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_NOW);
    VerifyIllegalArgument(fmt, UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_NOW);
    VerifyIllegalArgument(fmt, UDAT_DIRECTION_THIS, UDAT_ABSOLUTE_NOW);
}

void RelativeDateTimeFormatterTest::TestCustomNumberFormat() {
    NumberFormat *nf;
    UErrorCode status = U_ZERO_ERROR;
    {
        RelativeDateTimeFormatter fmt("en", status);
        if (U_FAILURE(status)) {
            dataerrln(
                    "Failure creating format object - %s", u_errorName(status));
            return;
        }
        nf = (NumberFormat *) fmt.getNumberFormat().clone();
    }
    nf->setMinimumFractionDigits(1);
    nf->setMaximumFractionDigits(1);
    RelativeDateTimeFormatter fmt("en", nf, status);

    // Test copy constructor.
    RelativeDateTimeFormatter fmt2(fmt);
    RunTest(fmt2, kEnglishDecimal, UPRV_LENGTHOF(kEnglishDecimal), "en decimal digits");

    // Test assignment
    fmt = RelativeDateTimeFormatter("es", status);
    RunTest(fmt, kSpanishNoQuantity, UPRV_LENGTHOF(kSpanishNoQuantity), "assignment operator");

}

void RelativeDateTimeFormatterTest::TestGetters() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(
            "en",
            NULL,
            UDAT_STYLE_NARROW,
            UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
            status);
    if (U_FAILURE(status)) {
        dataerrln("Failed call to RelativeDateTimeFormatter(\"en\", NULL, UDAT_STYLE_NARROW, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, status);) : %s", u_errorName(status));
        return;
    }
    RelativeDateTimeFormatter fmt3(status);

    // copy and assignment.
    RelativeDateTimeFormatter fmt2(fmt);
    fmt3 = fmt2;
    assertEquals("style", (int32_t)UDAT_STYLE_NARROW, fmt3.getFormatStyle());
    assertEquals(
            "context",
            (int32_t)UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
            fmt3.getCapitalizationContext());
    assertSuccess("", status);
}

void RelativeDateTimeFormatterTest::TestCombineDateAndTime() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt("en", status);
    if (U_FAILURE(status)) {
        dataerrln("Failure creating format object - %s", u_errorName(status));
        return;
    }
    UnicodeString actual;
    fmt.combineDateAndTime(
        UnicodeString("yesterday"),
        UnicodeString("3:50"),
        actual,
        status);
    UnicodeString expected("yesterday, 3:50");
    if (expected != actual) {
        errln("Expected "+expected+", got "+actual);
    }
}
    
void RelativeDateTimeFormatterTest::TestBadDisplayContext() {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(
            "en", NULL, UDAT_STYLE_LONG, UDISPCTX_STANDARD_NAMES, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR, got %s", u_errorName(status));
    }
}
    


void RelativeDateTimeFormatterTest::RunTest(
        const Locale& locale,
        const WithQuantityExpected* expectedResults,
        int32_t expectedResultLength) {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(locale, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create format object - %s", u_errorName(status));
        return;
    }
    RunTest(fmt, expectedResults, expectedResultLength, locale.getName());
}

void RelativeDateTimeFormatterTest::RunTest(
        const Locale& locale,
        const WithQuantityExpectedRelativeDateTimeUnit* expectedResults,
        int32_t expectedResultLength,
        bool numeric) {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(locale, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create format object - %s", u_errorName(status));
        return;
    }
    RunTest(fmt, expectedResults, expectedResultLength, locale.getName(), numeric);
}


void RelativeDateTimeFormatterTest::RunTest(
        const Locale& locale,
        UDateRelativeDateTimeFormatterStyle style,
        const WithQuantityExpected* expectedResults,
        int32_t expectedResultLength) {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(
            locale, NULL, style, UDISPCTX_CAPITALIZATION_NONE, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create format object - %s", u_errorName(status));
        return;
    }
    RunTest(fmt, expectedResults, expectedResultLength, locale.getName());
}

void RelativeDateTimeFormatterTest::RunTest(
        const Locale& locale,
        const WithoutQuantityExpected* expectedResults,
        int32_t expectedResultLength) {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(locale, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create format object - %s", u_errorName(status));
        return;
    }
    RunTest(fmt, expectedResults, expectedResultLength, locale.getName());
}

void RelativeDateTimeFormatterTest::RunTest(
        const Locale& locale,
        UDateRelativeDateTimeFormatterStyle style,
        const WithoutQuantityExpected* expectedResults,
        int32_t expectedResultLength) {
    UErrorCode status = U_ZERO_ERROR;
    RelativeDateTimeFormatter fmt(
            locale, NULL, style, UDISPCTX_CAPITALIZATION_NONE, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create format object - %s", u_errorName(status));
        return;
    }
    RunTest(fmt, expectedResults, expectedResultLength, locale.getName());
}

void RelativeDateTimeFormatterTest::RunTest(
        const RelativeDateTimeFormatter& fmt,
        const WithQuantityExpected* expectedResults,
        int32_t expectedResultLength,
        const char *description) {
    for (int32_t i = 0; i < expectedResultLength; ++i) {
        CheckExpectedResult(fmt, expectedResults[i], description);
    }
}

void RelativeDateTimeFormatterTest::RunTest(
        const RelativeDateTimeFormatter& fmt,
        const WithQuantityExpectedRelativeDateTimeUnit* expectedResults,
        int32_t expectedResultLength,
        const char *description,
        bool numeric) {
    for (int32_t i = 0; i < expectedResultLength; ++i) {
        CheckExpectedResult(fmt, expectedResults[i], description, numeric);
    }
}

void RelativeDateTimeFormatterTest::RunTest(
        const RelativeDateTimeFormatter& fmt,
        const WithoutQuantityExpected* expectedResults,
        int32_t expectedResultLength,
        const char *description) {
    for (int32_t i = 0; i < expectedResultLength; ++i) {
        CheckExpectedResult(fmt, expectedResults[i], description);
    }
}

void RelativeDateTimeFormatterTest::CheckExpectedResult(
        const RelativeDateTimeFormatter& fmt,
        const WithQuantityExpected& expectedResult,
        const char* description) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString actual;
    fmt.format(expectedResult.value, expectedResult.direction, expectedResult.unit, actual, status);
    UnicodeString expected(expectedResult.expected, -1, US_INV);
    expected = expected.unescape();
    char buffer[256];
    sprintf(
            buffer,
            "%s, %f, %s, %s",
            description,
            expectedResult.value,
            DirectionStr(expectedResult.direction),
            RelativeUnitStr(expectedResult.unit));
    if (actual != expected) {
        errln(UnicodeString("Fail: Expected: ") + expected
                + ", Got: " + actual
                + ", For: " + buffer);
    }
}

void RelativeDateTimeFormatterTest::CheckExpectedResult(
        const RelativeDateTimeFormatter& fmt,
        const WithQuantityExpectedRelativeDateTimeUnit& expectedResult,
        const char* description,
        bool numeric) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString actual;
    if (numeric) {
      fmt.formatNumeric(expectedResult.value, expectedResult.unit, actual, status);
    } else {
      fmt.format(expectedResult.value, expectedResult.unit, actual, status);
    }
    UnicodeString expected(expectedResult.expected, -1, US_INV);
    expected = expected.unescape();
    char buffer[256];
    sprintf(
            buffer,
            "%s, %f, %s",
            description,
            expectedResult.value,
            RelativeDateTimeUnitStr(expectedResult.unit));
    if (actual != expected) {
        errln(UnicodeString("Fail: Expected: ") + expected
                + ", Got: " + actual
                + ", For: " + buffer);
    }
}

void RelativeDateTimeFormatterTest::CheckExpectedResult(
        const RelativeDateTimeFormatter& fmt,
        const WithoutQuantityExpected& expectedResult,
        const char* description) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString actual;
    fmt.format(expectedResult.direction, expectedResult.unit, actual, status);
    UnicodeString expected(expectedResult.expected, -1, US_INV);
    expected = expected.unescape();
    char buffer[256];
    sprintf(
            buffer,
            "%s, %s, %s",
            description,
            DirectionStr(expectedResult.direction),
            AbsoluteUnitStr(expectedResult.unit));
    if (actual != expected) {
        errln(UnicodeString("Fail: Expected: ") + expected
                + ", Got: " + actual
                + ", For: " + buffer);
    }
}

void RelativeDateTimeFormatterTest::VerifyIllegalArgument(
        const RelativeDateTimeFormatter& fmt,
        UDateDirection direction,
        UDateRelativeUnit unit) {
    UnicodeString appendTo;
    UErrorCode status = U_ZERO_ERROR;
    fmt.format(1.0, direction, unit, appendTo, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR, got %s", u_errorName(status));
    }
}

void RelativeDateTimeFormatterTest::VerifyIllegalArgument(
        const RelativeDateTimeFormatter& fmt,
        UDateDirection direction,
        UDateAbsoluteUnit unit) {
    UnicodeString appendTo;
    UErrorCode status = U_ZERO_ERROR;
    fmt.format(direction, unit, appendTo, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR, got %s", u_errorName(status));
    }
}

/* Add tests to check "sideways" data loading. */
void RelativeDateTimeFormatterTest::TestSidewaysDataLoading(void) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString actual;
    UnicodeString expected;
    Locale enGbLocale("en_GB");

    RelativeDateTimeFormatter fmt(enGbLocale, NULL, UDAT_STYLE_NARROW,
                                  UDISPCTX_CAPITALIZATION_NONE, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create RelativeDateTimeFormatter - %s", u_errorName(status));
        return;
    }

    status = U_ZERO_ERROR;
    actual = "";
    fmt.format(3.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_MONTHS, actual, status);
    expected = "in 3 mo";
    assertEquals("narrow in 3 mo", expected, actual);

    fmt.format(3.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, actual.remove(), status);
    expected = "3 days ago";
    assertEquals("3 days ago (positive 3.0): ", expected, actual);

    expected = "-3 days ago";
    fmt.format(-3.0, UDAT_DIRECTION_LAST, UDAT_RELATIVE_DAYS, actual.remove(), status);
    assertEquals("3 days ago (negative 3.0): ", expected, actual);

    expected = "next yr.";
    fmt.format(UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_YEAR, actual.remove(), status);
    assertEquals("next year: ", expected, actual);

    // Testing the SHORT style
    RelativeDateTimeFormatter fmtshort(enGbLocale, NULL, UDAT_STYLE_SHORT,
                                  UDISPCTX_CAPITALIZATION_NONE, status);
    expected = "now";
    fmtshort.format(0.0, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, actual.remove(), status);

    expected = "next yr.";
    fmt.format(UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_YEAR, actual.remove(), status);
    assertEquals("next year: ", expected, actual);
}

void RelativeDateTimeFormatterTest::TestFormatNumeric() {
    RunTest("en", kEnglishFormatNumeric, UPRV_LENGTHOF(kEnglishFormatNumeric), true);
}

void RelativeDateTimeFormatterTest::TestFormat() {
    RunTest("en", kEnglishFormat, UPRV_LENGTHOF(kEnglishFormat), false);
}

void RelativeDateTimeFormatterTest::TestLocales() {
    int32_t numLocales = 0;
    const Locale *availableLocales = Locale::getAvailableLocales(numLocales);
    std::vector<std::unique_ptr<RelativeDateTimeFormatter>> allFormatters;
    for (int localeIdx=0; localeIdx<numLocales; localeIdx++) {
        const Locale &loc = availableLocales[localeIdx];
        UErrorCode status = U_ZERO_ERROR;
        std::unique_ptr<RelativeDateTimeFormatter> rdtf(new RelativeDateTimeFormatter(loc, status));
        allFormatters.push_back(std::move(rdtf));
        assertSuccess(loc.getName(), status);
    }
}

void RelativeDateTimeFormatterTest::TestFields() {
    IcuTestErrorCode status(*this, "TestFields");

    RelativeDateTimeFormatter fmt("en-US", status);

    {
        const char16_t* message = u"automatic absolute unit";
        FormattedRelativeDateTime fv = fmt.formatToValue(1, UDAT_REL_UNIT_DAY, status);
        const char16_t* expectedString = u"tomorrow";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 0, 8}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char16_t* message = u"automatic numeric unit";
        FormattedRelativeDateTime fv = fmt.formatToValue(3, UDAT_REL_UNIT_DAY, status);
        const char16_t* expectedString = u"in 3 days";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 0, 2},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 3, 4},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_NUMERIC_FIELD, 3, 4},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 5, 9}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char16_t* message = u"manual absolute unit";
        FormattedRelativeDateTime fv = fmt.formatToValue(UDAT_DIRECTION_NEXT, UDAT_ABSOLUTE_MONDAY, status);
        const char16_t* expectedString = u"next Monday";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 0, 11}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char16_t* message = u"manual numeric unit";
        FormattedRelativeDateTime fv = fmt.formatNumericToValue(1.5, UDAT_REL_UNIT_WEEK, status);
        const char16_t* expectedString = u"in 1.5 weeks";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 0, 2},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 3, 4},
            {UFIELD_CATEGORY_NUMBER, UNUM_DECIMAL_SEPARATOR_FIELD, 4, 5},
            {UFIELD_CATEGORY_NUMBER, UNUM_FRACTION_FIELD, 5, 6},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_NUMERIC_FIELD, 3, 6},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 7, 12}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char16_t* message = u"manual numeric resolved unit";
        FormattedRelativeDateTime fv = fmt.formatToValue(12, UDAT_DIRECTION_LAST, UDAT_RELATIVE_HOURS, status);
        const char16_t* expectedString = u"12 hours ago";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 0, 2},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_NUMERIC_FIELD, 0, 2},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 3, 12}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    // Test when the number field is at the end
    fmt = RelativeDateTimeFormatter("sw", status);
    {
        const char16_t* message = u"numeric field at end";
        FormattedRelativeDateTime fv = fmt.formatToValue(12, UDAT_REL_UNIT_HOUR, status);
        const char16_t* expectedString = u"baada ya saa 12";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 0, 12},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 13, 15},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_NUMERIC_FIELD, 13, 15}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
}

void RelativeDateTimeFormatterTest::TestRBNF() {
    IcuTestErrorCode status(*this, "TestRBNF");

    LocalPointer<RuleBasedNumberFormat> rbnf(new RuleBasedNumberFormat(URBNF_SPELLOUT, "en-us", status));
    if (status.errIfFailureAndReset()) { return; }
    RelativeDateTimeFormatter fmt("en-us", rbnf.orphan(), status);
    UnicodeString result;
    assertEquals("format (direction)", "in five seconds",
        fmt.format(5, UDAT_DIRECTION_NEXT, UDAT_RELATIVE_SECONDS, result, status));
    assertEquals("formatNumeric", "one week ago",
        fmt.formatNumeric(-1, UDAT_REL_UNIT_WEEK, result.remove(), status));
    assertEquals("format (absolute)", "yesterday",
        fmt.format(UDAT_DIRECTION_LAST, UDAT_ABSOLUTE_DAY, result.remove(), status));
    assertEquals("format (relative)", "in forty-two months",
        fmt.format(42, UDAT_REL_UNIT_MONTH, result.remove(), status));

    {
        const char16_t* message = u"formatToValue (relative)";
        FormattedRelativeDateTime fv = fmt.formatToValue(-100, UDAT_REL_UNIT_YEAR, status);
        const char16_t* expectedString = u"one hundred years ago";
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_NUMERIC_FIELD, 0, 11},
            {UFIELD_CATEGORY_RELATIVE_DATETIME, UDAT_REL_LITERAL_FIELD, 12, 21}};
        checkMixedFormattedValue(
            message,
            fv,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
}

static const char *kLast2 = "Last_2";
static const char *kLast = "Last";
static const char *kThis = "This";
static const char *kNext = "Next";
static const char *kNext2 = "Next_2";
static const char *kPlain = "Plain";

static const char *kSeconds = "Seconds";
static const char *kMinutes = "Minutes";
static const char *kHours = "Hours";
static const char *kDays = "Days";
static const char *kWeeks = "Weeks";
static const char *kMonths = "Months";
static const char *kQuarters = "Quarters";
static const char *kYears = "Years";
static const char *kSundays = "Sundays";
static const char *kMondays = "Mondays";
static const char *kTuesdays = "Tuesdays";
static const char *kWednesdays = "Wednesdays";
static const char *kThursdays = "Thursdays";
static const char *kFridays = "Fridays";
static const char *kSaturdays = "Saturdays";

static const char *kSunday = "Sunday";
static const char *kMonday = "Monday";
static const char *kTuesday = "Tuesday";
static const char *kWednesday = "Wednesday";
static const char *kThursday = "Thursday";
static const char *kFriday = "Friday";
static const char *kSaturday = "Saturday";
static const char *kDay = "Day";
static const char *kWeek = "Week";
static const char *kMonth = "Month";
static const char *kQuarter = "Quarter";
static const char *kYear = "Year";
static const char *kNow = "Now";

static const char *kUndefined = "Undefined";

static const char *DirectionStr(
        UDateDirection direction) {
    switch (direction) {
        case UDAT_DIRECTION_LAST_2:
            return kLast2;
        case UDAT_DIRECTION_LAST:
            return kLast;
        case UDAT_DIRECTION_THIS:
            return kThis;
        case UDAT_DIRECTION_NEXT:
            return kNext;
        case UDAT_DIRECTION_NEXT_2:
            return kNext2;
        case UDAT_DIRECTION_PLAIN:
            return kPlain;
        default:
            return kUndefined;
    }
    return kUndefined;
}

static const char *RelativeUnitStr(
        UDateRelativeUnit unit) {
    switch (unit) {
        case UDAT_RELATIVE_SECONDS:
            return kSeconds;
        case UDAT_RELATIVE_MINUTES:
            return kMinutes;
        case UDAT_RELATIVE_HOURS:
            return kHours;
        case UDAT_RELATIVE_DAYS:
            return kDays;
        case UDAT_RELATIVE_WEEKS:
            return kWeeks;
        case UDAT_RELATIVE_MONTHS:
            return kMonths;
        case UDAT_RELATIVE_YEARS:
            return kYears;
        default:
            return kUndefined;
    }
    return kUndefined;
}

static const char *RelativeDateTimeUnitStr(
        URelativeDateTimeUnit  unit) {
    switch (unit) {
        case UDAT_REL_UNIT_SECOND:
            return kSeconds;
        case UDAT_REL_UNIT_MINUTE:
            return kMinutes;
        case UDAT_REL_UNIT_HOUR:
            return kHours;
        case UDAT_REL_UNIT_DAY:
            return kDays;
        case UDAT_REL_UNIT_WEEK:
            return kWeeks;
        case UDAT_REL_UNIT_MONTH:
            return kMonths;
        case UDAT_REL_UNIT_QUARTER:
            return kQuarters;
        case UDAT_REL_UNIT_YEAR:
            return kYears;
        case UDAT_REL_UNIT_SUNDAY:
            return kSundays;
        case UDAT_REL_UNIT_MONDAY:
            return kMondays;
        case UDAT_REL_UNIT_TUESDAY:
            return kTuesdays;
        case UDAT_REL_UNIT_WEDNESDAY:
            return kWednesdays;
        case UDAT_REL_UNIT_THURSDAY:
            return kThursdays;
        case UDAT_REL_UNIT_FRIDAY:
            return kFridays;
        case UDAT_REL_UNIT_SATURDAY:
            return kSaturdays;
        default:
            return kUndefined;
    }
    return kUndefined;
}

static const char *AbsoluteUnitStr(
        UDateAbsoluteUnit unit) {
    switch (unit) {
        case UDAT_ABSOLUTE_SUNDAY:
            return kSunday;
        case UDAT_ABSOLUTE_MONDAY:
            return kMonday;
        case UDAT_ABSOLUTE_TUESDAY:
            return kTuesday;
        case UDAT_ABSOLUTE_WEDNESDAY:
            return kWednesday;
        case UDAT_ABSOLUTE_THURSDAY:
            return kThursday;
        case UDAT_ABSOLUTE_FRIDAY:
            return kFriday;
        case UDAT_ABSOLUTE_SATURDAY:
            return kSaturday;
        case UDAT_ABSOLUTE_DAY:
            return kDay;
        case UDAT_ABSOLUTE_WEEK:
            return kWeek;
        case UDAT_ABSOLUTE_MONTH:
            return kMonth;
        case UDAT_ABSOLUTE_QUARTER:
            return kQuarter;
        case UDAT_ABSOLUTE_YEAR:
            return kYear;
        case UDAT_ABSOLUTE_NOW:
            return kNow;
        default:
            return kUndefined;
    }
    return kUndefined;
}

extern IntlTest *createRelativeDateTimeFormatterTest() {
    return new RelativeDateTimeFormatterTest();
}

#endif
