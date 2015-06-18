package URI::file::Unix;

require URI::file::Base;
@ISA=qw(URI::file::Base);

use strict;
use URI::Escape qw(uri_unescape);

sub _file_extract_path
{
    my($class, $path) = @_;

    # tidy path
    $path =~ s,//+,/,g;
    $path =~ s,(/\.)+/,/,g;
    $path = "./$path" if $path =~ m,^[^:/]+:,,; # look like "scheme:"

    return $path;
}

sub _file_is_absolute {
    my($class, $path) = @_;
    return $path =~ m,^/,;
}

sub file
{
    my $class = shift;
    my $uri = shift;
    my @path;

    my $auth = $uri->authority;
    if (defined($auth)) {
	if (lc($auth) ne "localhost" && $auth ne "") {
	    $auth = uri_unescape($auth);
	    unless ($class->_file_is_localhost($auth)) {
		push(@path, "", "", $auth);
	    }
	}
    }

    my @ps = $uri->path_segments;
    shift @ps if @path;
    push(@path, @ps);

    for (@path) {
	# Unix file/directory names are not allowed to contain '\0' or '/'
	return undef if /\0/;
	return undef if /\//;  # should we really?
    }

    return join("/", @path);
}

1;
