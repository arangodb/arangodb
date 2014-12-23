#
#  $Id: ReadKey.pm,v 2.23 2005/01/11 21:16:31 jonathan Exp $
#

=head1 NAME

Term::ReadKey - A perl module for simple terminal control

=head1 SYNOPSIS

	use Term::ReadKey;
	ReadMode 4; # Turn off controls keys
	while (not defined ($key = ReadKey(-1))) {
		# No key yet
	}
	print "Get key $key\n";
	ReadMode 0; # Reset tty mode before exiting

=head1 DESCRIPTION

Term::ReadKey is a compiled perl module dedicated to providing simple
control over terminal driver modes (cbreak, raw, cooked, etc.,) support for
non-blocking reads, if the architecture allows, and some generalized handy
functions for working with terminals. One of the main goals is to have the
functions as portable as possible, so you can just plug in "use
Term::ReadKey" on any architecture and have a good likelyhood of it working.

=over 8

=item ReadMode MODE [, Filehandle]

Takes an integer argument, which can currently be one of the following 
values:

    0    Restore original settings.
    1    Change to cooked mode.
    2	 Change to cooked mode with echo off. 
          (Good for passwords)
    3    Change to cbreak mode.
    4    Change to raw mode.
    5    Change to ultra-raw mode. 
          (LF to CR/LF translation turned off) 
          
    Or, you may use the synonyms:
    
    restore
    normal
    noecho
    cbreak
    raw
    ultra-raw

These functions are automatically applied to the STDIN handle if no
other handle is supplied. Modes 0 and 5 have some special properties
worth mentioning: not only will mode 0 restore original settings, but it
cause the next ReadMode call to save a new set of default settings. Mode
5 is similar to mode 4, except no CR/LF translation is performed, and if
possible, parity will be disabled (only if not being used by the terminal,
however. It is no different from mode 4 under Windows.)

If you are executing another program that may be changing the terminal mode,
you will either want to say

    ReadMode 1
    system('someprogram');
    ReadMode 1;
    
which resets the settings after the program has run, or:

    $somemode=1;
    ReadMode 0;
    system('someprogram');
    ReadMode 1;
    
which records any changes the program may have made, before resetting the
mode.

=item ReadKey MODE [, Filehandle]

Takes an integer argument, which can currently be one of the following 
values:

    0    Perform a normal read using getc
    -1   Perform a non-blocked read
    >0	 Perform a timed read

(If the filehandle is not supplied, it will default to STDIN.) If there is
nothing waiting in the buffer during a non-blocked read, then undef will be
returned. Note that if the OS does not provide any known mechanism for
non-blocking reads, then a C<ReadKey -1> can die with a fatal error. This
will hopefully not be common.

If MODE is greater then zero, then ReadKey will use it as a timeout value in
seconds (fractional seconds are allowed), and won't return C<undef> until
that time expires. (Note, again, that some OS's may not support this timeout
behaviour.) If MODE is less then zero, then this is treated as a timeout
of zero, and thus will return immediately if no character is waiting. A MODE
of zero, however, will act like a normal getc.

There are currently some limitations with this call under Windows. It may be
possible that non-blocking reads will fail when reading repeating keys from
more then one console.

=item ReadLine MODE [, Filehandle]

Takes an integer argument, which can currently be one of the following 
values:

    0    Perform a normal read using scalar(<FileHandle>)
    -1   Perform a non-blocked read
    >0	 Perform a timed read

If there is nothing waiting in the buffer during a non-blocked read, then
undef will be returned. Note that if the OS does not provide any known
mechanism for non-blocking reads, then a C<ReadLine 1> can die with a fatal
error. This will hopefully not be common. Note that a non-blocking test is
only performed for the first character in the line, not the entire line.
This call will probably B<not> do what you assume, especially with
ReadMode's higher then 1. For example, pressing Space and then Backspace
would appear to leave you where you started, but any timeouts would now
be suspended.

This call is currently not available under Windows.

=item GetTerminalSize [Filehandle]

