copy /y c:\boost\modular\develop\libs\filesystem\doc\*.htm? C:\boost\filesystem-gh-pages
pushd C:\boost\filesystem-gh-pages
git commit -a -m "merge from develop"
git push
popd
rem Copyright Beman Dawes, 2015
rem Distributed under the Boost Software License, Version 1.0.
rem See www.boost.org/LICENSE_1_0.txt