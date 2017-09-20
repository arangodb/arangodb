Jemalloc
========

**This article is only relevant if you intend to compile arangodb on Ubuntu 16.10 or debian testing**

On more modern linux systems (development/floating at the time of this writing) you may get compile / link errors with arangodb regarding jemalloc.
This is due to compilers switching their default behaviour regarding the `PIC` - Position Independend Code. 
It seems common that jemalloc remains in a stage where this change isn't followed and causes arangodb to error out during the linking phase.

From now on cmake will detect this and give you this hint:

    the static system jemalloc isn't suitable! Recompile with the current compiler or disable using `-DCMAKE_CXX_FLAGS=-no-pie -DCMAKE_C_FLAGS=-no-pie`

Now you've got three choices.

Doing without jemalloc
----------------------

Fixes the compilation issue, but you will get problems with the glibcs heap fragmentation behaviour which in the longer run will lead to an ever increasing memory consumption of ArangoDB. 

So, while this may be suitable for development / testing systems, its definitely not for production.

Disabling PIC altogether
------------------------

This will build an arangod which doesn't use this compiler feature. It may be not so nice for development builds. It can be achieved by specifying these options on cmake:

    -DCMAKE_CXX_FLAGS=-no-pie -DCMAKE_C_FLAGS=-no-pie

Recompile jemalloc
------------------

The smartest way is to fix the jemalloc libraries packages on your system so its reflecting that new behaviour. On debian / ubuntu systems it can be achieved like this:

    apt-get install automake debhelper docbook-xsl xsltproc dpkg-dev
    apt source jemalloc
    cd jemalloc*
    dpkg-buildpackage
    cd ..
    dpkg -i *jemalloc*deb
