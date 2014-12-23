
require 5;
package Pod::Perldoc::ToXml;
use strict;
use warnings;
use vars qw($VERSION);

use base qw( Pod::Simple::XMLOutStream );

$VERSION   # so that ->VERSION is happy
# stop CPAN from seeing this
 =
$Pod::Simple::XMLOutStream::VERSION;


sub is_pageable        { 0 }
sub write_with_binmode { 0 }
sub output_extension   { 'xml' }

1;
__END__

=head1 NAME

Pod::Perldoc::ToXml - let Perldoc render Pod as XML

=head1 SYNOPSIS

  perldoc -o xml -d out.xml Some::Modulename

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to use
Pod::Simple::XMLOutStream as a formatter class.

This is actually a Pod::Simple::XMLOutStream subclass, and inherits
all its options.

You have to have installed Pod::Simple::XMLOutStream (from the Pod::Simple
dist), or this class won't work.


=head1 SEE ALSO

L<Pod::Simple::XMLOutStream>, L<Pod::Simple>, L<Pod::Perldoc>

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