Returns either an empty array if this operation is unsupported, or a four
element array containing: the width of the terminal in characters, the
height of the terminal in character, the width in pixels, and the height in
pixels. (The pixel size will only be valid in some environments.)

Under Windows, this function must be called with an "output" filehandle,
such as STDOUT, or a handle opened to CONOUT$.

=item SetTerminalSize WIDTH,HEIGHT,XPIX,YPIX [, Filehandle]

Return -1 on failure, 0 otherwise. Note that this terminal size is only for
B<informative> value, and changing the size via this mechanism will B<not>
change the size of the screen. For example, XTerm uses a call like this when
it resizes the screen. If any of the new measurements vary from the old, the
OS will probably send a SIGWINCH signal to anything reading that tty or pty.

This call does not work under Windows.

=item GetSpeeds [, Filehandle]

Returns either an empty array if the operation is unsupported, or a two
value array containing the terminal in and out speeds, in B<decimal>. E.g,
an in speed of 9600 baud and an out speed of 4800 baud would be returned as
(9600,4800). Note that currently the in and out speeds will always be
identical in some OS's. No speeds are reported under Windows.

=item GetControlChars [, Filehandle]

Returns an array containing key/value pairs suitable for a hash. The pairs
consist of a key, the name of the control character/signal, and the value
of that character, as a single character. This call does nothing under Windows.

Each key will be an entry from the following list:

	DISCARD
	DSUSPEND
	EOF
	EOL
	EOL2
	ERASE
	ERASEWORD
	INTERRUPT
	KILL
	MIN
	QUIT
	QUOTENEXT
	REPRINT
	START
	STATUS
	STOP
	SUSPEND
	SWITCH
	TIME

Thus, the following will always return the current interrupt character,
regardless of platform.

	%keys = GetControlChars;
	$int = $keys{INTERRUPT};

=item SetControlChars [, Filehandle]

Takes an array containing key/value pairs, as a hash will produce. The pairs
should consist of a key that is the name of a legal control
character/signal, and the value should be either a single character, or a
number in the range 0-255. SetControlChars will die with a runtime error if
an invalid character name is passed or there is an error changing the
settings. The list of valid names is easily available via

	%cchars = GetControlChars();
	@cnames = keys %cchars;

This call does nothing under Windows.

=back

=head1 AUTHOR

Kenneth Albanowski <kjahds@kjahds.com>

Currently maintained by Jonathan Stowe <jns@gellyfish.com>

=cut

package Term::ReadKey;

$VERSION = '2.30';

require Exporter;
require AutoLoader;
require DynaLoader;
use Carp;

@ISA = qw(Exporter AutoLoader DynaLoader);

# Items to export into callers namespace by default
# (move infrequently used names to @EXPORT_OK below)

@EXPORT = qw(
  ReadKey
  ReadMode
  ReadLine
  GetTerminalSize
  SetTerminalSize
  GetSpeed
  GetControlChars
  SetControlChars
);

@EXPORT_OK = qw();

bootstrap Term::ReadKey;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

# Should we use LINES and COLUMNS to try and get the terminal size?
# Change this to zero if you have systems where these are commonly
# set to erroneous values. (But if either are nero zero, they won't be
# used anyhow.)

$UseEnv = 1;

%modes = (
    original    => 0,
    restore     => 0,
    normal      => 1,
    noecho      => 2,
    cbreak      => 3,
    raw         => 4,
    'ultra-raw' => 5
);

sub ReadMode
{
    my ($mode) = $modes{ $_[0] };
    my ($fh) = normalizehandle( ( @_ > 1 ? $_[1] : \*STDIN ) );
    if ( defined($mode) ) { SetReadMode( $mode, $fh ) }
    elsif ( $_[0] =~ /^\d/ ) { SetReadMode( $_[0], $fh ) }
    else { croak("Unknown terminal mode `$_[0]'"); }
}

