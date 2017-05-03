

@brief SSL protocol type to use
`--ssl.protocol value`

Use this option to specify the default encryption protocol to be used.
The following variants are available:
- 1: SSLv2
- 2: SSLv2 or SSLv3 (negotiated)
- 3: SSLv3
- 4: TLSv1
- 5: TLSv1.2

The default *value* is 5 (TLSv1.2).

**Note**: this option is only relevant if at least one SSL endpoint is
used.

