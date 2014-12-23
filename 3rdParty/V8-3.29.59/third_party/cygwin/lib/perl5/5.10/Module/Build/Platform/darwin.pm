package Module::Build::Platform::darwin;

use strict;
use vars qw($VERSION);
$VERSION = '0.2808_01';
$VERSION = eval $VERSION;
use Module::Build::Platform::Unix;

use vars qw(@ISA);
@ISA = qw(Module::Build::Platform::Unix);

# This class isn't necessary anymore, but we can't delete it, because
# some people might still have the old copy in their @INC, containing
# code we don't want to execute, so we have to make sure an upgrade
# will replace it with this empty subclass.

1;
__END__


=head1 NAME

Module::Build::Platform::darwin - Builder class for Mac OS X platform

=head1 DESCRIPTION

This module provides some routines very specific to the Mac OS X
platform.

Please see the L<Module::Build> for the general docs.

=head1 AUTHOR

Ken Williams <kwilliams@cpan.org>

=head1 SEE ALSO

perl(1), Module::Build(3), ExtUtils::MakeMaker(3)

=cut
