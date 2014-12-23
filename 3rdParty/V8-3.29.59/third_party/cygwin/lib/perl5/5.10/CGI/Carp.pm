package CGI::Carp;

=head1 NAME

B<CGI::Carp> - CGI routines for writing to the HTTPD (or other) error log

=head1 SYNOPSIS

    use CGI::Carp;

    croak "We're outta here!";
    confess "It was my fault: $!";
    carp "It was your fault!";   
    warn "I'm confused";
    die  "I'm dying.\n";

    use CGI::Carp qw(cluck);
    cluck "I wouldn't do that if I were you";

    use CGI::Carp qw(fatalsToBrowser);
    die "Fatal error messages are now sent to browser";

=head1 DESCRIPTION

CGI scripts have a nasty habit of leaving warning messages in the error
logs that are neither time stamped nor fully identified.  Tracking down
the script that caused the error is a pain.  This fixes that.  Replace
the usual

    use Carp;

with

    use CGI::Carp

And the standard warn(), die (), croak(), confess() and carp() calls
will automagically be replaced with functions that write out nicely
time-stamped messages to the HTTP server error log.

For example:

   [Fri Nov 17 21:40:43 1995] test.pl: I'm confused at test.pl line 3.
   [Fri Nov 17 21:40:43 1995] test.pl: Got an error message: Permission denied.
   [Fri Nov 17 21:40:43 1995] test.pl: I'm dying.

=head1 REDIRECTING ERROR MESSAGES

By default, error messages are sent to STDERR.  Most HTTPD servers
direct STDERR to the server's error log.  Some applications may wish
to keep private error logs, distinct from the server's error log, or
they may wish to direct error messages to STDOUT so that the browser
will receive them.

The C<carpout()> function is provided for this purpose.  Since
carpout() is not exported by default, you must import it explicitly by
saying

   use CGI::Carp qw(carpout);

The carpout() function requires one argument, which should be a
reference to an open filehandle for writing errors.  It should be
called in a C<BEGIN> block at the top of the CGI application so that
compiler errors will be caught.  Example:

   BEGIN {
     use CGI::Carp qw(carpout);
     open(LOG, ">>/usr/local/cgi-logs/mycgi-log") or
       die("Unable to open mycgi-log: $!\n");
     carpout(LOG);
   }

carpout() does not handle file locking on the log for you at this point.

The real STDERR is not closed -- it is moved to CGI::Carp::SAVEERR.  Some
servers, when dealing with CGI scripts, close their connection to the
browser when the script closes STDOUT and STDERR.  CGI::Carp::SAVEERR is there to
prevent this from happening prematurely.

You can pass filehandles to carpout() in a variety of ways.  The "correct"
way according to Tom Christiansen is to pass a reference to a filehandle 
GLOB:

    carpout(\*LOG);

