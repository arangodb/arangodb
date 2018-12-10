ArangoDB Server Options
=======================

Usage: `arangod [<options>]`

The database directory can be specified as positional (unnamed) first parameter:

    arangod /path/to/datadir

Or explicitly as named parameter:

    arangod --database.directory /path/to/datadir

All other parameters need to be passed as named parameters.
That is two hyphens followed by the option name, an equals sign or a space and
finally the parameter value. The value needs to be wrapped in double quote marks
if the value contains whitespace. Extra whitespace around `=` is allowed:

    arangod --database.directory = "/path with spaces/to/datadir"

See [Configuration](../../Administration/Configuration/README.md)
if you want to translate startup parameters to configuration files.

See
[Fetch Current Configuration Options](../../Administration/Configuration/README.md#fetch-current-configuration-options)
if you want to query the `arangod` server for the current settings at runtime.

@startDocuBlock program_options_arangod
