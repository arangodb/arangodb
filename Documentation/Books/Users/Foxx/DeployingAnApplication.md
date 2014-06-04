<a name="deploying_a_foxx_application"></a>
# Deploying a Foxx application

When a Foxx application is ready to be used in production, it is time to leave the
development mode and deploy the app in a production environment.

The first step is to copy the application's script directory to the target ArangoDB 
server. If your development and production environment are the same, there is
nothing to do. If production runs on a different server, you should copy the
development application directory to some temporary place on the production server.

When the application code is present on the production server, you can use the
`fetch` and `mount` commands from the [Foxx Manager](../FoxxManager/README.md) to register the
application in the production ArangoDB instance and make it available.

Here are the individual steps to carry out:

- development: 
  - cd into the directory that application code is in. Then create a tar.gz file with 
    the application code (replace `app` with the actual name):

        cd /path/to/development/apps/directory
        tar cvfz app.tar.gz app

  - copy the tar.gz file to the production server:

        scp app.tar.gz production:/tmp/

- production:
  - create a temporary directory, e.g. `/tmp/apps` and extract the tar archive into 
    this directory:

        mkdir /tmp/apps
        cd /tmp/apps
        tar xvfz /tmp/app.tar.gz

  - start the ArangoDB shell and run the following commands in it:

        fm.fetch("directory", "/tmp/apps/app");
        fm.mount("app", "/app");

More information on how to deploy applications from different sources can be
found in the [Foxx Manager](../FoxxManager/README.md).
