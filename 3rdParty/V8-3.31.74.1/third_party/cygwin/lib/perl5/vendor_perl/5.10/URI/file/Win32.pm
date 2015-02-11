package URI::file::Win32;

require URI::file::Base;
@ISA=qw(URI::file::Base);

use strict;
use URI::Escape qw(uri_unescape);

sub _file_extract_authority
{
    my $class = shift;

    return $class->SUPER::_file_extract_authority($_[0])
	if defined $URI::file::DEFAULT_AUTHORITY;

    return $1 if $_[0] =~ s,^\\\\([^\\]+),,;  # UNC
    return $1 if $_[0] =~ s,^//([^/]+),,;     # UNC too?

    if ($_[0] =~ s,^([a-zA-Z]:),,) {
	my $auth = $1;
	$auth .= "relative" if $_[0] !~ m,^[\\/],;
	return $auth;
    }
    return undef;
}

sub _file_extract_path
{
    my($class, $path) = @_;
    $path =~ s,\\,/,g;
    #$path =~ s,//+,/,g;
    $path =~ s,(/\.)+/,/,g;

    if (defined $URI::file::DEFAULT_AUTHORITY) {
	$path =~ s,^([a-zA-Z]:),/$1,;
    }

    return $path;
}

sub _file_is_absolute {
    my($class, $path) = @_;
    return $path =~ m,^[a-zA-Z]:, || $path =~ m,^[/\\],;
}

sub file
{
    my $class = shift;
    my $uri = shift;
    my $auth = $uri->authority;
    my $rel; # is filename relative to drive specified in authority
    if (defined $auth) {
        $auth = uri_unescape($auth);
	if ($auth =~ /^([a-zA-Z])[:|](relative)?/) {
	    $auth = uc($1) . ":";
	    $rel++ if $2;
	} elsif (lc($auth) eq "localhost") {
	    $auth = "";
	} elsif (length $auth) {
	    $auth = "\\\\" . $auth;  # UNC
	}
    } else {
	$auth = "";
    }

    my @path = $uri->path_segments;
    for (@path) {
	return undef if /\0/;
	return undef if /\//;
	#return undef if /\\/;        # URLs with "\" is not uncommon
    }
    return undef unless $class->fix_path(@path);

    my $path = join("\\", @path);
    $path =~ s/^\\// if $rel;
    $path = $auth . $path;
    $path =~ s,^\\([a-zA-Z])[:|],\u$1:,;

    return $path;
}

sub fix_path { 1; }

1;
