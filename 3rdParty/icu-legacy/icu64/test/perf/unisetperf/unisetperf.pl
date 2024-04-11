#!/usr/bin/perl
#  ********************************************************************
#  * Copyright (C) 2016 and later: Unicode, Inc. and others.
#  * License & terms of use: http://www.unicode.org/copyright.html
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
    "title"=>"UnicodeSet span()/contains() performance",
    "headers"=>"Bv Bv0",
    "operationIs"=>"tested Unicode code point",
    "passes"=>"3",
    "time"=>"2",
    #"outputType"=>"HTML",
    "dataDir"=>$UDHRDataPath,
    "outputDir"=>"../results"
};

# programs
# tests will be done for all the programs. Results will be stored and connected
my $p;
if ($OnWindows) {
    $p = "cd ".$ICULatest."/bin && ".$ICUPathLatest."/unisetperf/$WindowsPlatform/Release/unisetperf.exe";
} else {
    $p = "LD_LIBRARY_PATH=".$ICULatest."/source/lib:".$ICULatest."/source/tools/ctestfw ".$ICUPathLatest."/unisetperf/unisetperf";
}

my $tests = {
    "Contains",
    [
        "$p,Contains --type Bv",
        "$p,Contains --type Bv0"
    ],
    "SpanUTF16",
    [
        "$p,SpanUTF16 --type Bv",
        "$p,SpanUTF16 --type Bv0"
    ]
};

my $dataFiles = {
    "",
    [
        "udhr_eng.txt",
        "udhr_deu_1996.txt",
        "udhr_fra.txt",
        "udhr_rus.txt",
        "udhr_tha.txt",
        "udhr_jpn.txt",
        "udhr_cmn_hans.txt",
        "udhr_cmn_hant.txt",
        "udhr_jpn.html"
    ]
};

runTests($options, $tests, $dataFiles);

$options = {
    "title"=>"UnicodeSet span()/contains() performance",
    "headers"=>"Bv BvF Bvp BvpF L Bvl",
    "operationIs"=>"tested Unicode code point",
    "passes"=>"3",
    "time"=>"2",
    #"outputType"=>"HTML",
    "dataDir"=>$UDHRDataPath,
    "outputDir"=>"../results"
};

$tests = {
    "SpanUTF8",
    [
        "$p,SpanUTF8 --type Bv",
        "$p,SpanUTF8 --type BvF",
        "$p,SpanUTF8 --type Bvp",
        "$p,SpanUTF8 --type BvpF",
        "$p,SpanUTF8 --type L",
        "$p,SpanUTF8 --type Bvl"
    ]
};

runTests($options, $tests, $dataFiles);
