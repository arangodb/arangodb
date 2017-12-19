!CHAPTER Install Applications from local file system

In this chapter we will make use of the Foxx manager as described [before](README.md).
This time we want to install an app that is located on our local file system.
At this point we have to mention that it if you connect to a remote ArangoDB with

```
unix> foxx-manager --server.endpoint tcp://example.com:8529
```

The file has to be available on your local machine, not on the remote server.
The only thing you need is the path to your application either relative or absolute.
You can install a Foxx application right from a directory:

```
unix> ls /Users/arangodb/hello-foxx
README.md     app.js        assets        files         kaffee.coffee lib
manifest.json models        scripts       thumbnail.png
unix> foxx-manager install /Users/arangodb/hello-foxx /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

Or you can pack the application into a zip archive.
And then install using this archive.

```
unix> unzip -l ../hello-foxx.zip
Archive:  hello-foxx.zip
0836dc2e81be8264e480a7695b46c1abe7ef153d
Length     Date   Time    Name
--------    ----   ----    ----
       0  09-10-14 15:35   hello-foxx/
    1256  09-10-14 15:35   hello-foxx/README.md
   11200  09-10-14 15:35   hello-foxx/app.js
       0  09-10-14 15:35   hello-foxx/assets/
       0  09-10-14 15:35   hello-foxx/assets/css/
      82  09-10-14 15:35   hello-foxx/assets/css/base.css
      86  09-10-14 15:35   hello-foxx/assets/css/custom.css
       0  09-10-14 15:35   hello-foxx/assets/vendor/
       0  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/
       0  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/css/
   22111  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/css/bootstrap-responsive.css
   16849  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/css/bootstrap-responsive.min.css
  127247  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/css/bootstrap.css
  105939  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/css/bootstrap.min.css
       0  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/img/
    8777  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/img/glyphicons-halflings-white.png
   12799  09-10-14 15:35   hello-foxx/assets/vendor/bootstrap/img/glyphicons-halflings.png
       0  09-10-14 15:35   hello-foxx/assets/vendor/jquery/
  268380  09-10-14 15:35   hello-foxx/assets/vendor/jquery/jquery.js
       0  09-10-14 15:35   hello-foxx/assets/vendor/sh/
    1981  09-10-14 15:35   hello-foxx/assets/vendor/sh/highlighter.css
    5563  09-10-14 15:35   hello-foxx/assets/vendor/sh/sh_javascript.js
    5305  09-10-14 15:35   hello-foxx/assets/vendor/sh/sh_main.min.js
       0  09-10-14 15:35   hello-foxx/files/
    3266  09-10-14 15:35   hello-foxx/files/index.html
     398  09-10-14 15:35   hello-foxx/files/static.html
     361  09-10-14 15:35   hello-foxx/kaffee.coffee
       0  09-10-14 15:35   hello-foxx/lib/
     108  09-10-14 15:35   hello-foxx/lib/a.js
      43  09-10-14 15:35   hello-foxx/lib/c.js
    1129  09-10-14 15:35   hello-foxx/manifest.json
       0  09-10-14 15:35   hello-foxx/models/
     330  09-10-14 15:35   hello-foxx/models/tiger.js
       0  09-10-14 15:35   hello-foxx/scripts/
    2065  09-10-14 15:35   hello-foxx/scripts/setup.js
    1798  09-10-14 15:35   hello-foxx/scripts/teardown.js
   17727  09-10-14 15:35   hello-foxx/thumbnail.png
--------                   -------
  614800                   37 files

unix> foxx-manager install ../hello-foxx.zip /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

You can use paths to directories in all functions of the Foxx-manager that allow to install Foxx applications:

**install**

```
unix> foxx-manager install /Users/arangodb/hello-foxx /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

**replace**

```
unix> foxx-manager replace /Users/arangodb/hello-foxx /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

**upgrade**
```
unix> foxx-manager upgrade /Users/arangodb/hello-foxx /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```
