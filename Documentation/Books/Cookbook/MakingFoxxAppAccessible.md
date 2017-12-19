# Make a Foxx app accessible from the Internet

## Problem

I want to make only a specific Foxx app accessible from the Internet, without exposing other
apps or the built-in server APIs.

## Solution

Bind ArangoDB to port 8529 and local IP `127.0.0.1` so it is not accessible from web
requests. Use a web server to proxy Internet requests to ArangoDB and return the results. 

This recipe uses Apache2 and mod_proxy, but any other proxy software should do as well.
The shell commands provided should work on Ubuntu, but they might be slightly different
on other systems.

### Prerequisites

This recipe assumes Apache2 is already installed on the server that is running ArangoDB.
The Apache server should already respond the web requests.

### Bind ArangoDB to local IP

To make ArangoDB not answer web requests, verify its configuration file (normally named
`/etc/arangodb.conf`) and make sure it contains the `endpoint` option value contains only 
the local IP:

```
[server]
endpoint = tcp://127.0.0.1:8529
```

If you find any public IPs or `0.0.0.0` in `endpoint`, it means ArangoDB may respond to
web requests. So you better change `endpoint` to value `tcp://127.0.0.1:8529` as above.

You may also want to [activate authentication](UsingAuthentication.html) in ArangoDB.

If you made any changes to the ArangoDB configuration, you need to restart ArangoDB now:

```
service arangodb restart
```

### Configure the proxy server - Apache

Now the proxy server has to be configured. This recipe assumes that Apache2 is installed.
To make Apache proxy requests, you have to activate `mod_proxy`. This can be done by running
the following commands:

```
$ sudo a2enmod proxy
$ sudo a2enmod proxy_http 
```

Now adjust your web server's configuration so it routes requests for a specific URL path
to ArangoDB. You probably want ArangoDB's responses to be returned to the caller, too.
This can be done by using the `ProxyPass` and `ProxyPassReverse` options.

For example, if your Foxx app resides in database `_system` and is mounted at mountpoint
`/myapp`, and you want to make it accessible via URL path `/great-app`, the proxy configuration 
should look as follows:

```
ProxyPass /great-app http://127.0.0.1:8529/_db/_system/myapp
ProxyPassReverse /great-app http://127.0.0.1:8529/_db/_system/myapp
```

The above lines should be added to the configuration file of the Apache vhost you want to adjust.
If you're still running the Apache default configuration, the vhost configuration file will 
be `/etc/apache2/sites-enabled/000-default.conf`, but it's likely that there are already
different vhost files present in `sites-enabled`. So you have to find the correct file and
add the two lines to it.

After that, restart Apache using this command:
```
$ service apache2 restart 
```

Making the arangodb administation interface accessible under the `/arangodb/` URL works like this:
(choose a part of your server configuration for elevated security)

```
ProxyPass /arangodb/ http://127.0.0.1:8529/
ProxyPassReverse /arangodb/ http://127.0.0.1:8529/
ProxyPass /_db/ http://127.0.0.1:8529/_db/
ProxyPassReverse /_db/ http://127.0.0.1:8529/_db/
ProxyPass /_api/ http://127.0.0.1:8529/_api/
ProxyPassReverse /_api/ http://127.0.0.1:8529/_api/

```

Restart Apache again.

###Configure the proxy server - NGINX
Nginx doesn't offer modules in a way apache does. However, HTTP-Proxying is one of its core features.

Adopting the above example for nginx may look like this (add it to a server section):

        location /great-app {
          allow all;
          proxy_pass http://127.0.0.1:8529/_db/_system/myapp;
        }

Respectively the availability to the Managementconsole:

        location /arango/ {
          allow all;
          proxy_pass http://127.0.0.1:8529/;
        }
        location /_db {
           allow all;
           proxy_pass http://127.0.0.1:8529/_db;
        }
        location /_api {
           allow all;
           proxy_pass http://127.0.0.1:8529/_api;
        }

### Validate the accessibility

Now it's time to validate that everything works as desired.

First check whether your web server is still accessible. To check this, you can run the 
following two commands from another server:

```
curl --dump - http://your.domain.com/
```

If that's still working, check if ArangoDB is still accessible from the outside:

```
curl --dump - http://your.domain.com:8529/_api/version
```

This should **not** work. If curl hangs or aborts with an error, that's good news.

Now check if your Foxx app is accessible via URL path `/great-app` on your server on port 80:

```
curl --dump - http://your.domain.com/great-app
```

You should also check whether any other APIs or apps in ArangoDB are unintentionally accessible 
from the outside, e.g.:

```
curl --dump - http://your.domain.com/_api/version
curl --dump - http://your.domain.com/path/to/other/app
```

If you don't have curl installed, you can also test accessibility with a browser. However, browsers 
tend to cache server responses (and redirections) so you should make sure that you are not getting 
stale results from the browser cache. You have been warned.

Note: the above is for HTTP on port 80. If you are running SSL, you need to change the commands to https.


### Caveats

This is not a recipe about general server security. Other security precautions such as keeping
the system up-to-date, using a firewall and shutting down unnecessary services on web-facing
hosts still apply. Get help from an expert when unsure about your general server configuration!


**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #apache2 #proxy #foxx #security
