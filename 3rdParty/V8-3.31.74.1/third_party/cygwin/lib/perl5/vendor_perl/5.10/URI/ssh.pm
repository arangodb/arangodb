package URI::ssh;
require URI::_login;
@ISA=qw(URI::_login);

# ssh://[USER@]HOST[:PORT]/SRC

sub default_port { 22 }

1;