sub normalizehandle
{
    my ($file) = @_;

    #	print "Handle = $file\n";
    if ( ref($file) ) { return $file; }    # Reference is fine

    #	if($file =~ /^\*/) { return $file; } # Type glob is good
    if ( ref( \$file ) eq 'GLOB' ) { return $file; }    # Glob is good

    #	print "Caller = ",(caller(1))[0],"\n";
    return \*{ ( ( caller(1) )[0] ) . "::$file" };
}

sub GetTerminalSize
{
    my ($file) = normalizehandle( ( @_ > 1 ? $_[1] : \*STDOUT ) );
    my (@results) = ();
    my (@fail);

    if ( &termsizeoptions() & 1 )                       # VIO
    {
        @results = GetTermSizeVIO($file);
        push( @fail, "VIOGetMode call" );
    }
    elsif ( &termsizeoptions() & 2 )                    # GWINSZ
    {
        @results = GetTermSizeGWINSZ($file);
        push( @fail, "TIOCGWINSZ ioctl" );
    }
    elsif ( &termsizeoptions() & 4 )                    # GSIZE
    {
        @results = GetTermSizeGSIZE($file);
        push( @fail, "TIOCGSIZE ioctl" );
    }
    elsif ( &termsizeoptions() & 8 )                    # WIN32
    {
        @results = GetTermSizeWin32($file);
        push( @fail, "Win32 GetConsoleScreenBufferInfo call" );
    }
    else
    {
        @results = ();
    }

    if ( @results < 4 and $UseEnv )
    {
        my ($C) = defined( $ENV{COLUMNS} ) ? $ENV{COLUMNS} : 0;
        my ($L) = defined( $ENV{LINES} )   ? $ENV{LINES}   : 0;
        if ( ( $C >= 2 ) and ( $L >= 2 ) )
        {
            @results = ( $C + 0, $L + 0, 0, 0 );
        }
        push( @fail, "COLUMNS and LINES environment variables" );
    }

    if ( @results < 4 )
    {
        my ($prog) = "resize";

        # Workaround for Solaris path sillyness
        if ( -f "/usr/openwin/bin/resize" ) {
            $prog = "/usr/openwin/bin/resize";
        }

        my ($resize) = scalar(`$prog 2>/dev/null`);
        if (
            defined $resize
            and (  $resize =~ /COLUMNS\s*=\s*(\d+)/
                or $resize =~ /setenv\s+COLUMNS\s+'?(\d+)/ )
          )
        {
            $results[0] = $1;
            if (   $resize =~ /LINES\s*=\s*(\d+)/
                or $resize =~ /setenv\s+LINES\s+'?(\d+)/ )
            {
                $results[1] = $1;
                @results[ 2, 3 ] = ( 0, 0 );
            }
            else
            {
                @results = ();
            }
        }
        else
        {
            @results = ();
        }
        push( @fail, "resize program" );
    }

    if ( @results < 4 )
    {
        die "Unable to get Terminal Size."
          . join( "", map( " The $_ didn't work.", @fail ) );
    }

    @results;
}

if ( &blockoptions() & 1 )    # Use nodelay
{
    if ( &blockoptions() & 2 )    #poll
    {
        eval <<'DONE';
		sub ReadKey {
		  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
                  if (defined $_[0] && $_[0] > 0) {
                    if ($_[0]) {
                      return undef if &pollfile($File,$_[0]) == 0;
                    }
		  }
                  if (defined $_[0] && $_[0] < 0) {
                     &setnodelay($File,1);
                  }
                  my ($value) = getc $File;
                  if (defined $_[0] && $_[0] < 0) {
                     &setnodelay($File,0);
                  }
                  $value;
		}
		sub ReadLine {
		  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));

                  if (defined $_[0] && $_[0] > 0) {
                     if ($_[0]) {
                       return undef if &pollfile($File,$_[0]) == 0;
                     }
		  }
                  if (defined $_[0] && $_[0] < 0) {
                     &setnodelay($File,1)
                  };
                  my ($value) = scalar(<$File>);
                  if ( defined $_[0] && $_[0]<0 ) {
                    &setnodelay($File,0)
                  };
                  $value;
		}
DONE
    }
    elsif ( &blockoptions() & 4 )    #select
    {
        eval <<'DONE';
		sub ReadKey {
		  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
                  if(defined $_[0] && $_[0]>0) {
				if($_[0]) {return undef if &selectfile($File,$_[0])==0}
		    }
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,1);}
			my($value) = getc $File;
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,0);}
			$value;
		}
		sub ReadLine {
		  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		    if(defined $_[0] && $_[0]>0) {
				if($_[0]) {return undef if &selectfile($File,$_[0])==0}
		    }
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,1)};
			my($value)=scalar(<$File>);
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,0)};
			$value;
		}
