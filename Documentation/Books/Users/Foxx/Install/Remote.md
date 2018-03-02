Install Applications from remote host
=====================================

In this chapter we will make use of the Foxx manager as described [before](README.md).
This time we want to install an app hosted on a server.
Currently the Foxx-manager supports downloads of applications via HTTP and HTTPS protocols.

Remote file format
------------------
The file on the remote server has to be a valid Foxx application packed in a zip archive.
The zip archive has to contain a top-level directory which is containing the sources of the Foxx application.
Especially the file **manifest.json** has to be directly contained in this top-level directory.
On an Unix system you can verify that your zip archive has the correct format if the content looks like this:

```
unix> unzip -l hello-foxx.zip
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
```

Next you have to make this file publicly available over HTTP or HTTPS on a webserver.
Assume we can download the app at **http://www.example.com/hello.zip**.

Install from remote server
--------------------------

Having the link to your Foxx application at hand you can just hand this link over to the Foxx manager:

```
unix> foxx-manager install http://www.example.com/hello.zip /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

ArangoDB will try to download and extract the file stored at the remote location.

This HTTP or HTTPS link can be used in all functions of the Foxx-manager that allow to install Foxx applications:

**install**

```
unix> foxx-manager install http://www.example.com/hello.zip /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

**replace**

```
unix> foxx-manager replace http://www.example.com/hello.zip /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

**upgrade**

```
unix> foxx-manager upgrade http://www.example.com/hello.zip /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```
