Tools
=====

If the VPack library is built with option `-DBuildTools=ON` (which is the default), 
then the following executables will be compiled:

* `json-to-vpack`: this tool can be used to convert a JSON input file into a VPack
  output file. The tool expects the JSON input file it should read from as its first 
  argument, and the filename for its output as the second argument.

  Further options for *json-to-vpack* are:
  * `--compact`: if specified, allows storing VPack Array and Object values in the 
    *compact* format, without indexes for sub values. This may generate smaller VPack 
    values, but their internals can later be accessed without indexes only.
  * `--no-compact`: the opposite of `--compact`.
  * `--compress`: if specified, an additional pass over the input JSON is made to
    collect and count all object keys in a dictionary. That dictionary is then used
    when building the VPack result value. Using a dictionary for object keys may
    drastically reduce the VPack result size if object keys repeat and are long.
    Note that to decode the VPack value the dictionary will be needed! 
  * `--no-compress`: the opposite of `--compress`.
  * `--hex`: will output a hex dump of the VPack result instead of the binary VPack
    value.

  On Linux, *json-to-vpack* supports the pseudo filenames `-` and `+` for stdin and
  stdout.

* `vpack-to-json`: this tool can be used to convert a VPack value in a file back into
  a JSON string. The tool expects the (binary) VPack input file it should read from 
  in its first argument, and the filename for the JSON output file as its second argument.

  Further options for *vpack-to-json* are:
  * `--pretty`: generate pretty-printed JSON to improve readability
  * `--no-pretty`: do not generate pretty-printed JSON
  * `--print-unsupported`: convert non-JSON types into something else
  * `--no-print-unsupported`: fail when encoutering a non-JSON type
  * `--hex`: try to turn hex-encoded input into binary vpack
  * `--validate`: validate input VelocyPack data
  * `--no-validate`: do not validate input VelocyPack data

  On Linux, *vpack-to-json* supports the pseudo filenames `-` and `+` for stdin and
  stdout.

* `vpack-validate`: this tool can be used to validate a VPack value in a file for
  correctness. The tool expects the (binary) VPack input file it should read from 
  in its first argument. It will return status code 0 if the VPack is valid, and
  a non-0 exit code if the VPack is invalid.

  Further options for *vpack-validate* are:
  * `--hex`: try to turn hex-encoded input into binary vpack

  On Linux, *vpack-validate* supports the pseudo filename `-` for stdin.
