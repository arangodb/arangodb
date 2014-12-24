package ExtUtils::MM_Darwin;

use strict;

BEGIN {
    require ExtUtils::MM_Unix;
    our @ISA = qw( ExtUtils::MM_Unix );
}


=head1 NAME

ExtUtils::MM_Darwin - special behaviors for OS X

=head1 SYNOPSIS

    For internal MakeMaker use only

=head1 DESCRIPTION

See L<ExtUtils::MM_Unix> for L<ExtUtils::MM_Any> for documention on the
methods overridden here.

=head2 Overriden Methods

=head3 init_dist

Turn off Apple tar's tendency to copy resource forks as "._foo" files.

=cut

sub init_dist {
    my $self = shift;
    
    # Thank you, Apple, for breaking tar and then breaking the work around.
    # 10.4 wants COPY_EXTENDED_ATTRIBUTES_DISABLE while 10.5 wants
    # COPYFILE_DISABLE.  I'm not going to push my luck and instead just
    # set both.
    $self->{TAR} ||= 
        'COPY_EXTENDED_ATTRIBUTES_DISABLE=1 COPYFILE_DISABLE=1 tar';
    
    $self->SUPER::init_dist(@_);
}

1;
