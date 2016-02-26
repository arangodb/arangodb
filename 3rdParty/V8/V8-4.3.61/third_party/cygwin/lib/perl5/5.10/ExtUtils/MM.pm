package ExtUtils::MM;

use strict;
use ExtUtils::MakeMaker::Config;

our $VERSION = '6.44';

require ExtUtils::Liblist;
require ExtUtils::MakeMaker;
our @ISA = qw(ExtUtils::Liblist ExtUtils::MakeMaker);

=head1 NAME

ExtUtils::MM - OS adjusted ExtUtils::MakeMaker subclass

=head1 SYNOPSIS

  require ExtUtils::MM;
  my $mm = MM->new(...);

=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY>

ExtUtils::MM is a subclass of ExtUtils::MakeMaker which automatically
chooses the appropriate OS specific subclass for you
(ie. ExtUils::MM_Unix, etc...).

It also provides a convenient alias via the MM class (I didn't want
MakeMaker modules outside of ExtUtils/).

This class might turn out to be a temporary solution, but MM won't go
away.

=cut

{
    # Convenient alias.
    package MM;
    our @ISA = qw(ExtUtils::MM);
    sub DESTROY {}
}

sub _is_win95 {
    # miniperl might not have the Win32 functions available and we need
    # to run in miniperl.
    return defined &Win32::IsWin95 ? Win32::IsWin95() 
                                   : ! defined $ENV{SYSTEMROOT}; 
}

my %Is = ();
$Is{VMS}    = $^O eq 'VMS';
$Is{OS2}    = $^O eq 'os2';
$Is{MacOS}  = $^O eq 'MacOS';
if( $^O eq 'MSWin32' ) {
    _is_win95() ? $Is{Win95} = 1 : $Is{Win32} = 1;
}
$Is{UWIN}   = $^O =~ /^uwin(-nt)?$/;
$Is{Cygwin} = $^O eq 'cygwin';
$Is{NW5}    = $Config{osname} eq 'NetWare';  # intentional
$Is{BeOS}   = $^O =~ /beos/i;    # XXX should this be that loose?
$Is{DOS}    = $^O eq 'dos';
if( $Is{NW5} ) {
    $^O = 'NetWare';
    delete $Is{Win32};
}
$Is{VOS}    = $^O eq 'vos';
$Is{QNX}    = $^O eq 'qnx';
$Is{AIX}    = $^O eq 'aix';
$Is{Darwin} = $^O eq 'darwin';

$Is{Unix}   = !grep { $_ } values %Is;

map { delete $Is{$_} unless $Is{$_} } keys %Is;
_assert( keys %Is == 1 );
my($OS) = keys %Is;


my $class = "ExtUtils::MM_$OS";
eval "require $class" unless $INC{"ExtUtils/MM_$OS.pm"}; ## no critic
die $@ if $@;
unshift @ISA, $class;


sub _assert {
    my $sanity = shift;
    die sprintf "Assert failed at %s line %d\n", (caller)[1,2] unless $sanity;
    return;
}
