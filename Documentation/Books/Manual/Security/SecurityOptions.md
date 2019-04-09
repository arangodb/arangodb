#Security Options

`arangod` has several options that allow you to make your installation more
secure. Below you will find an introduction to back- and white-lists. Followed
by a detailed description of all options that are relevant for your server's
security.

## Black- and White-lists

In oder to make proper use of the options presented below you must understand
how black-and white-lists work for ArangoDB C++ applications.

- white-lists overrule black-lists
- all provided parameters must be valid words in the language defined by the
  *Modified ECMAScript regular expression grammar*
  (https://en.cppreference.com/w/cpp/regex/ecmascript)
- same options given multiple times will be combined into a logical
  disjunction.

### Examples

The following parameters:
`./arangodb-c++-executable --fs-white-list /etc/required/ --fs-white-list /etc/mtab --fs-black-list /etc --fs-black-list /home`
will resolve internally to the following regular expressions:
`whitelist = "/etc/required/|/etc/mtab"` and `backlist = "/etc|/home"` files in `/etc/required` and `/etc/mtab` will be accessible,
even though the blacklist tries to deny the access to `/etc`. This is the case because white-lists always win.

## Contexts
    In arangod there are different contexts. Foxx code for example will typically
    started in one of the restricted context, while some JavaScript code that
    controls aspects of the cluster will be run in an interanl context.

## Options

   `--server.harden`
        If this option is set to true and authentication is enabled non admin
        users will be denied version information via the rest api and they will
        not be able to change the servers log level. (default: false)

  `--foxx.disable-api`
        If set to true this option disables the Foxx management api.
        foxxmanager and foxx tool will be affected as well. (default: false)

  `--foxx.disable-store`
        If set to true this option disables the Foxx store in arangod's web-ui.
        (default: false)

  `--javascript.allow-port-testing`
        If set to true this option enables the testPort() JavaScript function
        in all contexts. (default: false)

  `--javascript.allow-external-process-control`
        If set to true allows execution and control of external binaries in
        restricted contexts. (default: false)

  `--javascript.harden`
        If set to true it will deactivate the following JavaScript funtions:
        getPid(), processStatistics() and logLevel() in a all contexts (default: false)

  `--javascript.startup-options-white-list` and `--javascript.startup-options-black-list`
        Flags that control with startup options will be exposed to JavaScript in all
        contexts, following above rules for black- and white-lists.

  `--javascript.environment-variables-white-list` and `--javascript.environment-variables-black-list`
        Flags that control wich environment variables will be exposed to
        JavaScript in all contexts, following above rules for black- and
        white-lists.

  `--javascript.endpoints-white-list` and `--javascript.endpoints-black-list`
        Flags that control wich endpoints can be used with internal.download()
        in restricted contexts, following above rules for black- and
        white-lists. Endpoint values are passed into the filter in a normalized
        format starting with either of the prefixes `tcp://`, `ssl://`,
        `unix://` or `srv://`.
        Note that for HTTP/SSL-based endpoints the port number will be included
        too, and that the endpoint can be specified as an IP address or host
        name.

  `--javascript.files-white-list` and `--javascript.files-black-list`
        Flags that control wich paths can be accessed from JavaScript
        in restricted contexts, following above rules for black- and
        white-lists.

