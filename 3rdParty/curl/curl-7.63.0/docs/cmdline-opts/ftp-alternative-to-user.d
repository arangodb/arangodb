Long: ftp-alternative-to-user
Arg: <command>
Help: String to replace USER [name]
Protocols: FTP
Added: 7.15.5
---
If authenticating with the USER and PASS commands fails, send this command.
When connecting to Tumbleweed's Secure Transport server over FTPS using a
client certificate, using "SITE AUTH" will tell the server to retrieve the
username from the certificate.
