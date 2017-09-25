OpenSSL
========

OpenSSL 1.1 is on its way to mainstream. So far (ArangoDB 3.2) has only been thorougly tested with OpenSSL 1.0 and 1.1 is unsupported.

Building against 1.1 will currently result in a compile error:

```
/arangodb/arangodb/lib/SimpleHttpClient/SslClientConnection.cpp:224:14: error: use of undeclared identifier 'SSLv2_method'
      meth = SSLv2_method();
             ^
/arangodb/arangodb/lib/SimpleHttpClient/SslClientConnection.cpp:239:14: warning: 'TLSv1_method' is deprecated [-Wdeprecated-declarations]
      meth = TLSv1_method();
             ^
/usr/include/openssl/ssl.h:1612:45: note: 'TLSv1_method' has been explicitly marked deprecated here
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_method(void)) /* TLSv1.0 */
                                            ^
/arangodb/arangodb/lib/SimpleHttpClient/SslClientConnection.cpp:243:14: warning: 'TLSv1_2_method' is deprecated [-Wdeprecated-declarations]
      meth = TLSv1_2_method();
```

You should install openssl 1.0 (should be possible to install it alongside 1.1).

After that help cmake to find the 1.0 variant.

Example on Arch Linux:

```
cmake -DOPENSSL_INCLUDE_DIR=/usr/include/openssl-1.0/ -DOPENSSL_SSL_LIBRARY=/usr/lib/libssl.so.1.0.0 -DOPENSSL_CRYPTO_LIBRARY=/usr/lib/libcrypto.so.1.0.0 <SOURCE_PATH>
```

After that ArangoDB should compile fine.