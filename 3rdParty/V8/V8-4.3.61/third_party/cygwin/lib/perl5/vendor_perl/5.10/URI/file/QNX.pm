package URI::file::QNX;

require URI::file::Unix;
@ISA=qw(URI::file::Unix);

use strict;

sub _file_extract_path
{
    my($class, $path) = @_;
    # tidy path
    $path =~ s,(.)//+,$1/,g; # ^// is correct
    $path =~ s,(/\.)+/,/,g;
    $path = "./$path" if $path =~ m,^[^:/]+:,,; # look like "scheme:"
    $path;
}

1;
