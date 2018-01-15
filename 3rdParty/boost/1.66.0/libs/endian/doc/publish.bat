copy /y c:\boost\develop\libs\endian\doc\* d:\boost\endian-gh-pages
pushd d:\boost\endian-gh-pages
git commit -a -m "copy from develop"
git push
popd
rem Copyright Beman Dawes, 2014
rem Distributed under the Boost Software License, Version 1.0.
rem See www.boost.org/LICENSE_1_0.txt