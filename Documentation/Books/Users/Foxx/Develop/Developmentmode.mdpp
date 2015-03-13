!CHAPTER Application Development Mode

This chapter describes the development mode for Foxx applications.
It is only useful if you have write access to the files in ArangoDB's application folder.
The folder is printed in the web interface for development Foxxes and can be reconfigured using the startup option `--javascript.app-path`.
If you do not have access to this folder, e.g. using a hosted service like [myArangoDB](https://myarangodb.com/), you cannot make use of this feature.
You will have to stick to the procedure described in [New Versions](../Production/Upgrade.md).
<div class="versionDifference">
Before 2.5 the startup option `--javascript.dev-app-path` was required for the development mode.
This caused a lot of confusion and introduced problems when moving from development to production.
So we decided to unify both app paths and activate development mode for specific Apps during runtime.
The `--javascript.dev-app-path` parameter is not having any effect any more.
</div>

!SECTION Activation

Activating the development mode is done with a single command:

```
unix> foxx-manager development /example
Activated development mode for Application hello-foxx version 1.5.0 on mount point /example
```

Now the app will now be listed in **listDevelopment**:

```
unix> foxx-manager listDevelopment
Mount       Name          Author          Description                                 Version    Development
---------   -----------   -------------   -----------------------------------------   --------   ------------
/example    hello-foxx    Frank Celler    This is 'Hello World' for ArangoDB Foxx.    1.5.0      true
---------   -----------   -------------   -----------------------------------------   --------   ------------
1 application(s) found
```

!SECTION Effects

For a Foxx application in development mode the following effects apply:

**Reload on request**
Whenever a request is routed to this application its source is reloaded.
This means all requests are slightly slower than in production mode.
But you will get immediate live updates on your code changes.

**Exposed Debugging information**
This application will deliver error messages and stack traces to requesting client.
For more information see the [Debugging](Debugging.md) section.
