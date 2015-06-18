package URI::ldaps;
require URI::ldap;
@ISA=qw(URI::ldap);

sub default_port { 636 }

1;
