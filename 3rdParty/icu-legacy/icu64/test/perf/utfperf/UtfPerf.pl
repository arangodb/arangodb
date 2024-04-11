#!/usr/bin/perl
#  ********************************************************************
#  * Copyright (C) 2016 and later: Unicode, Inc. and others.
#  * License & terms of use: http://www.unicode.org/copyright.html#License
#  ********************************************************************
#  ********************************************************************
#  * COPYRIGHT:
#  * Copyright (c) 2005-2013, International Business Machines Corporation and
#  * others. All Rights Reserved.
#  ********************************************************************

#use strict;

require "../perldriver/Common.pl";

use lib '../perldriver';

use PerfFramework;

my $options = {
    "title"=>"UTF performance: ICU (".$ICUPreviousVersion." and ".$ICULatestVersion.")",
    "headers"=>"ICU".$ICUPreviousVersion." ICU".$ICULatestVersion,
    "operationIs"=>"gb18030 encoding string",
    "passes"=>"1",
    "time"=>"2",
    #"outputType"=>"HTML",
    "dataDir"=>$ConversionDataPath,
    "outputDir"=>"../results"
};

# programs
# tests will be done for all the programs. Results will be stored and connected
my $p1;
my $p2;

if ($OnWindows) {
    $p1 = "cd ".$ICUPrevious."/bin && ".$ICUPathPrevious."/utfperf/$WindowsPlatform/Release/utfperf.exe -e gb18030"; # Previous
    $p2 = "cd ".$ICULatest."/bin && ".$ICUPathLatest."/utfperf/$WindowsPlatform/Release/utfperf.exe -e gb18030"; # Latest
} else {
    $p1 = "LD_LIBRARY_PATH=".$ICUPrevious."/source/lib:".$ICUPrevious."/source/tools/ctestfw ".$ICUPathPrevious."/utfperf/utfperf -e gb18030"; # Previous
    $p2 = "LD_LIBRARY_PATH=".$ICULatest."/source/lib:".$ICULatest."/source/tools/ctestfw ".$ICUPathLatest."/utfperf/utfperf -e gb18030"; # Latest
}

my $tests = { 
    "Roundtrip",      ["$p1,Roundtrip",        "$p2,Roundtrip"],
    "FromUnicode",    ["$p1,FromUnicode",      "$p2,FromUnicode"],
    "FromUTF8",       ["$p1,FromUTF8",         "$p2,FromUTF8"],
};

my $dataFiles = {
    "", ["xuzhimo.txt"]
};

runTests($options, $tests, $dataFiles);
