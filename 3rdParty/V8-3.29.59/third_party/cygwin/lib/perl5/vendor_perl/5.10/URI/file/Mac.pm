package URI::file::Mac;

require URI::file::Base;
@ISA=qw(URI::file::Base);

use strict;
use URI::Escape qw(uri_unescape);



sub _file_extract_path
{
    my $class = shift;
    my $path = shift;

    my @pre;
    if ($path =~ s/^(:+)//) {
	if (length($1) == 1) {
	    @pre = (".") unless length($path);
	} else {
	    @pre = ("..") x (length($1) - 1);
	}
    } else { #absolute
	$pre[0] = "";
    }

    my $isdir = ($path =~ s/:$//);
    $path =~ s,([%/;]), URI::Escape::escape_char($1),eg;

    my @path = split(/:/, $path, -1);
    for (@path) {
	if ($_ eq "." || $_ eq "..") {
	    $_ = "%2E" x length($_);
	}
	$_ = ".." unless length($_);
    }
    push (@path,"") if $isdir;
    (join("/", @pre, @path), 1);
}


sub file
{
    my $class = shift;
    my $uri = shift;
    my @path;

    my $auth = $uri->authority;
    if (defined $auth) {
	if (lc($auth) ne "localhost" && $auth ne "") {
	    my $u_auth = uri_unescape($auth);
	    if (!$class->_file_is_localhost($u_auth)) {
		# some other host (use it as volume name)
		@path = ("", $auth);
		# XXX or just return to make it illegal;
	    }
	}
    }
    my @ps = split("/", $uri->path, -1);
    shift @ps if @path;
    push(@path, @ps);

    my $pre = "";
    if (!@path) {
	return;  # empty path; XXX return ":" instead?
    } elsif ($path[0] eq "") {
	# absolute
	shift(@path);
	if (@path == 1) {
	    return if $path[0] eq "";  # not root directory
	    push(@path, "");           # volume only, effectively append ":"
	}
	@ps = @path;
	@path = ();
        my $part;
	for (@ps) {  #fix up "." and "..", including interior, in relatives
	    next if $_ eq ".";
	    $part = $_ eq ".." ? "" : $_;
	    push(@path,$part);
	}
	if ($ps[-1] eq "..") {  #if this happens, we need another :
	    push(@path,"");
	}
	
    } else {
	$pre = ":";
	@ps = @path;
	@path = ();
        my $part;
	for (@ps) {  #fix up "." and "..", including interior, in relatives
	    next if $_ eq ".";
	    $part = $_ eq ".." ? "" : $_;
	    push(@path,$part);
	}
	if ($ps[-1] eq "..") {  #if this happens, we need another :
	    push(@path,"");
	}
	
    }
    return unless $pre || @path;
    for (@path) {
	s/;.*//;  # get rid of parameters
	#return unless length; # XXX
	$_ = uri_unescape($_);
	return if /\0/;
	return if /:/;  # Should we?
    }
    $pre . join(":", @path);
}

sub dir
{
    my $class = shift;
    my $path = $class->file(@_);
    return unless defined $path;
    $path .= ":" unless $path =~ /:$/;
    $path;
}

1;
