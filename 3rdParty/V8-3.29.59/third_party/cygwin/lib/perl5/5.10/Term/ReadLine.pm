=head1 NAME

Term::ReadLine - Perl interface to various C<readline> packages.
If no real package is found, substitutes stubs instead of basic functions.

=head1 SYNOPSIS

  use Term::ReadLine;
  my $term = new Term::ReadLine 'Simple Perl calc';
  my $prompt = "Enter your arithmetic expression: ";
  my $OUT = $term->OUT || \*STDOUT;
  while ( defined ($_ = $term->readline($prompt)) ) {
    my $res = eval($_);
    warn $@ if $@;
    print $OUT $res, "\n" unless $@;
    $term->addhistory($_) if /\S/;
  }

=head1 DESCRIPTION

This package is just a front end to some other packages. It's a stub to
set up a common interface to the various ReadLine implementations found on
CPAN (under the C<Term::ReadLine::*> namespace).

=head1 Minimal set of supported functions

All the supported functions should be called as methods, i.e., either as 

  $term = new Term::ReadLine 'name';

or as 

  $term->addhistory('row');

where $term is a return value of Term::ReadLine-E<gt>new().

=over 12

=item C<ReadLine>

returns the actual package that executes the commands. Among possible
values are C<Term::ReadLine::Gnu>, C<Term::ReadLine::Perl>,
C<Term::ReadLine::Stub>.

=item C<new>

returns the handle for subsequent calls to following
functions. Argument is the name of the application. Optionally can be
followed by two arguments for C<IN> and C<OUT> filehandles. These
arguments should be globs.

=item C<readline>

gets an input line, I<possibly> with actual C<readline>
support. Trailing newline is removed. Returns C<undef> on C<EOF>.

=item C<addhistory>

adds the line to the history of input, from where it can be used if
the actual C<readline> is present.

=item C<IN>, C<OUT>

return the filehandles for input and output or C<undef> if C<readline>
input and output cannot be used for Perl.

=item C<MinLine>

If argument is specified, it is an advice on minimal size of line to
be included into history.  C<undef> means do not include anything into
history. Returns the old value.

=item C<findConsole>

returns an array with two strings that give most appropriate names for
files for input and output using conventions C<"E<lt>$in">, C<"E<gt>out">.

=item Attribs

returns a reference to a hash which describes internal configuration
of the package. Names of keys in this hash conform to standard
conventions with the leading C<rl_> stripped.

=item C<Features>

Returns a reference to a hash with keys being features present in
current implementation. Several optional features are used in the
minimal interface: C<appname> should be present if the first argument
to C<new> is recognized, and C<minline> should be present if
C<MinLine> method is not dummy.  C<autohistory> should be present if
lines are put into history automatically (maybe subject to
C<MinLine>), and C<addhistory> if C<addhistory> method is not dummy.

If C<Features> method reports a feature C<attribs> as present, the
method C<Attribs> is not dummy.

=back

=head1 Additional supported functions

Actually C<Term::ReadLine> can use some other package, that will
support a richer set of commands.

All these commands are callable via method interface and have names
which conform to standard conventions with the leading C<rl_> stripped.

The stub package included with the perl distribution allows some
additional methods: 

=over 12

=item C<tkRunning>

makes Tk event loop run when waiting for user input (i.e., during
C<readline> method).

=item C<ornaments>

makes the command line stand out by using termcap data.  The argument
to C<ornaments> should be 0, 1, or a string of a form
C<"aa,bb,cc,dd">.  Four components of this string should be names of
I<terminal capacities>, first two will be issued to make the prompt
standout, last two to make the input line standout.

=item C<newTTY>

takes two arguments which are input filehandle and output filehandle.
Switches to use these filehandles.

=back

One can check whether the currently loaded ReadLine package supports
these methods by checking for corresponding C<Features>.

=head1 EXPORTS

None

=head1 ENVIRONMENT

The environment variable C<PERL_RL> governs which ReadLine clone is
loaded. If the value is false, a dummy interface is used. If the value
is true, it should be tail of the name of the package to use, such as
C<Perl> or C<Gnu>.  

As a special case, if the value of this variable is space-separated,
the tail might be used to disable the ornaments by setting the tail to
be C<o=0> or C<ornaments=0>.  The head should be as described above, say

