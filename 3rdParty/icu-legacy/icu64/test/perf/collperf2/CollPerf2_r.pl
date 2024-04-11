#!/usr/bin/perl
# ********************************************************************
#  Copyright (C) 2016 and later: Unicode, Inc. and others.
#  License & terms of use: http://www.unicode.org/copyright.html
# ********************************************************************
# ********************************************************************
#  COPYRIGHT:
#  Copyright (c) 2013-2014, International Business Machines Corporation and
#  others. All Rights Reserved.
# ********************************************************************

# Variables need to be set in ../perldriver/Common.pl for where ICU is on your machine.
# Copy Common.pl.template to Common.pl and modify it.
#
# Sample Common.pl "Settings by user" for a Linux out-of-source build:
#
# $ICULatestVersion = "collv2";
# $ICUPreviousVersion = "52";
#
# $PerformanceDataPath = "/home/mscherer/svn.icudata/trunk/src/test/perf";
#
# $ICULatest = "/home/mscherer/svn.icu/collv2/bld";
# $ICUPrevious = "/home/mscherer/svn.icu/trunk/bld";
#
# The first time around, you also need to
#   source/test/perf/collperf2$ mkdir ../results
# Then invoke
#   source/test/perf/collperf2$ ./CollPerf2_r.pl
#
# Sample debug invocation:
#   ~/svn.icu/trunk/dbg/test/perf/collperf2$ LD_LIBRARY_PATH=../../../lib:../../../tools/ctestfw ./collperf2 -t 5 -p 1  -L "de" -f /home/mscherer/svn.icudata/trunk/src/test/perf/collation/TestNames_Latin.txt TestStringPieceSort

#use strict;

use lib '../perldriver';

require "../perldriver/Common.pl";

use PerfFramework;

my $options = {
    "title"=>"Collation Performance Regression: ICU (".$ICUPreviousVersion." and ".$ICULatestVersion.")",
    "headers"=>"ICU".$ICUPreviousVersion." ICU".$ICULatestVersion,
    "operationIs"=>"Collator",
    "passes"=>"1",
    "time"=>"2",
    #"outputType"=>"HTML",
    "dataDir"=>$CollationDataPath,
    "outputDir"=>"../results"
};

# programs
# tests will be done for all the programs. Results will be stored and connected
my $p1, $p2;

if ($OnWindows) {
    $p1 = "cd ".$ICUPrevious."/bin && ".$ICUPathPrevious."/collperf2/$WindowsPlatform/Release/collperf2.exe";
    $p2 = "cd ".$ICULatest."/bin && ".$ICUPathLatest."/collperf2/$WindowsPlatform/Release/collperf2.exe";
} else {
    $p1 = "LD_LIBRARY_PATH=".$ICUPrevious."/lib:".$ICUPrevious."/tools/ctestfw ".$ICUPrevious."/test/perf/collperf2/collperf2";
    $p2 = "LD_LIBRARY_PATH=".$ICULatest."/lib:".$ICULatest."/tools/ctestfw ".$ICULatest."/test/perf/collperf2/collperf2";
}

