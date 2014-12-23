package TAP::Parser::Utils;

use strict;
use Exporter;
use vars qw($VERSION @ISA @EXPORT_OK);

@ISA       = qw( Exporter );
@EXPORT_OK = qw( split_shell );

=head1 NAME

TAP::Parser::Utils - Internal TAP::Parser utilities

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 SYNOPSIS

  use TAP::Parser::Utils qw( split_shell )
  my @switches = split_shell( $arg );

=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>

=head2 INTERFACE

=head3 C<split_shell>

Shell style argument parsing. Handles backslash escaping, single and
double quoted strings but not shell substitutions.

Pass one or more strings containing shell escaped arguments. The return
value is an array of arguments parsed from the input strings according
to (approximate) shell parsing rules. It's legal to pass C<undef> in
which case an empty array will be returned. That makes it possible to

    my @args = split_shell( $ENV{SOME_ENV_VAR} );

without worrying about whether the environment variable exists.

This is used to split HARNESS_PERL_ARGS into individual switches.

=cut

sub split_shell {
    my @parts = ();

    for my $switch ( grep defined && length, @_ ) {
        push @parts, $1 while $switch =~ /
        ( 
            (?:   [^\\"'\s]+
                | \\. 
                | " (?: \\. | [^"] )* "
                | ' (?: \\. | [^'] )* ' 
            )+
        ) /xg;
    }

    for (@parts) {
        s/ \\(.) | ['"] /defined $1 ? $1 : ''/exg;
    }

    return @parts;
}

1;
