Long: data-binary
Arg: <data>
Help: HTTP POST binary data
Protocols: HTTP
---
This posts data exactly as specified with no extra processing whatsoever.

If you start the data with the letter @, the rest should be a filename.  Data
is posted in a similar manner as --data does, except that newlines and
carriage returns are preserved and conversions are never done.

Like --data the default content-type sent to the server is
application/x-www-form-urlencoded. If you want the data to be treated as
arbitrary binary data by the server then set the content-type to octet-stream:
-H "Content-Type: application/octet-stream".

If this option is used several times, the ones following the first will append
data as described in --data.
