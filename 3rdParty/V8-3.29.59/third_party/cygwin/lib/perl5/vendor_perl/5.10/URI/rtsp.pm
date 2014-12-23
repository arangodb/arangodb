package URI::rtsp;

require URI::http;
@ISA=qw(URI::http);

sub default_port { 554 }

1;
