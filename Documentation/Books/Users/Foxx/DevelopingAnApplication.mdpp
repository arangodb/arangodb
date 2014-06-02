!CHAPTER Iteratively Developing an Application


While developing a Foxx application, it is recommended to use the development
mode. The development mode makes ArangoDB reload all components of all Foxx
applications on every request. While this has a negative impact on performance,
it allows to view and debug changes in the application instantly. It is not
recommended to use the development mode in production.

During development it is often necessary to log some debug output. ArangoDB
provides a few mechanisms for this:

- using `console.log`:
  ArangoDB provides the `console` module, which you can use from within your
  Foxx application like this:
        
      require("console").log(value);

  The `console` module will log all output to ArangoDB's logfile. If you are
  not redirecting to log output to stdout, it is recommended that you follow
  ArangoDB's logfile using a `tail -f` command or something similar.
  Please refer to @ref JSModuleConsole for details.

- using `internal.print`:
  The `print` method of the `internal` module writes data to the standard
  output of the ArangoDB server process. If you have start ArangoDB manually
  and do not run it as an (unattended) daemon, this is a convenient method:

      require("internal").print(value);

- tapping requests / responses:
  Foxx allows to tap incoming requests and outgoing responses using the `before`
  and `after` hooks. To print all incoming requests to the stdout of the ArangoDB
  server process, you could use some code like this in your controller:
   
      controller.before("/*", function (req, res) {
        require("internal").print(req);
      });

  Of course you can also use `console.log` or any other means of logging output.

