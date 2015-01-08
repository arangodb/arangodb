package ExtUtils::MM_Win95;

use strict;

our $VERSION = '6.44';

require ExtUtils::MM_Win32;
our @ISA = qw(ExtUtils::MM_Win32);

use ExtUtils::MakeMaker::Config;


=head1 NAME

ExtUtils::MM_Win95 - method to customize MakeMaker for Win9X

=head1 SYNOPSIS

  You should not be using this module directly.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Win32 containing changes necessary
to get MakeMaker playing nice with command.com and other Win9Xisms.

=head2 Overridden methods

Most of these make up for limitations in the Win9x/nmake command shell.
Mostly its lack of &&.

=over 4


=item xs_c

The && problem.

=cut

sub xs_c {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.c:
	$(XSUBPPRUN) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.c
	'
}


=item xs_cpp

The && problem

=cut

sub xs_cpp {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.cpp:
	$(XSUBPPRUN) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.cpp
	';
}

=item xs_o 

The && problem.

=cut

sub xs_o {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs$(OBJ_EXT):
	$(XSUBPPRUN) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.c
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.c
	';
}


=item max_exec_len

Win98 chokes on things like Encode if we set the max length to nmake's max
of 2K.  So we go for a more conservative value of 1K.

=cut

sub max_exec_len {
    my $self = shift;

    return $self->{_MAX_EXEC_LEN} ||= 1024;
}


=item os_flavor

Win95 and Win98 and WinME are collectively Win9x and Win32

=cut

sub os_flavor {
    my $self = shift;
    return ($self->SUPER::os_flavor, 'Win9x');
}


=back


=head1 AUTHOR

Code originally inside MM_Win32.  Original author unknown.

Currently maintained by Michael G Schwern C<schwern@pobox.com>.

Send patches and ideas to C<makemaker@perl.org>.

See http://www.makemaker.org.

=cut


1;
