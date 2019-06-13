@echo off

if not $%1==$--help goto nohelp
echo Invoke: boost_test [-ts toolset] [b2-options]
echo Default -ts is gcc-c++03,gcc-c++11,gcc-c++14,msvc-12.0,msvc-14.0,msvc-14.1
echo Example: boost_test -ts msvc-12.0 -a "define=BOOST_SOME_MACRO" config_info
goto done
:nohelp

if $%1==$-ts goto toolset

echo Begin test processing...
b2 -j3 --v2 --dump-tests --toolset=gcc-c++03,gcc-c++11,gcc-c++14,msvc-12.0,msvc-14.0,msvc-14.1 %* >b2.log 2>&1
goto jam_log

:toolset
echo Begin test processing...
b2 -j3 --v2 --dump-tests --toolset=%2 %3 %4 %5 %6 %7 %8 %9  >b2.log 2>&1

:jam_log
echo Begin log processing...
process_jam_log --v2 <b2.log
start b2.log
grep -i warning b2.log | sort | uniq

echo Begin compiler status processing...
compiler_status --v2 . test_status.html test_links.html
start test_status.html

:done