DONE
    }
    else
    {    #nothing
        eval <<'DONE';
		sub ReadKey {
		  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		    if(defined $_[0] && $_[0]>0) {
		    	# Nothing better seems to exist, so I just use time-of-day
		    	# to timeout the read. This isn't very exact, though.
		    	$starttime=time;
		    	$endtime=$starttime+$_[0];
				&setnodelay($File,1);
				my($value)=undef;
		    	while(time<$endtime) { # This won't catch wraparound!
		    		$value = getc $File;
		    		last if defined($value);
		    	}
				&setnodelay($File,0);
				return $value;
		    }
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,1);}
			my($value) = getc $File;
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,0);}
			$value;
		}
		sub ReadLine {
		  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		    if(defined $_[0] && $_[0]>0) {
		    	# Nothing better seems to exist, so I just use time-of-day
		    	# to timeout the read. This isn't very exact, though.
		    	$starttime=time;
		    	$endtime=$starttime+$_[0];
				&setnodelay($File,1);
				my($value)=undef;
		    	while(time<$endtime) { # This won't catch wraparound!
		    		$value = scalar(<$File>);
		    		last if defined($value);
		    	}
				&setnodelay($File,0);
				return $value;
		    }
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,1)};
			my($value)=scalar(<$File>);
			if(defined $_[0] && $_[0]<0) {&setnodelay($File,0)};
			$value;
		}
DONE
    }
}
elsif ( &blockoptions() & 2 )    # Use poll
{
    eval <<'DONE';
	sub ReadKey {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if(defined $_[0] && $_[0] != 0) {
                     return undef if &pollfile($File,$_[0]) == 0
                }
		getc $File;
	}
	sub ReadLine {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if(defined $_[0] && $_[0]!=0) {
                     return undef if &pollfile($File,$_[0]) == 0;
                }
		scalar(<$File>);
	}
DONE
}
elsif ( &blockoptions() & 4 )    # Use select
{
    eval <<'DONE';
	sub ReadKey {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if(defined $_[0] && $_[0] !=0 ) {
                     return undef if &selectfile($File,$_[0])==0
                }
		getc $File;
	}
	sub ReadLine {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if(defined $_[0] && $_[0] != 0) {
                     return undef if &selectfile($File,$_[0]) == 0;
                }
		scalar(<$File>);
	}
DONE
}
elsif ( &blockoptions() & 8 )    # Use Win32
{
    eval <<'DONE';
	sub ReadKey {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if ($_[0]) {
			Win32PeekChar($File, $_[0]);
		} else {
			getc $File;
		}
		#if ($_[0]!=0) {return undef if !Win32PeekChar($File, $_[0])};
		#getc $File;
	}
	sub ReadLine {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		#if ($_[0]!=0) {return undef if !Win32PeekChar($File, $_[0])};
		#scalar(<$File>);
		if($_[0]) 
			{croak("Non-blocking ReadLine is not supported on this architecture")}
		scalar(<$File>);
	}
DONE
}
else
{
    eval <<'DONE';
	sub ReadKey {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if($_[0]) 
			{croak("Non-blocking ReadKey is not supported on this architecture")}
		getc $File;
	}
	sub ReadLine {
	  my($File) = normalizehandle((@_>1?$_[1]:\*STDIN));
		if($_[0]) 
			{croak("Non-blocking ReadLine is not supported on this architecture")}
		scalar(<$File>);
	}
DONE
}

package Term::ReadKey;    # return to package ReadKey so AutoSplit is happy
1;

__END__;