my $tests = {
    "ucol_strcoll/len",             ["$p1,TestStrcoll", "$p2,TestStrcoll"],
    "ucol_strcoll/null",            ["$p1,TestStrcollNull", "$p2,TestStrcollNull"],
    "ucol_strcoll/len/similar",     ["$p1,TestStrcollSimilar", "$p2,TestStrcollSimilar"],

    "ucol_strcollUTF8/len",         ["$p1,TestStrcollUTF8", "$p2,TestStrcollUTF8"],
    "ucol_strcollUTF8/null",        ["$p1,TestStrcollUTF8Null", "$p2,TestStrcollUTF8Null"],
    "ucol_strcollUTF8/len/similar", ["$p1,TestStrcollUTF8Similar", "$p2,TestStrcollUTF8Similar"],

    "ucol_getSortKey/len",          ["$p1,TestGetSortKey", "$p2,TestGetSortKey"],
    "ucol_getSortKey/null",         ["$p1,TestGetSortKeyNull", "$p2,TestGetSortKeyNull"],

    "ucol_nextSortKeyPart/4_all",   ["$p1,TestNextSortKeyPart_4All", "$p2,TestNextSortKeyPart_4All"],
    "ucol_nextSortKeyPart/4x4",     ["$p1,TestNextSortKeyPart_4x4", "$p2,TestNextSortKeyPart_4x4"],
    "ucol_nextSortKeyPart/4x8",     ["$p1,TestNextSortKeyPart_4x8", "$p2,TestNextSortKeyPart_4x8"],
    "ucol_nextSortKeyPart/32_all",  ["$p1,TestNextSortKeyPart_32All", "$p2,TestNextSortKeyPart_32All"],
    "ucol_nextSortKeyPart/32x2",    ["$p1,TestNextSortKeyPart_32x2", "$p2,TestNextSortKeyPart_32x2"],

    "ucol_nextSortKeyPart/UTF8/4_all",  ["$p1,TestNextSortKeyPartUTF8_4All", "$p2,TestNextSortKeyPartUTF8_4All"],
    "ucol_nextSortKeyPart/UTF8/4x4",    ["$p1,TestNextSortKeyPartUTF8_4x4", "$p2,TestNextSortKeyPartUTF8_4x4"],
    "ucol_nextSortKeyPart/UTF8/4x8",    ["$p1,TestNextSortKeyPartUTF8_4x8", "$p2,TestNextSortKeyPartUTF8_4x8"],
    "ucol_nextSortKeyPart/UTF8/32_all", ["$p1,TestNextSortKeyPartUTF8_32All", "$p2,TestNextSortKeyPartUTF8_32All"],
    "ucol_nextSortKeyPart/UTF8/32x2",   ["$p1,TestNextSortKeyPartUTF8_32x2", "$p2,TestNextSortKeyPartUTF8_32x2"],

    "Collator::compare/len",                ["$p1,TestCppCompare", "$p2,TestCppCompare"],
    "Collator::compare/null",               ["$p1,TestCppCompareNull", "$p2,TestCppCompareNull"],
    "Collator::compare/len/similar",        ["$p1,TestCppCompareSimilar", "$p2,TestCppCompareSimilar"],

    "Collator::compareUTF8/len",            ["$p1,TestCppCompareUTF8", "$p2,TestCppCompareUTF8"],
    "Collator::compareUTF8/null",           ["$p1,TestCppCompareUTF8Null", "$p2,TestCppCompareUTF8Null"],
    "Collator::compareUTF8/len/similar",    ["$p1,TestCppCompareUTF8Similar", "$p2,TestCppCompareUTF8Similar"],

    "Collator::getCollationKey/len",        ["$p1,TestCppGetCollationKey", "$p2,TestCppGetCollationKey"],
    "Collator::getCollationKey/null",       ["$p1,TestCppGetCollationKeyNull", "$p2,TestCppGetCollationKeyNull"],

    "sort UnicodeString*[]: compare()",         ["$p1,TestUniStrSort", "$p2,TestUniStrSort"],
    "sort StringPiece[]: compareUTF8()",        ["$p1,TestStringPieceSortCpp", "$p2,TestStringPieceSortCpp"],
    "sort StringPiece[]: ucol_strcollUTF8()",   ["$p1,TestStringPieceSortC", "$p2,TestStringPieceSortC"],

    "binary search UnicodeString*[]: compare()",        ["$p1,TestUniStrBinSearch", "$p2,TestUniStrBinSearch"],
    "binary search StringPiece[]: compareUTF8()",       ["$p1,TestStringPieceBinSearchCpp", "$p2,TestStringPieceBinSearchCpp"],
    "binary search StringPiece[]: ucol_strcollUTF8()",  ["$p1,TestStringPieceBinSearchC", "$p2,TestStringPieceBinSearchC"],
};

my $dataFiles = {
    "en_US",
    [
        "TestNames_Latin.txt"
    ],

    "de-u-co-phonebk-ks-level2",
    [
        "TestRandomWordsUDHR_de.txt"
    ],

    "fr-u-ks-level1-kc",
    [
        "TestRandomWordsUDHR_fr.txt"
    ],

    "es",
    [
        "TestRandomWordsUDHR_es.txt"
    ],

    "pl",
    [
        "TestRandomWordsUDHR_pl.txt"
    ],

    "tr",
    [
        "TestRandomWordsUDHR_tr.txt"
    ],

    "el",
    [
        "TestRandomWordsUDHR_el.txt"
    ],

    "ru",
    [
        "TestNames_Russian.txt"
    ],

    "ru-u-ks-level4-ka-shifted",
    [
        "TestRandomWordsUDHR_ru.txt"
    ],

    "ja",
    [
        "TestNames_Japanese.txt",
        "TestNames_Japanese_h.txt",
        "TestNames_Japanese_k.txt"
    ],

    "ja-u-ks-identic",
    [
        "TestNames_Latin.txt",
        "TestNames_Japanese.txt"
    ],

    "ko",
    [
        "TestNames_Korean.txt"
    ],

    "zh_Hans",
    [
        "TestNames_Simplified_Chinese.txt"
    ],

    "zh_Hans-u-co-pinyin",
    [
        "TestNames_Latin.txt",
        "TestNames_Simplified_Chinese.txt"
    ],

    "zh_Hant",
    [
        "TestNames_Chinese.txt",
    ],

    "th-u-kk-ka-shifted",
    [
        "TestNames_Latin.txt",
        "TestNames_Thai.txt",
        "TestRandomWordsUDHR_th.txt"
    ],

    "ar",
    [
        "TestRandomWordsUDHR_ar.txt"
    ],

    "he",
    [
        "TestRandomWordsUDHR_he.txt"
    ]
};

runTests($options, $tests, $dataFiles);
