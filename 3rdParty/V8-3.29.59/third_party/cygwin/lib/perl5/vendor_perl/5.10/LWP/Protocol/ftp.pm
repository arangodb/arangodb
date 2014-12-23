package LWP::Protocol::ftp;

# Implementation of the ftp protocol (RFC 959). We let the Net::FTP
# package do all the dirty work.

use Carp ();

use HTTP::Status ();
use HTTP::Negotiate ();
use HTTP::Response ();
use LWP::MediaTypes ();
use File::Listing ();

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

use strict;
eval {
    package LWP::Protocol::MyFTP;

    require Net::FTP;
    Net::FTP->require_version(2.00);

    use vars qw(@ISA);
    @ISA=qw(Net::FTP);

    sub new {
	my $class = shift;
	LWP::Debug::trace('()');

	my $self = $class->SUPER::new(@_) || return undef;

	my $mess = $self->message;  # welcome message
	LWP::Debug::debug($mess);
	$mess =~ s|\n.*||s; # only first line left
	$mess =~ s|\s*ready\.?$||;
	# Make the version number more HTTP like
	$mess =~ s|\s*\(Version\s*|/| and $mess =~ s|\)$||;
	${*$self}{myftp_server} = $mess;
	#$response->header("Server", $mess);

	$self;
    }

    sub http_server {
	my $self = shift;
	${*$self}{myftp_server};
    }

    sub home {
	my $self = shift;
	my $old = ${*$self}{myftp_home};
	if (@_) {
	    ${*$self}{myftp_home} = shift;
	}
	$old;
    }

    sub go_home {
	LWP::Debug::trace('');
	my $self = shift;
	$self->cwd(${*$self}{myftp_home});
    }

    sub request_count {
	my $self = shift;
	++${*$self}{myftp_reqcount};
    }

    sub ping {
	LWP::Debug::trace('');
	my $self = shift;
	return $self->go_home;
    }

};
my $init_failed = $@;


sub _connect {
    my($self, $host, $port, $user, $account, $password, $timeout) = @_;

    my $key;
    my $conn_cache = $self->{ua}{conn_cache};
    if ($conn_cache) {
	$key = "$host:$port:$user";
	$key .= ":$account" if defined($account);
	if (my $ftp = $conn_cache->withdraw("ftp", $key)) {
	    if ($ftp->ping) {
		LWP::Debug::debug('Reusing old connection');
		# save it again
		$conn_cache->deposit("ftp", $key, $ftp);
		return $ftp;
	    }
	}
    }

    # try to make a connection
    my $ftp = LWP::Protocol::MyFTP->new($host,
					Port => $port,
					Timeout => $timeout,
				       );
    # XXX Should be some what to pass on 'Passive' (header??)
    unless ($ftp) {
	$@ =~ s/^Net::FTP: //;
	return HTTP::Response->new(&HTTP::Status::RC_INTERNAL_SERVER_ERROR, $@);
    }

    LWP::Debug::debug("Logging in as $user (password $password)...");
    unless ($ftp->login($user, $password, $account)) {
	# Unauthorized.  Let's fake a RC_UNAUTHORIZED response
	my $mess = scalar($ftp->message);
	LWP::Debug::debug($mess);
	$mess =~ s/\n$//;
	my $res =  HTTP::Response->new(&HTTP::Status::RC_UNAUTHORIZED, $mess);
	$res->header("Server", $ftp->http_server);
	$res->header("WWW-Authenticate", qq(Basic Realm="FTP login"));
	return $res;
    }
    LWP::Debug::debug($ftp->message);

    my $home = $ftp->pwd;
    LWP::Debug::debug("home: '$home'");
    $ftp->home($home);

    $conn_cache->deposit("ftp", $key, $ftp) if $conn_cache;

    return $ftp;
}


