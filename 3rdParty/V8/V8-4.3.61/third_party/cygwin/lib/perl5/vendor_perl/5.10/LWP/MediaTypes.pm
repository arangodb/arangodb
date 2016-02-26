package LWP::MediaTypes;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(guess_media_type media_suffix);
@EXPORT_OK = qw(add_type add_encoding read_media_types);
$VERSION = "5.810";

require LWP::Debug;
use strict;

# note: These hashes will also be filled with the entries found in
# the 'media.types' file.

my %suffixType = (
    'txt'   => 'text/plain',
    'html'  => 'text/html',
    'gif'   => 'image/gif',
    'jpg'   => 'image/jpeg',
    'xml'   => 'text/xml',
);

my %suffixExt = (
    'text/plain' => 'txt',
    'text/html'  => 'html',
    'image/gif'  => 'gif',
    'image/jpeg' => 'jpg',
    'text/xml'   => 'xml',
);

#XXX: there should be some way to define this in the media.types files.
my %suffixEncoding = (
    'Z'   => 'compress',
    'gz'  => 'gzip',
    'hqx' => 'x-hqx',
    'uu'  => 'x-uuencode',
    'z'   => 'x-pack',
    'bz2' => 'x-bzip2',
);

read_media_types();



sub _dump {
    require Data::Dumper;
    Data::Dumper->new([\%suffixType, \%suffixExt, \%suffixEncoding],
		      [qw(*suffixType *suffixExt *suffixEncoding)])->Dump;
}


sub guess_media_type
{
    my($file, $header) = @_;
    return undef unless defined $file;

    my $fullname;
    if (ref($file)) {
	# assume URI object
	$file = $file->path;
	#XXX should handle non http:, file: or ftp: URIs differently
    }
    else {
	$fullname = $file;  # enable peek at actual file
    }

    my @encoding = ();
    my $ct = undef;
    for (file_exts($file)) {
	# first check this dot part as encoding spec
	if (exists $suffixEncoding{$_}) {
	    unshift(@encoding, $suffixEncoding{$_});
	    next;
	}
	if (exists $suffixEncoding{lc $_}) {
	    unshift(@encoding, $suffixEncoding{lc $_});
	    next;
	}

	# check content-type
	if (exists $suffixType{$_}) {
	    $ct = $suffixType{$_};
	    last;
	}
	if (exists $suffixType{lc $_}) {
	    $ct = $suffixType{lc $_};
	    last;
	}

	# don't know nothing about this dot part, bail out
	last;
    }
    unless (defined $ct) {
	# Take a look at the file
	if (defined $fullname) {
	    $ct = (-T $fullname) ? "text/plain" : "application/octet-stream";
	}
	else {
	    $ct = "application/octet-stream";
	}
    }

    if ($header) {
	$header->header('Content-Type' => $ct);
	$header->header('Content-Encoding' => \@encoding) if @encoding;
    }

    wantarray ? ($ct, @encoding) : $ct;
}


sub media_suffix {
    if (!wantarray && @_ == 1 && $_[0] !~ /\*/) {
	return $suffixExt{$_[0]};
    }
    my(@type) = @_;
    my(@suffix, $ext, $type);
    foreach (@type) {
	if (s/\*/.*/) {
	    while(($ext,$type) = each(%suffixType)) {
		push(@suffix, $ext) if $type =~ /^$_$/;
	    }
	}
	else {
	    while(($ext,$type) = each(%suffixType)) {
		push(@suffix, $ext) if $type eq $_;
	    }
	}
    }
    wantarray ? @suffix : $suffix[0];
}


sub file_exts 
{
    require File::Basename;
    my @parts = reverse split(/\./, File::Basename::basename($_[0]));
    pop(@parts);        # never consider first part
    @parts;
}


sub add_type 
{
    my($type, @exts) = @_;
    for my $ext (@exts) {
	$ext =~ s/^\.//;
	$suffixType{$ext} = $type;
    }
    $suffixExt{$type} = $exts[0] if @exts;
}


