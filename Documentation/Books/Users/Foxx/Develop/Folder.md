Folder Structure
================

Now we are almost ready to write some code.
Hence it is time to introduce the folder structure created by Foxx.
We still follow the example of the app installed at `/example`.
The route to reach this application via HTTP(S) is constructed with the following parts:

* The ArangoDB endpoint `<arangodb>`: (e.g. `http://localhost:8529`)
* The selected database `<db>`: (e.g. `_system`)
* The mount point `<mount>`: (e.g. `/example`)

Now the route is constructed as follows:

```
<arangodb>/_db/<db>/<mount>
http://localhost:8529/_db/_system/example
```

For the sources of the application the path on your file system the path is constructed almost the same way.
But here we need some additional information:

* The app-path `<app-path>`: (e.g. `/var/lib/arangodb-apps`)
* The selected database `<db>`: (e.g. `_system`)
* The mount point `<mount>`: (e.g. `/example`)

**Note**: You can set your app-path to an arbitrary folder using the `--javascript.app-path` startup parameter.
Now the path is constructed as follows:

```
<app-path>/_db/<db>/<mount>/APP
Linux: /var/lib/arangodb-apps/_db/_system/example/APP
Mac: /usr/local/var/lib/arangodb-apps/_db/_system/example/APP
Windows: C:\Program Files\ArangoDB\js\apps\_db\_system\example\APP
```

Before 2.5 the folder was constructed using application name and version.
That was necessary because installation was a two step process:

<!-- <div class="versionDifference"-->
1. Including the Application sources into ArangoDB (and creating the folder)
2. Mounting the application to one specific mountpoint

This caused some confusion and a lot of unnecessary administration overhead.
One had to remember which apps are just known to ArangoDB, which ones are actually executed in which version etc.
The use-case we actually introduced this staging for was heavy reuse of equal apps.
However it turned out that this is rarely the case and the overhead by having redundant sources is small compared to the improved user experience not having this staging.
So we decided to entirely remove the staging and make installation an one step process without caching old versions of an app.
This means if you now **uninstall** an application it is removed from file system.
Before 2.5 you had to **purge** the application to make sure it is removed.
<!-- </div> -->

Now you can start modifying the files located there. As a good entry you should start with the [Controller](Controller.md)
