package LWP::Debug;

require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(level trace debug conns);

use Carp ();

my @levels = qw(trace debug conns);
%current_level = ();


sub import
{
    my $pack = shift;
    my $callpkg = caller(0);
    my @symbols = ();
    my @levels = ();
    for (@_) {
	if (/^[-+]/) {
	    push(@levels, $_);
	}
	else {
	    push(@symbols, $_);
	}
    }
    Exporter::export($pack, $callpkg, @symbols);
    level(@levels);
}


sub level
{
    for (@_) {
	if ($_ eq '+') {              # all on
	    # switch on all levels
	    %current_level = map { $_ => 1 } @levels;
	}
	elsif ($_ eq '-') {           # all off
	    %current_level = ();
	}
	elsif (/^([-+])(\w+)$/) {
	    $current_level{$2} = $1 eq '+';
	}
	else {
	    Carp::croak("Illegal level format $_");
	}
    }
}


sub trace  { _log(@_) if $current_level{'trace'}; }
sub debug  { _log(@_) if $current_level{'debug'}; }
sub conns  { _log(@_) if $current_level{'conns'}; }


sub _log
{
    my $msg = shift;
    $msg .= "\n" unless $msg =~ /\n$/;  # ensure trailing "\n"

    my($package,$filename,$line,$sub) = caller(2);
    print STDERR "$sub: $msg";
}

1;


__END__

=head1 NAME

LWP::Debug - debug routines for the libwww-perl library

=head1 SYNOPSIS

 use LWP::Debug qw(+ -conns);

 # Used internally in the library
 LWP::Debug::trace('send()');
 LWP::Debug::debug('url ok');
 LWP::Debug::conns("read $n bytes: $data");

=head1 DESCRIPTION

LWP::Debug provides tracing facilities. The trace(), debug() and
conns() function are called within the library and they log
information at increasing levels of detail. Which level of detail is
actually printed is controlled with the C<level()> function.

The following functions are available:

=over 4

=item level(...)

The C<level()> function controls the level of detail being
logged. Passing '+' or '-' indicates full and no logging
respectively. Individual levels can switched on and of by passing the
name of the level with a '+' or '-' prepended.  The levels are:

  trace   : trace function calls
  debug   : print debug messages
  conns   : show all data transfered over the connections

The LWP::Debug module provide a special import() method that allows
you to pass the level() arguments with initial use statement.  If a
use argument start with '+' or '-' then it is passed to the level
function, else the name is exported as usual.  The following two
statements are thus equivalent (if you ignore that the second pollutes
your namespace):

  use LWP::Debug qw(+);
  use LWP::Debug qw(level); level('+');

=item trace($msg)

The C<trace()> function is used for tracing function
calls. The package and calling subroutine name is
printed along with the passed argument. This should
be called at the start of every major function.

=item debug($msg)

The C<debug()> function is used for high-granularity
reporting of state in functions.

=item conns($msg)

The C<conns()> function is used to show data being
transferred over the connections. This may generate
considerable output.

=back
