#!/usr/bin/perl
#  ********************************************************************
#  * Copyright (C) 2016 and later: Unicode, Inc. and others.
#  * License & terms of use: http://www.unicode.org/copyright.html
#  ********************************************************************
#  ********************************************************************
#  * COPYRIGHT:
#  * Copyright (c) 2002-2013, International Business Machines
#  * Corporation and others. All Rights Reserved.
#  ********************************************************************

#use strict;

require "../perldriver/Common.pl";

use lib '../perldriver';

use PerfFramework;

my $options = {
    "title"=>"Normalization performance regression: ICU (".$ICUPreviousVersion." and ".$ICULatestVersion.")",
    "headers"=>"ICU".$ICUPreviousVersion." ICU".$ICULatestVersion,
    "operationIs"=>"code point",
    "timePerOperationIs"=>"Time per code point",
    "passes"=>"10",
    "time"=>"5",
    #"outputType"=>"HTML",
    "dataDir"=>$CollationDataPath,
    "outputDir"=>"../results"
};

# programs

my $p1; # Previous
my $p2; # Latest

if ($OnWindows) {
    $p1 = "cd ".$ICUPrevious."/bin && ".$ICUPathPrevious."/normperf/$WindowsPlatform/Release/normperf.exe";
    $p2 = "cd ".$ICULatest."/bin && ".$ICUPathLatest."/normperf/$WindowsPlatform/Release/normperf.exe";
} else {
    $p1 = "LD_LIBRARY_PATH=".$ICUPrevious."/source/lib:".$ICUPrevious."/source/tools/ctestfw ".$ICUPathPrevious."/normperf/normperf";
    $p2 = "LD_LIBRARY_PATH=".$ICULatest."/source/lib:".$ICULatest."/source/tools/ctestfw ".$ICUPathLatest."/normperf/normperf";
}

my $dataFiles = {
    "",
    [
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
        "th18057.txt",
        "thesis.txt",
        "vfear11a.txt",
    ]
};


my $tests = { 
    "NFC_NFD_Text",  ["$p1,TestICU_NFC_NFD_Text"  ,  "$p2,TestICU_NFC_NFD_Text" ],
    "NFC_NFC_Text",  ["$p1,TestICU_NFC_NFC_Text"  ,  "$p2,TestICU_NFC_NFC_Text" ],
    "NFC_Orig_Text", ["$p1,TestICU_NFC_Orig_Text" ,  "$p2,TestICU_NFC_Orig_Text"],
    "NFD_NFD_Text",  ["$p1,TestICU_NFD_NFD_Text"  ,  "$p2,TestICU_NFD_NFD_Text" ],
    "NFD_NFC_Text",  ["$p1,TestICU_NFD_NFC_Text"  ,  "$p2,TestICU_NFD_NFC_Text" ],
    "NFD_Orig_Text", ["$p1,TestICU_NFD_Orig_Text" ,  "$p2,TestICU_NFD_Orig_Text"],
    ##
    "QC_NFC_NFD_Text",  ["$p1,TestQC_NFC_NFD_Text"  ,  "$p2,TestQC_NFC_NFD_Text" ],
    "QC_NFC_NFC_Text",  ["$p1,TestQC_NFC_NFC_Text"  ,  "$p2,TestQC_NFC_NFC_Text" ],
    "QC_NFC_Orig_Text", ["$p1,TestQC_NFC_Orig_Text" ,  "$p2,TestQC_NFC_Orig_Text"],
    "QC_NFD_NFD_Text",  ["$p1,TestQC_NFD_NFD_Text"  ,  "$p2,TestQC_NFD_NFD_Text" ],
    "QC_NFD_NFC_Text",  ["$p1,TestQC_NFD_NFC_Text"  ,  "$p2,TestQC_NFD_NFC_Text" ],
    "QC_NFD_Orig_Text", ["$p1,TestQC_NFD_Orig_Text" ,  "$p2,TestQC_NFD_Orig_Text"],
    ##
    "IsNormalized_NFC_NFD_Text",  ["$p1,TestIsNormalized_NFC_NFD_Text"  ,  "$p2,TestIsNormalized_NFC_NFD_Text" ],
    "IsNormalized_NFC_NFC_Text",  ["$p1,TestIsNormalized_NFC_NFC_Text"  ,  "$p2,TestIsNormalized_NFC_NFC_Text" ],
    "IsNormalized_NFC_Orig_Text", ["$p1,TestIsNormalized_NFC_Orig_Text" ,  "$p2,TestIsNormalized_NFC_Orig_Text"],
    "IsNormalized_NFD_NFD_Text",  ["$p1,TestIsNormalized_NFD_NFD_Text"  ,  "$p2,TestIsNormalized_NFD_NFD_Text" ],
    "IsNormalized_NFD_NFC_Text",  ["$p1,TestIsNormalized_NFD_NFC_Text"  ,  "$p2,TestIsNormalized_NFD_NFC_Text" ],
    "IsNormalized_NFD_Orig_Text", ["$p1,TestIsNormalized_NFD_Orig_Text" ,  "$p2,TestIsNormalized_NFD_Orig_Text"]
};


runTests($options, $tests, $dataFiles);


