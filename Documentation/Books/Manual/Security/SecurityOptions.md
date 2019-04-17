# Server security options

_arangod_ provides a variety of options to make a setup more secure. 
Administrators can use these options to limit access to certain ArangoDB
server functionality as well as providing the leakage of information about
the environment that a server is running in.

## General security options

The following security options are available:

- `--server.harden`
  If this option is set to `true` and authentication is enabled, non-admin users
  will be denied access to the following REST APIs:
  
  * `/_admin/log`
  * `/_admin/log/level`
  * `/_admin/status`
  * `/_admin/statistics`
  * `/_admin/statistics-description`
  * `/_api/engine/stats`
 
  Additionally, no version details will be revealed by the version REST API at 
  `/_api/version`.
  `
  The default value for this option is `false`.

## JavaScript security options

`arangod` has several options that allow you to make your installation more
secure when it comes to running application code in it. Below you will find 
an overview of the relevant options.

### Whitelists

Several options exists to restrict JavaScript application code functionality 
to certain whitelists of allowed items. The whitelists can be used to allow access
to only certain functionality. Access to any item not specified in the whitelist
will then be disallowed.

Whitelist options need to be specified as ECMAScript regular expressions. Each 
option can be used multiple times. In this case, the individual values for each 
whitelist option will be combined with a _logical or_.

For example, the following combination of startup options

    --javascript.files-whitelist "^/etc/required/.*"
    --javascript.files-whitelist "^/etc/mtab/.*"

will resolve internally to the following regular expressions:

```
whitelist = "^/etc/required/.*|^/etc/mtab/.*"
```

Files in the directories `/etc/required` and `/etc/mtab` will then be
accessible, while access to files in any other directories will be disallowed.

Please note that access to the following directories will always be allowed via 
JavaScript code even if not explicitly whitelisted:

- ArangoDB's temporary directory. Note that the temporary directory location 
  can be specified via the `--temp.path` option at startup. If the option is
  not specified, ArangoDB will use a sub-directory of the system's temporary directory).
- ArangoDB's own JavaScript code, shipped with the ArangoDB release packages.
  The path is specified by the startup option `--javascript.startup-directory`.
  All JavaScript code is allowed to read from this directory and its subdirectories.

### Options for whitelisting

The following dedicated options are available for whitelisting access: 

- `--javascript.startup-options-whitelist`:
  This option controls which startup options will be exposed to JavaScript code, 
  following above rules for whitelists.

- `--javascript.environment-variables-whitelist`:
  This option controls which environment variables will be exposed to JavaScript
  code, following above rules for whitelists.

- `--javascript.endpoints-whitelist`:
  This option controls which endpoints can be used from within the `@arangodb/request`
  JavaScript module.
  Endpoint values are passed into the filter in a normalized format starting
  with either of the prefixes `tcp://`, `ssl://`, `unix://` or `srv://`.
  Note that for HTTP/SSL-based endpoints the port number will be included too,
  and that the endpoint can be specified either as an IP address or host name
  from application code.

- `--javascript.files-whitelist`:
  This option controls which filesystem paths can be accessed from JavaScript code 
  in restricted contexts, following above rules for whitelists.

### Additional JavaScript security options

In addition to the whitelisting security options, the following extra options 
are available for locking down JavaScript access to server functionality:

- `--javascript.allow-port-testing`:
  If set to `true`, this option enables the `testPort` JavaScript function in the
  `internal` module. The default value is `false`.

- `--javascript.allow-external-process-control`:
  If set to `true`, this option allows the execution and control of external processes
  from JavaScript code via the functions from the `internal` module:
  
  - executeExternal
  - executeExternalAndWait
  - getExternalSpawned
  - killExternal
  - suspendExternal
  - continueExternal
  - statusExternal

  The default value is `false`.

- `--javascript.harden`:
  If set to `true`, this setting will deactivate the following JavaScript functions
  which may leak information about the environment:

  - `internal.clientStatistics()`
  - `internal.httpStatistics()`
  - `internal.processStatistics()`
  - `internal.getPid()`
  - `internal.logLevel()`.

  The default value is `false`.

## Security options for managing Foxx applications

The following options are available for controlling the installation of Foxx applications
in an ArangoDB server:

- `--foxx.api`:
  If set to `false`, this option disables the Foxx management API, which will make it
  impossible to install and uninstall Foxx applications. Setting the option to `false`
  will also deactivate the "Services" section in the web interface. 
  The default value is `true`, meaning that Foxx apps can be installed and uninstalled.

- `--foxx.store`:
  If set to `false`, this option disables the Foxx app store in ArangoDB's web interface,
  which will also prevent ArangoDB and its web interface from making calls to the main 
  Foxx application Github repository at https://github.com/arangodb/foxx-apps.
  The default value is `true`.
