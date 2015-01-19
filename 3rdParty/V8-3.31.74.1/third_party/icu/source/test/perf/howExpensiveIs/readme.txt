# -*- mode: org -*-
# Copyright (C) 2012 International Business Machines Corporation and Others. All Rights Reserved. 

How Expensive Is It?

* Introduction
** The purpose of this test is to answer the question, "How expensive, relatively speaking, is ICU operation X?"
** ICU tests are compared with general purpose CPU operations to attempt to factor out differences between systems and load conditions
** Different ICU operations will have different levels of dependence on CPU, memory, disk, etc. So nothing can perfectly factor these conditions out.
* Running the Test
** Simply run "make check" in this directory, icu/source/test/perf/howExpensiveIs/
** Try to minimize other CPU loading throughout the test
** The test will take some time to run!
** Test runs outside of a margin of error will be thrown out. So, this will tend to produce more accurate results.
** After some time, the file howexpensive.xml will be created (an example is attached as Appendix I)
** The results may be read directly or processed such as with xslt.
** XML file contents:
*** Element <tests>: the outermost element. 
*** Attribute 'icu':  gives the basic ICU version number.
**** Element <test>: a particularized test.
***** Attribute 'name': names the particular test, see howExpensiveIs.cpp for details
***** Attribute 'standardizedTime': The SieveTest by definition has a standardized test of 1, it runs a prime number sieve as a benchmark. All other standardizedTimes are normalized against this value.
***** Attribute 'realDuration': the actual duration, in seconds, of the test.
***** Attribute 'marginOfError': the amount +/- error for the real duration. Gives an idea of how much variability was in the test. 
***** Attribute 'iterations': gives the total number of iterations run.
**** Element <icuSystemParams>: This element gives the full details of the target platform, in the XML format produced by the 'icuinfo' tool.  The contents are informative only and not documented here.
* Analysis
** The data shows that, for example, parsing a number and opening the GB18030 converter are about the same cost. It also shows that opening a number formatter is about 60 times as expensive as formatting a number.
** Appendix II shows a .CSV (spreadsheet) file which shows analysis of a sample run between different systems. 
** The Variation column for each Target system was calculated with the formula:  "(Control-Target)/Control" where Control and Target are the standardized times for the Control and Target systems, respectively.
* Appendices
** Appendix I: Sample File
<?xml version="1.0" encoding="UTF-8" ?>
<tests icu="49.0.2">
<!--  Copyright (C) 2011, International Business Machines Corporation and others. All Rights Reserved.  -->
   <test name="SieveTest" standardizedTime="1.000000" realDuration="0.022804" marginOfError="0.000073" iterations="1000000" />
   <test name="NullTest" standardizedTime="0.000017" realDuration="0.000000" marginOfError="0.000000" iterations="1000000" />
   <test name="NumParseTest" standardizedTime="77.869922" realDuration="1.775721" marginOfError="0.011742" iterations="1000000" />
   <test name="Test_unum_opendefault" standardizedTime="4855.974258" realDuration="110.734117" marginOfError="0.057131" iterations="1000000" />
   <test name="Test_ucnv_opengb18030" standardizedTime="70.488403" realDuration="1.607395" marginOfError="0.009261" iterations="1000000" />
 <icuSystemParams type="icu4c">
    <param name="copyright"> Copyright (C) 2011, International Business Machines Corporation and others. All Rights Reserved. </param>
    <param name="product">icu4c</param>
    <param name="product.full">International Components for Unicode for C/C++</param>
    <param name="version">49.0.2</param>
    <param name="version.unicode">6.1</param>
    <param name="platform.number">4000</param>
    <param name="platform.type">Other (POSIX-like)</param>
    <param name="locale.default">en_US</param>
    <param name="locale.default.bcp47">en-US</param>
    <param name="converter.default">UTF-8</param>
    <param name="icudata.name">icudt49l</param>
    <param name="icudata.path">../../data/out/build/icudt49l</param>
    <param name="cldr.version">21.0</param>
    <param name="tz.version">2011n</param>
    <param name="tz.default">America/Los_Angeles</param>
    <param name="cpu.bits">64</param>
    <param name="cpu.big_endian">0</param>
    <param name="os.wchar_width">4</param>
    <param name="os.charset_family">0</param>
    <param name="os.host">x86_64-unknown-linux-gnu</param>
    <param name="build.build">x86_64-unknown-linux-gnu</param>
    <param name="build.cc">gcc</param>
    <param name="build.cxx">g++</param>
 </icuSystemParams>
</tests>
** Appendix II: Analysis.csv
http://bugs.icu-project.org/trac/ticket/8653,"""Control"", linux i7 
Intel(R) Core(TM) i7-2720QM CPU @ 2.20GHz",MacBook 2.4ghz (Core2D),MacBook 2GhzCore2,AIX Power,MB 2.4 Variance,MB 2 variance,AIX Variance
SieveTest (=1.0),1,1,1,1,0.00%,0.00%,0.00%
NullTest (=0.0),0,0,0,0.08,#DIV/0!,#DIV/0!,#DIV/0!
NumParseTest,74.10642,21.220191493,56.912133,85.423525612,71.37%,23.20%,-15.27%
Test_unum_opendefault,4801.617798,1912.860018319,4522.900036,2580.805294162,60.16%,5.80%,46.25%
Test_ucnv_opengb18030,65.268309,30.547740077,84.075584,51.587649619,53.20%,-28.82%,20.96%
Test_unum_openpattern,4394.214773,1735.453339382,4472.298154,2263.471671239,60.51%,-1.78%,48.49%
Test_ures_openroot,75.302253,23.773982586,71.248439,57.471889114,68.43%,5.38%,23.68%
** Appendix III: Revision History
*** Feb 2012, ICU 49, srl: First revision
