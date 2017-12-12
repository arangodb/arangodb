# XCopy install ArangoDB on Windows

## Problem
Even if there is a nice guided installer for windows users, not all users prefer this kind of installation. In order to have a [portable application](http://en.wikipedia.org/wiki/Portable_application) [XCOPY deployment](http://en.wikipedia.org/wiki/XCOPY_deployment) is necessary.

## Solution
With Version 2.5.1 ArangoDB doesn't rely on registry entries anymore so we can deploy using a ZIP-file.

### Steps

#### Unzip archive
Open an explorer, choose a place where you want ArangoDB to be and unzip the files there. It will create its own toplevel directory with the version number in the string.

#### Alter configuration
**Optional**: Edit `etc\arangodb\arangod.conf` if the default values don't suit your needs (e.g. the location of the database files, ports to bind and so on).

#### Create Runtime directories
`arangod` leans on the existence of some directories in the **var** subdirectory, so you should create them:

```
C:\Program Files\ArangoDB-2.5.1>mkdir var\lib\arangodb
C:\Program Files\ArangoDB-2.5.1>mkdir var\lib\arangodb-apps
```

#### Run arangod
To start the database simply run it:

```
C:\Program Files\ArangoDB-2.5.1>bin\arangod
```

Now it takes a while to open all its databases, load system facilities, bootstrap the JavaScript environments and many more. Once it's ready the output is:

```
INFO ArangoDB (version 2.5.1 [windows]) is ready for business. Have fun!
```

Now you can open the administrative webinterface in your browser using http://127.0.0.1:8529/.

#### Installing as service
If you don't want to run `arangod` from a cmd-shell each time installing it as a system service is the right thing to do.
This requires administrative privileges. You need to *Run as Administrator* the cmd-shell.
First we need to grant the SYSTEM-user access to our database directory, since `arangod` is going to be running as that user:

```
C:\Program Files\ArangoDB-2.5.1>icacls var /grant SYSTEM:F /t
```

Next we can install the service itself:

```
C:\Program Files\ArangoDB-2.5.1>bin\arangod --install-service
```

Now you will have a new entry in the **Services** dialog labeled **ArangoDB - the multi-purpose database**. You can start it there or just do it on the `commandline` using:

```
C:\Program Files\ArangoDB-2.5.1>NET START ArangoDB
```

It will take a similar amount of time to start from the `comandline` above till the service is up and running.
Since you don't have any console to inspect the startup, messages of the severity FATAL & ERROR are also output into the windows eventlog, so in case of failure you can have a look at the **Eventlog** in the **Managementconsole**

**Author:** [Wilfried Goesgens](https://github.com/dothebart)

**Tags:** #windows #install
