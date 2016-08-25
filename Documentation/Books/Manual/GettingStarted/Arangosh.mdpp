!CHAPTER Details about the ArangoDB Shell

After the server has been started,
you can use the ArangoDB shell (_arangosh_) to administrate the
server. Without any arguments, the ArangoDB shell will try to contact
the server on port 8529 on the localhost. For more information see the
[ArangoDB Shell documentation](../Administration/Arangosh/README.md). You might need to set additional options
(endpoint, username and password) when connecting:

```
unix> ./arangosh --server.endpoint tcp://127.0.0.1:8529 --server.username root
```

The shell will print its own version number and if successfully connected
to a server the version number of the ArangoDB server.

!SECTION Command-Line Options

Use `--help` to get a list of command-line options:

```
unix> ./arangosh --help
STANDARD options:
  --audit-log <string>          audit log file to save commands and results to
  --configuration <string>      read configuration file
  --help                        help message
  --max-upload-size <uint64>    maximum size of import chunks (in bytes) (default: 500000)
  --no-auto-complete            disable auto completion
  --no-colors                   deactivate color support
  --pager <string>              output pager (default: "less -X -R -F -L")
  --pretty-print                pretty print values
  --quiet                       no banner
  --temp.path <string>          path for temporary files (default: "/tmp/arangodb")
  --use-pager                   use pager
  
JAVASCRIPT options:
  --javascript.check <string>                syntax check code JavaScript code from file
  --javascript.execute <string>              execute JavaScript code from file
  --javascript.execute-string <string>       execute JavaScript code from string
  --javascript.startup-directory <string>    startup paths containing the JavaScript files
  --javascript.unit-tests <string>           do not start as shell, run unit tests instead
  --jslint <string>                          do not start as shell, run jslint instead
  
LOGGING options:
  --log.level <string>    log level (default: "info")
  
CLIENT options:
  --server.connect-timeout <double>         connect timeout in seconds (default: 3)
  --server.authentication <bool>            whether or not to use authentication (default: true)
  --server.endpoint <string>                endpoint to connect to, use 'none' to start without a server (default: "tcp://127.0.0.1:8529")
  --server.password <string>                password to use when connecting (leave empty for prompt)
  --server.request-timeout <double>         request timeout in seconds (default: 300)
  --server.username <string>                username to use when connecting (default: "root")
```

!SECTION Database Wrappers

The [`db` object](../Appendix/References/DBObject.md) is available in *arangosh*
as well as on *arangod* i.e. if you're using [Foxx](../Foxx/README.md). While its
interface is persistant between the *arangosh* and the *arangod* implementations,
its underpinning is not. The *arangod* implementation are JavaScript wrappers
around ArangoDB's native C++ implementation, whereas the *arangosh* implementation
wraps HTTP accesses to ArangoDB's [RESTfull API](../../HTTP/index.html).

So while this code may produce similar results when executed in *arangosh* and
*arangod*, the cpu usage and time required will be really different:

```js
for (i = 0; i < 100000; i++) {
    db.test.save({ name: { first: "Jan" }, count: i});
}
```

Since the *arangosh* version will be doing around 100k HTTP requests, and the
*arangod* version will directly write to the database.

!SECTION Using `arangosh` via unix shebang mechanisms
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


