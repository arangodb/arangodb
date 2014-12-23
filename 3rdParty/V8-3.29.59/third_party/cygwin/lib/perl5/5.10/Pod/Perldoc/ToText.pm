
require 5;
package Pod::Perldoc::ToText;
use strict;
use warnings;

use base qw(Pod::Perldoc::BaseTo);

sub is_pageable        { 1 }
sub write_with_binmode { 0 }
sub output_extension   { 'txt' }

use Pod::Text ();

sub alt       { shift->_perldoc_elem('alt'     , @_) }
sub indent    { shift->_perldoc_elem('indent'  , @_) }
sub loose     { shift->_perldoc_elem('loose'   , @_) }
sub quotes    { shift->_perldoc_elem('quotes'  , @_) }
sub sentence  { shift->_perldoc_elem('sentence', @_) }
sub width     { shift->_perldoc_elem('width'   , @_) }

sub new { return bless {}, ref($_[0]) || $_[0] }

sub parse_from_file {
  my $self = shift;
  
  my @options =
    map {; $_, $self->{$_} }
      grep !m/^_/s,
        keys %$self
  ;
  
  defined(&Pod::Perldoc::DEBUG)
   and Pod::Perldoc::DEBUG()
   and print "About to call new Pod::Text ",
    $Pod::Text::VERSION ? "(v$Pod::Text::VERSION) " : '',
    "with options: ",
    @options ? "[@options]" : "(nil)", "\n";
  ;

  Pod::Text->new(@options)->parse_from_file(@_);
}

1;

=head1 NAME

Pod::Perldoc::ToText - let Perldoc render Pod as plaintext

=head1 SYNOPSIS

  perldoc -o text Some::Modulename

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to use
Pod::Text as a formatter class.

It supports the following options, which are explained in
L<Pod::Text>: alt, indent, loose, quotes, sentence, width

For example:

  perldoc -o text -w indent:5 Some::Modulename

=head1 CAVEAT

This module may change to use a different text formatter class in the
future, and this may change what options are supported.

=head1 SEE ALSO

L<Pod::Text>, L<Pod::Perldoc>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut

