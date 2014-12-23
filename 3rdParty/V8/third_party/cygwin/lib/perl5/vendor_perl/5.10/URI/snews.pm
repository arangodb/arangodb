package URI::snews;  # draft-gilman-news-url-01

require URI::news;
@ISA=qw(URI::news);

sub default_port { 563 }

1;
