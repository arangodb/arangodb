package ExtUtils::MakeMaker::vmsish;

use strict;

our $VERSION = 6.44;

my $IsVMS = $^O eq 'VMS';

require vmsish if $IsVMS;


sub import {
    return unless $IsVMS;

    shift;
    unshift @_, 'vmsish';

    goto &vmsish::import;
}

1;


=head1 NAME

ExtUtils::MakeMaker::vmsish - Platform-agnostic vmsish.pm

=head1 SYNOPSIS

  use just like vmsish.pm

=head1 DESCRIPTION

Until 5.8.0, vmsish.pm is only installed on VMS.  This means any code
which has 'use vmsish' in it won't even compile outside VMS.  This
makes ExtUtils::MM_VMS very hard to test.

ExtUtils::MakeMaker::vmsish is just a very thin wrapper around vmsish
which works just like it on VMS and everywhere else it does nothing.

=cut
