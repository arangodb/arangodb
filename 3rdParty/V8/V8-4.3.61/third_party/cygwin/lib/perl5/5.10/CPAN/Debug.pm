# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 4 -*-
package CPAN::Debug;
use strict;
use vars qw($VERSION);

$VERSION = sprintf "%.6f", substr(q$Rev: 2212 $,4)/1000000 + 5.4;
# module is internal to CPAN.pm

%CPAN::DEBUG = qw[
                  CPAN              1
                  Index             2
                  InfoObj           4
                  Author            8
                  Distribution     16
                  Bundle           32
                  Module           64
                  CacheMgr        128
                  Complete        256
                  FTP             512
                  Shell          1024
                  Eval           2048
                  HandleConfig   4096
                  Tarzip         8192
                  Version       16384
                  Queue         32768
                  FirstTime     65536
];

$CPAN::DEBUG ||= 0;

#-> sub CPAN::Debug::debug ;
sub debug {
    my($self,$arg) = @_;

    my @caller;
    my $i = 0;
    while () {
        my(@c) = (caller($i))[0 .. ($i ? 3 : 2)];
        last unless defined $c[0];
        push @caller, \@c;
        for (0,3) {
            last if $_ > $#c;
            $c[$_] =~ s/.*:://;
        }
        for (1) {
            $c[$_] =~ s|.*/||;
        }
        last if ++$i>=3;
    }
    pop @caller;
    if ($CPAN::DEBUG{$caller[0][0]} & $CPAN::DEBUG) {
        if ($arg and ref $arg) {
            eval { require Data::Dumper };
            if ($@) {
                $CPAN::Frontend->myprint($arg->as_string);
            } else {
                $CPAN::Frontend->myprint(Data::Dumper::Dumper($arg));
            }
        } else {
            my $outer = "";
            local $" = ",";
            if (@caller>1) {
                $outer = ",[@{$caller[1]}]";
            }
            $CPAN::Frontend->myprint("Debug(@{$caller[0]}$outer): $arg\n");
        }
    }
}

1;

__END__

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
