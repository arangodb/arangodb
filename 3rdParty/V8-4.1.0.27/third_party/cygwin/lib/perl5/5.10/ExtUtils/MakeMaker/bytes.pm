package ExtUtils::MakeMaker::bytes;

use strict;

our $VERSION = 6.44;

my $Have_Bytes = eval { require bytes; 1; };

sub import {
    return unless $Have_Bytes;

    shift;
    unshift @_, 'bytes';

    goto &bytes::import;
}

1;


=head1 NAME

ExtUtils::MakeMaker::bytes - Version-agnostic bytes.pm

=head1 SYNOPSIS

  use just like bytes.pm

=head1 DESCRIPTION

bytes.pm was introduced with 5.6.  This means any code which has 'use
bytes' in it won't even compile on 5.5.X.  Since bytes is a lexical
pragma and must be used at compile time we can't simply wrap it in
a BEGIN { eval 'use bytes' } block.

ExtUtils::MakeMaker::bytes is just a very thin wrapper around bytes
which works just like it when bytes.pm exists and everywhere else it
does nothing.

=cut
