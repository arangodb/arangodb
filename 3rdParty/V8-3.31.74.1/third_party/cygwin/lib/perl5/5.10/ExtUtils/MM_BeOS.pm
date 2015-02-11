package ExtUtils::MM_BeOS;

use strict;

=head1 NAME

ExtUtils::MM_BeOS - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_BeOS;	# Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over 4

=cut

use ExtUtils::MakeMaker::Config;
use File::Spec;
require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;

our @ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );
our $VERSION = '6.44';


=item os_flavor

BeOS is BeOS.

=cut

sub os_flavor {
    return('BeOS');
}

=item init_linker

libperl.a equivalent to be linked to dynamic extensions.

=cut

sub init_linker {
    my($self) = shift;

    $self->{PERL_ARCHIVE} ||= 
      File::Spec->catdir('$(PERL_INC)',$Config{libperl});
    $self->{PERL_ARCHIVE_AFTER} ||= '';
    $self->{EXPORT_LIST}  ||= '';
}

=back

1;
__END__

