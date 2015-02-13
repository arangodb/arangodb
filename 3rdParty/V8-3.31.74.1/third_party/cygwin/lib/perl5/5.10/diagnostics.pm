package diagnostics;

=head1 NAME

diagnostics, splain - produce verbose warning diagnostics

=head1 SYNOPSIS

Using the C<diagnostics> pragma:

    use diagnostics;
    use diagnostics -verbose;

    enable  diagnostics;
    disable diagnostics;

Using the C<splain> standalone filter program:

    perl program 2>diag.out
    splain [-v] [-p] diag.out

Using diagnostics to get stack traces from a misbehaving script:

    perl -Mdiagnostics=-traceonly my_script.pl

=head1 DESCRIPTION

=head2 The C<diagnostics> Pragma

This module extends the terse diagnostics normally emitted by both the
perl compiler and the perl interpreter (from running perl with a -w 
switch or C<use warnings>), augmenting them with the more
explicative and endearing descriptions found in L<perldiag>.  Like the
other pragmata, it affects the compilation phase of your program rather
than merely the execution phase.

To use in your program as a pragma, merely invoke

    use diagnostics;

at the start (or near the start) of your program.  (Note 
that this I<does> enable perl's B<-w> flag.)  Your whole
compilation will then be subject(ed :-) to the enhanced diagnostics.
These still go out B<STDERR>.

Due to the interaction between runtime and compiletime issues,
and because it's probably not a very good idea anyway,
you may not use C<no diagnostics> to turn them off at compiletime.
However, you may control their behaviour at runtime using the 
disable() and enable() methods to turn them off and on respectively.

The B<-verbose> flag first prints out the L<perldiag> introduction before
any other diagnostics.  The $diagnostics::PRETTY variable can generate nicer
escape sequences for pagers.

Warnings dispatched from perl itself (or more accurately, those that match
descriptions found in L<perldiag>) are only displayed once (no duplicate
descriptions).  User code generated warnings a la warn() are unaffected,
allowing duplicate user messages to be displayed.

This module also adds a stack trace to the error message when perl dies.
This is useful for pinpointing what caused the death. The B<-traceonly> (or
just B<-t>) flag turns off the explanations of warning messages leaving just
the stack traces. So if your script is dieing, run it again with

  perl -Mdiagnostics=-traceonly my_bad_script

to see the call stack at the time of death. By supplying the B<-warntrace>
(or just B<-w>) flag, any warnings emitted will also come with a stack
trace.

=head2 The I<splain> Program

While apparently a whole nuther program, I<splain> is actually nothing
more than a link to the (executable) F<diagnostics.pm> module, as well as
a link to the F<diagnostics.pod> documentation.  The B<-v> flag is like
the C<use diagnostics -verbose> directive.
The B<-p> flag is like the
$diagnostics::PRETTY variable.  Since you're post-processing with 
I<splain>, there's no sense in being able to enable() or disable() processing.

Output from I<splain> is directed to B<STDOUT>, unlike the pragma.

=head1 EXAMPLES

The following file is certain to trigger a few errors at both
runtime and compiletime:

    use diagnostics;
    print NOWHERE "nothing\n";
    print STDERR "\n\tThis message should be unadorned.\n";
    warn "\tThis is a user warning";
    print "\nDIAGNOSTIC TESTER: Please enter a <CR> here: ";
    my $a, $b = scalar <STDIN>;
    print "\n";
    print $x/$y;

If you prefer to run your program first and look at its problem
afterwards, do this:

    perl -w test.pl 2>test.out
    ./splain < test.out

Note that this is not in general possible in shells of more dubious heritage, 
as the theoretical 

    (perl -w test.pl >/dev/tty) >& test.out
    ./splain < test.out

Because you just moved the existing B<stdout> to somewhere else.

If you don't want to modify your source code, but still have on-the-fly
warnings, do this:

    exec 3>&1; perl -w test.pl 2>&1 1>&3 3>&- | splain 1>&2 3>&- 

Nifty, eh?

If you want to control warnings on the fly, do something like this.
Make sure you do the C<use> first, or you won't be able to get
at the enable() or disable() methods.

    use diagnostics; # checks entire compilation phase 
	print "\ntime for 1st bogus diags: SQUAWKINGS\n";
	print BOGUS1 'nada';
	print "done with 1st bogus\n";

    disable diagnostics; # only turns off runtime warnings
	print "\ntime for 2nd bogus: (squelched)\n";
	print BOGUS2 'nada';
	print "done with 2nd bogus\n";

    enable diagnostics; # turns back on runtime warnings
	print "\ntime for 3rd bogus: SQUAWKINGS\n";
	print BOGUS3 'nada';
	print "done with 3rd bogus\n";

    disable diagnostics;
	print "\ntime for 4th bogus: (squelched)\n";
	print BOGUS4 'nada';
	print "done with 4th bogus\n";

=head1 INTERNALS

Diagnostic messages derive from the F<perldiag.pod> file when available at
runtime.  Otherwise, they may be embedded in the file itself when the
splain package is built.   See the F<Makefile> for details.

If an extant $SIG{__WARN__} handler is discovered, it will continue
to be honored, but only after the diagnostics::splainthis() function 
(the module's $SIG{__WARN__} interceptor) has had its way with your
warnings.

There is a $diagnostics::DEBUG variable you may set if you're desperately
curious what sorts of things are being intercepted.

    BEGIN { $diagnostics::DEBUG = 1 } 


=head1 BUGS

Not being able to say "no diagnostics" is annoying, but may not be
insurmountable.

The C<-pretty> directive is called too late to affect matters.
You have to do this instead, and I<before> you load the module.

    BEGIN { $diagnostics::PRETTY = 1 } 

I could start up faster by delaying compilation until it should be
needed, but this gets a "panic: top_level" when using the pragma form
in Perl 5.001e.

While it's true that this documentation is somewhat subserious, if you use
a program named I<splain>, you should expect a bit of whimsy.

=head1 AUTHOR

Tom Christiansen <F<tchrist@mox.perl.com>>, 25 June 1995.

=cut

use strict;
use 5.009001;
use Carp;
$Carp::Internal{__PACKAGE__.""}++;

our $VERSION = 1.17;
our $DEBUG;
our $VERBOSE;
our $PRETTY;
our $TRACEONLY = 0;
our $WARNTRACE = 0;

use Config;
my($privlib, $archlib) = @Config{qw(privlibexp archlibexp)};
if ($^O eq 'VMS') {
    require VMS::Filespec;
    $privlib = VMS::Filespec::unixify($privlib);
    $archlib = VMS::Filespec::unixify($archlib);
}
my @trypod = (
	   "$archlib/pod/perldiag.pod",
	   "$privlib/pod/perldiag-$Config{version}.pod",
	   "$privlib/pod/perldiag.pod",
	   "$archlib/pods/perldiag.pod",
	   "$privlib/pods/perldiag-$Config{version}.pod",
	   "$privlib/pods/perldiag.pod",
	  );
# handy for development testing of new warnings etc
unshift @trypod, "./pod/perldiag.pod" if -e "pod/perldiag.pod";
(my $PODFILE) = ((grep { -e } @trypod), $trypod[$#trypod])[0];

if ($^O eq 'MacOS') {
    # just updir one from each lib dir, we'll find it ...
    ($PODFILE) = grep { -e } map { "$_:pod:perldiag.pod" } @INC;
}


$DEBUG ||= 0;
my $WHOAMI = ref bless [];  # nobody's business, prolly not even mine

local $| = 1;
my $_;

my $standalone;
my(%HTML_2_Troff, %HTML_2_Latin_1, %HTML_2_ASCII_7);

CONFIG: {
    our $opt_p = our $opt_d = our $opt_v = our $opt_f = '';

    unless (caller) {
	$standalone++;
	require Getopt::Std;
	Getopt::Std::getopts('pdvf:')
	    or die "Usage: $0 [-v] [-p] [-f splainpod]";
	$PODFILE = $opt_f if $opt_f;
	$DEBUG = 2 if $opt_d;
	$VERBOSE = $opt_v;
	$PRETTY = $opt_p;
    }

    if (open(POD_DIAG, $PODFILE)) {
	warn "Happy happy podfile from real $PODFILE\n" if $DEBUG;
	last CONFIG;
    } 

    if (caller) {
	INCPATH: {
	    for my $file ( (map { "$_/$WHOAMI.pm" } @INC), $0) {
		warn "Checking $file\n" if $DEBUG;
		if (open(POD_DIAG, $file)) {
		    while (<POD_DIAG>) {
			next unless
			    /^__END__\s*# wish diag dbase were more accessible/;
			print STDERR "podfile is $file\n" if $DEBUG;
			last INCPATH;
		    }
		}
	    } 
	}
    } else { 
	print STDERR "podfile is <DATA>\n" if $DEBUG;
	*POD_DIAG = *main::DATA;
    }
}
if (eof(POD_DIAG)) { 
    die "couldn't find diagnostic data in $PODFILE @INC $0";
}


%HTML_2_Troff = (
    'amp'	=>	'&',	#   ampersand
    'lt'	=>	'<',	#   left chevron, less-than
    'gt'	=>	'>',	#   right chevron, greater-than
    'quot'	=>	'"',	#   double quote

    "Aacute"	=>	"A\\*'",	#   capital A, acute accent
    # etc

);

%HTML_2_Latin_1 = (
    'amp'	=>	'&',	#   ampersand
    'lt'	=>	'<',	#   left chevron, less-than
    'gt'	=>	'>',	#   right chevron, greater-than
    'quot'	=>	'"',	#   double quote

    "Aacute"	=>	"\xC1"	#   capital A, acute accent

    # etc
);

%HTML_2_ASCII_7 = (
    'amp'	=>	'&',	#   ampersand
    'lt'	=>	'<',	#   left chevron, less-than
    'gt'	=>	'>',	#   right chevron, greater-than
    'quot'	=>	'"',	#   double quote

    "Aacute"	=>	"A"	#   capital A, acute accent
    # etc
);

our %HTML_Escapes;
*HTML_Escapes = do {
    if ($standalone) {
	$PRETTY ? \%HTML_2_Latin_1 : \%HTML_2_ASCII_7; 
    } else {
	\%HTML_2_Latin_1; 
    }
}; 

*THITHER = $standalone ? *STDOUT : *STDERR;

my %transfmt = (); 
my $transmo = <<EOFUNC;
sub transmo {
    #local \$^W = 0;  # recursive warnings we do NOT need!
    study;
EOFUNC

my %msg;
{
    print STDERR "FINISHING COMPILATION for $_\n" if $DEBUG;
    local $/ = '';
    my $header;
    my $for_item;
    while (<POD_DIAG>) {

	unescape();
	if ($PRETTY) {
	    sub noop   { return $_[0] }  # spensive for a noop
	    sub bold   { my $str =$_[0];  $str =~ s/(.)/$1\b$1/g; return $str; } 
	    sub italic { my $str = $_[0]; $str =~ s/(.)/_\b$1/g;  return $str; } 
	    s/C<<< (.*?) >>>|C<< (.*?) >>|[BC]<(.*?)>/bold($+)/ges;
	    s/[LIF]<(.*?)>/italic($1)/ges;
	} else {
	    s/C<<< (.*?) >>>|C<< (.*?) >>|[BC]<(.*?)>/$+/gs;
	    s/[LIF]<(.*?)>/$1/gs;
	} 
	unless (/^=/) {
	    if (defined $header) { 
		if ( $header eq 'DESCRIPTION' && 
		    (   /Optional warnings are enabled/ 
		     || /Some of these messages are generic./
		    ) )
		{
		    next;
		}
		s/^/    /gm;
		$msg{$header} .= $_;
	 	undef $for_item;	
	    }
	    next;
	} 
	unless ( s/=item (.*?)\s*\z//) {

	    if ( s/=head1\sDESCRIPTION//) {
		$msg{$header = 'DESCRIPTION'} = '';
		undef $for_item;
	    }
	    elsif( s/^=for\s+diagnostics\s*\n(.*?)\s*\z// ) {
		$for_item = $1;
	    } 
	    next;
	}

	if( $for_item ) { $header = $for_item; undef $for_item } 
	else {
	    $header = $1;
	    while( $header =~ /[;,]\z/ ) {
		<POD_DIAG> =~ /^\s*(.*?)\s*\z/;
		$header .= ' '.$1;
	    }
	}

	# strip formatting directives from =item line
	$header =~ s/[A-Z]<(.*?)>/$1/g;

        my @toks = split( /(%l?[dx]|%c|%(?:\.\d+)?s)/, $header );
	if (@toks > 1) {
            my $conlen = 0;
            for my $i (0..$#toks){
                if( $i % 2 ){
                    if(      $toks[$i] eq '%c' ){
                        $toks[$i] = '.';
                    } elsif( $toks[$i] eq '%d' ){
                        $toks[$i] = '\d+';
                    } elsif( $toks[$i] eq '%s' ){
                        $toks[$i] = $i == $#toks ? '.*' : '.*?';
                    } elsif( $toks[$i] =~ '%.(\d+)s' ){
                        $toks[$i] = ".{$1}";
                     } elsif( $toks[$i] =~ '^%l*x$' ){
                        $toks[$i] = '[\da-f]+';
                   }
                } elsif( length( $toks[$i] ) ){
                    $toks[$i] = quotemeta $toks[$i];
                    $conlen += length( $toks[$i] );
                }
            }  
            my $lhs = join( '', @toks );
	    $transfmt{$header}{pat} =
              "    s{^$lhs}\n     {\Q$header\E}s\n\t&& return 1;\n";
            $transfmt{$header}{len} = $conlen;
	} else {
            $transfmt{$header}{pat} =
	      "    m{^\Q$header\E} && return 1;\n";
            $transfmt{$header}{len} = length( $header );
	} 

	print STDERR "$WHOAMI: Duplicate entry: \"$header\"\n"
	    if $msg{$header};

	$msg{$header} = '';
    } 


    close POD_DIAG unless *main::DATA eq *POD_DIAG;

    die "No diagnostics?" unless %msg;

    # Apply patterns in order of decreasing sum of lengths of fixed parts
    # Seems the best way of hitting the right one.
    for my $hdr ( sort { $transfmt{$b}{len} <=> $transfmt{$a}{len} }
                  keys %transfmt ){
        $transmo .= $transfmt{$hdr}{pat};
    }
    $transmo .= "    return 0;\n}\n";
    print STDERR $transmo if $DEBUG;
    eval $transmo;
    die $@ if $@;
}

if ($standalone) {
    if (!@ARGV and -t STDIN) { print STDERR "$0: Reading from STDIN\n" } 
    while (defined (my $error = <>)) {
	splainthis($error) || print THITHER $error;
    } 
    exit;
} 

my $olddie;
my $oldwarn;

sub import {
    shift;
    $^W = 1; # yup, clobbered the global variable; 
	     # tough, if you want diags, you want diags.
    return if defined $SIG{__WARN__} && ($SIG{__WARN__} eq \&warn_trap);

    for (@_) {

	/^-d(ebug)?$/ 	   	&& do {
				    $DEBUG++;
				    next;
				   };

	/^-v(erbose)?$/ 	&& do {
				    $VERBOSE++;
				    next;
				   };

	/^-p(retty)?$/ 		&& do {
				    print STDERR "$0: I'm afraid it's too late for prettiness.\n";
				    $PRETTY++;
				    next;
			       };
	# matches trace and traceonly for legacy doc mixup reasons
	/^-t(race(only)?)?$/	&& do {
				    $TRACEONLY++;
				    next;
			       };
	/^-w(arntrace)?$/ 	&& do {
				    $WARNTRACE++;
				    next;
			       };

	warn "Unknown flag: $_";
    } 

    $oldwarn = $SIG{__WARN__};
    $olddie = $SIG{__DIE__};
    $SIG{__WARN__} = \&warn_trap;
    $SIG{__DIE__} = \&death_trap;
} 

sub enable { &import }

sub disable {
    shift;
    return unless $SIG{__WARN__} eq \&warn_trap;
    $SIG{__WARN__} = $oldwarn || '';
    $SIG{__DIE__} = $olddie || '';
} 

sub warn_trap {
    my $warning = $_[0];
    if (caller eq $WHOAMI or !splainthis($warning)) {
	if ($WARNTRACE) {
	    print STDERR Carp::longmess($warning);
	} else {
	    print STDERR $warning;
	}
    } 
    goto &$oldwarn if defined $oldwarn and $oldwarn and $oldwarn ne \&warn_trap;
};

sub death_trap {
    my $exception = $_[0];

    # See if we are coming from anywhere within an eval. If so we don't
    # want to explain the exception because it's going to get caught.
    my $in_eval = 0;
    my $i = 0;
    while (my $caller = (caller($i++))[3]) {
      if ($caller eq '(eval)') {
	$in_eval = 1;
	last;
      }
    }

    splainthis($exception) unless $in_eval;
    if (caller eq $WHOAMI) { print STDERR "INTERNAL EXCEPTION: $exception"; } 
    &$olddie if defined $olddie and $olddie and $olddie ne \&death_trap;

    return if $in_eval;

    # We don't want to unset these if we're coming from an eval because
    # then we've turned off diagnostics.

    # Switch off our die/warn handlers so we don't wind up in our own
    # traps.
    $SIG{__DIE__} = $SIG{__WARN__} = '';

    # Have carp skip over death_trap() when showing the stack trace.
    local($Carp::CarpLevel) = 1;

    confess "Uncaught exception from user code:\n\t$exception";
	# up we go; where we stop, nobody knows, but i think we die now
	# but i'm deeply afraid of the &$olddie guy reraising and us getting
	# into an indirect recursion loop
};

my %exact_duplicate;
my %old_diag;
my $count;
my $wantspace;
sub splainthis {
    return 0 if $TRACEONLY;
    $_ = shift;
    local $\;
    local $!;
    ### &finish_compilation unless %msg;
    s/\.?\n+$//;
    my $orig = $_;
    # return unless defined;

    # get rid of the where-are-we-in-input part
    s/, <.*?> (?:line|chunk).*$//;

    # Discard 1st " at <file> line <no>" and all text beyond
    # but be aware of messsages containing " at this-or-that"
    my $real = 0;
    my @secs = split( / at / );
    return unless @secs;
    $_ = $secs[0];
    for my $i ( 1..$#secs ){
        if( $secs[$i] =~ /.+? (?:line|chunk) \d+/ ){
            $real = 1;
            last;
        } else {
            $_ .= ' at ' . $secs[$i];
	}
    }
    
    # remove parenthesis occurring at the end of some messages 
    s/^\((.*)\)$/$1/;

    if ($exact_duplicate{$orig}++) {
	return &transmo;
    } else {
	return 0 unless &transmo;
    }

    $orig = shorten($orig);
    if ($old_diag{$_}) {
	autodescribe();
	print THITHER "$orig (#$old_diag{$_})\n";
	$wantspace = 1;
    } else {
	autodescribe();
	$old_diag{$_} = ++$count;
	print THITHER "\n" if $wantspace;
	$wantspace = 0;
	print THITHER "$orig (#$old_diag{$_})\n";
	if ($msg{$_}) {
	    print THITHER $msg{$_};
	} else {
	    if (0 and $standalone) { 
		print THITHER "    **** Error #$old_diag{$_} ",
			($real ? "is" : "appears to be"),
			" an unknown diagnostic message.\n\n";
	    }
	    return 0;
	} 
    }
    return 1;
} 

sub autodescribe {
    if ($VERBOSE and not $count) {
	print THITHER &{$PRETTY ? \&bold : \&noop}("DESCRIPTION OF DIAGNOSTICS"),
		"\n$msg{DESCRIPTION}\n";
    } 
} 

sub unescape { 
    s {
            E<  
            ( [A-Za-z]+ )       
            >   
    } { 
         do {   
             exists $HTML_Escapes{$1}
                ? do { $HTML_Escapes{$1} }
                : do {
                    warn "Unknown escape: E<$1> in $_";
                    "E<$1>";
                } 
         } 
    }egx;
}

sub shorten {
    my $line = $_[0];
    if (length($line) > 79 and index($line, "\n") == -1) {
	my $space_place = rindex($line, ' ', 79);
	if ($space_place != -1) {
	    substr($line, $space_place, 1) = "\n\t";
	} 
    } 
    return $line;
} 


1 unless $standalone;  # or it'll complain about itself
__END__ # wish diag dbase were more accessible
