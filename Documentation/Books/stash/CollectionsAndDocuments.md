Starting the JavaScript shell
-----------------------------

The easiest way to connect to the database is the JavaScript shell
_arangosh_. You can either start it from the command-line or as an
embedded version in the browser. Using the command-line tool has the
advantage that you can use autocompletion.

    unix> arangosh --server.password ""
                                           _ 
      __ _ _ __ __ _ _ __   __ _  ___  ___| |__ 
     / _` | '__/ _` | '_ \ / _` |/ _ \/ __| '_ \ 
    | (_| | | | (_| | | | | (_| | (_) \__ \ | | |
     \__,_|_|  \__,_|_| |_|\__, |\___/|___/_| |_|
                           |___/

    Welcome to arangosh 2.x.y. Copyright (c) 2012 triAGENS GmbH.
    Using Google V8 4.1.0.27 JavaScript engine.
    Using READLINE 6.1.

    Connected to Arango DB 127.0.0.1:8529 Version 2.2.0

    arangosh> help
    ------------------------------------- Help -------------------------------------
    Predefined objects:                                                 
      arango:                               ArangoConnection           
      db:                                   ArangoDatabase             
      fm:                                   FoxxManager  
    Example:                                                            
     > db._collections();                   list all collections       
     > db._create(<name>)                   create a new collection    
     > db._drop(<name>)                     drop a collection         
     > db.<name>.toArray()                  list all documents         
     > id = db.<name>.save({ ... })         save a document            
     > db.<name>.remove(<_id>)              delete a document          
     > db.<name>.document(<_id>)            retrieve a document        
     > db.<name>.replace(<_id>, {...})      overwrite a document       
     > db.<name>.update(<_id>, {...})       partially update a document
     > db.<name>.exists(<_id>)              check if document exists   
     > db._query(<query>).toArray()         execute an AQL query       
     > db._useDatabase(<name>)              switch database            
     > db._createDatabase(<name>)           create a new database      
     > db._databases()                      list existing databases    
     > help                                 show help pages            
     > exit                                         
    arangosh>

This gives you a prompt where you can issue JavaScript commands.

The standard setup does not require a password. Depending on your
setup you might need to specify the endpoint, username and password
in order to run the shell on your system. You can use the options
`--server.endpoint`, `--server.username` and `--server.password` for
this.

    unix> arangosh --server.endpoint tcp://127.0.0.1:8529 --server.username root

A default configuration is normally installed under
*/etc/arangodb/arangosh.conf*. It contains a default endpoint and an
empty password.

Querying for Documents
----------------------

All documents are stored in collections. All collections are stored in a
database. The database object is accessible via the variable *db*.

Creating a collection is simple. You can use the *_create* method
of the *db* variable.

    @startDocuBlockInline 01_workWithColl_create
    @EXAMPLE_ARANGOSH_OUTPUT{01_workWithColl_create}
    ~addIgnoreCollection("example")
    db._create("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 01_workWithColl_create

After the collection has been created you can easily access it using
the path *db.example*. The collection currently shows as *loaded*,
meaning that it's loaded into memory. If you restart the server and
access the collection again it will now show as *unloaded*. You can
also manually unload a collection.

    @startDocuBlockInline 02_workWithColl_unload
    @EXAMPLE_ARANGOSH_OUTPUT{02_workWithColl_unload}
    db.example.unload();
    db.example;
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 02_workWithColl_unload

Whenever you use a collection ArangoDB will automatically load it
into memory for you.

In order to create new documents in a collection use the *save*
operation. 

    @startDocuBlockInline 03_workWithColl_save
    @EXAMPLE_ARANGOSH_OUTPUT{03_workWithColl_save}
    db.example.save({ Hello : "World" });
    db.example.save({ "name" : "John Doe", "age" : 29 });
    db.example.save({ "name" : "Jane Smith", "age" : 31 });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 03_workWithColl_save

Just storing documents would be no fun. We now want to select some of
the stored documents again.  In order to select all elements of a
collection, one can use the *toArray* method:

    @startDocuBlockInline 04_workWithColl_directAcess
    @EXAMPLE_ARANGOSH_OUTPUT{04_workWithColl_directAcess}
    db.example.toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 04_workWithColl_directAcess

Now we want to look for a person with a given name. We can use
*byExample* for this. The method returns an array of documents
matching a given example.

    @startDocuBlockInline 05_workWithColl_byExample
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithColl_byExample}
    db.example.byExample({ name: "Jane Smith" }).toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithColl_byExample

While the *byExample* works very well for simple queries where you
combine the conditions with an `and`. The syntax above becomes messy for *joins*
and *or* conditions. Therefore ArangoDB also supports a full-blown
query language, AQL. To run an AQL query, use the *db._query* method:.

    @startDocuBlockInline 05_workWithColl_AQL_STR
    @EXAMPLE_ARANGOSH_OUTPUT{05_workWithColl_AQL_STR}
    db._query('FOR user IN example FILTER user.name == "Jane Smith" RETURN user').toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 05_workWithColl_AQL_STR

Searching for all persons with an age above 30:

    @startDocuBlockInline 06_workWithColl_AOQL_INT
    @EXAMPLE_ARANGOSH_OUTPUT{06_workWithColl_AOQL_INT}
    db._query('FOR user IN example FILTER user.age > 30 RETURN user').toArray()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 06_workWithColl_AOQL_INT


John was put in there by mistake – so let's delete him again. We fetch
the `_id` using *byExample*:

    @startDocuBlockInline 07_workWithColl_remove
    @EXAMPLE_ARANGOSH_OUTPUT{07_workWithColl_remove}
    db.example.remove(db.example.byExample({ name: "John Doe" }).toArray()[0]._id)
    db.example.toArray()
    ~removeIgnoreCollection("example")
    ~db._drop("example")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 07_workWithColl_remove


You can learn all about the query language [AQL](../../AQL/index.html). Note that
*_query* is a short-cut for *_createStatement* and *execute*. We will
come back to these functions when we talk about cursors.

ArangoDB's Front-End
--------------------

The ArangoDB server has a graphical front-end, which allows you to
inspect the current state of the server from within your browser. You
can use the front-end using the following URL:

    http://localhost:8529/

The front-end allows you to browse through the collections and
documents. If you need to administrate the database, please use
the ArangoDB shell described in the next section.


