package CGI::Fast;

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

# Copyright 1995,1996, Lincoln D. Stein.  All rights reserved.
# It may be used and modified freely, but I do request that this copyright
# notice remain attached to the file.  You may modify this module as you 
# wish, but if you redistribute a modified version, please attach a note
# listing the modifications you have made.

$CGI::Fast::VERSION='1.07';

use CGI;
use FCGI;
@ISA = ('CGI');

# workaround for known bug in libfcgi
while (($ignore) = each %ENV) { }

# override the initialization behavior so that
# state is NOT maintained between invocations 
sub save_request {
    # no-op
}

# If ENV{FCGI_SOCKET_PATH} is specified, we maintain a FCGI Request handle
# in this package variable.
use vars qw($Ext_Request);
BEGIN {
   # If ENV{FCGI_SOCKET_PATH} is given, explicitly open the socket,
   # and keep the request handle around from which to call Accept().
   if ($ENV{FCGI_SOCKET_PATH}) {
	my $path    = $ENV{FCGI_SOCKET_PATH};
	my $backlog = $ENV{FCGI_LISTEN_QUEUE} || 100;
	my $socket  = FCGI::OpenSocket( $path, $backlog );
	$Ext_Request = FCGI::Request( \*STDIN, \*STDOUT, \*STDERR, 
					\%ENV, $socket, 1 );
   }
}

# New is slightly different in that it calls FCGI's
# accept() method.
sub new {
     my ($self, $initializer, @param) = @_;
     unless (defined $initializer) {
	if ($Ext_Request) {
          return undef unless $Ext_Request->Accept() >= 0;
	} else {
         return undef unless FCGI::accept() >= 0;
     }
     }
     CGI->_reset_globals;
     $self->_setup_symbols(@SAVED_SYMBOLS) if @CGI::SAVED_SYMBOLS;
     return $CGI::Q = $self->SUPER::new($initializer, @param);
}

1;

=head1 NAME

CGI::Fast - CGI Interface for Fast CGI

=head1 SYNOPSIS

    use CGI::Fast qw(:standard);
    $COUNTER = 0;
    while (new CGI::Fast) {
	print header;
	print start_html("Fast CGI Rocks");
	print
	    h1("Fast CGI Rocks"),
	    "Invocation number ",b($COUNTER++),
            " PID ",b($$),".",
	    hr;
        print end_html;
    }

=head1 DESCRIPTION

CGI::Fast is a subclass of the CGI object created by
CGI.pm.  It is specialized to work well with the Open Market
FastCGI standard, which greatly speeds up CGI scripts by
turning them into persistently running server processes.  Scripts
that perform time-consuming initialization processes, such as
loading large modules or opening persistent database connections,
will see large performance improvements.

=head1 OTHER PIECES OF THE PUZZLE

In order to use CGI::Fast you'll need a FastCGI-enabled Web
server. See http://www.fastcgi.com/ for details.

=head1 WRITING FASTCGI PERL SCRIPTS

FastCGI scripts are persistent: one or more copies of the script 
are started up when the server initializes, and stay around until
the server exits or they die a natural death.  After performing
whatever one-time initialization it needs, the script enters a 
loop waiting for incoming connections, processing the request, and
waiting some more.

A typical FastCGI script will look like this:

    #!/usr/local/bin/perl    # must be a FastCGI version of perl!
    use CGI::Fast;
    &do_some_initialization();
    while ($q = new CGI::Fast) {
	&process_request($q);
    }

Each time there's a new request, CGI::Fast returns a
CGI object to your loop.  The rest of the time your script
waits in the call to new().  When the server requests that
your script be terminated, new() will return undef.  You can
of course exit earlier if you choose.  A new version of the
script will be respawned to take its place (this may be
necessary in order to avoid Perl memory leaks in long-running
scripts).

CGI.pm's default CGI object mode also works.  Just modify the loop
this way:

    while (new CGI::Fast) {
	&process_request;
    }

Calls to header(), start_form(), etc. will all operate on the
current request.

=head1 INSTALLING FASTCGI SCRIPTS

See the FastCGI developer's kit documentation for full details.  On
the Apache server, the following line must be added to srm.conf:

    AddType application/x-httpd-fcgi .fcgi

FastCGI scripts must end in the extension .fcgi.  For each script you
install, you must add something like the following to srm.conf:

    FastCgiServer /usr/etc/httpd/fcgi-bin/file_upload.fcgi -processes 2

This instructs Apache to launch two copies of file_upload.fcgi at 
startup time.

=head1 USING FASTCGI SCRIPTS AS CGI SCRIPTS

Any script that works correctly as a FastCGI script will also work
correctly when installed as a vanilla CGI script.  However it will
not see any performance benefit.

=head1 EXTERNAL FASTCGI SERVER INVOCATION

FastCGI supports a TCP/IP transport mechanism which allows FastCGI scripts to run
external to the webserver, perhaps on a remote machine.  To configure the
webserver to connect to an external FastCGI server, you would add the following
to your srm.conf:

    FastCgiExternalServer /usr/etc/httpd/fcgi-bin/file_upload.fcgi -host sputnik:8888

Two environment variables affect how the C<CGI::Fast> object is created,
allowing C<CGI::Fast> to be used as an external FastCGI server.  (See C<FCGI>
documentation for C<FCGI::OpenSocket> for more information.)

=over

=item FCGI_SOCKET_PATH

The address (TCP/IP) or path (UNIX Domain) of the socket the external FastCGI
script to which bind an listen for incoming connections from the web server.

=item FCGI_LISTEN_QUEUE

Maximum length of the queue of pending connections.  

=back

For example:

    #!/usr/local/bin/perl    # must be a FastCGI version of perl!
    use CGI::Fast;
    &do_some_initialization();
    $ENV{FCGI_SOCKET_PATH} = "sputnik:8888";
    $ENV{FCGI_LISTEN_QUEUE} = 100;
    while ($q = new CGI::Fast) {
	&process_request($q);
    }

=head1 CAVEATS

I haven't tested this very much.

=head1 AUTHOR INFORMATION

Copyright 1996-1998, Lincoln D. Stein.  All rights reserved.  

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Address bug reports and comments to: lstein@cshl.org

=head1 BUGS

This section intentionally left blank.

=head1 SEE ALSO

L<CGI::Carp>, L<CGI>

=cut
