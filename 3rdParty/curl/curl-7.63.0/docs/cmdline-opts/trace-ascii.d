Long: trace-ascii
Arg: <file>
Help: Like --trace, but without hex output
Mutexed: trace verbose
---
Enables a full trace dump of all incoming and outgoing data, including
descriptive information, to the given output file. Use "-" as filename to have
the output sent to stdout.

This is very similar to --trace, but leaves out the hex part and only shows
the ASCII part of the dump. It makes smaller output that might be easier to
read for untrained humans.

If this option is used several times, the last one will be used.
