# Server security options

_arangod_ provides a variety of options to make a setup more secure. 
Administrators can use these options to limit access to certain ArangoDB
server functionality as well as providing the leakage of information about
the environment that a server is running in.

## General security options

The following security options are available:

- `--server.harden`
  If this option is set to `true` and authentication is enabled, non-admin users
  will be denied version information via the REST version API at `/_api/version`, and 
  they will not be able to change the server's log level via the REST API at
  `/_admin/log/level`. The default value is `false`.


## JavaScript security options

`arangod` has several options that allow you to make your installation more
secure when it comes to running application code in it. Below you will find 
an overview of the relevant options.

### Blacklist and whitelists

Several options to restrict application code functionality consist of a 
blacklist part and whitelist part. Blacklists can be used to disallow access
to dedicated functionality, whereas whitelists can be used to allow access
to certain functionality.

If a functionality is covered in both a blacklist and a whitelist, the 
whitelist will overrule and access to the functionality will be allowed.

Values for blacklist and whitelist options need to be specified as ECMAScript 
regular expressions. Each option can be used multiple times. In this case,
the individual values for each option will be combined with a _logical or_.

For example, the following combination of startup options

    --javascript.files-white-list "/etc/required/"
    --javascript.files-white-list "/etc/mtab"
    --javascript.files-black-list "/etc"
    --javascript.files-black-list "/home"

will resolve internally to the following regular expressions:

```
whitelist = "/etc/required/|/etc/mtab"
blacklist = "/etc|/home"
```

Files in `/etc/required` and `/etc/mtab` will be accessible, because even though the 
blacklist regular expression matches `/etc` it, the access to it is explicitly
allowed via the whitelist.

### Options for blacklisting and whitelisting

The following options are available for blacklisting and whitelisting access
to dedicated functionality for application code:

- `--javascript.startup-options-white-list` and `--javascript.startup-options-black-list`:
  These options control which startup options will be exposed to JavaScript code, 
  following above rules for blacklists and whitelists.

- `--javascript.environment-variables-white-list` and `--javascript.environment-variables-black-list`:
  These options control which environment variables will be exposed to JavaScript
  code, following above rules for blacklists and whitelists.

- `--javascript.endpoints-white-list` and `--javascript.endpoints-black-list`:
  These options control which endpoints can be used from within the `@arangodb/request`
  JavaScript module.
  Endpoint values are passed into the filter in a normalized format starting
  with either of the prefixes `tcp://`, `ssl://`, `unix://` or `srv://`.
  Note that for HTTP/SSL-based endpoints the port number will be included too,
  and that the endpoint can be specified either as an IP address or host name
  from application code.

- `--javascript.files-white-list` and `--javascript.files-black-list`:
  These options control which filesystem paths can be accessed from JavaScript code 
  in restricted contexts, following above rules for blacklist and whitelists.

### Additional JavaScript security options

In addition to the blacklisting and whitelisting security options, the following
extra options are available for locking down JavaScript access to server functionality:

- `--javascript.allow-port-testing`:
  If set to `true`, this option enables the `testPort` JavaScript function in the
  `internal` module. The default value is `false`.

- `--javascript.allow-external-process-control`:
  If set to `true`, this option allows the execution and control of external processes
  from JavaScript code. The default value is `false`.

- `--javascript.harden`:
  If set to `true`, this setting will deactivate the following JavaScript functions
  which may leak information about the environment:

  - `internal.getPid()`
  - `internal.processStatistics()`
  - `internal.logLevel()`.

  The default value is `false`.

## Security options for managing Foxx applications

The following options are available for controlling the installation of Foxx applications
in an ArangoDB server:

- `--foxx.api`:
  If set to `false`, this option disables the Foxx management API, which will make it
  impossible to install and uninstall Foxx applications. The default value is `true`.

- `--foxx.store`:
  If set to `false`, this option disables the Foxx app store in ArangoDB's web interface,
  which will prevent ArangoDB and its web interface from making calls to the main Foxx 
  application Github repository at https://github.com/arangodb/foxx-apps.
  The default value is `true`.

