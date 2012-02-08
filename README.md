# AvocadoDB

My colleagues and I started developing a document-store, which focuses on durability 
of the data taking advantage of new technologies like SSD, support for graph and geo 
algorithms needed in social networks, ease of use for the developer and minimal 
effort to operate for the administrator. 

## Compilation

1. Install Dependencies: V8, boost, libev
2. make setup
3. ./configure --with-boost=PATH_TO_BOOST --with-libev=PATH_TO_LIBEV --with-v8=PATH_TO_V8
4. make
5. create a directory `/var/lib/avocado` where you are allowed to read and write
6. "./avocado /var/lib/avocado" to start a REST server or "./avocado /var/lib/avocado --shell" for debugging

### Mac OS X Hints

If you install AvocadoDB on Mac OS X we collected some hints for you:

* The version of bison delivered with OS X is out of date. Update it before installing.

## First Steps

    ./avocado --shell
    avocado> db.examples.count();
    avocado> db.examples.save({ Hallo: "World" });
    avocado> db.examples.select();
