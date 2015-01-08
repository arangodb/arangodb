package CPAN::Nox;
use strict;
use vars qw($VERSION @EXPORT);

BEGIN{
  $CPAN::Suppress_readline=1 unless defined $CPAN::term;
}

use base 'Exporter';
use CPAN;

$VERSION = sprintf "%.6f", substr(q$Rev: 2411 $,4)/1000000 + 5.4;
$CPAN::META->has_inst('Digest::MD5','no');
$CPAN::META->has_inst('LWP','no');
$CPAN::META->has_inst('Compress::Zlib','no');
@EXPORT = @CPAN::EXPORT;

*AUTOLOAD = \&CPAN::AUTOLOAD;

1;

__END__

=head1 NAME

CPAN::Nox - Wrapper around CPAN.pm without using any XS module

=head1 SYNOPSIS

Interactive mode:

  perl -MCPAN::Nox -e shell;

=head1 DESCRIPTION

This package has the same functionality as CPAN.pm, but tries to
prevent the usage of compiled extensions during its own
execution. Its primary purpose is a rescue in case you upgraded perl
and broke binary compatibility somehow.

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1  SEE ALSO

L<CPAN>

=cut

