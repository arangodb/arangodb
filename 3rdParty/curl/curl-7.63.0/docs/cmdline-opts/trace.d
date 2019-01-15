Long: trace
Arg: <file>
Help: Write a debug trace to FILE
Mutexed: verbose trace-ascii
---
Enables a full trace dump of all incoming and outgoing data, including
descriptive information, to the given output file. Use "-" as filename to have
the output sent to stdout. Use "%" as filename to have the output sent to
stderr.

If this option is used several times, the last one will be used.
