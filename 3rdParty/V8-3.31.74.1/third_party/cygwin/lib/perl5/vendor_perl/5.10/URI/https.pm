package URI::https;
require URI::http;
@ISA=qw(URI::http);

sub default_port { 443 }

1;
