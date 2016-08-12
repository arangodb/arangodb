@echo off
copy /y tutorial.html tutorial.bak
html_include_files +# tutorial.bak tutorial.html
tutorial.html
rem Copyright Beman Dawes 2015 
rem Distributed under the Boost Software License, Version 1.0.
rem See http://www.boost.org/LICENSE_1_0.txt
