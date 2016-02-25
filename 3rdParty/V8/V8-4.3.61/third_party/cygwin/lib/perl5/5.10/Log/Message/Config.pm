package Log::Message::Config;
use strict;

use Params::Check qw[check];
use Module::Load;
use FileHandle;
use Locale::Maketext::Simple Style => 'gettext';

BEGIN {
    use vars        qw[$VERSION $AUTOLOAD];
    $VERSION    =   0.01;
}

sub new {
    my $class = shift;
    my %hash  = @_;

    ### find out if the user specified a config file to use
    ### and/or a default configuration object
    ### and remove them from the argument hash
    my %special =   map { lc, delete $hash{$_} }
                    grep /^config|default$/i, keys %hash;

    ### allow provided arguments to override the values from the config ###
    my $tmpl = {
        private => { default => undef,  },
        verbose => { default => 1       },
        tag     => { default => 'NONE', },
        level   => { default => 'log',  },
        remove  => { default => 0       },
        chrono  => { default => 1       },
    };

    my %lc_hash = map { lc, $hash{$_} } keys %hash;

    my $file_conf;
    if( $special{config} ) {
        $file_conf = _read_config_file( $special{config} )
                        or ( warn( loc(q[Could not parse config file!]) ), return );
    }

    my $def_conf = \%{ $special{default} || {} };

    ### make sure to only include keys that are actually defined --
    ### the checker will assign even 'undef' if you have provided that
    ### as a value
    ### priorities goes as follows:
    ### 1: arguments passed
    ### 2: any config file passed
    ### 3: any default config passed
    my %to_check =  map     { @$_ }
                    grep    { defined $_->[1] }
                    map     {   [ $_ =>
                                    defined $lc_hash{$_}        ? $lc_hash{$_}      :
                                    defined $file_conf->{$_}    ? $file_conf->{$_}  :
                                    defined $def_conf->{$_}     ? $def_conf->{$_}   :
                                    undef
                                ]
                            } keys %$tmpl;

    my $rv = check( $tmpl, \%to_check, 1 )
                or ( warn( loc(q[Could not validate arguments!]) ), return );

    return bless $rv, $class;
}

sub _read_config_file {
    my $file = shift or return;

    my $conf = {};
    my $FH = new FileHandle;
    $FH->open("$file") or (
                        warn(loc(q[Could not open config file '%1': %2],$file,$!)),
                        return {}
                    );

    while(<$FH>) {
        next if     /\s*#/;
        next unless /\S/;

        chomp; s/^\s*//; s/\s*$//;

        my ($param,$val) = split /\s*=\s*/;

        if( (lc $param) eq 'include' ) {
            load $val;
            next;
        }

        ### add these to the config hash ###
        $conf->{ lc $param } = $val;
    }
    close $FH;

    return $conf;
}

sub AUTOLOAD {
    $AUTOLOAD =~ s/.+:://;

    my $self = shift;

    return $self->{ lc $AUTOLOAD } if exists $self->{ lc $AUTOLOAD };

    die loc(q[No such accessor '%1' for class '%2'], $AUTOLOAD, ref $self);
}

sub DESTROY { 1 }

1;

__END__

=pod

=head1 NAME

Log::Message::Config - Configuration options for Log::Message

=head1 SYNOPSIS

    # This module is implicitly used by Log::Message to create a config
    # which it uses to log messages.
    # For the options you can pass, see the C<Log::Message new()> method.

    # Below is a sample of a config file you could use

    # comments are denoted by a single '#'
    # use a shared stack, or have a private instance?
    # if none provided, set to '0',
    private = 1

    # do not be verbose
    verbose = 0

    # default tag to set on new items
    # if none provided, set to 'NONE'
    tag = SOME TAG

    # default level to handle items
    # if none provided, set to 'log'
    level = carp

    # extra files to include
    # if none provided, no files are auto included
    include = mylib.pl
    include = ../my/other/lib.pl

    # automatically delete items
    # when you retrieve them from the stack?
    # if none provided, set to '0'
    remove = 1

    # retrieve errors in chronological order, or not?
    # if none provided, set to '1'
    chrono = 0

=head1 DESCRIPTION

Log::Message::Config provides a standardized config object for
Log::Message objects.

It can either read options as perl arguments, or as a config file.
See the Log::Message manpage for more information about what arguments
are valid, and see the Synopsis for an example config file you can use

=head1 SEE ALSO

L<Log::Message>, L<Log::Message::Item>, L<Log::Message::Handlers>

=head1 AUTHOR

This module by
Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 Acknowledgements

Thanks to Ann Barcomb for her suggestions.

=head1 COPYRIGHT

This module is
copyright (c) 2002 Jos Boumans E<lt>kane@cpan.orgE<gt>.
All rights reserved.

This library is free software;
you may redistribute and/or modify it under the same
terms as Perl itself.

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
