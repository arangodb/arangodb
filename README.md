AvocadoDB
=========

My colleagues and I started developing a document-store, which focuses on durability 
of the data taking advantage of new technologies like SSD, support for graph and geo 
algorithms needed in social networks, ease of use for the developer and minimal 
effort to operate for the administrator. 

Compilation
===========

(1) Install V8

(2) ./configure --with-v8=/home/huerth/fceller/IPP/3rdParty/v8

(3) "./avocado" to start a REST server or "./avocado --shell" for debugging

First Steps
===========

 ./avocado --shell

 avocado> db.examples.count();

 avocado> db.examples.save({ Hallo: "World" });

 avocado> db.examples.select();
