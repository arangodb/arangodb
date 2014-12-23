package URI::_login;

require URI::_server;
require URI::_userpass;
@ISA = qw(URI::_server URI::_userpass);

# Generic terminal logins.  This is used as a base class for 'telnet',
# 'tn3270', and 'rlogin' URL schemes.

1;
