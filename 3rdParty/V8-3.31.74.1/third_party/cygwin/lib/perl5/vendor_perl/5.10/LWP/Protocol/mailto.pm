package LWP::Protocol::mailto;

# This module implements the mailto protocol.  It is just a simple
# frontend to the Unix sendmail program except on MacOS, where it uses
# Mail::Internet.

require LWP::Protocol;
require HTTP::Request;
require HTTP::Response;
require HTTP::Status;

use Carp;
use strict;
use vars qw(@ISA $SENDMAIL);

@ISA = qw(LWP::Protocol);

unless ($SENDMAIL = $ENV{SENDMAIL}) {
    for my $sm (qw(/usr/sbin/sendmail
		   /usr/lib/sendmail
		   /usr/ucblib/sendmail
		  ))
    {
	if (-x $sm) {
	    $SENDMAIL = $sm;
	    last;
	}
    }
    die "Can't find the 'sendmail' program" unless $SENDMAIL;
}

sub request
{
    my($self, $request, $proxy, $arg, $size) = @_;

    my ($mail, $addr) if $^O eq "MacOS";
    my @text = () if $^O eq "MacOS";

    # check proxy
    if (defined $proxy)
    {
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'You can not proxy with mail';
    }

    # check method
    my $method = $request->method;

    if ($method ne 'POST') {
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'Library does not allow method ' .
				  "$method for 'mailto:' URLs";
    }

    # check url
    my $url = $request->url;

    my $scheme = $url->scheme;
    if ($scheme ne 'mailto') {
	return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
			 "LWP::Protocol::mailto::request called for '$scheme'";
    }
    if ($^O eq "MacOS") {
	eval {
	    require Mail::Internet;
	};
	if($@) {
	    return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
	               "You don't have MailTools installed";
	}
	unless ($ENV{SMTPHOSTS}) {
	    return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
	               "You don't have SMTPHOSTS defined";
	}
    }
    else {
	unless (-x $SENDMAIL) {
	    return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
	               "You don't have $SENDMAIL";
    }
    }
    if ($^O eq "MacOS") {
	    $mail = Mail::Internet->new or
	    return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
	    "Can't get a Mail::Internet object";
    }
    else {
	open(SENDMAIL, "| $SENDMAIL -oi -t") or
	    return new HTTP::Response &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
	               "Can't run $SENDMAIL: $!";
    }
    if ($^O eq "MacOS") {
	$addr = $url->encoded822addr;
    }
    else {
	$request = $request->clone;  # we modify a copy
	my @h = $url->headers;  # URL headers override those in the request
	while (@h) {
	    my $k = shift @h;
	    my $v = shift @h;
	    next unless defined $v;
	    if (lc($k) eq "body") {
		$request->content($v);
	    }
	    else {
		$request->push_header($k => $v);
	    }
	}
    }
    if ($^O eq "MacOS") {
	$mail->add(To => $addr);
	$mail->add(split(/[:\n]/,$request->headers_as_string));
    }
    else {
	print SENDMAIL $request->headers_as_string;
	print SENDMAIL "\n";
    }
    my $content = $request->content;
    if (defined $content) {
	my $contRef = ref($content) ? $content : \$content;
	if (ref($contRef) eq 'SCALAR') {
	    if ($^O eq "MacOS") {
		@text = split("\n",$$contRef);
		foreach (@text) {
		    $_ .= "\n";
		}
	    }
	    else {
	    print SENDMAIL $$contRef;
	    }

	}
	elsif (ref($contRef) eq 'CODE') {
	    # Callback provides data
	    my $d;
	    if ($^O eq "MacOS") {
		my $stuff = "";
		while (length($d = &$contRef)) {
		    $stuff .= $d;
		}
		@text = split("\n",$stuff);
		foreach (@text) {
		    $_ .= "\n";
		}
	    }
	    else {
		print SENDMAIL $d;
	    }
	}
    }
    if ($^O eq "MacOS") {
	$mail->body(\@text);
	unless ($mail->smtpsend) {
	    return HTTP::Response->new(&HTTP::Status::RC_INTERNAL_SERVER_ERROR,
				       "Mail::Internet->smtpsend unable to send message to <$addr>");
	}
    }
    else {
	unless (close(SENDMAIL)) {
	    my $err = $! ? "$!" : "Exit status $?";
	    return HTTP::Response->new(&HTTP::Status::RC_INTERNAL_SERVER_ERROR,
				       "$SENDMAIL: $err");
	}
    }


    my $response = HTTP::Response->new(&HTTP::Status::RC_ACCEPTED,
				       "Mail accepted");
    $response->header('Content-Type', 'text/plain');
    if ($^O eq "MacOS") {
	$response->header('Server' => "Mail::Internet $Mail::Internet::VERSION");
	$response->content("Message sent to <$addr>\n");
    }
    else {
	$response->header('Server' => $SENDMAIL);
	my $to = $request->header("To");
	$response->content("Message sent to <$to>\n");
    }

    return $response;
}

1;
