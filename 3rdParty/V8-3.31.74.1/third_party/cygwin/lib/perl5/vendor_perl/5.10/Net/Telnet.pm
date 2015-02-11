package Net::Telnet;

## Copyright 1997, 2000, 2002 Jay Rogers.  All rights reserved.
## This program is free software; you can redistribute it and/or
## modify it under the same terms as Perl itself.

## See user documentation at the end of this file.  Search for =head

use strict;
require 5.002;

## Module export.
use vars qw(@EXPORT_OK);
@EXPORT_OK = qw(TELNET_IAC TELNET_DONT TELNET_DO TELNET_WONT TELNET_WILL
		TELNET_SB TELNET_GA TELNET_EL TELNET_EC TELNET_AYT TELNET_AO
		TELNET_IP TELNET_BREAK TELNET_DM TELNET_NOP TELNET_SE
		TELNET_EOR TELNET_ABORT TELNET_SUSP TELNET_EOF TELNET_SYNCH
		TELOPT_BINARY TELOPT_ECHO TELOPT_RCP TELOPT_SGA TELOPT_NAMS
		TELOPT_STATUS TELOPT_TM TELOPT_RCTE TELOPT_NAOL TELOPT_NAOP
		TELOPT_NAOCRD TELOPT_NAOHTS TELOPT_NAOHTD TELOPT_NAOFFD
		TELOPT_NAOVTS TELOPT_NAOVTD TELOPT_NAOLFD TELOPT_XASCII
		TELOPT_LOGOUT TELOPT_BM TELOPT_DET TELOPT_SUPDUP
		TELOPT_SUPDUPOUTPUT TELOPT_SNDLOC TELOPT_TTYPE TELOPT_EOR
		TELOPT_TUID TELOPT_OUTMRK TELOPT_TTYLOC TELOPT_3270REGIME
		TELOPT_X3PAD TELOPT_NAWS TELOPT_TSPEED TELOPT_LFLOW
		TELOPT_LINEMODE TELOPT_XDISPLOC TELOPT_OLD_ENVIRON
		TELOPT_AUTHENTICATION TELOPT_ENCRYPT TELOPT_NEW_ENVIRON
		TELOPT_EXOPL);

## Module import.
use Exporter ();
use Socket qw(AF_INET SOCK_STREAM inet_aton sockaddr_in);
use Symbol qw(qualify);

## Base classes.
use vars qw(@ISA);
@ISA = qw(Exporter);
if (&_io_socket_include) {  # successfully required module IO::Socket
    push @ISA, "IO::Socket::INET";
}
else {  # perl version < 5.004
    require FileHandle;
    push @ISA, "FileHandle";
}

## Global variables.
use vars qw($VERSION @Telopts);
$VERSION = "3.03";
@Telopts = ("BINARY", "ECHO", "RCP", "SUPPRESS GO AHEAD", "NAME", "STATUS",
	    "TIMING MARK", "RCTE", "NAOL", "NAOP", "NAOCRD", "NAOHTS",
	    "NAOHTD", "NAOFFD", "NAOVTS", "NAOVTD", "NAOLFD", "EXTEND ASCII",
	    "LOGOUT", "BYTE MACRO", "DATA ENTRY TERMINAL", "SUPDUP",
	    "SUPDUP OUTPUT", "SEND LOCATION", "TERMINAL TYPE", "END OF RECORD",
	    "TACACS UID", "OUTPUT MARKING", "TTYLOC", "3270 REGIME", "X.3 PAD",
	    "NAWS", "TSPEED", "LFLOW", "LINEMODE", "XDISPLOC", "OLD-ENVIRON",
	    "AUTHENTICATION", "ENCRYPT", "NEW-ENVIRON");


########################### Public Methods ###########################


sub new {
    my ($class) = @_;
    my (
	$errmode,
	$fh_open,
	$host,
	$self,
	%args,
	);
    local $_;

    ## Create a new object with defaults.
    $self = $class->SUPER::new;
    *$self->{net_telnet} = {
	bin_mode     	 => 0,
	blksize      	 => &_optimal_blksize(),
	buf          	 => "",
	cmd_prompt   	 => '/[\$%#>] $/',
	cmd_rm_mode  	 => "auto",
	dumplog      	 => '',
	eofile       	 => 1,
	errormode    	 => "die",
	errormsg     	 => "",
	fdmask       	 => '',
	host         	 => "localhost",
	inputlog     	 => '',
	last_line    	 => "",
	last_prompt    	 => "",
	maxbufsize   	 => 1_048_576,
	num_wrote    	 => 0,
	ofs          	 => "",
	opened       	 => '',
	opt_cback    	 => '',
	opt_log      	 => '',
	opts         	 => {},
	ors          	 => "\n",
	outputlog    	 => '',
	pending_errormsg => "",
	port         	 => 23,
	pushback_buf 	 => "",
	rs           	 => "\n",
	subopt_cback 	 => '',
	telnet_mode  	 => 1,
	time_out     	 => 10,
	timedout     	 => '',
	unsent_opts  	 => "",
    };

    ## Indicate that we'll accept an offer from remote side for it to echo
    ## and suppress go aheads.
    &_opt_accept($self,
		 { option    => &TELOPT_ECHO,
		   is_remote => 1,
		   is_enable => 1 },
		 { option    => &TELOPT_SGA,
		   is_remote => 1,
		   is_enable => 1 },
		 );

    ## Parse the args.
    if (@_ == 2) {  # one positional arg given
	$host = $_[1];
    }
    elsif (@_ > 2) {  # named args given
	## Get the named args.
	(undef, %args) = @_;

	## Parse all other named args.
	foreach (keys %args) {
	    if (/^-?binmode$/i) {
		$self->binmode($args{$_});
	    }
	    elsif (/^-?cmd_remove_mode$/i) {
		$self->cmd_remove_mode($args{$_});
	    }
	    elsif (/^-?dump_log$/i) {
		$self->dump_log($args{$_});
	    }
	    elsif (/^-?errmode$/i) {
		$errmode = $args{$_};
	    }
	    elsif (/^-?fhopen$/i) {
		$fh_open = $args{$_};
	    }
	    elsif (/^-?host$/i) {
		$host = $args{$_};
	    }
	    elsif (/^-?input_log$/i) {
		$self->input_log($args{$_});
	    }
	    elsif (/^-?input_record_separator$/i or /^-?rs$/i) {
		$self->input_record_separator($args{$_});
	    }
	    elsif (/^-?option_log$/i) {
		$self->option_log($args{$_});
	    }
	    elsif (/^-?output_log$/i) {
		$self->output_log($args{$_});
	    }
	    elsif (/^-?output_record_separator$/i or /^-?ors$/i) {
		$self->output_record_separator($args{$_});
	    }
	    elsif (/^-?port$/i) {
		$self->port($args{$_});
	    }
	    elsif (/^-?prompt$/i) {
		$self->prompt($args{$_});
	    }
	    elsif (/^-?telnetmode$/i) {
		$self->telnetmode($args{$_});
	    }
	    elsif (/^-?timeout$/i) {
		$self->timeout($args{$_});
	    }
	    else {
		&_croak($self, "bad named parameter \"$_\" given " .
			"to " . ref($self) . "::new()");
	    }
	}
    }

    if (defined $errmode) {  # user wants to set errmode
	$self->errmode($errmode);
    }

    if (defined $fh_open) {  # user wants us to attach to existing filehandle
	$self->fhopen($fh_open)
	    or return;
    }
    elsif (defined $host) {  # user wants us to open a connection to host
	$self->host($host);
	$self->open
	    or return;
    }

    $self;
} # end sub new


sub DESTROY {
} # end sub DESTROY


sub binmode {
    my ($self, $mode) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{bin_mode};

    if (@_ >= 2) {
	unless (defined $mode) {
	    $mode = 0;
	}

	$s->{bin_mode} = $mode;
    }

    $prev;
} # end sub binmode


sub break {
    my ($self) = @_;
    my $s = *$self->{net_telnet};
    my $break_cmd = "\xff\xf3";

    $s->{timedout} = '';

    &_put($self, \$break_cmd, "break");
} # end sub break


sub buffer {
    my ($self) = @_;
    my $s = *$self->{net_telnet};

    \$s->{buf};
} # end sub buffer


sub buffer_empty {
    my ($self) = @_;
    my (
	$buffer,
	);

    $buffer = $self->buffer;
    $$buffer = "";
} # end sub buffer_empty


sub close {
    my ($self) = @_;
    my $s = *$self->{net_telnet};

    $s->{eofile} = 1;
    $s->{opened} = '';
    close $self
	if defined fileno($self);

    1;
} # end sub close


sub cmd {
    my ($self, @args) = @_;
    my (
	$cmd_remove_mode,
	$errmode,
	$firstpos,
	$last_prompt,
	$lastpos,
	$lines,
	$ors,
	$output,
	$output_ref,
	$prompt,
	$remove_echo,
	$rs,
	$rs_len,
	$s,
	$telopt_echo,
	$timeout,
	%args,
	);
    my $cmd = "";
    local $_;

    ## Init.
    $self->timed_out('');
    $self->last_prompt("");
    $s = *$self->{net_telnet};
    $output = [];
    $cmd_remove_mode = $self->cmd_remove_mode;
    $errmode = $self->errmode;
    $ors = $self->output_record_separator;
    $prompt = $self->prompt;
    $rs = $self->input_record_separator;
    $timeout = $self->timeout;

    ## Parse args.
    if (@_ == 2) {  # one positional arg given
	$cmd = $_[1];
    }
    elsif (@_ > 2) {  # named args given
	## Get the named args.
	(undef, %args) = @_;

	## Parse the named args.
	foreach (keys %args) {
	    if (/^-?cmd_remove/i) {
		$cmd_remove_mode = &_parse_cmd_remove_mode($self, $args{$_});
	    }
	    elsif (/^-?errmode$/i) {
		$errmode = &_parse_errmode($self, $args{$_});
	    }
	    elsif (/^-?input_record_separator$/i or /^-?rs$/i) {
		$rs = &_parse_input_record_separator($self, $args{$_});
	    }
	    elsif (/^-?output$/i) {
		$output_ref = $args{$_};
		if (defined($output_ref) and ref($output_ref) eq "ARRAY") {
		    $output = $output_ref;
		}
	    }
	    elsif (/^-?output_record_separator$/i or /^-?ors$/i) {
		$ors = $self->output_record_separator($args{$_});
	    }
	    elsif (/^-?prompt$/i) {
		$prompt = &_parse_prompt($self, $args{$_});
	    }
	    elsif (/^-?string$/i) {
		$cmd = $args{$_};
	    }
	    elsif (/^-?timeout$/i) {
		$timeout = &_parse_timeout($self, $args{$_});
	    }
	    else {
		&_croak($self, "bad named parameter \"$_\" given " .
			"to " . ref($self) . "::cmd()");
	    }
	}
    }

    ## Override some user settings.
    local $s->{errormode} = "return";
    local $s->{time_out} = &_endtime($timeout);
    $self->errmsg("");

    ## Send command and wait for the prompt.
    $self->put($cmd . $ors)
	and ($lines, $last_prompt) = $self->waitfor($prompt);

    ## Check for failure.
    $s->{errormode} = $errmode;
    return $self->error("command timed-out") if $self->timed_out;
    return $self->error($self->errmsg) if $self->errmsg ne "";

    ## Save the most recently matched prompt.
    $self->last_prompt($last_prompt);

    ## Split lines into an array, keeping record separator at end of line.
    $firstpos = 0;
    $rs_len = length $rs;
    while (($lastpos = index($lines, $rs, $firstpos)) > -1) {
	push(@$output,
	     substr($lines, $firstpos, $lastpos - $firstpos + $rs_len));
	$firstpos = $lastpos + $rs_len;
    }

    if ($firstpos < length $lines) {
	push @$output, substr($lines, $firstpos);
    }

    ## Determine if we should remove the first line of output based
    ## on the assumption that it's an echoed back command.
    if ($cmd_remove_mode eq "auto") {
	## See if remote side told us they'd echo.
	$telopt_echo = $self->option_state(&TELOPT_ECHO);
	$remove_echo = $telopt_echo->{remote_enabled};
    }
    else {  # user explicitly told us how many lines to remove.
	$remove_echo = $cmd_remove_mode;
    }

    ## Get rid of possible echo back command.
    while ($remove_echo--) {
	shift @$output;
    }

    ## Ensure at least a null string when there's no command output - so
    ## "true" is returned in a list context.
    unless (@$output) {
	@$output = ("");
    }

    ## Return command output via named arg, if requested.
    if (defined $output_ref) {
	if (ref($output_ref) eq "SCALAR") {
	    $$output_ref = join "", @$output;
	}
	elsif (ref($output_ref) eq "HASH") {
	    %$output_ref = @$output;
	}
    }

    wantarray ? @$output : 1;
} # end sub cmd


sub cmd_remove_mode {
    my ($self, $mode) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{cmd_rm_mode};

    if (@_ >= 2) {
	$s->{cmd_rm_mode} = &_parse_cmd_remove_mode($self, $mode);
    }

    $prev;
} # end sub cmd_remove_mode


sub dump_log {
    my ($self, $name) = @_;
    my (
	$fh,
	$s,
	);

    $s = *$self->{net_telnet};
    $fh = $s->{dumplog};

    if (@_ >= 2) {
	unless (defined $name) {
	    $name = "";
	}

	$fh = &_fname_to_handle($self, $name)
	    or return;
	$s->{dumplog} = $fh;
    }

    $fh;
} # end sub dump_log


sub eof {
    my ($self) = @_;

    *$self->{net_telnet}{eofile};
} # end sub eof


sub errmode {
    my ($self, $mode) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{errormode};

    if (@_ >= 2) {
	$s->{errormode} = &_parse_errmode($self, $mode);
    }

    $prev;
} # end sub errmode


sub errmsg {
    my ($self, @errmsgs) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{errormsg};

    if (@_ >= 2) {
	$s->{errormsg} = join "", @errmsgs;
    }

    $prev;
} # end sub errmsg


sub error {
    my ($self, @errmsg) = @_;
    my (
	$errmsg,
	$func,
	$mode,
	$s,
	@args,
	);
    local $_;

    $s = *$self->{net_telnet};

    if (@_ >= 2) {
	## Put error message in the object.
	$errmsg = join "", @errmsg;
	$s->{errormsg} = $errmsg;

	## Do the error action as described by error mode.
	$mode = $s->{errormode};
	if (ref($mode) eq "CODE") {
	    &$mode($errmsg);
	    return;
	}
	elsif (ref($mode) eq "ARRAY") {
	    ($func, @args) = @$mode;
	    &$func(@args);
	    return;
	}
	elsif ($mode =~ /^return$/i) {
	    return;
	}
	else {  # die
	    if ($errmsg =~ /\n$/) {
		die $errmsg;
	    }
	    else {
		## Die and append caller's line number to message.
		&_croak($self, $errmsg);
	    }
	}
    }
    else {
	return $s->{errormsg} ne "";
    }
} # end sub error


