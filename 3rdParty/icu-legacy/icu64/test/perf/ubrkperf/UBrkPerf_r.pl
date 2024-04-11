#!/usr/bin/perl
#  ********************************************************************
#  * Copyright (C) 2016 and later: Unicode, Inc. and others.
#  * License & terms of use: http://www.unicode.org/copyright.html
#  ********************************************************************
#  ********************************************************************
#  * Copyright (c) 2002-2013, International Business Machines
#  * Corporation and others. All Rights Reserved.
#  ********************************************************************

#use strict;

require "../perldriver/Common.pl";

use lib '../perldriver';

use PerfFramework;

my $options = {
    "title"=>"BreakIterator performance regression: ICU (".$ICUPreviousVersion." and ".$ICULatestVersion.")",
    "headers"=>"ICU".$ICUPreviousVersion." ICU".$ICULatestVersion,
    "operationIs"=>"code point",
    "eventIs"=>"break",
    "passes"=>"10",
    "time"=>"5",
    #"outputType"=>"HTML",
    "dataDir"=>$CollationDataPath,
    "outputDir"=>"../results"
};

# programs
# tests will be done for all the programs. Results will be stored and connected
my $m1 = "-- -m char";
my $m2 = "-- -m word";
my $m3 = "-- -m line";
my $m4 = "-- -m sentence";

my $m;

if(@_ >= 0) {
  $m = "-- -m ".shift;
} else {
  $m = $m1;
}

my $p1; # Previous
my $p2; # Latest

if ($OnWindows) {
    $p1 = "cd ".$ICUPrevious."/bin && ".$ICUPathPrevious."/ubrkperf/$WindowsPlatform/Release/ubrkperf.exe";
    $p2 = "cd ".$ICULatest."/bin && ".$ICUPathLatest."/ubrkperf/$WindowsPlatform/Release/ubrkperf.exe";
} else {
    $p1 = "LD_LIBRARY_PATH=".$ICUPrevious."/source/lib:".$ICUPrevious."/source/tools/ctestfw ".$ICUPathPrevious."/ubrkperf/ubrkperf";
    $p2 = "LD_LIBRARY_PATH=".$ICULatest."/source/lib:".$ICULatest."/source/tools/ctestfw ".$ICUPathLatest."/ubrkperf/ubrkperf";
}

my $dataFiles = {
    "en",
    [
        "thesis.txt",
        "2drvb10.txt",
        "ulyss10.txt",
        "nvsbl10.txt",
        "vfear11a.txt",
        "TestNames_Asian.txt",
        "TestNames_Chinese.txt",
        "TestNames_Japanese.txt",
        "TestNames_Japanese_h.txt",
        "TestNames_Japanese_k.txt",
        "TestNames_Korean.txt",
        "TestNames_Latin.txt",
        "TestNames_SerbianSH.txt",
        "TestNames_SerbianSR.txt",
        "TestNames_Thai.txt",
        "TestNames_Russian.txt",
    ],
    "th",
    [
        "TestNames_Thai.txt",
        "th18057.txt"
    ]
};


my $tests = {
    "TestForwardChar",      ["$p1,$m1,TestICUForward", "$p2,$m1,TestICUForward"],
    "TestForwardWord",      ["$p1,$m2,TestICUForward", "$p2,$m2,TestICUForward"],
    "TestForwardLine",      ["$p1,$m3,TestICUForward", "$p2,$m3,TestICUForward"],
    "TestForwardSentence",  ["$p1,$m4,TestICUForward", "$p2,$m4,TestICUForward"],

    "TestIsBoundChar",      ["$p1,$m1,TestICUIsBound", "$p2,$m1,TestICUIsBound"],
    "TestIsBoundWord",      ["$p1,$m2,TestICUIsBound", "$p2,$m2,TestICUIsBound"],
    "TestIsBoundLine",      ["$p1,$m3,TestICUIsBound", "$p2,$m3,TestICUIsBound"],
    "TestIsBoundSentence",  ["$p1,$m4,TestICUIsBound", "$p2,$m4,TestICUIsBound"],
};

runTests($options, $tests, $dataFiles);


