

@brief ssl options to use
`--ssl-.options value`

This option can be used to set various SSL-related options. Individual
option values must be combined using bitwise OR.

Which options are available on your platform is determined by the OpenSSL
version you use. The list of options available on your platform might be
retrieved by the following shell command:

```
 > grep "#define SSL_OP_.*" /usr/include/openssl/ssl.h

 #define SSL_OP_MICROSOFT_SESS_ID_BUG                    0x00000001L
 #define SSL_OP_NETSCAPE_CHALLENGE_BUG                   0x00000002L
 #define SSL_OP_LEGACY_SERVER_CONNECT                    0x00000004L
 #define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG         0x00000008L
 #define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG              0x00000010L
 #define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER               0x00000020L
 ...
```

A description of the options can be found online in the
[OpenSSL documentation](http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html)

**Note**: this option is only relevant if at least one SSL endpoint is
used.

