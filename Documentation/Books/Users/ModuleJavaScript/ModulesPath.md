!CHAPTER Modules Path versus Modules Collection

ArangoDB comes with predefined modules defined in the file-system under the path
specified by *startup.startup-directory*. In a standard installation this
point to the system share directory. Even if you are an administrator of
ArangoDB you might not have write permissions to this location. On the other
hand, in order to deploy some extension for ArangoDB, you might need to install
additional JavaScript modules. This would require you to become root and copy
the files into the share directory. In order to ease the deployment of
extensions, ArangoDB uses a second mechanism to look up JavaScript modules.

JavaScript modules can either be stored in the filesystem as regular file or in
the database collection `_modules`.

If you execute

    require("com/example/extension")

then ArangoDB will try to locate the corresponding JavaScript as file as
follows

- There is a cache for the results of previous `require` calls. First of
  all ArangoDB checks if `com/example/extension` is already in the modules
  cache. If it is, the export object for this module is returned. No further
  JavaScript is executed.

- ArangoDB will then check, if there is a file called 

    com/example/extension.js

  in the system search path. If such a file exists, it is executed in a new
  module context and the value of `exports` object is returned. This value is
  also stored in the module cache.

- If no file can be found, ArangoDB will check if the collection `_modules`
  contains a document of the form
  
      {
        path: "/com/example/extension",
        content: "...."
      }
  
  Note that the leading `/´ is important - even if you call `require` without a
  leading `/´.  If such a document exists, then the value of the `content`
  attribute must contain the JavaScript code of the module. This string is
  executed in a new module context and the value of `exports` object is
  returned. This value is also stored in the module cache.

!SUBSECTION Modules Cache

As `require` uses a module cache to store the exports objects of the required
modules, changing the design documents for the modules in the `_modules` collection
might have no effect at all.

You need to clear the cache, when manually changing documents in the `_modules`
collection.

    arangosh> require("internal").flushServerModules()

This initiate a flush of the modules in the ArangoDB *server* process.

Please note, that the ArangoDB JavaScript shell uses the same mechanism as the
server to locate JavaScript modules. But the do not share the same module cache.
If you flush the server cache, this will not flush the shell cache - and vice
versa.

In order to flush the modules cache of the JavaScript shell, you should use

    arangosh> require("internal").flushModuleCache()