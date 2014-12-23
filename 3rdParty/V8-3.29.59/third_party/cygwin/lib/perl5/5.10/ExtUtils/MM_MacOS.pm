package ExtUtils::MM_MacOS;

use strict;

our $VERSION = 6.44;

sub new {
    die <<'UNSUPPORTED';
MacOS Classic (MacPerl) is no longer supported by MakeMaker.
Please use Module::Build instead.
UNSUPPORTED
}

=head1 NAME

ExtUtils::MM_MacOS - once produced Makefiles for MacOS Classic

=head1 SYNOPSIS

  # MM_MacOS no longer contains any code.  This is just a stub.

=head1 DESCRIPTION

Once upon a time, MakeMaker could produce an approximation of a correct
Makefile on MacOS Classic (MacPerl).  Due to a lack of maintainers, this
fell out of sync with the rest of MakeMaker and hadn't worked in years.
Since there's little chance of it being repaired, MacOS Classic is fading
away, and the code was icky to begin with, the code has been deleted to
make maintenance easier.

Those interested in writing modules for MacPerl should use Module::Build
which works better than MakeMaker ever did.

Anyone interested in resurrecting this file should pull the old version
from the MakeMaker CVS repository and contact makemaker@perl.org, but we
really encourage you to work on Module::Build instead.

=cut

1;
