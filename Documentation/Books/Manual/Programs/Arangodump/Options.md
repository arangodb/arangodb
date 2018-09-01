Arangodump Options
==================

Usage: `arangodump [<options>]`

@startDocuBlock program_options_arangodump

Notes
-----

### Encryption Option Details

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}
 
*--encryption.keyfile path-of-keyfile*

The file `path-to-keyfile` must contain the encryption key. This
file must be secured, so that only `arangodump` or `arangorestore` can access it.
You should also ensure that in case someone steals your hardware, they will not be
able to read the file. For example, by encrypting `/mytmpfs` or
creating an in-memory file-system under `/mytmpfs`. The encryption keyfile must 
contain 32 bytes of data.

*--encryption.key-generator path-to-my-generator*

This output is used if you want to use the program to generate your encryption key.
The program `path-to-my-generator` must output the encryption on standard output
and exit. The encryption keyfile must contain 32 bytes of data.

