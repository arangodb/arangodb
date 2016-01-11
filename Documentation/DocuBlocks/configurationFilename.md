

@brief config file
`--configuration filename`

`-c filename`

Specifies the name of the configuration file to use.

If this command is not passed to the server, then by default, the server
will attempt to first locate a file named *~/.arango/arangod.conf* in the
user's home directory.

If no such file is found, the server will proceed to look for a file
*arangod.conf* in the system configuration directory. The system
configuration directory is platform-specific, and may be changed when
compiling ArangoDB yourself. It may default to */etc/arangodb* or
*/usr/local/etc/arangodb*. This file is installed when using a package
manager like rpm or dpkg. If you modify this file and later upgrade to a
new
version of ArangoDB, then the package manager normally warns you about the
conflict. In order to avoid these warning for small adjustments, you can
put
local overrides into a file *arangod.conf.local*.

Only command line options with a value should be set within the
configuration file. Command line options which act as flags should be
entered on the command line when starting the server.

Whitespace in the configuration file is ignored. Each option is specified
on
a separate line in the form

```js
key = value
```

Alternatively, a header section can be specified and options pertaining to
that section can be specified in a shorter form

```js
[log]
level = trace
```

rather than specifying

```js
log.level = trace
```

Comments can be placed in the configuration file, only if the line begins
with one or more hash symbols (#).

There may be occasions where a configuration file exists and the user
wishes
to override configuration settings stored in a configuration file. Any
settings specified on the command line will overwrite the same setting
when
it appears in a configuration file. If the user wishes to completely
ignore
configuration files without necessarily deleting the file (or files), then
add the command line option

```js
-c none
```

or

```js
--configuration none
```

When starting up the server. Note that, the word *none* is
case-insensitive.