sub request
{
    my($self, $request, $proxy, $arg, $size, $timeout) = @_;

    $size = 4096 unless $size;

    LWP::Debug::trace('()');

    # check proxy
    if (defined $proxy)
    {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'You can not proxy through the ftp');
    }

    my $url = $request->url;
    if ($url->scheme ne 'ftp') {
	my $scheme = $url->scheme;
	return HTTP::Response->new(&HTTP::Status::RC_INTERNAL_SERVER_ERROR,
		       "LWP::Protocol::ftp::request called for '$scheme'");
    }

    # check method
    my $method = $request->method;

    unless ($method eq 'GET' || $method eq 'HEAD' || $method eq 'PUT') {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'Library does not allow method ' .
				   "$method for 'ftp:' URLs");
    }

    if ($init_failed) {
	return HTTP::Response->new(&HTTP::Status::RC_INTERNAL_SERVER_ERROR,
				   $init_failed);
    }

    my $host     = $url->host;
    my $port     = $url->port;
    my $user     = $url->user;
    my $password = $url->password;

    # If a basic autorization header is present than we prefer these over
    # the username/password specified in the URL.
    {
	my($u,$p) = $request->authorization_basic;
	if (defined $u) {
	    $user = $u;
	    $password = $p;
	}
    }

    # We allow the account to be specified in the "Account" header
    my $account = $request->header('Account');

    my $ftp = $self->_connect($host, $port, $user, $account, $password, $timeout);
    return $ftp if ref($ftp) eq "HTTP::Response"; # ugh!

    # Create an initial response object
    my $response = HTTP::Response->new(&HTTP::Status::RC_OK, "OK");
    $response->header(Server => $ftp->http_server);
    $response->header('Client-Request-Num' => $ftp->request_count);
    $response->request($request);

    # Get & fix the path
    my @path =  grep { length } $url->path_segments;
    my $remote_file = pop(@path);
    $remote_file = '' unless defined $remote_file;

    my $type;
    if (ref $remote_file) {
	my @params;
	($remote_file, @params) = @$remote_file;
	for (@params) {
	    $type = $_ if s/^type=//;
	}
    }

    if ($type && $type eq 'a') {
	$ftp->ascii;
    }
    else {
	$ftp->binary;
    }

    for (@path) {
	LWP::Debug::debug("CWD $_");
	unless ($ftp->cwd($_)) {
	    return HTTP::Response->new(&HTTP::Status::RC_NOT_FOUND,
				       "Can't chdir to $_");
	}
    }

    if ($method eq 'GET' || $method eq 'HEAD') {
	LWP::Debug::debug("MDTM");
	if (my $mod_time = $ftp->mdtm($remote_file)) {
	    $response->last_modified($mod_time);
	    if (my $ims = $request->if_modified_since) {
		if ($mod_time <= $ims) {
		    $response->code(&HTTP::Status::RC_NOT_MODIFIED);
		    $response->message("Not modified");
		    return $response;
		}
	    }
	}

	# We'll use this later to abort the transfer if necessary. 
	# if $max_size is defined, we need to abort early. Otherwise, it's
      # a normal transfer
	my $max_size = undef;

	# Set resume location, if the client requested it
	if ($request->header('Range') && $ftp->supported('REST'))
	{
		my $range_info = $request->header('Range');

		# Change bytes=2772992-6781209 to just 2772992
		my ($start_byte,$end_byte) = $range_info =~ /.*=\s*(\d+)-(\d+)?/;
		if ( defined $start_byte && !defined $end_byte ) {

		  # open range -- only the start is specified

		  $ftp->restart( $start_byte );
		  # don't define $max_size, we don't want to abort early
		}
		elsif ( defined $start_byte && defined $end_byte &&
			$start_byte >= 0 && $end_byte >= $start_byte ) {

		  $ftp->restart( $start_byte );
		  $max_size = $end_byte - $start_byte;
		}
		else {

		  return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
		     'Incorrect syntax for Range request');
		}
	}
	elsif ($request->header('Range') && !$ftp->supported('REST'))
	{
		return HTTP::Response->new(&HTTP::Status::RC_NOT_IMPLEMENTED,
	         "Server does not support resume.");
	}

	my $data;  # the data handle
	LWP::Debug::debug("retrieve file?");
	if (length($remote_file) and $data = $ftp->retr($remote_file)) {
	    my($type, @enc) = LWP::MediaTypes::guess_media_type($remote_file);
	    $response->header('Content-Type',   $type) if $type;
	    for (@enc) {
		$response->push_header('Content-Encoding', $_);
	    }
	    my $mess = $ftp->message;
	    LWP::Debug::debug($mess);
	    if ($mess =~ /\((\d+)\s+bytes\)/) {
		$response->header('Content-Length', "$1");
	    }

	    if ($method ne 'HEAD') {
		# Read data from server
		$response = $self->collect($arg, $response, sub {
		    my $content = '';
		    my $result = $data->read($content, $size);

                    # Stop early if we need to.
                    if (defined $max_size)
                    {
                      # We need an interface to Net::FTP::dataconn for getting
                      # the number of bytes already read
                      my $bytes_received = $data->bytes_read();

                      # We were already over the limit. (Should only happen
                      # once at the end.)
                      if ($bytes_received - length($content) > $max_size)
                      {
                        $content = '';
                      }
                      # We just went over the limit
                      elsif ($bytes_received  > $max_size)
                      {
                        # Trim content
                        $content = substr($content, 0,
                          $max_size - ($bytes_received - length($content)) );
                      }
                      # We're under the limit
                      else
                      {
                      }
                    }

		    return \$content;
		} );
	    }
	    # abort is needed for HEAD, it's == close if the transfer has
	    # already completed.
	    unless ($data->abort) {
		# Something did not work too well.  Note that we treat
		# responses to abort() with code 0 in case of HEAD as ok
		# (at least wu-ftpd 2.6.1(1) does that).
		if ($method ne 'HEAD' || $ftp->code != 0) {
		    $response->code(&HTTP::Status::RC_INTERNAL_SERVER_ERROR);
		    $response->message("FTP close response: " . $ftp->code .
				       " " . $ftp->message);
		}
	    }
	}
	elsif (!length($remote_file) || ( $ftp->code >= 400 && $ftp->code < 600 )) {
	    # not a plain file, try to list instead
	    if (length($remote_file) && !$ftp->cwd($remote_file)) {
		LWP::Debug::debug("chdir before listing failed");
		return HTTP::Response->new(&HTTP::Status::RC_NOT_FOUND,
					   "File '$remote_file' not found");
	    }

	    # It should now be safe to try to list the directory
	    LWP::Debug::debug("dir");
	    my @lsl = $ftp->dir;

	    # Try to figure out if the user want us to convert the
	    # directory listing to HTML.
	    my @variants =
	      (
	       ['html',  0.60, 'text/html'            ],
	       ['dir',   1.00, 'text/ftp-dir-listing' ]
	      );
	    #$HTTP::Negotiate::DEBUG=1;
	    my $prefer = HTTP::Negotiate::choose(\@variants, $request);

	    my $content = '';

	    if (!defined($prefer)) {
		return HTTP::Response->new(&HTTP::Status::RC_NOT_ACCEPTABLE,
			       "Neither HTML nor directory listing wanted");
	    }
	    elsif ($prefer eq 'html') {
		$response->header('Content-Type' => 'text/html');
		$content = "<HEAD><TITLE>File Listing</TITLE>\n";
		my $base = $request->url->clone;
		my $path = $base->path;
		$base->path("$path/") unless $path =~ m|/$|;
		$content .= qq(<BASE HREF="$base">\n</HEAD>\n);
		$content .= "<BODY>\n<UL>\n";
		for (File::Listing::parse_dir(\@lsl, 'GMT')) {
		    my($name, $type, $size, $mtime, $mode) = @$_;
		    $content .= qq(  <LI> <a href="$name">$name</a>);
		    $content .= " $size bytes" if $type eq 'f';
		    $content .= "\n";
		}
		$content .= "</UL></body>\n";
	    }
	    else {
		$response->header('Content-Type', 'text/ftp-dir-listing');
		$content = join("\n", @lsl, '');
	    }

	    $response->header('Content-Length', length($content));

	    if ($method ne 'HEAD') {
		$response = $self->collect_once($arg, $response, $content);
	    }
	}
	else {
	    my $res = HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
			  "FTP return code " . $ftp->code);
	    $res->content_type("text/plain");
	    $res->content($ftp->message);
	    return $res;
	}
    }
    elsif ($method eq 'PUT') {
	# method must be PUT
	unless (length($remote_file)) {
	    return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				       "Must have a file name to PUT to");
	}
	my $data;
	if ($data = $ftp->stor($remote_file)) {
	    LWP::Debug::debug($ftp->message);
	    LWP::Debug::debug("$data");
	    my $content = $request->content;
	    my $bytes = 0;
	    if (defined $content) {
		if (ref($content) eq 'SCALAR') {
		    $bytes = $data->write($$content, length($$content));
		}
		elsif (ref($content) eq 'CODE') {
		    my($buf, $n);
		    while (length($buf = &$content)) {
			$n = $data->write($buf, length($buf));
			last unless $n;
			$bytes += $n;
		    }
		}
		elsif (!ref($content)) {
		    if (defined $content && length($content)) {
			$bytes = $data->write($content, length($content));
		    }
		}
		else {
		    die "Bad content";
		}
	    }
	    $data->close;
	    LWP::Debug::debug($ftp->message);

	    $response->code(&HTTP::Status::RC_CREATED);
	    $response->header('Content-Type', 'text/plain');
	    $response->content("$bytes bytes stored as $remote_file on $host\n")

	}
	else {
	    my $res = HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
					  "FTP return code " . $ftp->code);
	    $res->content_type("text/plain");
	    $res->content($ftp->message);
	    return $res;
	}
    }
    else {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   "Illegal method $method");
    }

    $response;
}