sub fhopen {
    my ($self, $fh) = @_;
    my (
	$globref,
	$s,
	);

    ## Convert given filehandle to a typeglob reference, if necessary.
    $globref = &_qualify_fh($self, $fh);

    ## Ensure filehandle is already open.
    return $self->error("fhopen filehandle isn't already open")
	unless defined($globref) and defined(fileno $globref);

    ## Ensure we're closed.
    $self->close;

    ## Save our private data.
    $s = *$self->{net_telnet};

    ## Switch ourself with the given filehandle.
    *$self = *$globref;

    ## Restore our private data.
    *$self->{net_telnet} = $s;

    ## Re-initialize ourself.
    select((select($self), $|=1)[$[]);  # don't buffer writes
    $s = *$self->{net_telnet};
    $s->{blksize} = &_optimal_blksize((stat $self)[11]);
    $s->{buf} = "";
    $s->{eofile} = '';
    $s->{errormsg} = "";
    vec($s->{fdmask}='', fileno($self), 1) = 1;
    $s->{host} = "";
    $s->{last_line} = "";
    $s->{last_prompt} = "";
    $s->{num_wrote} = 0;
    $s->{opened} = 1;
    $s->{pending_errormsg} = "";
    $s->{port} = '';
    $s->{pushback_buf} = "";
    $s->{timedout} = '';
    $s->{unsent_opts} = "";
    &_reset_options($s->{opts});

    1;
} # end sub fhopen


sub get {
    my ($self, %args) = @_;
    my (
	$binmode,
	$endtime,
	$errmode,
	$line,
	$s,
	$telnetmode,
	$timeout,
	);
    local $_;

    ## Init.
    $s = *$self->{net_telnet};
    $timeout = $s->{time_out};
    $s->{timedout} = '';
    return if $s->{eofile};

    ## Parse the named args.
    foreach (keys %args) {
	if (/^-?binmode$/i) {
	    $binmode = $args{$_};
	    unless (defined $binmode) {
		$binmode = 0;
	    }
	}
	elsif (/^-?errmode$/i) {
	    $errmode = &_parse_errmode($self, $args{$_});
	}
	elsif (/^-?telnetmode$/i) {
	    $telnetmode = $args{$_};
	    unless (defined $telnetmode) {
		$telnetmode = 0;
	    }
	}
	elsif (/^-?timeout$/i) {
	    $timeout = &_parse_timeout($self, $args{$_});
	}
	else {
	    &_croak($self, "bad named parameter \"$_\" given " .
		    "to " . ref($self) . "::get()");
	}
    }

    ## If any args given, override corresponding instance data.
    local $s->{errormode} = $errmode
	if defined $errmode;
    local $s->{bin_mode} = $binmode
	if defined $binmode;
    local $s->{telnet_mode} = $telnetmode
	if defined $telnetmode;

    ## Set wall time when we time out.
    $endtime = &_endtime($timeout);

    ## Try to send any waiting option negotiation.
    if (length $s->{unsent_opts}) {
	&_flush_opts($self);
    }

    ## Try to read just the waiting data using return error mode.
    {
	local $s->{errormode} = "return";
	$s->{errormsg} = "";
	&_fillbuf($self, $s, 0);
    }

    ## We're done if we timed-out and timeout value is set to "poll".
    return $self->error($s->{errormsg})
	if ($s->{timedout} and defined($timeout) and $timeout == 0
	    and !length $s->{buf});

    ## We're done if we hit an error other than timing out.
    if ($s->{errormsg} and !$s->{timedout}) {
	if (!length $s->{buf}) {
	    return $self->error($s->{errormsg});
	}
	else {  # error encountered but there's some data in buffer
	    $s->{pending_errormsg} = $s->{errormsg};
	}
    }

    ## Clear time-out error from first read.
    $s->{timedout} = '';
    $s->{errormsg} = "";

    ## If buffer is still empty, try to read according to user's timeout.
    if (!length $s->{buf}) {
	&_fillbuf($self, $s, $endtime)
	    or do {
		return if $s->{timedout};

		## We've reached end-of-file.
		$self->close;
		return;
	    };
    }

    ## Extract chars from buffer.
    $line = $s->{buf};
    $s->{buf} = "";

    $line;
} # end sub get


sub getline {
    my ($self, %args) = @_;
    my (
	$binmode,
	$endtime,
	$errmode,
	$len,
	$line,
	$offset,
	$pos,
	$rs,
	$s,
	$telnetmode,
	$timeout,
	);
    local $_;

    ## Init.
    $s = *$self->{net_telnet};
    $s->{timedout} = '';
    return if $s->{eofile};
    $rs = $s->{rs};
    $timeout = $s->{time_out};

    ## Parse the named args.
    foreach (keys %args) {
	if (/^-?binmode$/i) {
	    $binmode = $args{$_};
	    unless (defined $binmode) {
		$binmode = 0;
	    }
	}
	elsif (/^-?errmode$/i) {
	    $errmode = &_parse_errmode($self, $args{$_});
	}
	elsif (/^-?input_record_separator$/i or /^-?rs$/i) {
	    $rs = &_parse_input_record_separator($self, $args{$_});
	}
	elsif (/^-?telnetmode$/i) {
	    $telnetmode = $args{$_};
	    unless (defined $telnetmode) {
		$telnetmode = 0;
	    }
	}
	elsif (/^-?timeout$/i) {
	    $timeout = &_parse_timeout($self, $args{$_});
	}
	else {
	    &_croak($self, "bad named parameter \"$_\" given " .
		    "to " . ref($self) . "::getline()");
	}
    }

    ## If any args given, override corresponding instance data.
    local $s->{bin_mode} = $binmode
	if defined $binmode;
    local $s->{errormode} = $errmode
	if defined $errmode;
    local $s->{telnet_mode} = $telnetmode
	if defined $telnetmode;

    ## Set wall time when we time out.
    $endtime = &_endtime($timeout);

    ## Try to send any waiting option negotiation.
    if (length $s->{unsent_opts}) {
	&_flush_opts($self);
    }

    ## Keep reading into buffer until end-of-line is read.
    $offset = 0;
    while (($pos = index($s->{buf}, $rs, $offset)) == -1) {
	$offset = length $s->{buf};
	&_fillbuf($self, $s, $endtime)
	    or do {
		return if $s->{timedout};

		## We've reached end-of-file.
		$self->close;
		if (length $s->{buf}) {
		    return $s->{buf};
		}
		else {
		    return;
		}
	    };
    }

    ## Extract line from buffer.
    $len = $pos + length $rs;
    $line = substr($s->{buf}, 0, $len);
    substr($s->{buf}, 0, $len) = "";

    $line;
} # end sub getline


sub getlines {
    my ($self, %args) = @_;
    my (
	$binmode,
	$errmode,
	$line,
	$rs,
	$s,
	$telnetmode,
	$timeout,
	);
    my $all = 1;
    my @lines = ();
    local $_;

    ## Init.
    $s = *$self->{net_telnet};
    $s->{timedout} = '';
    return if $s->{eofile};
    $timeout = $s->{time_out};

    ## Parse the named args.
    foreach (keys %args) {
	if (/^-?all$/i) {
	    $all = $args{$_};
	    unless (defined $all) {
		$all = '';
	    }
	}
	elsif (/^-?binmode$/i) {
	    $binmode = $args{$_};
	    unless (defined $binmode) {
		$binmode = 0;
	    }
	}
	elsif (/^-?errmode$/i) {
	    $errmode = &_parse_errmode($self, $args{$_});
	}
	elsif (/^-?input_record_separator$/i or /^-?rs$/i) {
	    $rs = &_parse_input_record_separator($self, $args{$_});
	}
	elsif (/^-?telnetmode$/i) {
	    $telnetmode = $args{$_};
	    unless (defined $telnetmode) {
		$telnetmode = 0;
	    }
	}
	elsif (/^-?timeout$/i) {
	    $timeout = &_parse_timeout($self, $args{$_});
	}
	else {
	    &_croak($self, "bad named parameter \"$_\" given " .
		    "to " . ref($self) . "::getlines()");
	}
    }

    ## If any args given, override corresponding instance data.
    local $s->{bin_mode} = $binmode
	if defined $binmode;
    local $s->{errormode} = $errmode
	if defined $errmode;
    local $s->{rs} = $rs
	if defined $rs;
    local $s->{telnet_mode} = $telnetmode
	if defined $telnetmode;
    local $s->{time_out} = &_endtime($timeout);

    ## User requested only the currently available lines.
    if (! $all) {
	return &_next_getlines($self, $s);
    }

    ## Read lines until eof or error.
    while (1) {
	$line = $self->getline
	    or last;
	push @lines, $line;
    }

    ## Check for error.
    return if ! $self->eof;

    @lines;
} # end sub getlines


sub host {
    my ($self, $host) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{host};

    if (@_ >= 2) {
	unless (defined $host) {
	    $host = "";
	}

	$s->{host} = $host;
    }

    $prev;
} # end sub host


sub input_log {
    my ($self, $name) = @_;
    my (
	$fh,
	$s,
	);

    $s = *$self->{net_telnet};
    $fh = $s->{inputlog};

    if (@_ >= 2) {
	unless (defined $name) {
	    $name = "";
	}

	$fh = &_fname_to_handle($self, $name)
	    or return;
	$s->{inputlog} = $fh;
    }

    $fh;
} # end sub input_log


sub input_record_separator {
    my ($self, $rs) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{rs};

    if (@_ >= 2) {
	$s->{rs} = &_parse_input_record_separator($self, $rs);
    }

    $prev;
} # end sub input_record_separator


sub last_prompt {
    my ($self, $string) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{last_prompt};

    if (@_ >= 2) {
	unless (defined $string) {
	    $string = "";
	}

	$s->{last_prompt} = $string;
    }

    $prev;
} # end sub last_prompt


sub lastline {
    my ($self, $line) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{last_line};

    if (@_ >= 2) {
	unless (defined $line) {
	    $line = "";
	}

	$s->{last_line} = $line;
    }

    $prev;
} # end sub lastline


sub login {
    my ($self) = @_;
    my (
	$errmode,
	$error,
	$is_passwd_arg,
	$is_username_arg,
	$lastline,
	$match,
	$ors,
	$passwd,
	$prematch,
	$prompt,
	$s,
	$timeout,
	$username,
	%args,
	);
    local $_;

    ## Init.
    $self->timed_out('');
    $self->last_prompt("");
    $s = *$self->{net_telnet};
    $timeout = $self->timeout;
    $ors = $self->output_record_separator;
    $prompt = $self->prompt;

    ## Parse args.
    if (@_ == 3) {  # just username and passwd given
	$username = $_[1];
	$passwd = $_[2];

	$is_username_arg = 1;
	$is_passwd_arg = 1;
    }
    else {  # named args given
	## Get the named args.
	(undef, %args) = @_;

	## Parse the named args.
	foreach (keys %args) {
	    if (/^-?errmode$/i) {
		$errmode = &_parse_errmode($self, $args{$_});
	    }
	    elsif (/^-?name$/i) {
		$username = $args{$_};
		unless (defined $username) {
		    $username = "";
		}

		$is_username_arg = 1;
	    }
	    elsif (/^-?pass/i) {
		$passwd = $args{$_};
		unless (defined $passwd) {
		    $passwd = "";
		}

		$is_passwd_arg = 1;
	    }
	    elsif (/^-?prompt$/i) {
		$prompt = &_parse_prompt($self, $args{$_});
	    }
	    elsif (/^-?timeout$/i) {
		$timeout = &_parse_timeout($self, $args{$_});
	    }
	    else {
		&_croak($self, "bad named parameter \"$_\" given ",
			"to " . ref($self) . "::login()");
	    }
	}
    }

    ## Ensure both username and password argument given.
    &_croak($self,"Name argument not given to " . ref($self) . "::login()")
	unless $is_username_arg;
    &_croak($self,"Password argument not given to " . ref($self) . "::login()")
	unless $is_passwd_arg;

    ## Override some user settings.
    local $s->{errormode} = $errmode
	if defined $errmode;
    local $s->{time_out} = &_endtime($timeout);

    ## Create a subroutine to generate an error.
    $error
	= sub {
	    my ($errmsg) = @_;

	    if ($self->timed_out) {
		return $self->error($errmsg);
	    }
	    elsif ($self->eof) {
		($lastline = $self->lastline) =~ s/\n+//;
		return $self->error($errmsg, ": ", $lastline);
	    }
	    else {
		return $self->error($self->errmsg);
	    }
	};


    return $self->error("login failed: filehandle isn't open")
	if $self->eof;

    ## Wait for login prompt.
    $self->waitfor(Match => '/login[: ]*$/i',
		   Match => '/username[: ]*$/i',
		   Errmode => "return")
	or do {
	    return &$error("eof read waiting for login prompt")
		if $self->eof;
	    return &$error("timed-out waiting for login prompt");
	};

    ## Delay sending response because of bug in Linux login program.
    &_sleep(0.01);

    ## Send login name.
    $self->put(String => $username . $ors,
	       Errmode => "return")
	or return &$error("login disconnected");

    ## Wait for password prompt.
    $self->waitfor(Match => '/password[: ]*$/i',
		   Errmode => "return")
	or do {
	    return &$error("eof read waiting for password prompt")
		if $self->eof;
	    return &$error("timed-out waiting for password prompt");
	};

    ## Delay sending response because of bug in Linux login program.
    &_sleep(0.01);

    ## Send password.
    $self->put(String => $passwd . $ors,
	       Errmode => "return")
	or return &$error("login disconnected");

    ## Wait for command prompt or another login prompt.
    ($prematch, $match) = $self->waitfor(Match => '/login[: ]*$/i',
					 Match => '/username[: ]*$/i',
					 Match => $prompt,
					 Errmode => "return")
	or do {
	    return &$error("eof read waiting for command prompt")
		if $self->eof;
	    return &$error("timed-out waiting for command prompt");
	};

    ## It's a bad login if we got another login prompt.
    return $self->error("login failed: bad name or password")
	if $match =~ /login[: ]*$/i or $match =~ /username[: ]*$/i;

    ## Save the most recently matched command prompt.
    $self->last_prompt($match);

    1;
} # end sub login


sub max_buffer_length {
    my ($self, $maxbufsize) = @_;
    my (
	$prev,
	$s,
	);
    my $minbufsize = 512;

    $s = *$self->{net_telnet};
    $prev = $s->{maxbufsize};

    if (@_ >= 2) {
	## Ensure a positive integer value.
	unless (defined $maxbufsize
		and $maxbufsize =~ /^\d+$/
		and $maxbufsize)
	{
	    &_carp($self, "ignoring bad Max_buffer_length " .
		   "argument \"$maxbufsize\": it's not a positive integer");
	    $maxbufsize = $prev;
	}

	## Adjust up values that are too small.
	if ($maxbufsize < $minbufsize) {
	    $maxbufsize = $minbufsize;
	}

	$s->{maxbufsize} = $maxbufsize;
    }

    $prev;
} # end sub max_buffer_length


## Make ofs() synonymous with output_field_separator().
*ofs = \&output_field_separator;


sub open {
    my ($self) = @_;
    my (
	$errmode,
	$errno,
	$host,
	$ip_addr,
	$port,
	$s,
	$timeout,
	%args,
	);
    local $_;

    ## Init.
    $s = *$self->{net_telnet};
    $timeout = $s->{time_out};
    $s->{timedout} = '';

    if (@_ == 2) {  # one positional arg given
	$self->host($_[1]);
    }
    elsif (@_ > 2) {  # named args given
	## Get the named args.
	(undef, %args) = @_;

	## Parse the named args.
	foreach (keys %args) {
	    if (/^-?errmode$/i) {
		$errmode = &_parse_errmode($self, $args{$_});
	    }
	    elsif (/^-?host$/i) {
		$self->host($args{$_});
	    }
	    elsif (/^-?port$/i) {
		$self->port($args{$_})
		    or return;
	    }
	    elsif (/^-?timeout$/i) {
		$timeout = &_parse_timeout($self, $args{$_});
	    }
	    else {
		&_croak($self, "bad named parameter \"$_\" given ",
			"to " . ref($self) . "::open()");
	    }
	}
    }

    ## If any args given, override corresponding instance data.
    local $s->{errormode} = $errmode
	if defined $errmode;

    ## Get host and port.
    $host = $self->host;
    $port = $self->port;

    ## Ensure we're already closed.
    $self->close;

    ## Connect with or without a timeout.
    if (defined($timeout) and &_have_alarm) {  # use a timeout
	## Convert possible absolute timeout to relative timeout.
	if ($timeout >= $^T) {  # it's an absolute time
	    $timeout = $timeout - time;
	}

	## Ensure a valid timeout value for alarm.
	if ($timeout < 1) {
	    $timeout = 1;
	}
	$timeout = int($timeout + 1.5);

	## Connect to server, timing out if it takes too long.
	eval {
	    ## Turn on timer.
	    local $SIG{"__DIE__"} = "DEFAULT";
	    local $SIG{ALRM} = sub { die "timed-out\n" };
	    alarm $timeout;

	    ## Lookup server's IP address.
	    $ip_addr = inet_aton $host
		or die "unknown remote host: $host\n";

	    ## Create a socket and attach the filehandle to it.
	    socket $self, AF_INET, SOCK_STREAM, 0
		or die "problem creating socket: $!\n";

	    ## Open connection to server.
	    connect $self, sockaddr_in($port, $ip_addr)
		or die "problem connecting to \"$host\", port $port: $!\n";
	};
	alarm 0;

	## Check for error.
	if ($@ =~ /^timed-out$/) {  # time out failure
	    $s->{timedout} = 1;
	    $self->close;
	    if (!$ip_addr) {
		return $self->error("unknown remote host: $host: ",
				    "name lookup timed-out");
	    }
	    else {
		return $self->error("problem connecting to \"$host\", ",
				    "port $port: connect timed-out");
	    }
	}
	elsif ($@) {  # hostname lookup or connect failure
	    $self->close;
	    chomp $@;
	    return $self->error($@);
	}
    }
    else {  # don't use a timeout
	$timeout = undef;

	## Lookup server's IP address.
	$ip_addr = inet_aton $host
	    or return $self->error("unknown remote host: $host");

	## Create a socket and attach the filehandle to it.
	socket $self, AF_INET, SOCK_STREAM, 0
	    or return $self->error("problem creating socket: $!");

	## Open connection to server.
	connect $self, sockaddr_in($port, $ip_addr)
	    or do {
		$errno = "$!";
		$self->close;
		return $self->error("problem connecting to \"$host\", ",
				    "port $port: $errno");
	    };
    }

    select((select($self), $|=1)[$[]);  # don't buffer writes
    $s->{blksize} = &_optimal_blksize((stat $self)[11]);
    $s->{buf} = "";
    $s->{eofile} = '';
    $s->{errormsg} = "";
    vec($s->{fdmask}='', fileno($self), 1) = 1;
    $s->{last_line} = "";
    $s->{num_wrote} = 0;
    $s->{opened} = 1;
    $s->{pending_errormsg} = "";
    $s->{pushback_buf} = "";
    $s->{timedout} = '';
    $s->{unsent_opts} = "";
    &_reset_options($s->{opts});

    1;
} # end sub open


sub option_accept {
    my ($self, @args) = @_;
    my (
	$arg,
	$option,
	$s,
	@opt_args,
	);
    local $_;

    ## Init.
    $s = *$self->{net_telnet};

    ## Parse the named args.
    while (($_, $arg) = splice @args, 0, 2) {
	## Verify and save arguments.
	if (/^-?do$/i) {
	    ## Make sure a callback is defined.
	    return $self->error("usage: an option callback must already ",
				"be defined when enabling with $_")
		unless $s->{opt_cback};

	    $option = &_verify_telopt_arg($self, $arg, $_);
	    return unless defined $option;
	    push @opt_args, { option    => $option,
			      is_remote => '',
			      is_enable => 1,
			  };
	}
	elsif (/^-?dont$/i) {
	    $option = &_verify_telopt_arg($self, $arg, $_);
	    return unless defined $option;
	    push @opt_args, { option    => $option,
			      is_remote => '',
			      is_enable => '',
			  };
	}
	elsif (/^-?will$/i) {
	    ## Make sure a callback is defined.
	    return $self->error("usage: an option callback must already ",
				"be defined when enabling with $_")
		unless $s->{opt_cback};

	    $option = &_verify_telopt_arg($self, $arg, $_);
	    return unless defined $option;
	    push @opt_args, { option    => $option,
			      is_remote => 1,
			      is_enable => 1,
			  };
	}
	elsif (/^-?wont$/i) {
	    $option = &_verify_telopt_arg($self, $arg, $_);
	    return unless defined $option;
	    push @opt_args, { option    => $option,
			      is_remote => 1,
			      is_enable => '',
			  };
	}
	else {
	    return $self->error('usage: $obj->option_accept(' .
				'[Do => $telopt,] ',
				'[Dont => $telopt,] ',
				'[Will => $telopt,] ',
				'[Wont => $telopt,]');
	}
    }

    ## Set "receive ok" for options specified.
    &_opt_accept($self, @opt_args);
} # end sub option_accept


sub option_callback {
    my ($self, $callback) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{opt_cback};

    if (@_ >= 2) {
	unless (defined $callback and ref($callback) eq "CODE") {
	    &_carp($self, "ignoring Option_callback argument because it's " .
		   "not a code ref");
	    $callback = $prev;
	}

	$s->{opt_cback} = $callback;
    }

    $prev;
} # end sub option_callback


sub option_log {
    my ($self, $name) = @_;
    my (
	$fh,
	$s,
	);

    $s = *$self->{net_telnet};
    $fh = $s->{opt_log};

    if (@_ >= 2) {
	unless (defined $name) {
	    $name = "";
	}

	$fh = &_fname_to_handle($self, $name)
	    or return;
	$s->{opt_log} = $fh;
    }

    $fh;
} # end sub option_log


sub option_state {
    my ($self, $option) = @_;
    my (
	$opt_state,
	$s,
	%opt_state,
	);

    ## Ensure telnet option is non-negative integer.
    $option = &_verify_telopt_arg($self, $option);
    return unless defined $option;

    ## Init.
    $s = *$self->{net_telnet};
    unless (defined $s->{opts}{$option}) {
	&_set_default_option($s, $option);
    }

    ## Return hashref to a copy of the values.
    $opt_state = $s->{opts}{$option};
    %opt_state = %$opt_state;
    \%opt_state;
} # end sub option_state


## Make ors() synonymous with output_record_separator().
*ors = \&output_record_separator;


sub output_field_separator {
    my ($self, $ofs) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{ofs};

    if (@_ >= 2) {
	unless (defined $ofs) {
	    $ofs = "";
	}

	$s->{ofs} = $ofs;
    }

    $prev;
} # end sub output_field_separator


sub output_log {
    my ($self, $name) = @_;
    my (
	$fh,
	$s,
	);

    $s = *$self->{net_telnet};
    $fh = $s->{outputlog};

    if (@_ >= 2) {
	unless (defined $name) {
	    $name = "";
	}

	$fh = &_fname_to_handle($self, $name)
	    or return;
	$s->{outputlog} = $fh;
    }

    $fh;
} # end sub output_log


sub output_record_separator {
    my ($self, $ors) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{ors};

    if (@_ >= 2) {
	unless (defined $ors) {
	    $ors = "";
	}

	$s->{ors} = $ors;
    }

    $prev;
} # end sub output_record_separator


sub port {
    my ($self, $port) = @_;
    my (
	$prev,
	$s,
	$service,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{port};

    if (@_ >= 2) {
	unless (defined $port) {
	    $port = "";
	}

	if (!$port) {
	    &_carp($self, "ignoring bad Port argument \"$port\"");
	    $port = $prev;
	}
	elsif ($port !~ /^\d+$/) {  # port isn't all digits
	    $service = $port;
	    $port = getservbyname($service, "tcp");
	    unless ($port) {
		&_carp($self, "ignoring bad Port argument \"$service\": " .
		       "it's an unknown TCP service");
		$port = $prev;
	    }
	}

	$s->{port} = $port;
    }

    $prev;
} # end sub port


sub print {
    my ($self) = shift;
    my (
	$buf,
	$fh,
	$s,
	);

    $s = *$self->{net_telnet};
    $s->{timedout} = '';
    return $self->error("write error: filehandle isn't open")
	unless $s->{opened};

    ## Add field and record separators.
    $buf = join($s->{ofs}, @_) . $s->{ors};

    ## Log the output if requested.
    if ($s->{outputlog}) {
	&_log_print($s->{outputlog}, $buf);
    }

    ## Convert native newlines to CR LF.
    if (!$s->{bin_mode}) {
	$buf =~ s(\n)(\015\012)g;
    }

    ## Escape TELNET IAC and also CR not followed by LF.
    if ($s->{telnet_mode}) {
	$buf =~ s(\377)(\377\377)g;
	&_escape_cr(\$buf);
    }

    &_put($self, \$buf, "print");
} # end sub print


sub print_length {
    my ($self) = @_;

    *$self->{net_telnet}{num_wrote};
} # end sub print_length


sub prompt {
    my ($self, $prompt) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{cmd_prompt};

    ## Parse args.
    if (@_ == 2) {
	$s->{cmd_prompt} = &_parse_prompt($self, $prompt);
    }

    $prev;
} # end sub prompt


sub put {
    my ($self) = @_;
    my (
	$binmode,
	$buf,
	$errmode,
	$is_timeout_arg,
	$s,
	$telnetmode,
	$timeout,
	%args,
	);
    local $_;

    ## Init.
    $s = *$self->{net_telnet};
    $s->{timedout} = '';

    ## Parse args.
    if (@_ == 2) {  # one positional arg given
	$buf = $_[1];
    }
    elsif (@_ > 2) {  # named args given
	## Get the named args.
	(undef, %args) = @_;

	## Parse the named args.
	foreach (keys %args) {
	    if (/^-?binmode$/i) {
		$binmode = $args{$_};
		unless (defined $binmode) {
		    $binmode = 0;
		}
	    }
	    elsif (/^-?errmode$/i) {
		$errmode = &_parse_errmode($self, $args{$_});
	    }
	    elsif (/^-?string$/i) {
		$buf = $args{$_};
	    }
	    elsif (/^-?telnetmode$/i) {
		$telnetmode = $args{$_};
		unless (defined $telnetmode) {
		    $telnetmode = 0;
		}
	    }
	    elsif (/^-?timeout$/i) {
		$timeout = &_parse_timeout($self, $args{$_});
		$is_timeout_arg = 1;
	    }
	    else {
		&_croak($self, "bad named parameter \"$_\" given ",
			"to " . ref($self) . "::put()");
	    }
	}
    }

    ## If any args given, override corresponding instance data.
    local $s->{bin_mode} = $binmode
	if defined $binmode;
    local $s->{errormode} = $errmode
	if defined $errmode;
    local $s->{telnet_mode} = $telnetmode
	if defined $telnetmode;
    local $s->{time_out} = $timeout
	if defined $is_timeout_arg;

    ## Check for errors.
    return $self->error("write error: filehandle isn't open")
	unless $s->{opened};

    ## Log the output if requested.
    if ($s->{outputlog}) {
	&_log_print($s->{outputlog}, $buf);
    }

    ## Convert native newlines to CR LF.
    if (!$s->{bin_mode}) {
	$buf =~ s(\n)(\015\012)g;
    }

    ## Escape TELNET IAC and also CR not followed by LF.
    if ($s->{telnet_mode}) {
	$buf =~ s(\377)(\377\377)g;
	&_escape_cr(\$buf);
    }

    &_put($self, \$buf, "print");
} # end sub put


## Make rs() synonymous input_record_separator().
*rs = \&input_record_separator;


sub suboption_callback {
    my ($self, $callback) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{subopt_cback};

    if (@_ >= 2) {
	unless (defined $callback and ref($callback) eq "CODE") {
	    &_carp($self,"ignoring Suboption_callback argument because it's " .
		   "not a code ref");
	    $callback = $prev;
	}

	$s->{subopt_cback} = $callback;
    }

    $prev;
} # end sub suboption_callback


sub telnetmode {
    my ($self, $mode) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{telnet_mode};

    if (@_ >= 2) {
	unless (defined $mode) {
	    $mode = 0;
	}

	$s->{telnet_mode} = $mode;
    }

    $prev;
} # end sub telnetmode


sub timed_out {
    my ($self, $value) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{timedout};

    if (@_ >= 2) {
	unless (defined $value) {
	    $value = "";
	}

	$s->{timedout} = $value;
    }

    $prev;
} # end sub timed_out


sub timeout {
    my ($self, $timeout) = @_;
    my (
	$prev,
	$s,
	);

    $s = *$self->{net_telnet};
    $prev = $s->{time_out};

    if (@_ >= 2) {
	$s->{time_out} = &_parse_timeout($self, $timeout);
    }

    $prev;
} # end sub timeout


sub waitfor {
    my ($self, @args) = @_;
    my (
	$arg,
	$binmode,
	$endtime,
	$errmode,
	$len,
	$match,
	$match_op,
	$pos,
	$prematch,
	$s,
	$search,
	$search_cond,
	$telnetmode,
	$timeout,
	@match_cond,
	@match_ops,
	@search_cond,
	@string_cond,
	@warns,
	);
    local $_;

    ## Init.
    $s = *$self->{net_telnet};
    $s->{timedout} = '';
    return if $s->{eofile};
    return unless @args;
    $timeout = $s->{time_out};

    ## Code template used to build string match conditional.
    ## Values between array elements must be supplied later.
    @string_cond =
	('if (($pos = index $s->{buf}, ', ') > -1) {
	    $len = ', ';
	    $prematch = substr $s->{buf}, 0, $pos;
	    $match = substr $s->{buf}, $pos, $len;
	    substr($s->{buf}, 0, $pos + $len) = "";
	    last;
	}');

    ## Code template used to build pattern match conditional.
    ## Values between array elements must be supplied later.
    @match_cond =
	('if ($s->{buf} =~ ', ') {
	    $prematch = $`;
	    $match = $&;
	    substr($s->{buf}, 0, length($`) + length($&)) = "";
	    last;
	}');

    ## Parse args.
    if (@_ == 2) {  # one positional arg given
	$arg = $_[1];

	## Fill in the blanks in the code template.
	push @match_ops, $arg;
	push @search_cond, join("", $match_cond[0], $arg, $match_cond[1]);
    }
    elsif (@_ > 2) {  # named args given
	## Parse the named args.
	while (($_, $arg) = splice @args, 0, 2) {
	    if (/^-?binmode$/i) {
		$binmode = $arg;
		unless (defined $binmode) {
		    $binmode = 0;
		}
	    }
	    elsif (/^-?errmode$/i) {
		$errmode = &_parse_errmode($self, $arg);
	    }
	    elsif (/^-?match$/i) {
		## Fill in the blanks in the code template.
		push @match_ops, $arg;
		push @search_cond, join("",
					$match_cond[0], $arg, $match_cond[1]);
	    }
	    elsif (/^-?string$/i) {
		## Fill in the blanks in the code template.
		$arg =~ s/'/\\'/g;  # quote ticks
		push @search_cond, join("",
					$string_cond[0], "'$arg'",
					$string_cond[1], length($arg),
					$string_cond[2]);
	    }
	    elsif (/^-?telnetmode$/i) {
		$telnetmode = $arg;
		unless (defined $telnetmode) {
		    $telnetmode = 0;
		}
	    }
	    elsif (/^-?timeout$/i) {
		$timeout = &_parse_timeout($self, $arg);
	    }
	    else {
		&_croak($self, "bad named parameter \"$_\" given " .
			"to " . ref($self) . "::waitfor()");
	    }
	}
    }

    ## If any args given, override corresponding instance data.
    local $s->{errormode} = $errmode
	if defined $errmode;
    local $s->{bin_mode} = $binmode
	if defined $binmode;
    local $s->{telnet_mode} = $telnetmode
	if defined $telnetmode;

    ## Check for bad match operator argument.
    foreach $match_op (@match_ops) {
	return $self->error("missing opening delimiter of match operator ",
			    "in argument \"$match_op\" given to ",
			    ref($self) . "::waitfor()")
	    unless $match_op =~ m(^\s*/) or $match_op =~ m(^\s*m\s*\W);
    }

    ## Construct conditional to check for requested string and pattern matches.
    ## Turn subsequent "if"s into "elsif".
    $search_cond = join "\n\tels", @search_cond;

    ## Construct loop to fill buffer until string/pattern, timeout, or eof.
    $search = join "", "
    while (1) {\n\t",
	$search_cond, '
	&_fillbuf($self, $s, $endtime)
	    or do {
		last if $s->{timedout};
		$self->close;
		last;
	    };
    }';

    ## Set wall time when we timeout.
    $endtime = &_endtime($timeout);

    ## Run the loop.
    {
	local $^W = 1;
	local $SIG{"__WARN__"} = sub { push @warns, @_ };
	local $s->{errormode} = "return";
	$s->{errormsg} = "";
	eval $search;
    }

    ## Check for failure.
    return $self->error("pattern match timed-out") if $s->{timedout};
    return $self->error($s->{errormsg}) if $s->{errormsg} ne "";
    return $self->error("pattern match read eof") if $s->{eofile};

    ## Check for Perl syntax errors or warnings.
    if ($@ or @warns) {
	foreach $match_op (@match_ops) {
	    &_match_check($self, $match_op)
		or return;
	}
	return $self->error($@) if $@;
	return $self->error(@warns) if @warns;
    }

    wantarray ? ($prematch, $match) : 1;
} # end sub waitfor


######################## Private Subroutines #########################


sub _append_lineno {
    my ($obj, @msgs) = @_;
    my (
	$file,
	$line,
	$pkg,
	);

    ## Find the caller that's not in object's class or one of its base classes.
    ($pkg, $file , $line) = &_user_caller($obj);
    join("", @msgs, " at ", $file, " line ", $line, "\n");
} # end sub _append_lineno


sub _carp {
    warn &_append_lineno(@_);
} # end sub _carp


sub _croak {
    die &_append_lineno(@_);
} # end sub _croak


sub _endtime {
    my ($interval) = @_;

    ## Compute wall time when timeout occurs.
    if (defined $interval) {
	if ($interval >= $^T) {  # it's already an absolute time
	    return $interval;
	}
	elsif ($interval > 0) {  # it's relative to the current time
	    return int(time + 1.5 + $interval);
	}
	else {  # it's a one time poll
	    return 0;
	}
    }
    else {  # there's no timeout
	return undef;
    }
} # end sub _endtime


sub _escape_cr {
    my ($string) = @_;
    my (
	$nextchar,
	);
    my $pos = 0;

    ## Convert all CR (not followed by LF) to CR NULL.
    while (($pos = index($$string, "\015", $pos)) > -1) {
	$nextchar = substr $$string, $pos + 1, 1;

	substr($$string, $pos, 1) = "\015\000"
	    unless $nextchar eq "\012";

	$pos++;
    }

    1;
} # end sub _escape_cr


sub _fillbuf {
    my ($self, $s, $endtime) = @_;
    my (
	$msg,
	$nfound,
	$nread,
	$pushback_len,
	$read_pos,
	$ready,
	$timed_out,
	$timeout,
	$unparsed_pos,
	);

    ## If error from last read not yet reported then do it now.
    if ($s->{pending_errormsg}) {
	$msg = $s->{pending_errormsg};
	$s->{pending_errormsg} = "";
	return $self->error($msg);
    }

    return unless $s->{opened};

    while (1) {
	## Maximum buffer size exceeded?
	return $self->error("maximum input buffer length exceeded: ",
			    $s->{maxbufsize}, " bytes")
	    unless length($s->{buf}) <= $s->{maxbufsize};

	## Determine how long to wait for input ready.
	($timed_out, $timeout) = &_timeout_interval($endtime);
	if ($timed_out) {
	    $s->{timedout} = 1;
	    return $self->error("read timed-out");
	}

	## Wait for input ready.
	$nfound = select $ready=$s->{fdmask}, "", "", $timeout;

	## Handle any errors while waiting.
	if (!defined $nfound or $nfound <= 0) {  # input not ready
	    if (defined $nfound and $nfound == 0) {  # timed-out
		$s->{timedout} = 1;
		return $self->error("read timed-out");
	    }
	    else {  # error waiting for input ready
		next if $! =~ /^interrupted/i;

		$s->{opened} = '';
		return $self->error("read error: $!");
	    }
	}

	## Append to buffer any partially processed telnet or CR sequence.
	$pushback_len = length $s->{pushback_buf};
	if ($pushback_len) {
	    $s->{buf} .= $s->{pushback_buf};
	    $s->{pushback_buf} = "";
	}

	## Read the waiting data.
	$read_pos = length $s->{buf};
	$unparsed_pos = $read_pos - $pushback_len;
	$nread = sysread $self, $s->{buf}, $s->{blksize}, $read_pos;

	## Handle any read errors.
	if (!defined $nread) {  # read failed
	    next if $! =~ /^interrupted/i;  # restart interrupted syscall

	    $s->{opened} = '';
	    return $self->error("read error: $!");
	}

	## Handle eof.
	if ($nread == 0) {  # eof read
	    $s->{opened} = '';
	    return;
	}

	## Display network traffic if requested.
	if ($s->{dumplog}) {
	    &_log_dump('<', $s->{dumplog}, \$s->{buf}, $read_pos);
	}

	## Process any telnet commands in the data stream.
	if ($s->{telnet_mode} and index($s->{buf},"\377",$unparsed_pos) > -1) {
	    &_interpret_tcmd($self, $s, $unparsed_pos);
	}

	## Process any carriage-return sequences in the data stream.
	&_interpret_cr($s, $unparsed_pos);

	## Read again if all chars read were consumed as telnet cmds.
	next if $unparsed_pos >= length $s->{buf};

	## Log the input if requested.
	if ($s->{inputlog}) {
	    &_log_print($s->{inputlog}, substr($s->{buf}, $unparsed_pos));
	}

	## Save the last line read.
	&_save_lastline($s);

	## We've successfully read some data into the buffer.
	last;
    } # end while(1)

    1;
} # end sub _fillbuf


sub _flush_opts {
    my ($self) = @_;
    my (
	$option_chars,
	);
    my $s = *$self->{net_telnet};

    ## Get option and clear the output buf.
    $option_chars = $s->{unsent_opts};
    $s->{unsent_opts} = "";

    ## Try to send options without waiting.
    {
	local $s->{errormode} = "return";
	local $s->{time_out} = 0;
	&_put($self, \$option_chars, "telnet option negotiation")
	    or do {
		## Save chars not printed for later.
		substr($option_chars, 0, $self->print_length) = "";
		$s->{unsent_opts} .= $option_chars;
	    };
    }

    1;
} # end sub _flush_opts


sub _fname_to_handle {
    my ($self, $fh) = @_;
    my (
	$filename,
	);

    ## Ensure valid input.
    return ""
	unless defined $fh and (ref $fh or length $fh);

    ## Open a new filehandle if input is a filename.
    no strict "refs";
    if (!ref($fh) and !defined(fileno $fh)) {  # fh is a filename
	$filename = $fh;
	$fh = &_new_handle();
	CORE::open $fh, "> $filename"
	    or return $self->error("problem creating $filename: $!");
    }

    select((select($fh), $|=1)[$[]);  # don't buffer writes
    $fh;
} # end sub _fname_to_handle


sub _have_alarm {
    eval {
	local $SIG{"__DIE__"} = "DEFAULT";
	local $SIG{ALRM} = sub { die };
	alarm 0;
    };

    ! $@;
} # end sub _have_alarm


sub _interpret_cr {
    my ($s, $pos) = @_;
    my (
	$nextchar,
	);

    while (($pos = index($s->{buf}, "\015", $pos)) > -1) {
	$nextchar = substr($s->{buf}, $pos + 1, 1);
	if ($nextchar eq "\0") {
	    ## Convert CR NULL to CR when in telnet mode.
	    if ($s->{telnet_mode}) {
		substr($s->{buf}, $pos + 1, 1) = "";
	    }
	}
	elsif ($nextchar eq "\012") {
	    ## Convert CR LF to newline when not in binary mode.
	    if (!$s->{bin_mode}) {
		substr($s->{buf}, $pos, 2) = "\n";
	    }
	}
	elsif (!length($nextchar) and ($s->{telnet_mode} or !$s->{bin_mode})) {
	    ## Save CR in alt buffer for possible CR LF or CR NULL conversion.
	    $s->{pushback_buf} .= "\015";
	    chop $s->{buf};
	}

	$pos++;
    }

    1;
} # end sub _interpret_cr


sub _interpret_tcmd {
    my ($self, $s, $offset) = @_;
    my (
	$callback,
	$endpos,
	$nextchar,
	$option,
	$parameters,
	$pos,
	$subcmd,
	);
    local $_;

    ## Parse telnet commands in the data stream.
    $pos = $offset;
    while (($pos = index $s->{buf}, "\377", $pos) > -1) {  # unprocessed IAC
	$nextchar = substr $s->{buf}, $pos + 1, 1;

	## Save command if it's only partially read.
	if (!length $nextchar) {
	    $s->{pushback_buf} .= "\377";
	    chop $s->{buf};
	    last;
	}

	if ($nextchar eq "\377") {  # IAC is escaping "\377" char
	    ## Remove escape char from data stream.
	    substr($s->{buf}, $pos, 1) = "";
	    $pos++;
	}
	elsif ($nextchar eq "\375" or $nextchar eq "\373" or
	       $nextchar eq "\374" or $nextchar eq "\376") {  # opt negotiation
	    $option = substr $s->{buf}, $pos + 2, 1;

	    ## Save command if it's only partially read.
	    if (!length $option) {
		$s->{pushback_buf} .= "\377" . $nextchar;
		chop $s->{buf};
		chop $s->{buf};
		last;
	    }

	    ## Remove command from data stream.
	    substr($s->{buf}, $pos, 3) = "";

	    ## Handle option negotiation.
	    &_negotiate_recv($self, $s, $nextchar, ord($option), $pos);
	}
	elsif ($nextchar eq "\372") {  # start of subnegotiation parameters
	    ## Save command if it's only partially read.
	    $endpos = index $s->{buf}, "\360", $pos;
	    if ($endpos == -1) {
		$s->{pushback_buf} .= substr $s->{buf}, $pos;
		substr($s->{buf}, $pos) = "";
		last;
	    }

	    ## Remove subnegotiation cmd from buffer.
	    $subcmd = substr($s->{buf}, $pos, $endpos - $pos + 1);
	    substr($s->{buf}, $pos, $endpos - $pos + 1) = "";

	    ## Invoke subnegotiation callback.
	    if ($s->{subopt_cback} and length($subcmd) >= 5) {
		$option = unpack "C", substr($subcmd, 2, 1);
		if (length($subcmd) >= 6) {
		    $parameters = substr $subcmd, 3, length($subcmd) - 5;
		}
		else {
		    $parameters = "";
		}

		$callback = $s->{subopt_cback};
		&$callback($self, $option, $parameters);
	    }
	}
	else {  # various two char telnet commands
	    ## Ignore and remove command from data stream.
	    substr($s->{buf}, $pos, 2) = "";
	}
    }

    ## Try to send any waiting option negotiation.
    if (length $s->{unsent_opts}) {
	&_flush_opts($self);
    }

    1;
} # end sub _interpret_tcmd


sub _io_socket_include {
    local $SIG{"__DIE__"} = "DEFAULT";
    eval "require IO::Socket";
} # end sub io_socket_include


sub _log_dump {
    my ($direction, $fh, $data, $offset, $len) = @_;
    my (
	$addr,
	$hexvals,
	$line,
	);

    $addr = 0;
    $len = length($$data) - $offset
	if !defined $len;
    return 1 if $len <= 0;

    ## Print data in dump format.
    while ($len > 0) {
	## Convert up to the next 16 chars to hex, padding w/ spaces.
	if ($len >= 16) {
	    $line = substr $$data, $offset, 16;
	}
	else {
	    $line = substr $$data, $offset, $len;
	}
	$hexvals = unpack("H*", $line);
	$hexvals .= ' ' x (32 - length $hexvals);

	## Place in 16 columns, each containing two hex digits.
	$hexvals = sprintf("%s %s %s %s  " x 4,
			   unpack("a2" x 16, $hexvals));

	## For the ASCII column, change unprintable chars to a period.
	$line =~ s/[\000-\037,\177-\237]/./g;

	## Print the line in dump format.
	&_log_print($fh, sprintf("%s 0x%5.5lx: %s%s\n",
				 $direction, $addr, $hexvals, $line));

	$addr += 16;
	$offset += 16;
	$len -= 16;
    }

    &_log_print($fh, "\n");

    1;
} # end sub _log_dump


sub _log_option {
    my ($fh, $direction, $request, $option) = @_;
    my (
	$name,
	);

    if ($option >= 0 and $option <= $#Telopts) {
	$name = $Telopts[$option];
    }
    else {
	$name = $option;
    }

    &_log_print($fh, "$direction $request $name\n");
} # end sub _log_option


sub _log_print {
    my ($fh, $buf) = @_;
    local $\ = '';

    if (ref($fh) and ref($fh) ne "GLOB") {  # fh is blessed ref
	$fh->print($buf);
    }
    else {  # fh isn't blessed ref
	print $fh $buf;
    }
} # end sub _log_print


sub _match_check {
    my ($self, $code) = @_;
    my $error;
    my @warns = ();

    ## Use eval to check for syntax errors or warnings.
    {
	local $SIG{"__DIE__"} = "DEFAULT";
	local $SIG{"__WARN__"} = sub { push @warns, @_ };
	local $^W = 1;
	local $_ = '';
	eval "\$_ =~ $code;";
    }
    if ($@) {
	## Remove useless lines numbers from message.
	($error = $@) =~ s/ at \(eval \d+\) line \d+.?//;
	chomp $error;
	return $self->error("bad match operator: $error");
    }
    elsif (@warns) {
	## Remove useless lines numbers from message.
	($error = shift @warns) =~ s/ at \(eval \d+\) line \d+.?//;
	$error =~ s/ while "strict subs" in use//;
	chomp $error;
	return $self->error("bad match operator: $error");
    }

    1;
} # end sub _match_check


sub _negotiate_callback {
    my ($self, $opt, $is_remote, $is_enabled, $was_enabled, $opt_bufpos) = @_;
    my (
	$callback,
	$s,
	);
    local $_;

    ## Keep track of remote echo.
    if ($is_remote and $opt == &TELOPT_ECHO) {  # received WILL or WONT ECHO
	$s = *$self->{net_telnet};

	if ($is_enabled and !$was_enabled) {  # received WILL ECHO
	    $s->{remote_echo} = 1;
	}
	elsif (!$is_enabled and $was_enabled) {  # received WONT ECHO
	    $s->{remote_echo} = '';
	}
    }

    ## Invoke callback, if there is one.
    $callback = $self->option_callback;
    if ($callback) {
	&$callback($self, $opt, $is_remote,
		   $is_enabled, $was_enabled, $opt_bufpos);
    }

    1;
} # end sub _negotiate_callback


sub _negotiate_recv {
    my ($self, $s, $opt_request, $opt, $opt_bufpos) = @_;

    ## Ensure data structure exists for this option.
    unless (defined $s->{opts}{$opt}) {
	&_set_default_option($s, $opt);
    }

    ## Process the option.
    if ($opt_request eq "\376") {  # DONT
	&_negotiate_recv_disable($self, $s, $opt, "dont", $opt_bufpos,
				 $s->{opts}{$opt}{local_enable_ok},
				 \$s->{opts}{$opt}{local_enabled},
				 \$s->{opts}{$opt}{local_state});
    }
    elsif ($opt_request eq "\375") {  # DO
	&_negotiate_recv_enable($self, $s, $opt, "do", $opt_bufpos,
				$s->{opts}{$opt}{local_enable_ok},
				\$s->{opts}{$opt}{local_enabled},
				\$s->{opts}{$opt}{local_state});
    }
    elsif ($opt_request eq "\374") {  # WONT
	&_negotiate_recv_disable($self, $s, $opt, "wont", $opt_bufpos,
				 $s->{opts}{$opt}{remote_enable_ok},
				 \$s->{opts}{$opt}{remote_enabled},
				 \$s->{opts}{$opt}{remote_state});
    }
    elsif ($opt_request eq "\373") {  # WILL
	&_negotiate_recv_enable($self, $s, $opt, "will", $opt_bufpos,
				$s->{opts}{$opt}{remote_enable_ok},
				\$s->{opts}{$opt}{remote_enabled},
				\$s->{opts}{$opt}{remote_state});
    }
    else {  # internal error
	die;
    }

    1;
} # end sub _negotiate_recv


sub _negotiate_recv_disable {
    my ($self, $s, $opt, $opt_request,
	$opt_bufpos, $enable_ok, $is_enabled, $state) = @_;
    my (
	$ack,
	$disable_cmd,
	$enable_cmd,
	$is_remote,
	$nak,
	$was_enabled,
	);

    ## What do we use to request enable/disable or respond with ack/nak.
    if ($opt_request eq "wont") {
	$enable_cmd  = "\377\375" . pack("C", $opt);  # do command
	$disable_cmd = "\377\376" . pack("C", $opt);  # dont command
	$is_remote = 1;
	$ack = "DO";
	$nak = "DONT";

	&_log_option($s->{opt_log}, "RCVD", "WONT", $opt)
	    if $s->{opt_log};
    }
    elsif ($opt_request eq "dont") {
	$enable_cmd  = "\377\373" . pack("C", $opt);  # will command
	$disable_cmd = "\377\374" . pack("C", $opt);  # wont command
	$is_remote = '';
	$ack = "WILL";
	$nak = "WONT";

	&_log_option($s->{opt_log}, "RCVD", "DONT", $opt)
	    if $s->{opt_log};
    }
    else {  # internal error
	die;
    }

    ## Respond to WONT or DONT based on the current negotiation state.
    if ($$state eq "no") {  # state is already disabled
    }
    elsif ($$state eq "yes") {  # they're initiating disable
	$$is_enabled = '';
	$$state = "no";

	## Send positive acknowledgment.
	$s->{unsent_opts} .= $disable_cmd;
	&_log_option($s->{opt_log}, "SENT", $nak, $opt)
	    if $s->{opt_log};

	## Invoke callbacks.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantno") {  # they sent positive ack
	$$is_enabled = '';
	$$state = "no";

	## Invoke callback.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantno opposite") {  # pos ack but we changed our mind
	## Indicate disabled but now we want to enable.
	$$is_enabled = '';
	$$state = "wantyes";

	## Send queued request.
	$s->{unsent_opts} .= $enable_cmd;
	&_log_option($s->{opt_log}, "SENT", $ack, $opt)
	    if $s->{opt_log};

	## Invoke callback.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantyes") {  # they sent negative ack
	$$is_enabled = '';
	$$state = "no";

	## Invoke callback.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantyes opposite") {  # nak but we changed our mind
	$$is_enabled = '';
	$$state = "no";

	## Invoke callback.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
} # end sub _negotiate_recv_disable


sub _negotiate_recv_enable {
    my ($self, $s, $opt, $opt_request,
	$opt_bufpos, $enable_ok, $is_enabled, $state) = @_;
    my (
	$ack,
	$disable_cmd,
	$enable_cmd,
	$is_remote,
	$nak,
	$was_enabled,
	);

    ## What we use to send enable/disable request or send ack/nak response.
    if ($opt_request eq "will") {
	$enable_cmd  = "\377\375" . pack("C", $opt);  # do command
	$disable_cmd = "\377\376" . pack("C", $opt);  # dont command
	$is_remote = 1;
	$ack = "DO";
	$nak = "DONT";

	&_log_option($s->{opt_log}, "RCVD", "WILL", $opt)
	    if $s->{opt_log};
    }
    elsif ($opt_request eq "do") {
	$enable_cmd  = "\377\373" . pack("C", $opt);  # will command
	$disable_cmd = "\377\374" . pack("C", $opt);  # wont command
	$is_remote = '';
	$ack = "WILL";
	$nak = "WONT";

	&_log_option($s->{opt_log}, "RCVD", "DO", $opt)
	    if $s->{opt_log};
    }
    else {  # internal error
	die;
    }

    ## Save current enabled state.
    $was_enabled = $$is_enabled;

    ## Respond to WILL or DO based on the current negotiation state.
    if ($$state eq "no") {  # they're initiating enable
	if ($enable_ok) {  # we agree they/us should enable
	    $$is_enabled = 1;
	    $$state = "yes";

	    ## Send positive acknowledgment.
	    $s->{unsent_opts} .= $enable_cmd;
	    &_log_option($s->{opt_log}, "SENT", $ack, $opt)
		if $s->{opt_log};

	    ## Invoke callbacks.
	    &_negotiate_callback($self, $opt, $is_remote,
				 $$is_enabled, $was_enabled, $opt_bufpos);
	}
	else {  # we disagree they/us should enable
	    ## Send negative acknowledgment.
	    $s->{unsent_opts} .= $disable_cmd;
	    &_log_option($s->{opt_log}, "SENT", $nak, $opt)
		if $s->{opt_log};
	}
    }
    elsif ($$state eq "yes") {  # state is already enabled
    }
    elsif ($$state eq "wantno") {  # error: our disable req answered by enable
	$$is_enabled = '';
	$$state = "no";

	## Invoke callbacks.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantno opposite") { # err: disable req answerd by enable
	$$is_enabled = 1;
	$$state = "yes";

	## Invoke callbacks.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantyes") {  # they sent pos ack
	$$is_enabled = 1;
	$$state = "yes";

	## Invoke callback.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }
    elsif ($$state eq "wantyes opposite") {  # pos ack but we changed our mind
	## Indicate enabled but now we want to disable.
	$$is_enabled = 1;
	$$state = "wantno";

	## Inform other side we changed our mind.
	$s->{unsent_opts} .= $disable_cmd;
	&_log_option($s->{opt_log}, "SENT", $nak, $opt)
	    if $s->{opt_log};

	## Invoke callback.
	&_negotiate_callback($self, $opt, $is_remote,
			     $$is_enabled, $was_enabled, $opt_bufpos);
    }

    1;
} # end sub _negotiate_recv_enable


sub _new_handle {
    if ($INC{"IO/Handle.pm"}) {
	return IO::Handle->new;
    }
    else {
	require FileHandle;
	return FileHandle->new;
    }
} # end sub _new_handle


sub _next_getlines {
    my ($self, $s) = @_;
    my (
	$len,
	$line,
	$pos,
	@lines,
	);

    ## Fill buffer and get first line.
    $line = $self->getline
	or return;
    push @lines, $line;

    ## Extract subsequent lines from buffer.
    while (($pos = index($s->{buf}, $s->{rs})) != -1) {
	$len = $pos + length $s->{rs};
	push @lines, substr($s->{buf}, 0, $len);
	substr($s->{buf}, 0, $len) = "";
    }

    @lines;
} # end sub _next_getlines


sub _opt_accept {
    my ($self, @args) = @_;
    my (
	$arg,
	$option,
	$s,
	);

    ## Init.
    $s = *$self->{net_telnet};

    foreach $arg (@args) {
	## Ensure data structure defined for this option.
	$option = $arg->{option};
	if (!defined $s->{opts}{$option}) {
	    &_set_default_option($s, $option);
	}

	## Save whether we'll accept or reject this option.
	if ($arg->{is_remote}) {
	    $s->{opts}{$option}{remote_enable_ok} = $arg->{is_enable};
	}
	else {
	    $s->{opts}{$option}{local_enable_ok} = $arg->{is_enable};
	}
    }

    1;
} # end sub _opt_accept


sub _optimal_blksize {
    my ($blksize) = @_;
    local $^W = '';  # avoid non-numeric warning for ms-windows blksize of ""

    ## Use default when block size is invalid.
    return 8192
	unless defined $blksize and $blksize >= 1 and $blksize <= 1_048_576;

    $blksize;
} # end sub _optimal_blksize


sub _parse_cmd_remove_mode {
    my ($self, $mode) = @_;

    if (!defined $mode) {
	$mode = 0;
    }
    elsif ($mode =~ /^\s*auto\s*$/i) {
	$mode = "auto";
    }
    elsif ($mode !~ /^\d+$/) {
	&_carp($self, "ignoring bad Cmd_remove_mode " .
	       "argument \"$mode\": it's not \"auto\" or a " .
	       "non-negative integer");
	$mode = *$self->{net_telnet}{cmd_rm_mode};
    }

    $mode;
} # end sub _parse_cmd_remove_mode


sub _parse_errmode {
    my ($self, $errmode) = @_;

    ## Set the error mode.
    if (!defined $errmode) {
	&_carp($self, "ignoring undefined Errmode argument");
	$errmode = *$self->{net_telnet}{errormode};
    }
    elsif ($errmode =~ /^\s*return\s*$/i) {
	$errmode = "return";
    }
    elsif ($errmode =~ /^\s*die\s*$/i) {
	$errmode = "die";
    }
    elsif (ref($errmode) eq "CODE") {
    }
    elsif (ref($errmode) eq "ARRAY") {
	unless (ref($errmode->[0]) eq "CODE") {
	    &_carp($self, "ignoring bad Errmode argument: " .
		   "first list item isn't a code ref");
	    $errmode = *$self->{net_telnet}{errormode};
	}
    }
    else {
	&_carp($self, "ignoring bad Errmode argument \"$errmode\"");
	$errmode = *$self->{net_telnet}{errormode};
    }

    $errmode;
} # end sub _parse_errmode


sub _parse_input_record_separator {
    my ($self, $rs) = @_;

    unless (defined $rs and length $rs) {
	&_carp($self, "ignoring null Input_record_separator argument");
	$rs = *$self->{net_telnet}{rs};
    }

    $rs;
} # end sub _parse_input_record_separator


sub _parse_prompt {
    my ($self, $prompt) = @_;

    unless (defined $prompt) {
	$prompt = "";
    }

    unless ($prompt =~ m(^\s*/) or $prompt =~ m(^\s*m\s*\W)) {
	&_carp($self, "ignoring bad Prompt argument \"$prompt\": " .
	       "missing opening delimiter of match operator");
	$prompt = *$self->{net_telnet}{cmd_prompt};
    }

    $prompt;
} # end sub _parse_prompt


sub _parse_timeout {
    my ($self, $timeout) = @_;

    ## Ensure valid timeout.
    if (defined $timeout) {
	## Test for non-numeric or negative values.
	eval {
	    local $SIG{"__DIE__"} = "DEFAULT";
	    local $SIG{"__WARN__"} = sub { die "non-numeric\n" };
	    local $^W = 1;
	    $timeout *= 1;
	};
	if ($@) {  # timeout arg is non-numeric
	    &_carp($self,
		   "ignoring non-numeric Timeout argument \"$timeout\"");
	    $timeout = *$self->{net_telnet}{time_out};
	}
	elsif ($timeout < 0) {  # timeout arg is negative
	    &_carp($self, "ignoring negative Timeout argument \"$timeout\"");
	    $timeout = *$self->{net_telnet}{time_out};
	}
    }

    $timeout;
} # end sub _parse_timeout


sub _put {
    my ($self, $buf, $subname) = @_;
    my (
	$endtime,
	$len,
	$nfound,
	$nwrote,
	$offset,
	$ready,
	$s,
	$timed_out,
	$timeout,
	$zero_wrote_count,
	);

    ## Init.
    $s = *$self->{net_telnet};
    $s->{num_wrote} = 0;
    $zero_wrote_count = 0;
    $offset = 0;
    $len = length $$buf;
    $endtime = &_endtime($s->{time_out});

    return $self->error("write error: filehandle isn't open")
	unless $s->{opened};

    ## Try to send any waiting option negotiation.
    if (length $s->{unsent_opts}) {
	&_flush_opts($self);
    }

    ## Write until all data blocks written.
    while ($len) {
	## Determine how long to wait for output ready.
	($timed_out, $timeout) = &_timeout_interval($endtime);
	if ($timed_out) {
	    $s->{timedout} = 1;
	    return $self->error("$subname timed-out");
	}

	## Wait for output ready.
	$nfound = select "", $ready=$s->{fdmask}, "", $timeout;

	## Handle any errors while waiting.
	if (!defined $nfound or $nfound <= 0) {  # output not ready
	    if (defined $nfound and $nfound == 0) {  # timed-out
		$s->{timedout} = 1;
		return $self->error("$subname timed-out");
	    }
	    else {  # error waiting for output ready
		next if $! =~ /^interrupted/i;

		$s->{opened} = '';
		return $self->error("write error: $!");
	    }
	}

	## Write the data.
	$nwrote = syswrite $self, $$buf, $len, $offset;

	## Handle any write errors.
	if (!defined $nwrote) {  # write failed
	    next if $! =~ /^interrupted/i;  # restart interrupted syscall

	    $s->{opened} = '';
	    return $self->error("write error: $!");
	}
	elsif ($nwrote == 0) {  # zero chars written
	    ## Try ten more times to write the data.
	    if ($zero_wrote_count++ <= 10) {
		&_sleep(0.01);
		next;
	    }

	    $s->{opened} = '';
	    return $self->error("write error: zero length write: $!");
	}

	## Display network traffic if requested.
	if ($s->{dumplog}) {
	    &_log_dump('>', $s->{dumplog}, $buf, $offset, $nwrote);
	}

	## Increment.
	$s->{num_wrote} += $nwrote;
	$offset += $nwrote;
	$len -= $nwrote;
    }

    1;
} # end sub _put


sub _qualify_fh {
    my ($obj, $name) = @_;
    my (
	$user_class,
	);
    local $_;

    ## Get user's package name.
    ($user_class) = &_user_caller($obj);

    ## Ensure name is qualified with a package name.
    $name = qualify($name, $user_class);

    ## If it's not already, make it a typeglob ref.
    if (!ref $name) {
	no strict;
	local $^W = 0;

	$name =~ s/^\*+//;
	$name = eval "\\*$name";
	return unless ref $name;
    }

    $name;
} # end sub _qualify_fh


sub _reset_options {
    my ($opts) = @_;
    my (
	$opt,
	);

    foreach $opt (keys %$opts) {
	$opts->{$opt}{remote_enabled} = '';
	$opts->{$opt}{remote_state} = "no";
	$opts->{$opt}{local_enabled} = '';
	$opts->{$opt}{local_state} = "no";
    }

    1;
} # end sub _reset_options


sub _save_lastline {
    my ($s) = @_;
    my (
	$firstpos,
	$lastpos,
	$len_w_sep,
	$len_wo_sep,
	$offset,
	);
    my $rs = "\n";

    if (($lastpos = rindex $s->{buf}, $rs) > -1) {  # eol found
	while (1) {
	    ## Find beginning of line.
	    $firstpos = rindex $s->{buf}, $rs, $lastpos - 1;
	    if ($firstpos == -1) {
		$offset = 0;
	    }
	    else {
		$offset = $firstpos + length $rs;
	    }

	    ## Determine length of line with and without separator.
	    $len_wo_sep = $lastpos - $offset;
	    $len_w_sep = $len_wo_sep + length $rs;

	    ## Save line if it's not blank.
	    if (substr($s->{buf}, $offset, $len_wo_sep)
		!~ /^\s*$/)
	    {
		$s->{last_line} = substr($s->{buf},
					 $offset,
					 $len_w_sep);
		last;
	    }

	    last if $firstpos == -1;

	    $lastpos = $firstpos;
	}
    }

    1;
} # end sub _save_lastline


sub _set_default_option {
    my ($s, $option) = @_;

    $s->{opts}{$option} = {
	remote_enabled   => '',
	remote_state     => "no",
	remote_enable_ok => '',
	local_enabled    => '',
	local_state      => "no",
	local_enable_ok  => '',
    };
} # end sub _set_default_option


sub _sleep {
    my ($secs) = @_;
    my $bitmask = "";
    local *SOCK;

    socket SOCK, AF_INET, SOCK_STREAM, 0;
    vec($bitmask, fileno(SOCK), 1) = 1;
    select $bitmask, "", "", $secs;
    CORE::close SOCK;

    1;
} # end sub _sleep


sub _timeout_interval {
    my ($endtime) = @_;
    my (
	$timeout,
	);

    ## Return timed-out boolean and timeout interval.
    if (defined $endtime) {
	## Is it a one-time poll.
	return ('', 0) if $endtime == 0;

	## Calculate the timeout interval.
	$timeout = $endtime - time;

	## Did we already timeout.
	return (1, 0) unless $timeout > 0;

	return ('', $timeout);
    }
    else {  # there is no timeout
	return ('', undef);
    }
} # end sub _timeout_interval


sub _user_caller {
    my ($obj) = @_;
    my (
	$class,
	$curr_pkg,
	$file,
	$i,
	$line,
	$pkg,
	%isa,
	@isa,
	);
    local $_;

    ## Create a boolean hash to test for isa.  Make sure current
    ## package and the object's class are members.
    $class = ref $obj;
    @isa = eval "\@${class}::ISA";
    push @isa, $class;
    ($curr_pkg) = caller 1;
    push @isa, $curr_pkg;
    %isa = map { $_ => 1 } @isa;

    ## Search back in call frames for a package that's not in isa.
    $i = 1;
    while (($pkg, $file, $line) = caller ++$i) {
	next if $isa{$pkg};

	return ($pkg, $file, $line);
    }

    ## If not found, choose outer most call frame.
    ($pkg, $file, $line) = caller --$i;
    return ($pkg, $file, $line);
} # end sub _user_caller


sub _verify_telopt_arg {
    my ($self, $option, $argname) = @_;

    ## If provided, use argument name in error message.
    if (defined $argname) {
	$argname = "for arg $argname";
    }
    else {
	$argname = "";
    }

    ## Ensure telnet option is a non-negative integer.
    eval {
	local $SIG{"__DIE__"} = "DEFAULT";
	local $SIG{"__WARN__"} = sub { die "non-numeric\n" };
	local $^W = 1;
	$option = abs(int $option);
    };
    return $self->error("bad telnet option $argname: non-numeric")
	if $@;

    return $self->error("bad telnet option $argname: option > 255")
	unless $option <= 255;

    $option;
} # end sub _verify_telopt_arg


######################## Exported Constants ##########################


sub TELNET_IAC ()	    {255}; # interpret as command:
sub TELNET_DONT	()	    {254}; # you are not to use option
sub TELNET_DO ()	    {253}; # please, you use option
sub TELNET_WONT ()	    {252}; # I won't use option
sub TELNET_WILL ()	    {251}; # I will use option
sub TELNET_SB ()	    {250}; # interpret as subnegotiation
sub TELNET_GA ()	    {249}; # you may reverse the line
sub TELNET_EL ()	    {248}; # erase the current line
sub TELNET_EC ()	    {247}; # erase the current character
sub TELNET_AYT ()	    {246}; # are you there
sub TELNET_AO ()	    {245}; # abort output--but let prog finish
sub TELNET_IP ()	    {244}; # interrupt process--permanently
sub TELNET_BREAK ()	    {243}; # break
sub TELNET_DM ()	    {242}; # data mark--for connect. cleaning
sub TELNET_NOP ()	    {241}; # nop
sub TELNET_SE ()	    {240}; # end sub negotiation
sub TELNET_EOR ()	    {239}; # end of record (transparent mode)
sub TELNET_ABORT ()	    {238}; # Abort process
sub TELNET_SUSP ()	    {237}; # Suspend process
sub TELNET_EOF ()	    {236}; # End of file
sub TELNET_SYNCH ()	    {242}; # for telfunc calls

sub TELOPT_BINARY ()	      {0}; # Binary Transmission
sub TELOPT_ECHO ()	      {1}; # Echo
sub TELOPT_RCP ()	      {2}; # Reconnection
sub TELOPT_SGA ()	      {3}; # Suppress Go Ahead
sub TELOPT_NAMS ()	      {4}; # Approx Message Size Negotiation
sub TELOPT_STATUS ()	      {5}; # Status
sub TELOPT_TM ()	      {6}; # Timing Mark
sub TELOPT_RCTE ()	      {7}; # Remote Controlled Trans and Echo
sub TELOPT_NAOL ()	      {8}; # Output Line Width
sub TELOPT_NAOP ()	      {9}; # Output Page Size
sub TELOPT_NAOCRD ()	     {10}; # Output Carriage-Return Disposition
sub TELOPT_NAOHTS ()	     {11}; # Output Horizontal Tab Stops
sub TELOPT_NAOHTD ()	     {12}; # Output Horizontal Tab Disposition
sub TELOPT_NAOFFD ()	     {13}; # Output Formfeed Disposition
sub TELOPT_NAOVTS ()	     {14}; # Output Vertical Tabstops
sub TELOPT_NAOVTD ()	     {15}; # Output Vertical Tab Disposition
sub TELOPT_NAOLFD ()	     {16}; # Output Linefeed Disposition
sub TELOPT_XASCII ()	     {17}; # Extended ASCII
sub TELOPT_LOGOUT ()	     {18}; # Logout
sub TELOPT_BM ()	     {19}; # Byte Macro
sub TELOPT_DET ()	     {20}; # Data Entry Terminal
sub TELOPT_SUPDUP ()	     {21}; # SUPDUP
sub TELOPT_SUPDUPOUTPUT ()   {22}; # SUPDUP Output
sub TELOPT_SNDLOC ()	     {23}; # Send Location
sub TELOPT_TTYPE ()	     {24}; # Terminal Type
sub TELOPT_EOR ()	     {25}; # End of Record
sub TELOPT_TUID ()	     {26}; # TACACS User Identification
sub TELOPT_OUTMRK ()	     {27}; # Output Marking
sub TELOPT_TTYLOC ()	     {28}; # Terminal Location Number
sub TELOPT_3270REGIME ()     {29}; # Telnet 3270 Regime
sub TELOPT_X3PAD ()	     {30}; # X.3 PAD
sub TELOPT_NAWS ()	     {31}; # Negotiate About Window Size
sub TELOPT_TSPEED ()	     {32}; # Terminal Speed
sub TELOPT_LFLOW ()	     {33}; # Remote Flow Control
sub TELOPT_LINEMODE ()	     {34}; # Linemode
sub TELOPT_XDISPLOC ()	     {35}; # X Display Location
sub TELOPT_OLD_ENVIRON ()    {36}; # Environment Option
sub TELOPT_AUTHENTICATION () {37}; # Authentication Option
sub TELOPT_ENCRYPT ()	     {38}; # Encryption Option
sub TELOPT_NEW_ENVIRON ()    {39}; # New Environment Option
sub TELOPT_EXOPL ()	    {255}; # Extended-Options-List


1;
__END__;


######################## User Documentation ##########################


## To format the following documentation into a more readable format,
## use one of these programs: perldoc; pod2man; pod2html; pod2text.
## For example, to nicely format this documentation for printing, you
## may use pod2man and groff to convert to postscript:
##   pod2man Net/Telnet.pm | groff -man -Tps > Net::Telnet.ps

=head1 NAME

Net::Telnet - interact with TELNET port or other TCP ports

=head1 SYNOPSIS

C<use Net::Telnet ();>

see METHODS section below

=head1 DESCRIPTION

Net::Telnet allows you to make client connections to a TCP port and do
network I/O, especially to a port using the TELNET protocol.  Simple
I/O methods such as print, get, and getline are provided.  More
sophisticated interactive features are provided because connecting to
a TELNET port ultimately means communicating with a program designed
for human interaction.  These interactive features include the ability
to specify a time-out and to wait for patterns to appear in the input
stream, such as the prompt from a shell.

Other reasons to use this module than strictly with a TELNET port are:

=over 2

=item *

You're not familiar with sockets and you want a simple way to make
client connections to TCP services.

=item *

You want to be able to specify your own time-out while connecting,
reading, or writing.

=item *

You're communicating with an interactive program at the other end of
some socket or pipe and you want to wait for certain patterns to
appear.

=back

Here's an example that prints who's logged-on to the remote host
sparky.  In addition to a username and password, you must also know
the user's shell prompt, which for this example is C<bash$>

    use Net::Telnet ();
    $t = new Net::Telnet (Timeout => 10,
                          Prompt => '/bash\$ $/');
    $t->open("sparky");
    $t->login($username, $passwd);
    @lines = $t->cmd("who");
    print @lines;

More examples are in the B<EXAMPLES> section below.

Usage questions should be directed to the Usenet newsgroup
comp.lang.perl.modules.

Contact me, Jay Rogers <jay@rgrs.com>, if you find any bugs or have
suggestions for improvement.

=head2 What To Know Before Using

=over 2

=item *

All output is flushed while all input is buffered.  Each object
contains its own input buffer.

=item *

The output record separator for C<print()> and C<cmd()> is set to
C<"\n"> by default, so that you don't have to append all your commands
with a newline.  To avoid printing a trailing C<"\n"> use C<put()> or
set the I<output_record_separator> to C<"">.

=item *

The methods C<login()> and C<cmd()> use the I<prompt> setting in the
object to determine when a login or remote command is complete.  Those
methods will fail with a time-out if you don't set the prompt
correctly.

=item *

Use a combination of C<print()> and C<waitfor()> as an alternative to
C<login()> or C<cmd()> when they don't do what you want.

=item *

Errors such as timing-out are handled according to the error mode
action.  The default action is to print an error message to standard
error and have the program die.  See the C<errmode()> method for more
information.

=item *

When constructing the match operator argument for C<prompt()> or
C<waitfor()>, always use single quotes instead of double quotes to
avoid unexpected backslash interpretation (e.g. C<'/bash\$ $/'>).  If
you're constructing a DOS like file path, you'll need to use four
backslashes to represent one (e.g. C<'/c:\\\\users\\\\billE<gt>$/i'>).

Of course don't forget about regexp metacharacters like C<.>, C<[>, or
C<$>.  You'll only need a single backslash to quote them.  The anchor
metacharacters C<^> and C<$> refer to positions in the input buffer.
To avoid matching characters read that look like a prompt, it's a good
idea to end your prompt pattern with the C<$> anchor.  That way the
prompt will only match if it's the last thing read.

=item *

In the input stream, each sequence of I<carriage return> and I<line
feed> (i.e. C<"\015\012"> or CR LF) is converted to C<"\n">.  In the
output stream, each occurrence of C<"\n"> is converted to a sequence
of CR LF.  See C<binmode()> to change the behavior.  TCP protocols
typically use the ASCII sequence, carriage return and line feed to
designate a newline.

=item *

Timing-out while making a connection is disabled for machines that
don't support the C<alarm()> function.  Most notably these include
MS-Windows machines.

=item *

You'll need to be running at least Perl version 5.002 to use this
module.  This module does not require any libraries that don't already
come with a standard Perl distribution.

If you have the IO:: libraries installed (they come standard with
perl5.004 and later) then IO::Socket::INET is used as a base class,
otherwise FileHandle is used.

=item *

Contact me, Jay Rogers <jay@rgrs.com>, if you find any bugs or have
suggestions for improvement.

=back

=head2 Debugging

The typical usage bug causes a time-out error because you've made
incorrect assumptions about what the remote side actually sends.  The
easiest way to reconcile what the remote side sends with your
expectations is to use C<input_log()> or C<dump_log()>.

C<dump_log()> allows you to see the data being sent from the remote
side before any translation is done, while C<input_log()> shows you
the results after translation.  The translation includes converting
end of line characters, removing and responding to TELNET protocol
commands in the data stream.

=head2 Style of Named Parameters

Two different styles of named parameters are supported.  This document
only shows the IO:: style:

    Net::Telnet->new(Timeout => 20);

however the dash-option style is also allowed:

    Net::Telnet->new(-timeout => 20);

=head2 Connecting to a Remote MS-Windows Machine

By default MS-Windows doesn't come with a TELNET server.  However
third party TELNET servers are available.  Unfortunately many of these
servers falsely claim to be a TELNET server.  This is especially true
of the so-called "Microsoft Telnet Server" that comes installed with
some newer versions MS-Windows.

When a TELNET server first accepts a connection, it must use the ASCII
control characters carriage-return and line-feed to start a new line
(see RFC854).  A server like the "Microsoft Telnet Server" that
doesn't do this, isn't a TELNET server.  These servers send ANSI
terminal escape sequences to position to a column on a subsequent line
and to even position while writing characters that are adjacent to
each other.  Worse, when sending output these servers resend
previously sent command output in a misguided attempt to display an
entire terminal screen.

Connecting Net::Telnet to one of these false TELNET servers makes your
job of parsing command output very difficult.  It's better to replace
a false TELNET server with a real TELNET server.  The better TELNET
servers for MS-Windows allow you to avoid the ANSI escapes by turning
off something some of them call I<console mode>.


=head1 METHODS

In the calling sequences below, square brackets B<[]> represent
optional parameters.

=over 4

=item B<new> - create a new Net::Telnet object

    $obj = new Net::Telnet ([$host]);

    $obj = new Net::Telnet ([Binmode    => $mode,]
                            [Cmd_remove_mode => $mode,]
                            [Dump_Log   => $filename,]
                            [Errmode    => $errmode,]
                            [Fhopen     => $filehandle,]
                            [Host       => $host,]
                            [Input_log  => $file,]
                            [Input_record_separator => $chars,]
                            [Option_log => $file,]
                            [Ors        => $chars,]
                            [Output_log => $file,]
                            [Output_record_separator => $chars,]
                            [Port       => $port,]
                            [Prompt     => $matchop,]
                            [Rs         => $chars,]
                            [Telnetmode => $mode,]
                            [Timeout    => $secs,]);

This is the constructor for Net::Telnet objects.  A new object is
returned on success, the error mode action is performed on failure -
see C<errmode()>.  The optional arguments are short-cuts to methods of
the same name.

If the I<$host> argument is given then the object is opened by
connecting to TCP I<$port> on I<$host>.  Also see C<open()>.  The new
object returned is given the following defaults in the absence of
corresponding named parameters:

=over 4

=item

The default I<Host> is C<"localhost">

=item

The default I<Port> is C<23>

=item

The default I<Prompt> is C<'/[\$%#E<gt>] $/'>

=item

The default I<Timeout> is C<10>

=item

The default I<Errmode> is C<"die">

=item

The default I<Output_record_separator> is C<"\n">.  Note that I<Ors>
is synonymous with I<Output_record_separator>.

=item

The default I<Input_record_separator> is C<"\n">.  Note that I<Rs> is
synonymous with I<Input_record_separator>.

=item

The default I<Binmode> is C<0>, which means do newline translation.

=item

The default I<Telnetmode> is C<1>, which means respond to TELNET
commands in the data stream.

=item

The default I<Cmd_remove_mode> is C<"auto">

=item

The defaults for I<Dump_log>, I<Input_log>, I<Option_log>, and
I<Output_log> are C<"">, which means that logging is turned-off.

=back

=back


=over 4

=item B<binmode> - toggle newline translation

    $mode = $obj->binmode;

    $prev = $obj->binmode($mode);

This method controls whether or not sequences of carriage returns and
line feeds (CR LF or more specifically C<"\015\012">) are translated.
By default they are translated (i.e. binmode is C<0>).

If no argument is given, the current mode is returned.

If I<$mode> is C<1> then binmode is I<on> and newline translation is
not done.

If I<$mode> is C<0> then binmode is I<off> and newline translation is
done.  In the input stream, each sequence of CR LF is converted to
C<"\n"> and in the output stream, each occurrence of C<"\n"> is
converted to a sequence of CR LF.

Note that input is always buffered.  Changing binmode doesn't effect
what's already been read into the buffer.  Output is not buffered and
changing binmode will have an immediate effect.

=back


=over 4

=item B<break> - send TELNET break character

    $ok = $obj->break;

This method sends the TELNET break character.  This character is
provided because it's a signal outside the ASCII character set which
is currently given local meaning within many systems.  It's intended
to indicate that the Break Key or the Attention Key was hit.

This method returns C<1> on success, or performs the error mode action
on failure.

=back


=over 4

=item B<buffer> - scalar reference to object's input buffer

    $ref = $obj->buffer;

This method returns a scalar reference to the input buffer for
I<$obj>.  Data in the input buffer is data that has been read from the
remote side but has yet to be read by the user.  Modifications to the
input buffer are returned by a subsequent read.

=back


=over 4

=item B<buffer_empty> - discard all data in object's input buffer

    $obj->buffer_empty;

This method removes all data in the input buffer for I<$obj>.

=back


=over 4

=item B<close> - close object

    $ok = $obj->close;

This method closes the socket, file, or pipe associated with the
object.  It always returns a value of C<1>.

=back


=over 4

=item B<cmd> - issue command and retrieve output

    $ok = $obj->cmd($string);
    $ok = $obj->cmd(String   => $string,
                    [Output  => $ref,]
                    [Cmd_remove_mode => $mode,]
                    [Errmode => $mode,]
                    [Input_record_separator => $chars,]
                    [Ors     => $chars,]
                    [Output_record_separator => $chars,]
                    [Prompt  => $match,]
                    [Rs      => $chars,]
                    [Timeout => $secs,]);

    @output = $obj->cmd($string);
    @output = $obj->cmd(String   => $string,
                        [Output  => $ref,]
                        [Cmd_remove_mode => $mode,]
                        [Errmode => $mode,]
                        [Input_record_separator => $chars,]
                        [Ors     => $chars,]
                        [Output_record_separator => $chars,]
                        [Prompt  => $match,]
                        [Rs      => $chars,]
                        [Timeout => $secs,]);

This method sends the command I<$string>, and reads the characters
sent back by the command up until and including the matching prompt.
It's assumed that the program to which you're sending is some kind of
command prompting interpreter such as a shell.

The command I<$string> is automatically appended with the
output_record_separator, By default that's C<"\n">.  This is similar
to someone typing a command and hitting the return key.  Set the
output_record_separator to change this behavior.

In a scalar context, the characters read from the remote side are
discarded and C<1> is returned on success.  On time-out, eof, or other
failures, the error mode action is performed.  See C<errmode()>.

In a list context, just the output generated by the command is
returned, one line per element.  In other words, all the characters in
between the echoed back command string and the prompt are returned.
If the command happens to return no output, a list containing one
element, the empty string is returned.  This is so the list will
indicate true in a boolean context.  On time-out, eof, or other
failures, the error mode action is performed.  See C<errmode()>.

The characters that matched the prompt may be retrieved using
C<last_prompt()>.

Many command interpreters echo back the command sent.  In most
situations, this method removes the first line returned from the
remote side (i.e. the echoed back command).  See C<cmd_remove_mode()>
for more control over this feature.

Use C<dump_log()> to debug when this method keeps timing-out and you
don't think it should.

Consider using a combination of C<print()> and C<waitfor()> as an
alternative to this method when it doesn't do what you want, e.g. the
command you send prompts for input.

The I<Output> named parameter provides an alternative method of
receiving command output.  If you pass a scalar reference, all the
output (even if it contains multiple lines) is returned in the
referenced scalar.  If you pass an array or hash reference, the lines
of output are returned in the referenced array or hash.  You can use
C<input_record_separator()> to change the notion of what separates a
line.

Optional named parameters are provided to override the current
settings of cmd_remove_mode, errmode, input_record_separator, ors,
output_record_separator, prompt, rs, and timeout.  Rs is synonymous
with input_record_separator and ors is synonymous with
output_record_separator.

=back


=over 4

=item B<cmd_remove_mode> - toggle removal of echoed commands

    $mode = $obj->cmd_remove_mode;

    $prev = $obj->cmd_remove_mode($mode);

This method controls how to deal with echoed back commands in the
output returned by cmd().  Typically, when you send a command to the
remote side, the first line of output returned is the command echoed
back.  Use this mode to remove the first line of output normally
returned by cmd().

If no argument is given, the current mode is returned.

If I<$mode> is C<0> then the command output returned from cmd() has no
lines removed.  If I<$mode> is a positive integer, then the first
I<$mode> lines of command output are stripped.

By default, I<$mode> is set to C<"auto">.  Auto means that whether or
not the first line of command output is stripped, depends on whether
or not the remote side offered to echo.  By default, Net::Telnet
always accepts an offer to echo by the remote side.  You can change
the default to reject such an offer using C<option_accept()>.

A warning is printed to STDERR when attempting to set this attribute
to something that's not C<"auto"> or a non-negative integer.

=back


=over 4

=item B<dump_log> - log all I/O in dump format

    $fh = $obj->dump_log;

    $fh = $obj->dump_log($fh);

    $fh = $obj->dump_log($filename);

This method starts or stops dump format logging of all the object's
input and output.  The dump format shows the blocks read and written
in a hexadecimal and printable character format.  This method is
useful when debugging, however you might want to first try
C<input_log()> as it's more readable.

If no argument is given, the current log filehandle is returned.  An
empty string indicates logging is off.

To stop logging, use an empty string as an argument.

If an open filehandle is given, it is used for logging and returned.
Otherwise, the argument is assumed to be the name of a file, the file
is opened and a filehandle to it is returned.  If the file can't be
opened for writing, the error mode action is performed.

=back


=over 4

=item B<eof> - end of file indicator

    $eof = $obj->eof;

This method returns C<1> if end of file has been read, otherwise it
returns an empty string.  Because the input is buffered this isn't the
same thing as I<$obj> has closed.  In other words I<$obj> can be
closed but there still can be stuff in the buffer to be read.  Under
this condition you can still read but you won't be able to write.

=back


=over 4

=item B<errmode> - define action to be performed on error

    $mode = $obj->errmode;

    $prev = $obj->errmode($mode);

This method gets or sets the action used when errors are encountered
using the object.  The first calling sequence returns the current
error mode.  The second calling sequence sets it to I<$mode> and
returns the previous mode.  Valid values for I<$mode> are C<"die">
(the default), C<"return">, a I<coderef>, or an I<arrayref>.

When mode is C<"die"> and an error is encountered using the object,
then an error message is printed to standard error and the program
dies.

When mode is C<"return"> then the method generating the error places
an error message in the object and returns an undefined value in a
scalar context and an empty list in list context.  The error message
may be obtained using C<errmsg()>.

When mode is a I<coderef>, then when an error is encountered
I<coderef> is called with the error message as its first argument.
Using this mode you may have your own subroutine handle errors.  If
I<coderef> itself returns then the method generating the error returns
undefined or an empty list depending on context.

When mode is an I<arrayref>, the first element of the array must be a
I<coderef>.  Any elements that follow are the arguments to I<coderef>.
When an error is encountered, the I<coderef> is called with its
arguments.  Using this mode you may have your own subroutine handle
errors.  If the I<coderef> itself returns then the method generating
the error returns undefined or an empty list depending on context.

A warning is printed to STDERR when attempting to set this attribute
to something that's not C<"die">, C<"return">, a I<coderef>, or an
I<arrayref> whose first element isn't a I<coderef>.

=back


=over 4

=item B<errmsg> - most recent error message

    $msg = $obj->errmsg;

    $prev = $obj->errmsg(@msgs);

The first calling sequence returns the error message associated with
the object.  The empty string is returned if no error has been
encountered yet.  The second calling sequence sets the error message
for the object to the concatenation of I<@msgs> and returns the
previous error message.  Normally, error messages are set internally
by a method when an error is encountered.

=back


=over 4

=item B<error> - perform the error mode action

    $obj->error(@msgs);

This method concatenates I<@msgs> into a string and places it in the
object as the error message.  Also see C<errmsg()>.  It then performs
the error mode action.  Also see C<errmode()>.

If the error mode doesn't cause the program to die, then an undefined
value or an empty list is returned depending on the context.

This method is primarily used by this class or a sub-class to perform
the user requested action when an error is encountered.

=back


=over 4

=item B<fhopen> - use already open filehandle for I/O

    $ok = $obj->fhopen($fh);

This method associates the open filehandle I<$fh> with I<$obj> for
further I/O.  Filehandle I<$fh> must already be opened.

Suppose you want to use the features of this module to do I/O to
something other than a TCP port, for example STDIN or a filehandle
opened to read from a process.  Instead of opening the object for I/O
to a TCP port by using C<open()> or C<new()>, call this method
instead.

The value C<1> is returned success, the error mode action is performed
on failure.

=back


=over 4

=item B<get> - read block of data

    $data = $obj->get([Binmode    => $mode,]
                      [Errmode    => $errmode,]
                      [Telnetmode => $mode,]
                      [Timeout    => $secs,]);

This method reads a block of data from the object and returns it along
with any buffered data.  If no buffered data is available to return,
it will wait for data to read using the timeout specified in the
object.  You can override that timeout using I<$secs>.  Also see
C<timeout()>.  If buffered data is available to return, it also checks
for a block of data that can be immediately read.

On eof an undefined value is returned.  On time-out or other failures,
the error mode action is performed.  To distinguish between eof or an
error occurring when the error mode is not set to C<"die">, use
C<eof()>.

Optional named parameters are provided to override the current
settings of binmode, errmode, telnetmode, and timeout.

=back


=over 4

=item B<getline> - read next line

    $line = $obj->getline([Binmode    => $mode,]
                          [Errmode    => $errmode,]
                          [Input_record_separator => $chars,]
                          [Rs         => $chars,]
                          [Telnetmode => $mode,]
                          [Timeout    => $secs,]);

This method reads and returns the next line of data from the object.
You can use C<input_record_separator()> to change the notion of what
separates a line.  The default is C<"\n">.  If a line isn't
immediately available, this method blocks waiting for a line or a
time-out.

On eof an undefined value is returned.  On time-out or other failures,
the error mode action is performed.  To distinguish between eof or an
error occurring when the error mode is not set to C<"die">, use
C<eof()>.

Optional named parameters are provided to override the current
settings of binmode, errmode, input_record_separator, rs, telnetmode,
and timeout.  Rs is synonymous with input_record_separator.

=back


=over 4

=item B<getlines> - read next lines

    @lines = $obj->getlines([Binmode    => $mode,]
                            [Errmode    => $errmode,]
                            [Input_record_separator => $chars,]
                            [Rs         => $chars,]
                            [Telnetmode => $mode,]
                            [Timeout    => $secs,]
                            [All        => $boolean,]);

This method reads and returns all the lines of data from the object
until end of file is read.  You can use C<input_record_separator()> to
change the notion of what separates a line.  The default is C<"\n">.
A time-out error occurs if all the lines can't be read within the
time-out interval.  See C<timeout()>.

The behavior of this method was changed in version 3.03.  Prior to
version 3.03 this method returned just the lines available from the
next read.  To get that old behavior, use the optional named parameter
I<All> and set I<$boolean> to C<""> or C<0>.

If only eof is read then an empty list is returned.  On time-out or
other failures, the error mode action is performed.  Use C<eof()> to
distinguish between reading only eof or an error occurring when the
error mode is not set to C<"die">.

Optional named parameters are provided to override the current
settings of binmode, errmode, input_record_separator, rs, telnetmode,
and timeout.  Rs is synonymous with input_record_separator.

=back


=over 4

=item B<host> - name of remote host

    $host = $obj->host;

    $prev = $obj->host($host);

This method designates the remote host for C<open()>.  With no
argument it returns the current host name set in the object.  With an
argument it sets the current host name to I<$host> and returns the
previous host name.  You may indicate the remote host using either a
hostname or an IP address.

The default value is C<"localhost">.  It may also be set by C<open()>
or C<new()>.

=back


=over 4

=item B<input_log> - log all input

    $fh = $obj->input_log;

    $fh = $obj->input_log($fh);

    $fh = $obj->input_log($filename);

This method starts or stops logging of input.  This is useful when
debugging.  Also see C<dump_log()>.  Because most command interpreters
echo back commands received, it's likely all your output will also be
in this log.  Note that input logging occurs after newline
translation.  See C<binmode()> for details on newline translation.

If no argument is given, the log filehandle is returned.  An empty
string indicates logging is off.

To stop logging, use an empty string as an argument.

If an open filehandle is given, it is used for logging and returned.
Otherwise, the argument is assumed to be the name of a file, the file
is opened for logging and a filehandle to it is returned.  If the file
can't be opened for writing, the error mode action is performed.

=back


=over 4

=item B<input_record_separator> - input line delimiter

    $chars = $obj->input_record_separator;

    $prev = $obj->input_record_separator($chars);

This method designates the line delimiter for input.  It's used with
C<getline()>, C<getlines()>, and C<cmd()> to determine lines in the
input.

With no argument this method returns the current input record
separator set in the object.  With an argument it sets the input
record separator to I<$chars> and returns the previous value.  Note
that I<$chars> must have length.

A warning is printed to STDERR when attempting to set this attribute
to a string with no length.

=back


=over 4

=item B<last_prompt> - last prompt read

    $string = $obj->last_prompt;

    $prev = $obj->last_prompt($string);

With no argument this method returns the last prompt read by cmd() or
login().  See C<prompt()>.  With an argument it sets the last prompt
read to I<$string> and returns the previous value.  Normally, only
internal methods set the last prompt.

=back


=over 4

=item B<lastline> - last line read

    $line = $obj->lastline;

    $prev = $obj->lastline($line);

This method retrieves the last line read from the object.  This may be
a useful error message when the remote side abnormally closes the
connection.  Typically the remote side will print an error message
before closing.

With no argument this method returns the last line read from the
object.  With an argument it sets the last line read to I<$line> and
returns the previous value.  Normally, only internal methods set the
last line.

=back


=over 4

=item B<login> - perform standard login

    $ok = $obj->login($username, $password);

    $ok = $obj->login(Name     => $username,
                      Password => $password,
                      [Errmode => $mode,]
                      [Prompt  => $match,]
                      [Timeout => $secs,]);

This method performs a standard login by waiting for a login prompt
and responding with I<$username>, then waiting for the password prompt
and responding with I<$password>, and then waiting for the command
interpreter prompt.  If any of those prompts sent by the remote side
don't match what's expected, this method will time-out, unless timeout
is turned off.

Login prompt must match either of these case insensitive patterns:

    /login[: ]*$/i
    /username[: ]*$/i

Password prompt must match this case insensitive pattern:

    /password[: ]*$/i

The command interpreter prompt must match the current setting of
prompt.  See C<prompt()>.

Use C<dump_log()> to debug when this method keeps timing-out and you
don't think it should.

Consider using a combination of C<print()> and C<waitfor()> as an
alternative to this method when it doesn't do what you want, e.g. the
remote host doesn't prompt for a username.

On success, C<1> is returned.  On time out, eof, or other failures,
the error mode action is performed.  See C<errmode()>.

Optional named parameters are provided to override the current
settings of errmode, prompt, and timeout.

=back


=over 4

=item B<max_buffer_length> - maximum size of input buffer

    $len = $obj->max_buffer_length;

    $prev = $obj->max_buffer_length($len);

This method designates the maximum size of the input buffer.  An error
is generated when a read causes the buffer to exceed this limit.  The
default value is 1,048,576 bytes (1MB).  The input buffer can grow
much larger than the block size when you continuously read using
C<getline()> or C<waitfor()> and the data stream contains no newlines
or matching waitfor patterns.

With no argument, this method returns the current maximum buffer
length set in the object.  With an argument it sets the maximum buffer
length to I<$len> and returns the previous value.  Values of I<$len>
smaller than 512 will be adjusted to 512.

A warning is printed to STDERR when attempting to set this attribute
to something that isn't a positive integer.

=back


=over 4

=item B<ofs> - field separator for print

    $chars = $obj->ofs

    $prev = $obj->ofs($chars);

This method is synonymous with C<output_field_separator()>.

=back


=over 4

=item B<open> - connect to port on remote host

    $ok = $obj->open($host);

    $ok = $obj->open([Host    => $host,]
                     [Port    => $port,]
                     [Errmode => $mode,]
                     [Timeout => $secs,]);

This method opens a TCP connection to I<$port> on I<$host>.  If either
argument is missing then the current value of C<host()> or C<port()>
is used.  Optional named parameters are provided to override the
current setting of errmode and timeout.

On success C<1> is returned.  On time-out or other connection
failures, the error mode action is performed.  See C<errmode()>.

Time-outs don't work for this method on machines that don't implement
SIGALRM - most notably MS-Windows machines.  For those machines, an
error is returned when the system reaches its own time-out while
trying to connect.

A side effect of this method is to reset the alarm interval associated
with SIGALRM.

=back


=over 4

=item B<option_accept> - indicate willingness to accept a TELNET option

    $fh = $obj->option_accept([Do   => $telopt,]
                              [Dont => $telopt,]
                              [Will => $telopt,]
                              [Wont => $telopt,]);

This method is used to indicate whether to accept or reject an offer
to enable a TELNET option made by the remote side.  If you're using
I<Do> or I<Will> to indicate a willingness to enable, then a
notification callback must have already been defined by a prior call
to C<option_callback()>.  See C<option_callback()> for details on
receiving enable/disable notification of a TELNET option.

You can give multiple I<Do>, I<Dont>, I<Will>, or I<Wont> arguments
for different TELNET options in the same call to this method.

The following example describes the meaning of the named parameters.
A TELNET option, such as C<TELOPT_ECHO> used below, is an integer
constant that you can import from Net::Telnet.  See the source in file
Telnet.pm for the complete list.

=over 4

=item

I<Do> => C<TELOPT_ECHO>

=over 4

=item

we'll accept an offer to enable the echo option on the local side

=back

=item

I<Dont> => C<TELOPT_ECHO>

=over 4

=item

we'll reject an offer to enable the echo option on the local side

=back

=item

I<Will> => C<TELOPT_ECHO>

=over 4

=item

we'll accept an offer to enable the echo option on the remote side

=back

=item

I<Wont> => C<TELOPT_ECHO>

=over 4

=item

we'll reject an offer to enable the echo option on the remote side

=back

=back

=item

Use C<option_send()> to send a request to the remote side to enable or
disable a particular TELNET option.

=back


=over 4

=item B<option_callback> - define the option negotiation callback

    $coderef = $obj->option_callback;

    $prev = $obj->option_callback($coderef);

This method defines the callback subroutine that's called when a
TELNET option is enabled or disabled.  Once defined, the
I<option_callback> may not be undefined.  However, calling this method
with a different I<$coderef> changes it.

A warning is printed to STDERR when attempting to set this attribute
to something that isn't a coderef.

Here are the circumstances that invoke I<$coderef>:

=over 4

=item

An option becomes enabled because the remote side requested an enable
and C<option_accept()> had been used to arrange that it be accepted.

=item

The remote side arbitrarily decides to disable an option that is
currently enabled.  Note that Net::Telnet always accepts a request to
disable from the remote side.

=item

C<option_send()> was used to send a request to enable or disable an
option and the response from the remote side has just been received.
Note, that if a request to enable is rejected then I<$coderef> is
still invoked even though the option didn't change.

=back

=item

Here are the arguments passed to I<&$coderef>:

    &$coderef($obj, $option, $is_remote,
              $is_enabled, $was_enabled, $buf_position);

=over 4

=item

1.  I<$obj> is the Net::Telnet object

=item

2.  I<$option> is the TELNET option.  Net::Telnet exports constants
for the various TELNET options which just equate to an integer.

=item

3.  I<$is_remote> is a boolean indicating for which side the option
applies.

=item

4.  I<$is_enabled> is a boolean indicating the option is enabled or
disabled

=item

5.  I<$was_enabled> is a boolean indicating the option was previously
enabled or disabled

=item

6.  I<$buf_position> is an integer indicating the position in the
object's input buffer where the option takes effect.  See C<buffer()>
to access the object's input buffer.

=back

=back


=over 4

=item B<option_log> - log all TELNET options sent or received

    $fh = $obj->option_log;

    $fh = $obj->option_log($fh);

    $fh = $obj->option_log($filename);

This method starts or stops logging of all TELNET options being sent
or received.  This is useful for debugging when you send options via
C<option_send()> or you arrange to accept option requests from the
remote side via C<option_accept()>.  Also see C<dump_log()>.

If no argument is given, the log filehandle is returned.  An empty
string indicates logging is off.

To stop logging, use an empty string as an argument.

If an open filehandle is given, it is used for logging and returned.
Otherwise, the argument is assumed to be the name of a file, the file
is opened for logging and a filehandle to it is returned.  If the file
can't be opened for writing, the error mode action is performed.

=back


=over 4

=item B<option_send> - send TELNET option negotiation request

    $ok = $obj->option_send([Do    => $telopt,]
                            [Dont  => $telopt,]
                            [Will  => $telopt,]
                            [Wont  => $telopt,]
                            [Async => $boolean,]);

This method is not yet implemented.  Look for it in a future version.

=back


=over 4

=item B<option_state> - get current state of a TELNET option

    $hashref = $obj->option_state($telopt);

This method returns a hashref containing a copy of the current state
of TELNET option I<$telopt>.

Here are the values returned in the hash:

=over 4

=item

I<$hashref>->{remote_enabled}

=over 4

=item

boolean that indicates if the option is enabled on the remote side.

=back

=item

I<$hashref>->{remote_enable_ok}

=over 4

=item

boolean that indicates if it's ok to accept an offer to enable this
option on the remote side.

=back

=item

I<$hashref>->{remote_state}

=over 4

=item

string used to hold the internal state of option negotiation for this
option on the remote side.

=back

=item

I<$hashref>->{local_enabled}

=over 4

=item

boolean that indicates if the option is enabled on the local side.

=back

=item

I<$hashref>->{local_enable_ok}

=over 4

=item

boolean that indicates if it's ok to accept an offer to enable this
option on the local side.

=back

=item

I<$hashref>->{local_state}

=over 4

=item

string used to hold the internal state of option negotiation for this
option on the local side.

=back

=back

=back


=over 4

=item B<ors> - output line delimiter

    $chars = $obj->ors;

    $prev = $obj->ors($chars);

This method is synonymous with C<output_record_separator()>.

=back


=over 4

=item B<output_field_separator> - field separator for print

    $chars = $obj->output_field_separator;

    $prev = $obj->output_field_separator($chars);

This method designates the output field separator for C<print()>.
Ordinarily the print method simply prints out the comma separated
fields you specify.  Set this to specify what's printed between
fields.

With no argument this method returns the current output field
separator set in the object.  With an argument it sets the output
field separator to I<$chars> and returns the previous value.

By default it's set to an empty string.

=back


=over 4

=item B<output_log> - log all output

    $fh = $obj->output_log;

    $fh = $obj->output_log($fh);

    $fh = $obj->output_log($filename);

This method starts or stops logging of output.  This is useful when
debugging.  Also see C<dump_log()>.  Because most command interpreters
echo back commands received, it's likely all your output would also be
in an input log.  See C<input_log()>.  Note that output logging occurs
before newline translation.  See C<binmode()> for details on newline
translation.

If no argument is given, the log filehandle is returned.  An empty
string indicates logging is off.

To stop logging, use an empty string as an argument.

If an open filehandle is given, it is used for logging and returned.
Otherwise, the argument is assumed to be the name of a file, the file
is opened for logging and a filehandle to it is returned.  If the file
can't be opened for writing, the error mode action is performed.

=back


=over 4

=item B<output_record_separator> - output line delimiter

    $chars = $obj->output_record_separator;

    $prev = $obj->output_record_separator($chars);

This method designates the output line delimiter for C<print()> and
C<cmd()>.  Set this to specify what's printed at the end of C<print()>
and C<cmd()>.

The output record separator is set to C<"\n"> by default, so there's
no need to append all your commands with a newline.  To avoid printing
the output_record_separator use C<put()> or set the
output_record_separator to an empty string.

With no argument this method returns the current output record
separator set in the object.  With an argument it sets the output
record separator to I<$chars> and returns the previous value.

=back


=over 4

=item B<port> - remote port

    $port = $obj->port;

    $prev = $obj->port($port);

This method designates the remote TCP port.  With no argument this
method returns the current port number.  With an argument it sets the
current port number to I<$port> and returns the previous port.  If
I<$port> is a TCP service name, then it's first converted to a port
number using the perl function C<getservbyname()>.

The default value is C<23>.  It may also be set by C<open()> or
C<new()>.

A warning is printed to STDERR when attempting to set this attribute
to something that's not a positive integer or a valid TCP service
name.

=back


=over 4

=item B<print> - write to object

    $ok = $obj->print(@list);

This method writes I<@list> followed by the I<output_record_separator>
to the open object and returns C<1> if all data was successfully
written.  On time-out or other failures, the error mode action is
performed.  See C<errmode()>.

By default, the C<output_record_separator()> is set to C<"\n"> so all
your commands automatically end with a newline.  In most cases your
output is being read by a command interpreter which won't accept a
command until newline is read.  This is similar to someone typing a
command and hitting the return key.  To avoid printing a trailing
C<"\n"> use C<put()> instead or set the output_record_separator to an
empty string.

On failure, it's possible that some data was written.  If you choose
to try and recover from a print timing-out, use C<print_length()> to
determine how much was written before the error occurred.

You may also use the output field separator to print a string between
the list elements.  See C<output_field_separator()>.

=back


=over 4

=item B<print_length> - number of bytes written by print

    $num = $obj->print_length;

This returns the number of bytes successfully written by the most
recent C<print()> or C<put()>.

=back


=over 4

=item B<prompt> - pattern to match a prompt

    $matchop = $obj->prompt;

    $prev = $obj->prompt($matchop);

This method sets the pattern used to find a prompt in the input
stream.  It must be a string representing a valid perl pattern match
operator.  The methods C<login()> and C<cmd()> try to read until
matching the prompt.  They will fail with a time-out error if the
pattern you've chosen doesn't match what the remote side sends.

With no argument this method returns the prompt set in the object.
With an argument it sets the prompt to I<$matchop> and returns the
previous value.

The default prompt is C<'/[\$%#E<gt>] $/'>

Always use single quotes, instead of double quotes, to construct
I<$matchop> (e.g. C<'/bash\$ $/'>).  If you're constructing a DOS like
file path, you'll need to use four backslashes to represent one
(e.g. C<'/c:\\\\users\\\\billE<gt>$/i'>).

Of course don't forget about regexp metacharacters like C<.>, C<[>, or
C<$>.  You'll only need a single backslash to quote them.  The anchor
metacharacters C<^> and C<$> refer to positions in the input buffer.

A warning is printed to STDERR when attempting to set this attribute
with a match operator missing its opening delimiter.

=back


=over 4

=item B<put> - write to object

    $ok = $obj->put($string);

    $ok = $obj->put(String      => $string,
                    [Binmode    => $mode,]
                    [Errmode    => $errmode,]
                    [Telnetmode => $mode,]
                    [Timeout    => $secs,]);

This method writes I<$string> to the opened object and returns C<1> if
all data was successfully written.  This method is like C<print()>
except that it doesn't write the trailing output_record_separator
("\n" by default).  On time-out or other failures, the error mode
action is performed.  See C<errmode()>.

On failure, it's possible that some data was written.  If you choose
to try and recover from a put timing-out, use C<print_length()> to
determine how much was written before the error occurred.

Optional named parameters are provided to override the current
settings of binmode, errmode, telnetmode, and timeout.

=back


=over 4

=item B<rs> - input line delimiter

    $chars = $obj->rs;

    $prev = $obj->rs($chars);

This method is synonymous with C<input_record_separator()>.

=back


=over 4

=item B<telnetmode> - turn off/on telnet command interpretation

    $mode = $obj->telnetmode;

    $prev = $obj->telnetmode($mode);

This method controls whether or not TELNET commands in the data stream
are recognized and handled.  The TELNET protocol uses certain
character sequences sent in the data stream to control the session.
If the port you're connecting to isn't using the TELNET protocol, then
you should turn this mode off.  The default is I<on>.

If no argument is given, the current mode is returned.

If I<$mode> is C<0> then telnet mode is off.  If I<$mode> is C<1> then
telnet mode is on.

=back


=over 4

=item B<timed_out> - time-out indicator

    $boolean = $obj->timed_out;

    $prev = $obj->timed_out($boolean);

This method indicates if a previous read, write, or open method
timed-out.  Remember that timing-out is itself an error.  To be able
to invoke C<timed_out()> after a time-out error, you'd have to change
the default error mode to something other than C<"die">.  See
C<errmode()>.

With no argument this method returns C<1> if the previous method
timed-out.  With an argument it sets the indicator.  Normally, only
internal methods set this indicator.

=back


=over 4

=item B<timeout> - I/O time-out interval

    $secs = $obj->timeout;

    $prev = $obj->timeout($secs);

This method sets the timeout interval that's used when performing I/O
or connecting to a port.  When a method doesn't complete within the
timeout interval then it's an error and the error mode action is
performed.

A timeout may be expressed as a relative or absolute value.  If
I<$secs> is greater than or equal to the time the program started, as
determined by $^T, then it's an absolute time value for when time-out
occurs.  The perl function C<time()> may be used to obtain an absolute
time value.  For a relative time-out value less than $^T, time-out
happens I<$secs> from when the method begins.

If I<$secs> is C<0> then time-out occurs if the data cannot be
immediately read or written.  Use the undefined value to turn off
timing-out completely.

With no argument this method returns the timeout set in the object.
With an argument it sets the timeout to I<$secs> and returns the
previous value.  The default timeout value is C<10> seconds.

A warning is printed to STDERR when attempting to set this attribute
to something that's not an C<undef> or a non-negative integer.

=back


=over 4

=item B<waitfor> - wait for pattern in the input

    $ok = $obj->waitfor($matchop);
    $ok = $obj->waitfor([Match      => $matchop,]
                        [String     => $string,]
                        [Binmode    => $mode,]
                        [Errmode    => $errmode,]
                        [Telnetmode => $mode,]
                        [Timeout    => $secs,]);

    ($prematch, $match) = $obj->waitfor($matchop);
    ($prematch, $match) = $obj->waitfor([Match      => $matchop,]
                                        [String     => $string,]
                                        [Binmode    => $mode,]
                                        [Errmode    => $errmode,]
                                        [Telnetmode => $mode,]
                                        [Timeout    => $secs,]);

This method reads until a pattern match or string is found in the
input stream.  All the characters before and including the match are
removed from the input stream.

In a list context the characters before the match and the matched
characters are returned in I<$prematch> and I<$match>.  In a scalar
context, the matched characters and all characters before it are
discarded and C<1> is returned on success.  On time-out, eof, or other
failures, for both list and scalar context, the error mode action is
performed.  See C<errmode()>.

You can specify more than one pattern or string by simply providing
multiple I<Match> and/or I<String> named parameters.  A I<$matchop>
must be a string representing a valid Perl pattern match operator.
The I<$string> is just a substring to find in the input stream.

Use C<dump_log()> to debug when this method keeps timing-out and you
don't think it should.

An optional named parameter is provided to override the current
setting of timeout.

To avoid unexpected backslash interpretation, always use single quotes
instead of double quotes to construct a match operator argument for
C<prompt()> and C<waitfor()> (e.g. C<'/bash\$ $/'>).  If you're
constructing a DOS like file path, you'll need to use four backslashes
to represent one (e.g. C<'/c:\\\\users\\\\billE<gt>$/i'>).

Of course don't forget about regexp metacharacters like C<.>, C<[>, or
C<$>.  You'll only need a single backslash to quote them.  The anchor
metacharacters C<^> and C<$> refer to positions in the input buffer.

Optional named parameters are provided to override the current
settings of binmode, errmode, telnetmode, and timeout.

=back


=head1 SEE ALSO

=over 2

=item RFC 854

S<TELNET Protocol Specification>

S<ftp://ftp.isi.edu/in-notes/rfc854.txt>

=item RFC 1143

S<Q Method of Implementing TELNET Option Negotiation>

S<ftp://ftp.isi.edu/in-notes/rfc1143.txt>

=item TELNET Option Assignments

S<http://www.iana.org/assignments/telnet-options>

=back


=head1 EXAMPLES

This example gets the current weather forecast for Brainerd, Minnesota.

    my ($forecast, $t);

    use Net::Telnet ();
    $t = new Net::Telnet;
    $t->open("rainmaker.wunderground.com");

    ## Wait for first prompt and "hit return".
    $t->waitfor('/continue:.*$/');
    $t->print("");

    ## Wait for second prompt and respond with city code.
    $t->waitfor('/city code.*$/');
    $t->print("BRD");

    ## Read and print the first page of forecast.
    ($forecast) = $t->waitfor('/[ \t]+press return to continue/i');
    print $forecast;

    exit;


This example checks a POP server to see if you have mail.

    my ($hostname, $line, $passwd, $pop, $username);

    $hostname = "your_destination_host_here";
    $username = "your_username_here";
    $passwd = "your_password_here";

    use Net::Telnet ();
    $pop = new Net::Telnet (Telnetmode => 0);
    $pop->open(Host => $hostname,
               Port => 110);


    ## Read connection message.
    $line = $pop->getline;
    die $line unless $line =~ /^\+OK/;

    ## Send user name.
    $pop->print("user $username");
    $line = $pop->getline;
    die $line unless $line =~ /^\+OK/;

    ## Send password.
    $pop->print("pass $passwd");
    $line = $pop->getline;
    die $line unless $line =~ /^\+OK/;

    ## Request status of messages.
    $pop->print("list");
    $line = $pop->getline;
    print $line;

    exit;


Here's an example that uses the ssh program to connect to a remote
host.  Because the ssh program reads and writes to its controlling
terminal, the IO::Pty module is used to create a new pseudo terminal
for use by ssh.  A new Net::Telnet object is then created to read and
write to that pseudo terminal.  To use the code below, substitute
"changeme" with the actual host, user, password, and command prompt.

    ## Main program.
    {
        my ($pty, $ssh, @lines);
        my $host = "changeme";
        my $user = "changeme";
        my $password = "changeme";
        my $prompt = '/changeme:~> $/';

        ## Start ssh program.
        $pty = &spawn("ssh", "-l", $user, $host);  # spawn() defined below

        ## Create a Net::Telnet object to perform I/O on ssh's tty.
        use Net::Telnet;
        $ssh = new Net::Telnet (-fhopen => $pty,
                                -prompt => $prompt,
                                -telnetmode => 0,
                                -cmd_remove_mode => 1,
                                -output_record_separator => "\r");

        ## Login to remote host.
        $ssh->waitfor(-match => '/password: ?$/i',
                      -errmode => "return")
            or die "problem connecting to host: ", $ssh->lastline;
        $ssh->print($password);
        $ssh->waitfor(-match => $ssh->prompt,
                      -errmode => "return")
            or die "login failed: ", $ssh->lastline;

        ## Send command, get and print its output.
        @lines = $ssh->cmd("who");
        print @lines;

        exit;
    } # end main program

    sub spawn {
        my(@cmd) = @_;
        my($pid, $pty, $tty, $tty_fd);

        ## Create a new pseudo terminal.
        use IO::Pty ();
        $pty = new IO::Pty
            or die $!;

        ## Execute the program in another process.
        unless ($pid = fork) {  # child process
            die "problem spawning program: $!\n" unless defined $pid;

            ## Disassociate process from existing controlling terminal.
            use POSIX ();
            POSIX::setsid
                or die "setsid failed: $!";

            ## Associate process with a new controlling terminal.
            $tty = $pty->slave;
            $tty_fd = $tty->fileno;
            close $pty;

            ## Make stdio use the new controlling terminal.
            open STDIN, "<&$tty_fd" or die $!;
            open STDOUT, ">&$tty_fd" or die $!;
            open STDERR, ">&STDOUT" or die $!;
            close $tty;

            ## Execute requested program.
            exec @cmd
                or die "problem executing $cmd[0]\n";
        } # end child process

        $pty;
    } # end sub spawn


Here's an example that changes a user's login password.  Because the
passwd program always prompts for passwords on its controlling
terminal, the IO::Pty module is used to create a new pseudo terminal
for use by passwd.  A new Net::Telnet object is then created to read
and write to that pseudo terminal.  To use the code below, substitute
"changeme" with the actual old and new passwords.

    my ($pty, $passwd);
    my $oldpw = "changeme";
    my $newpw = "changeme";

    ## Start passwd program.
    $pty = &spawn("passwd");  # spawn() defined above

    ## Create a Net::Telnet object to perform I/O on passwd's tty.
    use Net::Telnet;
    $passwd = new Net::Telnet (-fhopen => $pty,
                               -timeout => 2,
                               -output_record_separator => "\r",
                               -telnetmode => 0,
                               -cmd_remove_mode => 1);
    $passwd->errmode("return");

    ## Send existing password.
    $passwd->waitfor('/password: ?$/i')
        or die "no old password prompt: ", $passwd->lastline;
    $passwd->print($oldpw);

    ## Send new password.
    $passwd->waitfor('/new password: ?$/i')
        or die "bad old password: ", $passwd->lastline;
    $passwd->print($newpw);

    ## Send new password verification.
    $passwd->waitfor('/new password: ?$/i')
        or die "bad new password: ", $passwd->lastline;
    $passwd->print($newpw);

    ## Display success or failure.
    $passwd->waitfor('/changed/')
        or die "bad new password: ", $passwd->lastline;
    print $passwd->lastline;

    $passwd->close;
    exit;


Here's an example you can use to down load a file of any type.  The
file is read from the remote host's standard output using cat.  To
prevent any output processing, the remote host's standard output is
put in raw mode using the Bourne shell.  The Bourne shell is used
because some shells, notably tcsh, prevent changing tty modes.  Upon
completion, FTP style statistics are printed to stderr.

    my ($block, $filename, $host, $hostname, $k_per_sec, $line,
        $num_read, $passwd, $prevblock, $prompt, $size, $size_bsd,
        $size_sysv, $start_time, $total_time, $username);

    $hostname = "your_destination_host_here";
    $username = "your_username_here";
    $passwd = "your_password_here";
    $filename = "your_download_file_here";

    ## Connect and login.
    use Net::Telnet ();
    $host = new Net::Telnet (Timeout => 30,
                             Prompt => '/[%#>] $/');
    $host->open($hostname);
    $host->login($username, $passwd);

    ## Make sure prompt won't match anything in send data.
    $prompt = "_funkyPrompt_";
    $host->prompt("/$prompt\$/");
    $host->cmd("set prompt = '$prompt'");

    ## Get size of file.
    ($line) = $host->cmd("/bin/ls -l $filename");
    ($size_bsd, $size_sysv) = (split ' ', $line)[3,4];
    if ($size_sysv =~ /^\d+$/) {
        $size = $size_sysv;
    }
    elsif ($size_bsd =~ /^\d+$/) {
        $size = $size_bsd;
    }
    else {
        die "$filename: no such file on $hostname";
    }

    ## Start sending the file.
    binmode STDOUT;
    $host->binmode(1);
    $host->print("/bin/sh -c 'stty raw; cat $filename'");
    $host->getline;    # discard echoed back line

    ## Read file a block at a time.
    $num_read = 0;
    $prevblock = "";
    $start_time = time;
    while (($block = $host->get) and ($block !~ /$prompt$/o)) {
        if (length $block >= length $prompt) {
            print $prevblock;
            $num_read += length $prevblock;
            $prevblock = $block;
        }
        else {
            $prevblock .= $block;
        }

    }
    $host->close;

    ## Print last block without trailing prompt.
    $prevblock .= $block;
    $prevblock =~ s/$prompt$//;
    print $prevblock;
    $num_read += length $prevblock;
    die "error: expected size $size, received size $num_read\n"
        unless $num_read == $size;

    ## Print totals.
    $total_time = (time - $start_time) || 1;
    $k_per_sec = ($size / 1024) / $total_time;
    $k_per_sec = sprintf "%3.1f", $k_per_sec;
    warn("$num_read bytes received in $total_time seconds ",
         "($k_per_sec Kbytes/s)\n");

    exit;


=head1 AUTHOR

Jay Rogers <jay@rgrs.com>


=head1 COPYRIGHT

Copyright 1997, 2000, 2002 by Jay Rogers.  All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.
