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

Start the server:

    > mkdir /tmp/vocbase
    > ./avocado /tmp/vocbase
    2012-03-30T12:54:19Z [11794] INFO AvocadoDB (version 0.x.y) is ready for business
    2012-03-30T12:54:19Z [11794] INFO HTTP client port: 127.0.0.1:8529
    2012-03-30T12:54:19Z [11794] INFO HTTP admin port: 127.0.0.1:8530
    2012-03-30T12:54:19Z [11794] INFO Have Fun!

Start the shell in another windows:

    > ./avocsh
                                _         
       __ ___   _____   ___ ___| |__      
      / _` \ \ / / _ \ / __/ __| '_ \   
     | (_| |\ V / (_) | (__\__ \ | | | 
      \__,_| \_/ \___/ \___|___/_| |_|   

    Welcome to avocsh 0.3.5. Copyright (c) 2012 triAGENS GmbH.
    Using Google V8 3.9.4.0 JavaScript engine.
    Using READLINE 6.1.

    Connected to Avocado DB 127.0.0.1:8529 Version 0.3.5

    avocsh> db._create("examples")
    [AvocadoCollection 106097, "examples]

    avocsh> db.examples.save({ Hallo: "World" });
    {"error":false,"_id":"106097/2333739","_rev":2333739}

    avocsh> db.examples.all();
    [{ _id : "82883/1524675", _rev : 1524675, Hallo : "World" }]

## Caveat

Please note that this is a very early version of AvocadoDB. There will be
bugs and we'd really appreciate it if you 
<a href="https://github.com/triAGENS/AvocadoDB/issues">report</a> them:

  https://github.com/triAGENS/AvocadoDB/issues
