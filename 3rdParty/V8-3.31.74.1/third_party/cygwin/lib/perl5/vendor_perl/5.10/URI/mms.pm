package URI::mms;

require URI::http;
@ISA=qw(URI::http);

sub default_port { 1755 }

1;
