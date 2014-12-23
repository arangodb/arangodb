package Term::ReadLine::Perl;
use Carp;
@ISA = qw(Term::ReadLine::Stub Term::ReadLine::Compa Term::ReadLine::Perl::AU);
#require 'readline.pl';

$VERSION = $VERSION = 1.0302;

sub readline {
  shift; 
  #my $in = 
  &readline::readline(@_);
  #$loaded = defined &Term::ReadKey::ReadKey;
  #print STDOUT "\nrl=`$in', loaded = `$loaded'\n";
  #if (ref \$in eq 'GLOB') {	# Bug under debugger
  #  ($in = "$in") =~ s/^\*(\w+::)+//;
  #}
  #print STDOUT "rl=`$in'\n";
  #$in;
}

#sub addhistory {}
*addhistory = \&AddHistory;

#$term;
$readline::minlength = 1;	# To peacify -w
$readline::rl_readline_name = undef; # To peacify -w
$readline::rl_basic_word_break_characters = undef; # To peacify -w

sub new {
  if (defined $term) {
    warn "Cannot create second readline interface, falling back to dumb.\n";
    return Term::ReadLine::Stub::new(@_);
  }
  shift;			# Package
  if (@_) {
    if ($term) {
      warn "Ignoring name of second readline interface.\n" if defined $term;
      shift;
    } else {
      $readline::rl_readline_name = shift; # Name
    }
  }
  if (!@_) {
    if (!defined $term) {
      ($IN,$OUT) = Term::ReadLine->findConsole();
      # Old Term::ReadLine did not have a workaround for a bug in Win devdriver
      $IN = 'CONIN$' if $^O eq 'MSWin32' and "\U$IN" eq 'CON';
      open IN,
	# A workaround for another bug in Win device driver
	(($IN eq 'CONIN$' and $^O eq 'MSWin32') ? "+< $IN" : "< $IN")
	  or croak "Cannot open $IN for read";
      open(OUT,">$OUT") || croak "Cannot open $OUT for write";
      $readline::term_IN = \*IN;
      $readline::term_OUT = \*OUT;
    }
  } else {
    if (defined $term and ($term->IN ne $_[0] or $term->OUT ne $_[1]) ) {
      croak "Request for a second readline interface with different terminal";
    }
    $readline::term_IN = shift;
    $readline::term_OUT = shift;    
  }
  eval {require Term::ReadLine::readline}; die $@ if $@;
  # The following is here since it is mostly used for perl input:
  # $readline::rl_basic_word_break_characters .= '-:+/*,[])}';
  $term = bless [$readline::term_IN,$readline::term_OUT];
  unless ($ENV{PERL_RL} and $ENV{PERL_RL} =~ /\bo\w*=0/) {
    local $Term::ReadLine::termcap_nowarn = 1; # With newer Perls
    local $SIG{__WARN__} = sub {}; # With older Perls
    $term->ornaments(1);
  }
  return $term;
}
sub newTTY {
  my ($self, $in, $out) = @_;
  $readline::term_IN   = $self->[0] = $in;
  $readline::term_OUT  = $self->[1] = $out;
  my $sel = select($out);
  $| = 1;				# for DB::OUT
  select($sel);
}
sub ReadLine {'Term::ReadLine::Perl'}
sub MinLine {
  my $old = $readline::minlength;
  $readline::minlength = $_[1] if @_ == 2;
  return $old;
}
sub SetHistory {
  shift;
  @readline::rl_History = @_;
  $readline::rl_HistoryIndex = @readline::rl_History;
}
sub GetHistory {
  @readline::rl_History;
}
sub AddHistory {
  shift;
  push @readline::rl_History, @_;
  $readline::rl_HistoryIndex = @readline::rl_History + @_;
}
%features =  (appname => 1, minline => 1, autohistory => 1, getHistory => 1,
	      setHistory => 1, addHistory => 1, preput => 1, 
	      attribs => 1, 'newTTY' => 1,
	      tkRunning => Term::ReadLine::Stub->Features->{'tkRunning'},
	      ornaments => Term::ReadLine::Stub->Features->{'ornaments'},
	     );
sub Features { \%features; }
# my %attribs;
tie %attribs, 'Term::ReadLine::Perl::Tie' or die ;
sub Attribs {
  \%attribs;
}
sub DESTROY {}

package Term::ReadLine::Perl::AU;

sub AUTOLOAD {
  { $AUTOLOAD =~ s/.*:://; }		# preserve match data
  my $name = "readline::rl_$AUTOLOAD";
  die "Cannot do `$AUTOLOAD' in Term::ReadLine::Perl" 
    unless exists $readline::{"rl_$AUTOLOAD"};
  *$AUTOLOAD = sub { shift; &$name };
  goto &$AUTOLOAD;
}

package Term::ReadLine::Perl::Tie;

sub TIEHASH { bless {} }
sub DESTROY {}

sub STORE {
  my ($self, $name) = (shift, shift);
  $ {'readline::rl_' . $name} = shift;
}
sub FETCH {
  my ($self, $name) = (shift, shift);
  $ {'readline::rl_' . $name};
}

package Term::ReadLine::Compa;

sub get_c {
  my $self = shift;
  getc($self->[0]);
}

sub get_line {
  my $self = shift;
  my $fh = $self->[0];
  scalar <$fh>;
}

1;