1;

__END__

# This is what RFC 1738 has to say about FTP access:
# --------------------------------------------------
#
# 3.2. FTP
#
#    The FTP URL scheme is used to designate files and directories on
#    Internet hosts accessible using the FTP protocol (RFC959).
#
#    A FTP URL follow the syntax described in Section 3.1.  If :<port> is
#    omitted, the port defaults to 21.
#
# 3.2.1. FTP Name and Password
#
#    A user name and password may be supplied; they are used in the ftp
#    "USER" and "PASS" commands after first making the connection to the
#    FTP server.  If no user name or password is supplied and one is
#    requested by the FTP server, the conventions for "anonymous" FTP are
#    to be used, as follows:
#
#         The user name "anonymous" is supplied.
#
#         The password is supplied as the Internet e-mail address
#         of the end user accessing the resource.
#
#    If the URL supplies a user name but no password, and the remote
#    server requests a password, the program interpreting the FTP URL
#    should request one from the user.
#
# 3.2.2. FTP url-path
#
#    The url-path of a FTP URL has the following syntax:
#
#         <cwd1>/<cwd2>/.../<cwdN>/<name>;type=<typecode>
#
#    Where <cwd1> through <cwdN> and <name> are (possibly encoded) strings
#    and <typecode> is one of the characters "a", "i", or "d".  The part
#    ";type=<typecode>" may be omitted. The <cwdx> and <name> parts may be
#    empty. The whole url-path may be omitted, including the "/"
#    delimiting it from the prefix containing user, password, host, and
#    port.
#
#    The url-path is interpreted as a series of FTP commands as follows:
#
#       Each of the <cwd> elements is to be supplied, sequentially, as the
#       argument to a CWD (change working directory) command.
#
#       If the typecode is "d", perform a NLST (name list) command with
#       <name> as the argument, and interpret the results as a file
#       directory listing.
#
#       Otherwise, perform a TYPE command with <typecode> as the argument,
#       and then access the file whose name is <name> (for example, using
#       the RETR command.)
#
#    Within a name or CWD component, the characters "/" and ";" are
#    reserved and must be encoded. The components are decoded prior to
#    their use in the FTP protocol.  In particular, if the appropriate FTP
#    sequence to access a particular file requires supplying a string
#    containing a "/" as an argument to a CWD or RETR command, it is
#    necessary to encode each "/".
#
#    For example, the URL <URL:ftp://myname@host.dom/%2Fetc/motd> is
#    interpreted by FTP-ing to "host.dom", logging in as "myname"
#    (prompting for a password if it is asked for), and then executing
#    "CWD /etc" and then "RETR motd". This has a different meaning from
#    <URL:ftp://myname@host.dom/etc/motd> which would "CWD etc" and then
#    "RETR motd"; the initial "CWD" might be executed relative to the
#    default directory for "myname". On the other hand,
#    <URL:ftp://myname@host.dom//etc/motd>, would "CWD " with a null
#    argument, then "CWD etc", and then "RETR motd".
#
#    FTP URLs may also be used for other operations; for example, it is
#    possible to update a file on a remote file server, or infer
#    information about it from the directory listings. The mechanism for
#    doing so is not spelled out here.
#
# 3.2.3. FTP Typecode is Optional
#
#    The entire ;type=<typecode> part of a FTP URL is optional. If it is
#    omitted, the client program interpreting the URL must guess the
#    appropriate mode to use. In general, the data content type of a file
#    can only be guessed from the name, e.g., from the suffix of the name;
#    the appropriate type code to be used for transfer of the file can
#    then be deduced from the data content of the file.
#
# 3.2.4 Hierarchy
#
#    For some file systems, the "/" used to denote the hierarchical
#    structure of the URL corresponds to the delimiter used to construct a
#    file name hierarchy, and thus, the filename will look similar to the
#    URL path. This does NOT mean that the URL is a Unix filename.
#
# 3.2.5. Optimization
#
#    Clients accessing resources via FTP may employ additional heuristics
#    to optimize the interaction. For some FTP servers, for example, it
#    may be reasonable to keep the control connection open while accessing
#    multiple URLs from the same server. However, there is no common
#    hierarchical model to the FTP protocol, so if a directory change
#    command has been given, it is impossible in general to deduce what
#    sequence should be given to navigate to another directory for a
#    second retrieval, if the paths are different.  The only reliable
#    algorithm is to disconnect and reestablish the control connection.
