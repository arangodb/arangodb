package URI::telnet;
require URI::_login;
@ISA = qw(URI::_login);

sub default_port { 23 }

1;
