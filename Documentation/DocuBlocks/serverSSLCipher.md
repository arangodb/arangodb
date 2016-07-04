

@brief ssl cipher list to use
`--ssl.cipher-list cipher-list`

This option can be used to restrict the server to certain SSL ciphers
only,
and to define the relative usage preference of SSL ciphers.

The format of *cipher-list* is documented in the OpenSSL documentation.

To check which ciphers are available on your platform, you may use the
following shell command:

```
> openssl ciphers -v

ECDHE-RSA-AES256-SHA    SSLv3 Kx=ECDH     Au=RSA  Enc=AES(256)  Mac=SHA1
ECDHE-ECDSA-AES256-SHA  SSLv3 Kx=ECDH     Au=ECDSA Enc=AES(256)  Mac=SHA1
DHE-RSA-AES256-SHA      SSLv3 Kx=DH       Au=RSA  Enc=AES(256)  Mac=SHA1
DHE-DSS-AES256-SHA      SSLv3 Kx=DH       Au=DSS  Enc=AES(256)  Mac=SHA1
DHE-RSA-CAMELLIA256-SHA SSLv3 Kx=DH       Au=RSA  Enc=Camellia(256)
Mac=SHA1
...
```

The default value for *cipher-list* is "ALL".

**Note**: this option is only relevant if at least one SSL endpoint is
used.

