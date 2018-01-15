@echo off
copy /y styles.css ..\..\..\..\filesystem-gh-pages
copy /y deprecated.html ..\..\..\..\filesystem-gh-pages
copy /y design.htm ..\..\..\..\filesystem-gh-pages
copy /y faq.htm ..\..\..\..\filesystem-gh-pages
copy /y index.htm ..\..\..\..\filesystem-gh-pages
copy /y issue_reporting.html ..\..\..\..\filesystem-gh-pages
copy /y portability_guide.htm ..\..\..\..\filesystem-gh-pages
copy /y reference.html ..\..\..\..\filesystem-gh-pages
copy /y relative_proposal.html ..\..\..\..\filesystem-gh-pages
copy /y release_history.html ..\..\..\..\filesystem-gh-pages
copy /y tutorial.html ..\..\..\..\filesystem-gh-pages
copy /y v3.html ..\..\..\..\filesystem-gh-pages
copy /y v3_design.html ..\..\..\..\filesystem-gh-pages
pushd ..\..\..\..\filesystem-gh-pages
git commit -a -m "copy from develop"
git push
popd
rem Copyright Beman Dawes, 2015
rem Distributed under the Boost Software License, Version 1.0.
rem See www.boost.org/LICENSE_1_0.txt