If the variable is not set, or if the head of space-separated list is
empty, the best available package is loaded.

  export "PERL_RL=Perl o=0"	# Use Perl ReadLine without ornaments
  export "PERL_RL= o=0"		# Use best available ReadLine without ornaments

(Note that processing of C<PERL_RL> for ornaments is in the discretion of the 
particular used C<Term::ReadLine::*> package).

=head1 CAVEATS

It seems that using Term::ReadLine from Emacs minibuffer doesn't work
quite right and one will get an error message like

    Cannot open /dev/tty for read at ...

One possible workaround for this is to explicitly open /dev/tty like this

    open (FH, "/dev/tty" )
      or eval 'sub Term::ReadLine::findConsole { ("&STDIN", "&STDERR") }';
    die $@ if $@;
    close (FH);

or you can try using the 4-argument form of Term::ReadLine->new().

=cut

use strict;

package Term::ReadLine::Stub;
our @ISA = qw'Term::ReadLine::Tk Term::ReadLine::TermCap';

$DB::emacs = $DB::emacs;	# To peacify -w
our @rl_term_set;
*rl_term_set = \@Term::ReadLine::TermCap::rl_term_set;

sub PERL_UNICODE_STDIN () { 0x0001 }

sub ReadLine {'Term::ReadLine::Stub'}
sub readline {
  my $self = shift;
  my ($in,$out,$str) = @$self;
  my $prompt = shift;
  print $out $rl_term_set[0], $prompt, $rl_term_set[1], $rl_term_set[2]; 
  $self->register_Tk 
     if not $Term::ReadLine::registered and $Term::ReadLine::toloop
	and defined &Tk::DoOneEvent;
  #$str = scalar <$in>;
  $str = $self->get_line;
  $str =~ s/^\s*\Q$prompt\E// if ($^O eq 'MacOS');
  utf8::upgrade($str)
      if (${^UNICODE} & PERL_UNICODE_STDIN || defined ${^ENCODING}) &&
         utf8::valid($str);
  print $out $rl_term_set[3]; 
  # bug in 5.000: chomping empty string creats length -1:
  chomp $str if defined $str;
  $str;
}
sub addhistory {}

sub findConsole {
    my $console;
    my $consoleOUT;

    if ($^O eq 'MacOS') {
        $console = "Dev:Console";
    } elsif (-e "/dev/tty") {
	$console = "/dev/tty";
    } elsif (-e "con" or $^O eq 'MSWin32') {
       $console = 'CONIN$';
       $consoleOUT = 'CONOUT$';
    } else {
	$console = "sys\$command";
    }

    if (($^O eq 'amigaos') || ($^O eq 'beos') || ($^O eq 'epoc')) {
	$console = undef;
    }
    elsif ($^O eq 'os2') {
      if ($DB::emacs) {
	$console = undef;
      } else {
	$console = "/dev/con";
      }
    }

    $consoleOUT = $console unless defined $consoleOUT;
    $console = "&STDIN" unless defined $console;
    if (!defined $consoleOUT) {
      $consoleOUT = defined fileno(STDERR) && $^O ne 'MSWin32' ? "&STDERR" : "&STDOUT";
    }
    ($console,$consoleOUT);
}

sub new {
  die "method new called with wrong number of arguments" 
    unless @_==2 or @_==4;
  #local (*FIN, *FOUT);
  my ($FIN, $FOUT, $ret);
  if (@_==2) {
    my($console, $consoleOUT) = $_[0]->findConsole;


    # the Windows CONIN$ needs GENERIC_WRITE mode to allow
    # a SetConsoleMode() if we end up using Term::ReadKey
    open FIN, (  $^O eq 'MSWin32' && $console eq 'CONIN$' ) ? "+<$console" :
                                                              "<$console";
    open FOUT,">$consoleOUT";

    #OUT->autoflush(1);		# Conflicts with debugger?
    my $sel = select(FOUT);
    $| = 1;				# for DB::OUT
    select($sel);
    $ret = bless [\*FIN, \*FOUT];
  } else {			# Filehandles supplied
    $FIN = $_[2]; $FOUT = $_[3];
    #OUT->autoflush(1);		# Conflicts with debugger?
    my $sel = select($FOUT);
    $| = 1;				# for DB::OUT
    select($sel);
    $ret = bless [$FIN, $FOUT];
  }
  if ($ret->Features->{ornaments} 
      and not ($ENV{PERL_RL} and $ENV{PERL_RL} =~ /\bo\w*=0/)) {
    local $Term::ReadLine::termcap_nowarn = 1;
    $ret->ornaments(1);
  }
  return $ret;
}

