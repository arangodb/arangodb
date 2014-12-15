#!/usr/bin/perl
#  ********************************************************************
#  * COPYRIGHT:
#  * Copyright (c) 2008-2013, International Business Machines
#  * Corporation and others. All Rights Reserved.
#  ********************************************************************

#use strict;

require "../perldriver/Common.pl";

use lib '../perldriver';

use PerfFramework;

my $options = {
    "title" => "String search performance regression: ICU ("
      . $ICUPreviousVersion . " and "
      . $ICULatestVersion . ")",
    "headers"     => "ICU" . $ICUPreviousVersion . " ICU" . $ICULatestVersion,
    "operationIs" => "code point",
    "timePerOperationIs" => "Time per code point",
    "passes"             => "10",
    "time"               => "5",

    #"outputType"=>"HTML",
    "dataDir"   => $UDHRDataPath,
    "outputDir" => "../results"
};

# programs

my $p1;    # Previous
my $p2;    # Latest

if ($OnWindows) {
    $p1 = "cd ".$ICUPrevious."/bin && ".$ICUPathPrevious . "/strsrchperf/$WindowsPlatform/Release/strsrchperf.exe -b";
    $p2 = "cd ".$ICULatest."/bin && ".$ICUPathLatest . "/strsrchperf/$WindowsPlatform/Release/strsrchperf.exe -b";
}
else {
    $p1 = "LD_LIBRARY_PATH=".$ICUPrevious."/source/lib:".$ICUPrevious."/source/tools/ctestfw ".$ICUPathPrevious . "/strsrchperf/strsrchperf -b";
    $p2 = "LD_LIBRARY_PATH=".$ICULatest."/source/lib:".$ICULatest."/source/tools/ctestfw ".$ICUPathLatest . "/strsrchperf/strsrchperf -b";
}

my $dataFiles = {
    "en" => ["udhr_eng.txt"],
    "de" => ["udhr_deu_1996.txt"],
    "fr" => ["udhr_fra.txt"],
    "ru" => ["udhr_rus.txt"],
    "th" => ["udhr_tha.txt"],
    "ja" => ["udhr_jpn.txt"],
    "zh" => ["udhr_cmn_hans.txt"],
};

my $tests = {
    "ICU Forward Search", [ "$p1 Test_ICU_Forward_Search", "$p2 Test_ICU_Forward_Search" ],
    "ICU Backward Search",[ "$p1 Test_ICU_Backward_Search", "$p2 Test_ICU_Backward_Search" ],
};

runTests( $options, $tests, $dataFiles );


