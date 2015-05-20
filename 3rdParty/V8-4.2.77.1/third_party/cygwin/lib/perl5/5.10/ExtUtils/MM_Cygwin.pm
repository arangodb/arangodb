package ExtUtils::MM_Cygwin;

use strict;

use ExtUtils::MakeMaker::Config;
use File::Spec;

require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
our @ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );

our $VERSION = '6.44';


=head1 NAME

ExtUtils::MM_Cygwin - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Cygwin; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided there.

=over 4

=item os_flavor

We're Unix and Cygwin.

=cut

sub os_flavor {
    return('Unix', 'Cygwin');
}

=item cflags

if configured for dynamic loading, triggers #define EXT in EXTERN.h

=cut

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    return '' unless $self->needs_linking();

    my $base = $self->SUPER::cflags($libperl);
    foreach (split /\n/, $base) {
        /^(\S*)\s*=\s*(\S*)$/ and $self->{$1} = $2;
    };
    $self->{CCFLAGS} .= " -DUSEIMPORTLIB" if ($Config{useshrplib} eq 'true');

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
};

}


=item replace_manpage_separator

replaces strings '::' with '.' in MAN*POD man page names

=cut

sub replace_manpage_separator {
    my($self, $man) = @_;
    $man =~ s{/+}{.}g;
    return $man;
}

=item init_linker

points to libperl.a

=cut

sub init_linker {
    my $self = shift;

    if ($Config{useshrplib} eq 'true') {
        my $libperl = '$(PERL_INC)' .'/'. "$Config{libperl}";
        if( $] >= 5.006002 ) {
            $libperl =~ s/a$/dll.a/;
        }
        $self->{PERL_ARCHIVE} = $libperl;
    } else {
        $self->{PERL_ARCHIVE} = 
          '$(PERL_INC)' .'/'. ("$Config{libperl}" or "libperl.a");
    }

    $self->{PERL_ARCHIVE_AFTER} ||= '';
    $self->{EXPORT_LIST}  ||= '';
}

=back

=cut

1;
