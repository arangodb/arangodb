# AvocadoDB

My colleagues and I started developing a document-store, which focuses on durability 
of the data taking advantage of new technologies like SSD, support for graph and geo 
algorithms needed in social networks, ease of use for the developer and minimal 
effort to operate for the administrator. 

## Compilation

Please check the <a href="https://github.com/triAGENS/AvocadoDB/wiki">wiki</a>
for installation and compilation instructions:

### Mac OS X Hints

On Mac OS X you can install AvocadoDB using the packagemanager [Homebrew](http://mxcl.github.com/homebrew/). We are currently waiting for approval of our package, therefore you need to use this:

* `brew install https://raw.github.com/tisba/homebrew/master/Library/Formula/avocadodb.rb`

This will install AvocadoDB and all dependencies. 

## First Steps

    ./avocado --shell
    avocado> db.examples.count();
    avocado> db.examples.save({ Hallo: "World" });
    avocado> db.examples.select();

## Caveat

Please note that this is a very early version if AvocadoDB. There will be
bugs and we'd realy appreciate it if you 
<a href="https://github.com/triAGENS/AvocadoDB/issues">report</a> them:

  https://github.com/triAGENS/AvocadoDB/issues
