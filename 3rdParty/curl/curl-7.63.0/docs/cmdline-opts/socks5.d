Long: socks5
Arg: <host[:port]>
Help: SOCKS5 proxy on given host + port
Added: 7.18.0
---
Use the specified SOCKS5 proxy - but resolve the host name locally. If the
port number is not specified, it is assumed at port 1080.

This option overrides any previous use of --proxy, as they are mutually
exclusive.

Since 7.21.7, this option is superfluous since you can specify a socks5 proxy
with --proxy using a socks5:// protocol prefix.

Since 7.52.0, --preproxy can be used to specify a SOCKS proxy at the same time
--proxy is used with an HTTP/HTTPS proxy. In such a case curl first connects to
the SOCKS proxy and then connects (through SOCKS) to the HTTP or HTTPS proxy.

If this option is used several times, the last one will be used.

This option (as well as --socks4) does not work with IPV6, FTPS or LDAP.
