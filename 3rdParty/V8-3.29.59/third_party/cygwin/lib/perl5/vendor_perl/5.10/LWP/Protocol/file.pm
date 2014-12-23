package LWP::Protocol::file;

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

use strict;

require LWP::MediaTypes;
require HTTP::Request;
require HTTP::Response;
require HTTP::Status;
require HTTP::Date;


sub request
{
    my($self, $request, $proxy, $arg, $size) = @_;

    LWP::Debug::trace('()');

    $size = 4096 unless defined $size and $size > 0;

    # check proxy
    if (defined $proxy)
    {
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'You can not proxy through the filesystem';
    }

    # check method
    my $method = $request->method;
    unless ($method eq 'GET' || $method eq 'HEAD') {
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'Library does not allow method ' .
				  "$method for 'file:' URLs";
    }

    # check url
    my $url = $request->url;

    my $scheme = $url->scheme;
    if ($scheme ne 'file') {
	return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
			   "LWP::Protocol::file::request called for '$scheme'";
    }

    # URL OK, look at file
    my $path  = $url->file;

    # test file exists and is readable
    unless (-e $path) {
	return new HTTP::Response &HTTP::Status::RC_NOT_FOUND,
				  "File `$path' does not exist";
    }
    unless (-r _) {
	return new HTTP::Response &HTTP::Status::RC_FORBIDDEN,
				  'User does not have read permission';
    }

    # looks like file exists
    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$filesize,
       $atime,$mtime,$ctime,$blksize,$blocks)
	    = stat(_);

    # XXX should check Accept headers?

    # check if-modified-since
    my $ims = $request->header('If-Modified-Since');
    if (defined $ims) {
	my $time = HTTP::Date::str2time($ims);
	if (defined $time and $time >= $mtime) {
	    return new HTTP::Response &HTTP::Status::RC_NOT_MODIFIED,
				      "$method $path";
	}
    }

    # Ok, should be an OK response by now...
    my $response = new HTTP::Response &HTTP::Status::RC_OK;

    # fill in response headers
    $response->header('Last-Modified', HTTP::Date::time2str($mtime));

    if (-d _) {         # If the path is a directory, process it
	# generate the HTML for directory
	opendir(D, $path) or
	   return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
				     "Cannot read directory '$path': $!";
	my(@files) = sort readdir(D);
	closedir(D);

	# Make directory listing
	require URI::Escape;
	require HTML::Entities;
        my $pathe = $path . ( $^O eq 'MacOS' ? ':' : '/');
	for (@files) {
	    my $furl = URI::Escape::uri_escape($_);
            if ( -d "$pathe$_" ) {
                $furl .= '/';
                $_ .= '/';
            }
	    my $desc = HTML::Entities::encode($_);
	    $_ = qq{<LI><A HREF="$furl">$desc</A>};
	}
	# Ensure that the base URL is "/" terminated
	my $base = $url->clone;
	unless ($base->path =~ m|/$|) {
	    $base->path($base->path . "/");
	}
	my $html = join("\n",
			"<HTML>\n<HEAD>",
			"<TITLE>Directory $path</TITLE>",
			"<BASE HREF=\"$base\">",
			"</HEAD>\n<BODY>",
			"<H1>Directory listing of $path</H1>",
			"<UL>", @files, "</UL>",
			"</BODY>\n</HTML>\n");

	$response->header('Content-Type',   'text/html');
	$response->header('Content-Length', length $html);
	$html = "" if $method eq "HEAD";

	return $self->collect_once($arg, $response, $html);

    }

    # path is a regular file
    $response->header('Content-Length', $filesize);
    LWP::MediaTypes::guess_media_type($path, $response);

    # read the file
    if ($method ne "HEAD") {
	open(F, $path) or return new
	    HTTP::Response(&HTTP::Status::RC_INTERNAL_SERVER_ERROR,
			   "Cannot read file '$path': $!");
	binmode(F);
	$response =  $self->collect($arg, $response, sub {
	    my $content = "";
	    my $bytes = sysread(F, $content, $size);
	    return \$content if $bytes > 0;
	    return \ "";
	});
	close(F);
    }

    $response;
}

1;