sub newTTY {
  my ($self, $in, $out) = @_;
  $self->[0] = $in;
  $self->[1] = $out;
  my $sel = select($out);
  $| = 1;				# for DB::OUT
  select($sel);
}

sub IN { shift->[0] }
sub OUT { shift->[1] }
sub MinLine { undef }
sub Attribs { {} }

my %features = (tkRunning => 1, ornaments => 1, 'newTTY' => 1);
sub Features { \%features }

sub get_line {
  my $self = shift;
  my $in = $self->IN;
  local ($/) = "\n";
  return scalar <$in>;
}

package Term::ReadLine;		# So late to allow the above code be defined?

our $VERSION = '1.03';

my ($which) = exists $ENV{PERL_RL} ? split /\s+/, $ENV{PERL_RL} : undef;
if ($which) {
  if ($which =~ /\bgnu\b/i){
    eval "use Term::ReadLine::Gnu;";
  } elsif ($which =~ /\bperl\b/i) {
    eval "use Term::ReadLine::Perl;";
  } else {
    eval "use Term::ReadLine::$which;";
  }
} elsif (defined $which and $which ne '') {	# Defined but false
  # Do nothing fancy
} else {
  eval "use Term::ReadLine::Gnu; 1" or eval "use Term::ReadLine::Perl; 1";
}

#require FileHandle;

# To make possible switch off RL in debugger: (Not needed, work done
# in debugger).
our @ISA;
if (defined &Term::ReadLine::Gnu::readline) {
  @ISA = qw(Term::ReadLine::Gnu Term::ReadLine::Stub);
} elsif (defined &Term::ReadLine::Perl::readline) {
  @ISA = qw(Term::ReadLine::Perl Term::ReadLine::Stub);
} elsif (defined $which && defined &{"Term::ReadLine::$which\::readline"}) {
  @ISA = "Term::ReadLine::$which";
} else {
  @ISA = qw(Term::ReadLine::Stub);
}

package Term::ReadLine::TermCap;

# Prompt-start, prompt-end, command-line-start, command-line-end
#     -- zero-width beautifies to emit around prompt and the command line.
our @rl_term_set = ("","","","");
# string encoded:
our $rl_term_set = ',,,';

our $terminal;
sub LoadTermCap {
  return if defined $terminal;
  
  require Term::Cap;
  $terminal = Tgetent Term::Cap ({OSPEED => 9600}); # Avoid warning.
}

sub ornaments {
  shift;
  return $rl_term_set unless @_;
  $rl_term_set = shift;
  $rl_term_set ||= ',,,';
  $rl_term_set = 'us,ue,md,me' if $rl_term_set eq '1';
  my @ts = split /,/, $rl_term_set, 4;
  eval { LoadTermCap };
  unless (defined $terminal) {
    warn("Cannot find termcap: $@\n") unless $Term::ReadLine::termcap_nowarn;
    $rl_term_set = ',,,';
    return;
  }
  @rl_term_set = map {$_ ? $terminal->Tputs($_,1) || '' : ''} @ts;
  return $rl_term_set;
}


package Term::ReadLine::Tk;

our($count_handle, $count_DoOne, $count_loop);
$count_handle = $count_DoOne = $count_loop = 0;

our($giveup);
sub handle {$giveup = 1; $count_handle++}

sub Tk_loop {
  # Tk->tkwait('variable',\$giveup);	# needs Widget
  $count_DoOne++, Tk::DoOneEvent(0) until $giveup;
  $count_loop++;
  $giveup = 0;
}

sub register_Tk {
  my $self = shift;
  $Term::ReadLine::registered++ 
    or Tk->fileevent($self->IN,'readable',\&handle);
}

sub tkRunning {
  $Term::ReadLine::toloop = $_[1] if @_ > 1;
  $Term::ReadLine::toloop;
}

sub get_c {
  my $self = shift;
  $self->Tk_loop if $Term::ReadLine::toloop && defined &Tk::DoOneEvent;
  return getc $self->IN;
}

sub get_line {
  my $self = shift;
  $self->Tk_loop if $Term::ReadLine::toloop && defined &Tk::DoOneEvent;
  my $in = $self->IN;
  local ($/) = "\n";
  return scalar <$in>;
}

1;

