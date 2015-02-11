package URI::rlogin;
require URI::_login;
@ISA = qw(URI::_login);

sub default_port { 513 }

1;
