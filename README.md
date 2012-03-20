# AvocadoDB

My colleagues and I started developing a document-store, which focuses on durability 
of the data taking advantage of new technologies like SSD, support for graph and geo 
algorithms needed in social networks, ease of use for the developer and minimal 
effort to operate for the administrator. 

## Compilation

Please check the <a href="https://github.com/triAGENS/AvocadoDB/wiki">wiki</a>
for installation and compilation instructions:

### Mac OS X Hints

On Mac OS X you can install AvocadoDB using the packagemanager [Homebrew](http://mxcl.github.com/homebrew/):

* `brew install avocadodb` (use `--HEAD` in order to build AvocadoDB from current master)

This will install AvocadoDB and all dependencies. 

## First Steps

    ./avocado --shell
    avocado> db.examples.save({ Hallo: "World" });
    avocado> db.examples.count();
    avocado> db.examples.all().toArray();

## Caveat

Please note that this is a very early version of AvocadoDB. There will be
bugs and we'd really appreciate it if you 
<a href="https://github.com/triAGENS/AvocadoDB/issues">report</a> them:

  https://github.com/triAGENS/AvocadoDB/issues
