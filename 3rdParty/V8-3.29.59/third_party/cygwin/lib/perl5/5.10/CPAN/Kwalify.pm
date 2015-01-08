=head1 NAME

CPAN::Kwalify - Interface between CPAN.pm and Kwalify.pm

=head1 SYNOPSIS

  use CPAN::Kwalify;
  validate($schema_name, $data, $file, $doc);

=head1 DESCRIPTION

=over

=item _validate($schema_name, $data, $file, $doc)

$schema_name is the name of a supported schema. Currently only
C<distroprefs> is supported. $data is the data to be validated. $file
is the absolute path to the file the data are coming from. $doc is the
index of the document within $doc that is to be validated. The last
two arguments are only there for better error reporting.

Relies on being called from within CPAN.pm.

Dies if something fails. Does not return anything useful.

=item yaml($schema_name)

Returns the YAML text of that schema. Dies if something fails.

=back

=head1 AUTHOR

Andreas Koenig C<< <andk@cpan.org> >>

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>



=cut


use strict;

package CPAN::Kwalify;
use vars qw($VERSION $VAR1);
$VERSION = sprintf "%.6f", substr(q$Rev: 1418 $,4)/1000000 + 5.4;

use File::Spec ();

my %vcache = ();

my $schema_loaded = {};

sub _validate {
    my($schema_name,$data,$abs,$y) = @_;
    my $yaml_module = CPAN->_yaml_module;
    if (
        $CPAN::META->has_inst($yaml_module)
        &&
        $CPAN::META->has_inst("Kwalify")
       ) {
        my $load = UNIVERSAL::can($yaml_module,"Load");
        unless ($schema_loaded->{$schema_name}) {
            eval {
                my $schema_yaml = yaml($schema_name);
                $schema_loaded->{$schema_name} = $load->($schema_yaml);
            };
            if ($@) {
                # we know that YAML.pm 0.62 cannot parse the schema,
                # so we try a fallback
                my $content = do {
                    my $path = __FILE__;
                    $path =~ s/\.pm$//;
                    $path = File::Spec->catfile($path, "$schema_name.dd");
                    local *FH;
                    open FH, $path or die "Could not open '$path': $!";
                    local $/;
                    <FH>;
                };
                $VAR1 = undef;
                eval $content;
                die "parsing of '$schema_name.dd' failed: $@" if $@;
                $schema_loaded->{$schema_name} = $VAR1;
            }
        }
    }
    if (my $schema = $schema_loaded->{$schema_name}) {
        my $mtime = (stat $abs)[9];
        for my $k (keys %{$vcache{$abs}}) {
            delete $vcache{$abs}{$k} unless $k eq $mtime;
        }
        return if $vcache{$abs}{$mtime}{$y}++;
        eval { Kwalify::validate($schema, $data) };
        if ($@) {
            die "validation of distropref '$abs'[$y] failed: $@";
        }
    }
}

sub _clear_cache {
    %vcache = ();
}

sub yaml {
    my($schema_name) = @_;
    my $content = do {
        my $path = __FILE__;
        $path =~ s/\.pm$//;
        $path = File::Spec->catfile($path, "$schema_name.yml");
        local *FH;
        open FH, $path or die "Could not open '$path': $!";
        local $/;
        <FH>;
    };
    return $content;
}

1;

# Local Variables:
# mode: cperl
# cperl-indent-level: 4
# End:

