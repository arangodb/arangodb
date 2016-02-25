package HTTP::Request::Common;

use strict;
use vars qw(@EXPORT @EXPORT_OK $VERSION $DYNAMIC_FILE_UPLOAD);

$DYNAMIC_FILE_UPLOAD ||= 0;  # make it defined (don't know why)

require Exporter;
*import = \&Exporter::import;
@EXPORT =qw(GET HEAD PUT POST);
@EXPORT_OK = qw($DYNAMIC_FILE_UPLOAD);

require HTTP::Request;
use Carp();

$VERSION = "5.811";

my $CRLF = "\015\012";   # "\r\n" is not portable

sub GET  { _simple_req('GET',  @_); }
sub HEAD { _simple_req('HEAD', @_); }
sub PUT  { _simple_req('PUT' , @_); }

sub POST
{
    my $url = shift;
    my $req = HTTP::Request->new(POST => $url);
    my $content;
    $content = shift if @_ and ref $_[0];
    my($k, $v);
    while (($k,$v) = splice(@_, 0, 2)) {
	if (lc($k) eq 'content') {
	    $content = $v;
	}
	else {
	    $req->push_header($k, $v);
	}
    }
    my $ct = $req->header('Content-Type');
    unless ($ct) {
	$ct = 'application/x-www-form-urlencoded';
    }
    elsif ($ct eq 'form-data') {
	$ct = 'multipart/form-data';
    }

    if (ref $content) {
	if ($ct =~ m,^multipart/form-data\s*(;|$),i) {
	    require HTTP::Headers::Util;
	    my @v = HTTP::Headers::Util::split_header_words($ct);
	    Carp::carp("Multiple Content-Type headers") if @v > 1;
	    @v = @{$v[0]};

	    my $boundary;
	    my $boundary_index;
	    for (my @tmp = @v; @tmp;) {
		my($k, $v) = splice(@tmp, 0, 2);
		if (lc($k) eq "boundary") {
		    $boundary = $v;
		    $boundary_index = @v - @tmp - 1;
		    last;
		}
	    }

	    ($content, $boundary) = form_data($content, $boundary, $req);

	    if ($boundary_index) {
		$v[$boundary_index] = $boundary;
	    }
	    else {
		push(@v, boundary => $boundary);
	    }

	    $ct = HTTP::Headers::Util::join_header_words(@v);
	}
	else {
	    # We use a temporary URI object to format
	    # the application/x-www-form-urlencoded content.
	    require URI;
	    my $url = URI->new('http:');
	    $url->query_form(ref($content) eq "HASH" ? %$content : @$content);
	    $content = $url->query;
	}
    }

    $req->header('Content-Type' => $ct);  # might be redundant
    if (defined($content)) {
	$req->header('Content-Length' =>
		     length($content)) unless ref($content);
	$req->content($content);
    }
    else {
        $req->header('Content-Length' => 0);
    }
    $req;
}


sub _simple_req
{
    my($method, $url) = splice(@_, 0, 2);
    my $req = HTTP::Request->new($method => $url);
    my($k, $v);
    while (($k,$v) = splice(@_, 0, 2)) {
	if (lc($k) eq 'content') {
	    $req->add_content($v);
            $req->header("Content-Length", length(${$req->content_ref}));
	}
	else {
	    $req->push_header($k, $v);
	}
    }
    $req;
}


sub form_data   # RFC1867
{
    my($data, $boundary, $req) = @_;
    my @data = ref($data) eq "HASH" ? %$data : @$data;  # copy
    my $fhparts;
    my @parts;
    my($k,$v);
    while (($k,$v) = splice(@data, 0, 2)) {
	if (!ref($v)) {
	    $k =~ s/([\\\"])/\\$1/g;  # escape quotes and backslashes
	    push(@parts,
		 qq(Content-Disposition: form-data; name="$k"$CRLF$CRLF$v));
	}
	else {
	    my($file, $usename, @headers) = @$v;
	    unless (defined $usename) {
		$usename = $file;
		$usename =~ s,.*/,, if defined($usename);
	    }
            $k =~ s/([\\\"])/\\$1/g;
	    my $disp = qq(form-data; name="$k");
            if (defined($usename) and length($usename)) {
                $usename =~ s/([\\\"])/\\$1/g;
                $disp .= qq(; filename="$usename");
            }
	    my $content = "";
	    my $h = HTTP::Headers->new(@headers);
	    if ($file) {
		require Symbol;
		my $fh = Symbol::gensym();
		open($fh, $file) or Carp::croak("Can't open file $file: $!");
		binmode($fh);
		if ($DYNAMIC_FILE_UPLOAD) {
		    # will read file later
		    $content = $fh;
		}
		else {
		    local($/) = undef; # slurp files
		    $content = <$fh>;
		    close($fh);
		}
		unless ($h->header("Content-Type")) {
		    require LWP::MediaTypes;
		    LWP::MediaTypes::guess_media_type($file, $h);
		}
	    }
	    if ($h->header("Content-Disposition")) {
		# just to get it sorted first
		$disp = $h->header("Content-Disposition");
		$h->remove_header("Content-Disposition");
	    }
	    if ($h->header("Content")) {
		$content = $h->header("Content");
		$h->remove_header("Content");
	    }
	    my $head = join($CRLF, "Content-Disposition: $disp",
			           $h->as_string($CRLF),
			           "");
	    if (ref $content) {
		push(@parts, [$head, $content]);
		$fhparts++;
	    }
	    else {
		push(@parts, $head . $content);
	    }
	}
    }
    return ("", "none") unless @parts;

    my $content;
    if ($fhparts) {
	$boundary = boundary(10) # hopefully enough randomness
	    unless $boundary;

	# add the boundaries to the @parts array
	for (1..@parts-1) {
	    splice(@parts, $_*2-1, 0, "$CRLF--$boundary$CRLF");
	}
	unshift(@parts, "--$boundary$CRLF");
	push(@parts, "$CRLF--$boundary--$CRLF");

	# See if we can generate Content-Length header
	my $length = 0;
	for (@parts) {
	    if (ref $_) {
	 	my ($head, $f) = @$_;
		my $file_size;
		unless ( -f $f && ($file_size = -s _) ) {
		    # The file is either a dynamic file like /dev/audio
		    # or perhaps a file in the /proc file system where
		    # stat may return a 0 size even though reading it
		    # will produce data.  So we cannot make
		    # a Content-Length header.  
		    undef $length;
		    last;
		}
	    	$length += $file_size + length $head;
	    }
	    else {
		$length += length;
	    }
        }
        $length && $req->header('Content-Length' => $length);

	# set up a closure that will return content piecemeal
	$content = sub {
	    for (;;) {
		unless (@parts) {
		    defined $length && $length != 0 &&
		    	Carp::croak "length of data sent did not match calculated Content-Length header.  Probably because uploaded file changed in size during transfer.";
		    return;
		}
		my $p = shift @parts;
		unless (ref $p) {
		    $p .= shift @parts while @parts && !ref($parts[0]);
		    defined $length && ($length -= length $p);
		    return $p;
		}
		my($buf, $fh) = @$p;
		my $buflength = length $buf;
		my $n = read($fh, $buf, 2048, $buflength);
		if ($n) {
		    $buflength += $n;
		    unshift(@parts, ["", $fh]);
		}
		else {
		    close($fh);
		}
		if ($buflength) {
		    defined $length && ($length -= $buflength);
		    return $buf 
	    	}
	    }
	};

    }
    else {
	$boundary = boundary() unless $boundary;

	my $bno = 0;
      CHECK_BOUNDARY:
	{
	    for (@parts) {
		if (index($_, $boundary) >= 0) {
		    # must have a better boundary
		    $boundary = boundary(++$bno);
		    redo CHECK_BOUNDARY;
		}
	    }
	    last;
	}
	$content = "--$boundary$CRLF" .
	           join("$CRLF--$boundary$CRLF", @parts) .
		   "$CRLF--$boundary--$CRLF";
    }

    wantarray ? ($content, $boundary) : $content;
}


sub boundary
{
    my $size = shift || return "xYzZY";
    require MIME::Base64;
    my $b = MIME::Base64::encode(join("", map chr(rand(256)), 1..$size*3), "");
    $b =~ s/[\W]/X/g;  # ensure alnum only
    $b;
}

1;

__END__

=head1 NAME

HTTP::Request::Common - Construct common HTTP::Request objects

=head1 SYNOPSIS

  use HTTP::Request::Common;
  $ua = LWP::UserAgent->new;
  $ua->request(GET 'http://www.sn.no/');
  $ua->request(POST 'http://somewhere/foo', [foo => bar, bar => foo]);

=head1 DESCRIPTION

This module provide functions that return newly created C<HTTP::Request>
objects.  These functions are usually more convenient to use than the
standard C<HTTP::Request> constructor for the most common requests.  The
following functions are provided:

=over 4

=item GET $url

=item GET $url, Header => Value,...

The GET() function returns an C<HTTP::Request> object initialized with
the "GET" method and the specified URL.  It is roughly equivalent to the
following call

  HTTP::Request->new(
     GET => $url,
     HTTP::Headers->new(Header => Value,...),
  )

but is less cluttered.  What is different is that a header named
C<Content> will initialize the content part of the request instead of
setting a header field.  Note that GET requests should normally not
have a content, so this hack makes more sense for the PUT() and POST()
functions described below.

The get(...) method of C<LWP::UserAgent> exists as a shortcut for
$ua->request(GET ...).

=item HEAD $url

=item HEAD $url, Header => Value,...

Like GET() but the method in the request is "HEAD".

The head(...)  method of "LWP::UserAgent" exists as a shortcut for
$ua->request(HEAD ...).

=item PUT $url

=item PUT $url, Header => Value,...

=item PUT $url, Header => Value,..., Content => $content

Like GET() but the method in the request is "PUT".

The content of the request can be specified using the "Content"
pseudo-header.  This steals a bit of the header field namespace as
there is no way to directly specify a header that is actually called
"Content".  If you really need this you must update the request
returned in a separate statement.

=item POST $url

=item POST $url, Header => Value,...

=item POST $url, $form_ref, Header => Value,...

=item POST $url, Header => Value,..., Content => $form_ref

=item POST $url, Header => Value,..., Content => $content

This works mostly like PUT() with "POST" as the method, but this
function also takes a second optional array or hash reference
parameter $form_ref.  As for PUT() the content can also be specified
directly using the "Content" pseudo-header, and you may also provide
the $form_ref this way.

The $form_ref argument can be used to pass key/value pairs for the
form content.  By default we will initialize a request using the
C<application/x-www-form-urlencoded> content type.  This means that
you can emulate a HTML E<lt>form> POSTing like this:

  POST 'http://www.perl.org/survey.cgi',
       [ name   => 'Gisle Aas',
         email  => 'gisle@aas.no',
         gender => 'M',
         born   => '1964',
         perc   => '3%',
       ];

This will create a HTTP::Request object that looks like this:

  POST http://www.perl.org/survey.cgi
  Content-Length: 66
  Content-Type: application/x-www-form-urlencoded

  name=Gisle%20Aas&email=gisle%40aas.no&gender=M&born=1964&perc=3%25

Multivalued form fields can be specified by either repeating the field
name or by passing the value as an array reference.

The POST method also supports the C<multipart/form-data> content used
for I<Form-based File Upload> as specified in RFC 1867.  You trigger
this content format by specifying a content type of C<'form-data'> as
one of the request headers.  If one of the values in the $form_ref is
an array reference, then it is treated as a file part specification
with the following interpretation:

  [ $file, $filename, Header => Value... ]
  [ undef, $filename, Header => Value,..., Content => $content ]

The first value in the array ($file) is the name of a file to open.
This file will be read and its content placed in the request.  The
routine will croak if the file can't be opened.  Use an C<undef> as
$file value if you want to specify the content directly with a
C<Content> header.  The $filename is the filename to report in the
request.  If this value is undefined, then the basename of the $file
will be used.  You can specify an empty string as $filename if you
want to suppress sending the filename when you provide a $file value.

If a $file is provided by no C<Content-Type> header, then C<Content-Type>
and C<Content-Encoding> will be filled in automatically with the values
returned by LWP::MediaTypes::guess_media_type()

Sending my F<~/.profile> to the survey used as example above can be
achieved by this:

  POST 'http://www.perl.org/survey.cgi',
       Content_Type => 'form-data',
       Content      => [ name  => 'Gisle Aas',
                         email => 'gisle@aas.no',
                         gender => 'M',
                         born   => '1964',
                         init   => ["$ENV{HOME}/.profile"],
                       ]

This will create a HTTP::Request object that almost looks this (the
boundary and the content of your F<~/.profile> is likely to be
different):

  POST http://www.perl.org/survey.cgi
  Content-Length: 388
  Content-Type: multipart/form-data; boundary="6G+f"

  --6G+f
  Content-Disposition: form-data; name="name"

  Gisle Aas
  --6G+f
  Content-Disposition: form-data; name="email"

  gisle@aas.no
  --6G+f
  Content-Disposition: form-data; name="gender"

  M
  --6G+f
  Content-Disposition: form-data; name="born"

  1964
  --6G+f
  Content-Disposition: form-data; name="init"; filename=".profile"
  Content-Type: text/plain

  PATH=/local/perl/bin:$PATH
  export PATH

  --6G+f--

If you set the $DYNAMIC_FILE_UPLOAD variable (exportable) to some TRUE
value, then you get back a request object with a subroutine closure as
the content attribute.  This subroutine will read the content of any
files on demand and return it in suitable chunks.  This allow you to
upload arbitrary big files without using lots of memory.  You can even
upload infinite files like F</dev/audio> if you wish; however, if
the file is not a plain file, there will be no Content-Length header
defined for the request.  Not all servers (or server
applications) like this.  Also, if the file(s) change in size between
the time the Content-Length is calculated and the time that the last
chunk is delivered, the subroutine will C<Croak>.

The post(...)  method of "LWP::UserAgent" exists as a shortcut for
$ua->request(POST ...).

=back

=head1 SEE ALSO

L<HTTP::Request>, L<LWP::UserAgent>


=head1 COPYRIGHT

Copyright 1997-2004, Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

