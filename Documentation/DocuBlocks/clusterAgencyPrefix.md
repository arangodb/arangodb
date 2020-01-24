

@brief global Agency prefix
`--cluster.agency-prefix prefix`

The global key prefix used in all requests to the Agency. The specified
prefix will become part of each Agency key. Specifying the key prefix
allows managing multiple ArangoDB clusters with the same Agency
server(s).

*prefix* must consist of the letters *a-z*, *A-Z* and the digits *0-9*
only. Specifying a prefix is mandatory.

@EXAMPLES

```
--cluster.prefix mycluster
```