sub add_encoding
{
    my($type, @exts) = @_;
    for my $ext (@exts) {
	$ext =~ s/^\.//;
	$suffixEncoding{$ext} = $type;
    }
}


sub read_media_types 
{
    my(@files) = @_;

    local($/, $_) = ("\n", undef);  # ensure correct $INPUT_RECORD_SEPARATOR

    my @priv_files = ();
    if($^O eq "MacOS") {
	push(@priv_files, "$ENV{HOME}:media.types", "$ENV{HOME}:mime.types")
	    if defined $ENV{HOME};  # Some does not have a home (for instance Win32)
    }
    else {
	push(@priv_files, "$ENV{HOME}/.media.types", "$ENV{HOME}/.mime.types")
	    if defined $ENV{HOME};  # Some doesn't have a home (for instance Win32)
    }

    # Try to locate "media.types" file, and initialize %suffixType from it
    my $typefile;
    unless (@files) {
	if($^O eq "MacOS") {
	    @files = map {$_."LWP:media.types"} @INC;
	}
	else {
	    @files = map {"$_/LWP/media.types"} @INC;
	}
	push @files, @priv_files;
    }
    for $typefile (@files) {
	local(*TYPE);
	open(TYPE, $typefile) || next;
        LWP::Debug::debug("Reading media types from $typefile");
	while (<TYPE>) {
	    next if /^\s*#/; # comment line
	    next if /^\s*$/; # blank line
	    s/#.*//;         # remove end-of-line comments
	    my($type, @exts) = split(' ', $_);
	    add_type($type, @exts);
	}
	close(TYPE);
    }
}

1;


__END__

=head1 NAME

LWP::MediaTypes - guess media type for a file or a URL

=head1 SYNOPSIS

 use LWP::MediaTypes qw(guess_media_type);
 $type = guess_media_type("/tmp/foo.gif");

=head1 DESCRIPTION

This module provides functions for handling media (also known as
MIME) types and encodings.  The mapping from file extensions to media
types is defined by the F<media.types> file.  If the F<~/.media.types>
file exists it is used instead.
For backwards compatibility we will also look for F<~/.mime.types>.

The following functions are exported by default:

=over 4

=item guess_media_type( $filename )

=item guess_media_type( $uri )

=item guess_media_type( $filename_or_uri, $header_to_modify )

This function tries to guess media type and encoding for a file or a URI.
It returns the content type, which is a string like C<"text/html">.
In array context it also returns any content encodings applied (in the
order used to encode the file).  You can pass a URI object
reference, instead of the file name.

If the type can not be deduced from looking at the file name,
then guess_media_type() will let the C<-T> Perl operator take a look.
If this works (and C<-T> returns a TRUE value) then we return
I<text/plain> as the type, otherwise we return
I<application/octet-stream> as the type.

The optional second argument should be a reference to a HTTP::Headers
object or any object that implements the $obj->header method in a
similar way.  When it is present the values of the
'Content-Type' and 'Content-Encoding' will be set for this header.

=item media_suffix( $type, ... )

This function will return all suffixes that can be used to denote the
specified media type(s).  Wildcard types can be used.  In a scalar
context it will return the first suffix found. Examples:

  @suffixes = media_suffix('image/*', 'audio/basic');
  $suffix = media_suffix('text/html');

=back

The following functions are only exported by explicit request:

=over 4

=item add_type( $type, @exts )

Associate a list of file extensions with the given media type.
Example:

    add_type("x-world/x-vrml" => qw(wrl vrml));

=item add_encoding( $type, @ext )

Associate a list of file extensions with an encoding type.
Example:

 add_encoding("x-gzip" => "gz");

=item read_media_types( @files )

Parse media types files and add the type mappings found there.
Example:

    read_media_types("conf/mime.types");

=back

=head1 COPYRIGHT

Copyright 1995-1999 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