This looks weird to mere mortals however, so the following syntaxes are
accepted as well:

    carpout(LOG);
    carpout(main::LOG);
    carpout(main'LOG);
    carpout(\LOG);
    carpout(\'main::LOG');

    ... and so on

FileHandle and other objects work as well.

Use of carpout() is not great for performance, so it is recommended
for debugging purposes or for moderate-use applications.  A future
version of this module may delay redirecting STDERR until one of the
CGI::Carp methods is called to prevent the performance hit.

=head1 MAKING PERL ERRORS APPEAR IN THE BROWSER WINDOW

If you want to send fatal (die, confess) errors to the browser, ask to
import the special "fatalsToBrowser" subroutine:

    use CGI::Carp qw(fatalsToBrowser);
    die "Bad error here";

Fatal errors will now be echoed to the browser as well as to the log.  CGI::Carp
arranges to send a minimal HTTP header to the browser so that even errors that
occur in the early compile phase will be seen.
Nonfatal errors will still be directed to the log file only (unless redirected
with carpout).

Note that fatalsToBrowser does B<not> work with mod_perl version 2.0
and higher.

=head2 Changing the default message

By default, the software error message is followed by a note to
contact the Webmaster by e-mail with the time and date of the error.
If this message is not to your liking, you can change it using the
set_message() routine.  This is not imported by default; you should
import it on the use() line:

    use CGI::Carp qw(fatalsToBrowser set_message);
    set_message("It's not a bug, it's a feature!");

You may also pass in a code reference in order to create a custom
error message.  At run time, your code will be called with the text
of the error message that caused the script to die.  Example:

    use CGI::Carp qw(fatalsToBrowser set_message);
    BEGIN {
       sub handle_errors {
          my $msg = shift;
          print "<h1>Oh gosh</h1>";
          print "<p>Got an error: $msg</p>";
      }
      set_message(\&handle_errors);
    }

In order to correctly intercept compile-time errors, you should call
set_message() from within a BEGIN{} block.

=head1 DOING MORE THAN PRINTING A MESSAGE IN THE EVENT OF PERL ERRORS

If fatalsToBrowser in conjunction with set_message does not provide 
you with all of the functionality you need, you can go one step 
further by specifying a function to be executed any time a script
calls "die", has a syntax error, or dies unexpectedly at runtime
with a line like "undef->explode();". 

    use CGI::Carp qw(set_die_handler);
    BEGIN {
       sub handle_errors {
          my $msg = shift;
          print "content-type: text/html\n\n";
          print "<h1>Oh gosh</h1>";
          print "<p>Got an error: $msg</p>";

          #proceed to send an email to a system administrator,
          #write a detailed message to the browser and/or a log,
          #etc....
      }
      set_die_handler(\&handle_errors);
    }

Notice that if you use set_die_handler(), you must handle sending
HTML headers to the browser yourself if you are printing a message.

If you use set_die_handler(), you will most likely interfere with 
the behavior of fatalsToBrowser, so you must use this or that, not 
both. 

Using set_die_handler() sets SIG{__DIE__} (as does fatalsToBrowser),
and there is only one SIG{__DIE__}. This means that if you are 
attempting to set SIG{__DIE__} yourself, you may interfere with 
this module's functionality, or this module may interfere with 
your module's functionality.

=head1 MAKING WARNINGS APPEAR AS HTML COMMENTS

It is now also possible to make non-fatal errors appear as HTML
comments embedded in the output of your program.  To enable this
feature, export the new "warningsToBrowser" subroutine.  Since sending
warnings to the browser before the HTTP headers have been sent would
cause an error, any warnings are stored in an internal buffer until
you call the warningsToBrowser() subroutine with a true argument:

    use CGI::Carp qw(fatalsToBrowser warningsToBrowser);
    use CGI qw(:standard);
    print header();
    warningsToBrowser(1);

You may also give a false argument to warningsToBrowser() to prevent
warnings from being sent to the browser while you are printing some
content where HTML comments are not allowed:

    warningsToBrowser(0);    # disable warnings
    print "<script type=\"text/javascript\"><!--\n";
    print_some_javascript_code();
    print "//--></script>\n";
    warningsToBrowser(1);    # re-enable warnings

Note: In this respect warningsToBrowser() differs fundamentally from
fatalsToBrowser(), which you should never call yourself!

=head1 OVERRIDING THE NAME OF THE PROGRAM

CGI::Carp includes the name of the program that generated the error or
warning in the messages written to the log and the browser window.
Sometimes, Perl can get confused about what the actual name of the
executed program was.  In these cases, you can override the program
name that CGI::Carp will use for all messages.

The quick way to do that is to tell CGI::Carp the name of the program
in its use statement.  You can do that by adding
"name=cgi_carp_log_name" to your "use" statement.  For example:

    use CGI::Carp qw(name=cgi_carp_log_name);

.  If you want to change the program name partway through the program,
you can use the C<set_progname()> function instead.  It is not
exported by default, you must import it explicitly by saying

    use CGI::Carp qw(set_progname);

Once you've done that, you can change the logged name of the program
at any time by calling

    set_progname(new_program_name);

You can set the program back to the default by calling

    set_progname(undef);

Note that this override doesn't happen until after the program has
compiled, so any compile-time errors will still show up with the
non-overridden program name
  
=head1 CHANGE LOG

1.29 Patch from Peter Whaite to fix the unfixable problem of CGI::Carp
     not behaving correctly in an eval() context.

1.05 carpout() added and minor corrections by Marc Hedlund
     <hedlund@best.com> on 11/26/95.

1.06 fatalsToBrowser() no longer aborts for fatal errors within
     eval() statements.

1.08 set_message() added and carpout() expanded to allow for FileHandle
     objects.

1.09 set_message() now allows users to pass a code REFERENCE for 
     really custom error messages.  croak and carp are now
     exported by default.  Thanks to Gunther Birznieks for the
     patches.

1.10 Patch from Chris Dean (ctdean@cogit.com) to allow 
     module to run correctly under mod_perl.

1.11 Changed order of &gt; and &lt; escapes.

1.12 Changed die() on line 217 to CORE::die to avoid B<-w> warning.

1.13 Added cluck() to make the module orthogonal with Carp.
     More mod_perl related fixes.

1.20 Patch from Ilmari Karonen (perl@itz.pp.sci.fi):  Added
     warningsToBrowser().  Replaced <CODE> tags with <PRE> in
     fatalsToBrowser() output.

1.23 ineval() now checks both $^S and inspects the message for the "eval" pattern
     (hack alert!) in order to accommodate various combinations of Perl and
     mod_perl.

1.24 Patch from Scott Gifford (sgifford@suspectclass.com): Add support
     for overriding program name.

1.26 Replaced CORE::GLOBAL::die with the evil $SIG{__DIE__} because the
     former isn't working in some people's hands.  There is no such thing
     as reliable exception handling in Perl.

1.27 Replaced tell STDOUT with bytes=tell STDOUT.

=head1 AUTHORS

Copyright 1995-2002, Lincoln D. Stein.  All rights reserved.  

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Address bug reports and comments to: lstein@cshl.org

=head1 SEE ALSO

Carp, CGI::Base, CGI::BasePlus, CGI::Request, CGI::MiniSvr, CGI::Form,
CGI::Response
    if (defined($CGI::Carp::PROGNAME)) 
    {
      $file = $CGI::Carp::PROGNAME;
    }

=cut

require 5.000;
use Exporter;
#use Carp;
BEGIN { 
  require Carp; 
  *CORE::GLOBAL::die = \&CGI::Carp::die;
}

use File::Spec;

@ISA = qw(Exporter);
@EXPORT = qw(confess croak carp);
@EXPORT_OK = qw(carpout fatalsToBrowser warningsToBrowser wrap set_message set_die_handler set_progname cluck ^name= die);

$main::SIG{__WARN__}=\&CGI::Carp::warn;

$CGI::Carp::VERSION     = '1.30_01';
$CGI::Carp::CUSTOM_MSG  = undef;
$CGI::Carp::DIE_HANDLER = undef;


# fancy import routine detects and handles 'errorWrap' specially.
sub import {
    my $pkg = shift;
    my(%routines);
    my(@name);
    if (@name=grep(/^name=/,@_))
      {
        my($n) = (split(/=/,$name[0]))[1];
        set_progname($n);
        @_=grep(!/^name=/,@_);
      }

    grep($routines{$_}++,@_,@EXPORT);
    $WRAP++ if $routines{'fatalsToBrowser'} || $routines{'wrap'};
    $WARN++ if $routines{'warningsToBrowser'};
    my($oldlevel) = $Exporter::ExportLevel;
    $Exporter::ExportLevel = 1;
    Exporter::import($pkg,keys %routines);
    $Exporter::ExportLevel = $oldlevel;
    $main::SIG{__DIE__} =\&CGI::Carp::die if $routines{'fatalsToBrowser'};
#    $pkg->export('CORE::GLOBAL','die');
}

# These are the originals
sub realwarn { CORE::warn(@_); }
sub realdie { CORE::die(@_); }

sub id {
    my $level = shift;
    my($pack,$file,$line,$sub) = caller($level);
    my($dev,$dirs,$id) = File::Spec->splitpath($file);
    return ($file,$line,$id);
}

sub stamp {
    my $time = scalar(localtime);
    my $frame = 0;
    my ($id,$pack,$file,$dev,$dirs);
    if (defined($CGI::Carp::PROGNAME)) {
        $id = $CGI::Carp::PROGNAME;
    } else {
        do {
  	  $id = $file;
	  ($pack,$file) = caller($frame++);
        } until !$file;
    }
    ($dev,$dirs,$id) = File::Spec->splitpath($id);
    return "[$time] $id: ";
}

sub set_progname {
    $CGI::Carp::PROGNAME = shift;
    return $CGI::Carp::PROGNAME;
}


sub warn {
    my $message = shift;
    my($file,$line,$id) = id(1);
    $message .= " at $file line $line.\n" unless $message=~/\n$/;
    _warn($message) if $WARN;
    my $stamp = stamp;
    $message=~s/^/$stamp/gm;
    realwarn $message;
}

sub _warn {
    my $msg = shift;
    if ($EMIT_WARNINGS) {
	# We need to mangle the message a bit to make it a valid HTML
	# comment.  This is done by substituting similar-looking ISO
	# 8859-1 characters for <, > and -.  This is a hack.
	$msg =~ tr/<>-/\253\273\255/;
	chomp $msg;
	print STDOUT "<!-- warning: $msg -->\n";
    } else {
	push @WARNINGS, $msg;
    }
}


# The mod_perl package Apache::Registry loads CGI programs by calling
# eval.  These evals don't count when looking at the stack backtrace.
sub _longmess {
    my $message = Carp::longmess();
    $message =~ s,eval[^\n]+(ModPerl|Apache)/(?:Registry|Dispatch)\w*\.pm.*,,s
        if exists $ENV{MOD_PERL};
    return $message;
}

sub ineval {
  (exists $ENV{MOD_PERL} ? 0 : $^S) || _longmess() =~ /eval [\{\']/m
}

sub die {
  my ($arg,@rest) = @_;

  if ($DIE_HANDLER) {
      &$DIE_HANDLER($arg,@rest);
  }

  if ( ineval() )  {
    if (!ref($arg)) {
      $arg = join("",($arg,@rest)) || "Died";
      my($file,$line,$id) = id(1);
      $arg .= " at $file line $line.\n" unless $arg=~/\n$/;
      realdie($arg);
    }
    else {
      realdie($arg,@rest);
    }
  }

  if (!ref($arg)) {
    $arg = join("", ($arg,@rest));
    my($file,$line,$id) = id(1);
    $arg .= " at $file line $line." unless $arg=~/\n$/;
    &fatalsToBrowser($arg) if $WRAP;
    if (($arg =~ /\n$/) || !exists($ENV{MOD_PERL})) {
      my $stamp = stamp;
      $arg=~s/^/$stamp/gm;
    }
    if ($arg !~ /\n$/) {
      $arg .= "\n";
    }
  }
  realdie $arg;
}

sub set_message {
    $CGI::Carp::CUSTOM_MSG = shift;
    return $CGI::Carp::CUSTOM_MSG;
}

sub set_die_handler {

    my ($handler) = shift;
    
    #setting SIG{__DIE__} here is necessary to catch runtime
    #errors which are not called by literally saying "die",
    #such as the line "undef->explode();". however, doing this
    #will interfere with fatalsToBrowser, which also sets 
    #SIG{__DIE__} in the import() function above (or the 
    #import() function above may interfere with this). for
    #this reason, you should choose to either set the die
    #handler here, or use fatalsToBrowser, not both. 
    $main::SIG{__DIE__} = $handler;
    
    $CGI::Carp::DIE_HANDLER = $handler; 
    
    return $CGI::Carp::DIE_HANDLER;
}

sub confess { CGI::Carp::die Carp::longmess @_; }
sub croak   { CGI::Carp::die Carp::shortmess @_; }
sub carp    { CGI::Carp::warn Carp::shortmess @_; }
sub cluck   { CGI::Carp::warn Carp::longmess @_; }

# We have to be ready to accept a filehandle as a reference
# or a string.
sub carpout {
    my($in) = @_;
    my($no) = fileno(to_filehandle($in));
    realdie("Invalid filehandle $in\n") unless defined $no;
    
    open(SAVEERR, ">&STDERR");
    open(STDERR, ">&$no") or 
	( print SAVEERR "Unable to redirect STDERR: $!\n" and exit(1) );
}

sub warningsToBrowser {
    $EMIT_WARNINGS = @_ ? shift : 1;
    _warn(shift @WARNINGS) while $EMIT_WARNINGS and @WARNINGS;
}

# headers
sub fatalsToBrowser {
  my($msg) = @_;
  $msg=~s/&/&amp;/g;
  $msg=~s/>/&gt;/g;
  $msg=~s/</&lt;/g;
  $msg=~s/\"/&quot;/g;
  my($wm) = $ENV{SERVER_ADMIN} ? 
    qq[the webmaster (<a href="mailto:$ENV{SERVER_ADMIN}">$ENV{SERVER_ADMIN}</a>)] :
      "this site's webmaster";
  my ($outer_message) = <<END;
For help, please send mail to $wm, giving this error message 
and the time and date of the error.
END
  ;
  my $mod_perl = exists $ENV{MOD_PERL};

  if ($CUSTOM_MSG) {
    if (ref($CUSTOM_MSG) eq 'CODE') {
      print STDOUT "Content-type: text/html\n\n" 
        unless $mod_perl;
      &$CUSTOM_MSG($msg); # nicer to perl 5.003 users
      return;
    } else {
      $outer_message = $CUSTOM_MSG;
    }
  }

  my $mess = <<END;
<h1>Software error:</h1>
<pre>$msg</pre>
<p>
$outer_message
</p>
END
  ;

  if ($mod_perl) {
    my $r;
    if ($ENV{MOD_PERL_API_VERSION} && $ENV{MOD_PERL_API_VERSION} == 2) {
      $mod_perl = 2;
      require Apache2::RequestRec;
      require Apache2::RequestIO;
      require Apache2::RequestUtil;
      require APR::Pool;
      require ModPerl::Util;
      require Apache2::Response;
      $r = Apache2::RequestUtil->request;
    }
    else {
      $r = Apache->request;
    }
    # If bytes have already been sent, then
    # we print the message out directly.
    # Otherwise we make a custom error
    # handler to produce the doc for us.
    if ($r->bytes_sent) {
      $r->print($mess);
      $mod_perl == 2 ? ModPerl::Util::exit(0) : $r->exit;
    } else {
      # MSIE won't display a custom 500 response unless it is >512 bytes!
      if ($ENV{HTTP_USER_AGENT} =~ /MSIE/) {
        $mess = "<!-- " . (' ' x 513) . " -->\n$mess";
      }
      $r->custom_response(500,$mess);
    }
  } else {
    my $bytes_written = eval{tell STDOUT};
    if (defined $bytes_written && $bytes_written > 0) {
        print STDOUT $mess;
    }
    else {
        print STDOUT "Status: 500\n";
        print STDOUT "Content-type: text/html\n\n";
        print STDOUT $mess;
    }
  }

  warningsToBrowser(1);    # emit warnings before dying
}

# Cut and paste from CGI.pm so that we don't have the overhead of
# always loading the entire CGI module.
sub to_filehandle {
    my $thingy = shift;
    return undef unless $thingy;
    return $thingy if UNIVERSAL::isa($thingy,'GLOB');
    return $thingy if UNIVERSAL::isa($thingy,'FileHandle');
    if (!ref($thingy)) {
	my $caller = 1;
	while (my $package = caller($caller++)) {
	    my($tmp) = $thingy=~/[\':]/ ? $thingy : "$package\:\:$thingy"; 
	    return $tmp if defined(fileno($tmp));
	}
    }
    return undef;
}

1;
