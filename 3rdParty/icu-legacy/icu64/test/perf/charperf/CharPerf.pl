#!/usr/bin/perl
#  ********************************************************************
#  * Copyright (C) 2016 and later: Unicode, Inc. and others.
#  * License & terms of use: http://www.unicode.org/copyright.html
#  ********************************************************************
#  ********************************************************************
#  * COPYRIGHT:
#  * Copyright (c) 2002-2013, International Business Machines Corporation and
#  * others. All Rights Reserved.
#  ********************************************************************


#use strict;

require "../perldriver/Common.pl";

use lib '../perldriver';

use PerfFramework;

my $options = {
    "title"=>"Character property performance: ICU".$ICULatestVersion." vs. STDLib",
    "headers"=>"StdLib ICU".$ICULatestVersion,
    "operationIs"=>"code point",
    "timePerOperationIs"=>"Time per code point",
    "passes"=>"10",
    "time"=>"5",
    #"outputType"=>"HTML",
    "dataDir"=>"Not Using Data Files",
    "outputDir"=>"../results"
};


# programs
# tests will be done for all the programs. Results will be stored and connected
my $p;
if ($OnWindows) {
    $p = "cd ".$ICULatest."/bin && ".$ICUPathLatest."/charperf/$WindowsPlatform/Release/charperf.exe";
} else {
    $p = "LD_LIBRARY_PATH=".$ICULatest."/source/lib:".$ICULatest."/source/tools/ctestfw ".$ICUPathLatest."/charperf/charperf";
}

my $tests = { 
    "isAlpha",        ["$p,TestStdLibIsAlpha"        , "$p,TestIsAlpha"        ],
    "isUpper",        ["$p,TestStdLibIsUpper"        , "$p,TestIsUpper"        ],
    "isLower",        ["$p,TestStdLibIsLower"        , "$p,TestIsLower"        ],
    "isDigit",        ["$p,TestStdLibIsDigit"        , "$p,TestIsDigit"        ],
    "isSpace",        ["$p,TestStdLibIsSpace"        , "$p,TestIsSpace"        ],
    "isAlphaNumeric", ["$p,TestStdLibIsAlphaNumeric" , "$p,TestIsAlphaNumeric" ],
    "isPrint",        ["$p,TestStdLibIsPrint"        , "$p,TestIsPrint"        ],
    "isControl",      ["$p,TestStdLibIsControl"      , "$p,TestIsControl"      ],
    "toLower",        ["$p,TestStdLibToLower"        , "$p,TestToLower"        ],
    "toUpper",        ["$p,TestStdLibToUpper"        , "$p,TestToUpper"        ],
    "isWhiteSpace",   ["$p,TestStdLibIsWhiteSpace"   , "$p,TestIsWhiteSpace"   ],
};

my $dataFiles;

runTests($options, $tests, $dataFiles);
