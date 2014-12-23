package ExtUtils::MM_AIX;

use strict;
our $VERSION = '6.44';

require ExtUtils::MM_Unix;
our @ISA = qw(ExtUtils::MM_Unix);

use ExtUtils::MakeMaker qw(neatvalue);


=head1 NAME

ExtUtils::MM_AIX - AIX specific subclass of ExtUtils::MM_Unix

=head1 SYNOPSIS

  Don't use this module directly.
  Use ExtUtils::MM and let it choose.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Unix which contains functionality for
AIX.

Unless otherwise stated it works just like ExtUtils::MM_Unix

=head2 Overridden methods

=head3 dlsyms

Define DL_FUNCS and DL_VARS and write the *.exp files.

=cut

sub dlsyms {
    my($self,%attribs) = @_;

    return '' unless $self->needs_linking();

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS} || $self->{DL_VARS} || [];
    my($funclist)  = $attribs{FUNCLIST} || $self->{FUNCLIST} || [];
    my(@m);

    push(@m,"
dynamic :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'dynamic'}; # dynamic and static are subs, so...

    push(@m,"
static :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'static'};  # we avoid a warning if we tick them

    push(@m,"
$self->{BASEEXT}.exp: Makefile.PL
",'	$(PERLRUN) -e \'use ExtUtils::Mksymlists; \\
	Mksymlists("NAME" => "',$self->{NAME},'", "DL_FUNCS" => ',
	neatvalue($funcs), ', "FUNCLIST" => ', neatvalue($funclist),
	', "DL_VARS" => ', neatvalue($vars), ');\'
');

    join('',@m);
}


=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> with code from ExtUtils::MM_Unix

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut


1;
