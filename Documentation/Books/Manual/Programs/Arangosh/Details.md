Arangosh Details
================

Interaction
-----------

You can paste multiple lines into Arangosh, given the first line ends with an
opening brace:

    @startDocuBlockInline shellPaste
    @EXAMPLE_ARANGOSH_OUTPUT{shellPaste}
    |for (var i = 0; i < 10; i ++) {
    |         require("@arangodb").print("Hello world " + i + "!\n");
    }
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock shellPaste


To load your own JavaScript code into the current JavaScript interpreter context,
use the load command:

    require("internal").load("/tmp/test.js")     // <- Linux / MacOS
    require("internal").load("c:\\tmp\\test.js") // <- Windows

Exiting arangosh can be done using the key combination ```<CTRL> + D``` or by
typing ```quit<CR>```

Shell Output
------------

The ArangoDB shell will print the output of the last evaluated expression
by default:
    
    @startDocuBlockInline lastExpressionResult
    @EXAMPLE_ARANGOSH_OUTPUT{lastExpressionResult}
    42 * 23
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock lastExpressionResult
    
In order to prevent printing the result of the last evaluated expression,
the expression result can be captured in a variable, e.g.

    @startDocuBlockInline lastExpressionResultCaptured
    @EXAMPLE_ARANGOSH_OUTPUT{lastExpressionResultCaptured}
    var calculationResult = 42 * 23
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock lastExpressionResultCaptured

There is also the `print` function to explicitly print out values in the
ArangoDB shell:

    @startDocuBlockInline printFunction
    @EXAMPLE_ARANGOSH_OUTPUT{printFunction}
    print({ a: "123", b: [1,2,3], c: "test" });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock printFunction

By default, the ArangoDB shell uses a pretty printer when JSON documents are
printed. This ensures documents are printed in a human-readable way:

    @startDocuBlockInline usingToArray
    @EXAMPLE_ARANGOSH_OUTPUT{usingToArray}
    db._create("five")
    for (i = 0; i < 5; i++) db.five.save({value:i})
    db.five.toArray()
    ~db._drop("five");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock usingToArray

While the pretty-printer produces nice looking results, it will need a lot of
screen space for each document. Sometimes a more dense output might be better.
In this case, the pretty printer can be turned off using the command
*stop_pretty_print()*.

To turn on pretty printing again, use the *start_pretty_print()* command.

Escaping
--------

In AQL, escaping is done traditionally with the backslash character: `\`.
As seen above, this leads to double backslashes when specifying Windows paths.
Arangosh requires another level of escaping, also with the backslash character.
It adds up to four backslashes that need to be written in Arangosh for a single
literal backslash (`c:\tmp\test.js`):

    db._query('RETURN "c:\\\\tmp\\\\test.js"')

You can use [bind variables](../../../AQL/Invocation/WithArangosh.html) to
mitigate this:

    var somepath = "c:\\tmp\\test.js"
    db._query(aql`RETURN ${somepath}`)

Database Wrappers
-----------------

_Arangosh_ provides the *db* object by default, and this object can
be used for switching to a different database and managing collections inside the
current database.

For a list of available methods for the *db* object, type 
    
    @startDocuBlockInline shellHelp
    @EXAMPLE_ARANGOSH_OUTPUT{shellHelp}
    db._help(); 
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock shellHelp
  
The [`db` object](../../Appendix/References/DBObject.md) is available in *arangosh*
as well as on *arangod* i.e. if you're using [Foxx](../../Foxx/README.md). While its
interface is persistent between the *arangosh* and the *arangod* implementations,
its underpinning is not. The *arangod* implementation are JavaScript wrappers
around ArangoDB's native C++ implementation, whereas the *arangosh* implementation
wraps HTTP accesses to ArangoDB's [RESTfull API](../../../HTTP/index.html).

So while this code may produce similar results when executed in *arangosh* and
*arangod*, the CPU usage and time required will be really different since the
*arangosh* version will be doing around 100k HTTP requests, and the
*arangod* version will directly write to the database:

```js
for (i = 0; i < 100000; i++) {
    db.test.save({ name: { first: "Jan" }, count: i});
}
```

Using `arangosh` via unix shebang mechanisms
--------------------------------------------
In unix operating systems you can start scripts by specifying the interpreter in the first line of the script.
This is commonly called `shebang` or `hash bang`. You can also do that with `arangosh`, i.e. create `~/test.js`:

    #!/usr/bin/arangosh --javascript.execute 
    require("internal").print("hello world")
    db._query("FOR x IN test RETURN x").toArray()

Note that the first line has to end with a blank in order to make it work.
Mark it executable to the OS: 

    #> chmod a+x ~/test.js

and finaly try it out:

    #> ~/test.js


Shell Configuration
-------------------

_arangosh_ will look for a user-defined startup script named *.arangosh.rc* in the
user's home directory on startup. The home directory will likely be `/home/<username>/`
on Unix/Linux, and is determined on Windows by peeking into the environment variables
`%HOMEDRIVE%` and `%HOMEPATH%`. 

If the file *.arangosh.rc* is present in the home directory, _arangosh_ will execute
the contents of this file inside the global scope.

You can use this to define your own extra variables and functions that you need often.
For example, you could put the following into the *.arangosh.rc* file in your home
directory:

```js
// "var" keyword avoided intentionally...
// otherwise "timed" would not survive the scope of this script
global.timed = function (cb) {
  console.time("callback");
  cb();
  console.timeEnd("callback");
};
```

This will make a function named *timed* available in _arangosh_ in the global scope.

You can now start _arangosh_ and invoke the function like this:

```js
timed(function () { 
  for (var i = 0; i < 1000; ++i) {
    db.test.save({ value: i }); 
  }
});
```

Please keep in mind that, if present, the *.arangosh.rc* file needs to contain valid
JavaScript code. If you want any variables in the global scope to survive you need to
omit the *var* keyword for them. Otherwise the variables will only be visible inside
the script itself, but not outside.
