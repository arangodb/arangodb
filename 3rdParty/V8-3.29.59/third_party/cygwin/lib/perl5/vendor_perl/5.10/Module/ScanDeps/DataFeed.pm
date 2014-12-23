package Module::ScanDeps::DataFeed;
# No compile time deps!
#use strict; 
$Module::ScanDeps::DataFeed::VERSION = '0.09';

=head1 NAME

Module::ScanDeps::DataFeed - Runtime dependency scanning helper

=head1 SYNOPSIS

(internal use only)

=head1 DESCRIPTION

No user-serviceable parts inside.

This module is used by the L<Module::ScanDeps> run- and compile-time scanners.
It is included in the code run by C<Module::ScanDeps> and will write
a string of loaded modules and C<@INC> entries to a file. This is
achieved using an C<END {}> hook.

Implementation might change, so don't use it outside of Module::ScanDeps!

=cut

my $_filename;

sub import {
    my ($pkg, $filename) = @_;
    # This is the file we'll write the @INC and %INC info to at END.
    $_filename = $filename;

    my $fname = __PACKAGE__;
    $fname =~ s{::}{/}g;
    delete $INC{"$fname.pm"} unless $Module::ScanDeps::DataFeed::Loaded;
    $Module::ScanDeps::DataFeed::Loaded++;
}

END {
    # Write %INC and @INC to the file specified at compile time in import()
    defined $_filename or return;

    my %inc = %INC;
    my @inc = @INC;
    my @dl_so = _dl_shared_objects();

    require Cwd;
    require File::Basename;
    require DynaLoader;

    open(FH, "> $_filename") or die "Couldn't open $_filename\n";
    print FH '%inchash = (' . "\n\t";
    print FH join(
        ',' => map {
            sprintf(
                "\n\t'$_' => '%s/%s'",
                Cwd::abs_path(File::Basename::dirname($inc{$_})),
                File::Basename::basename($inc{$_}),
            ),
        } keys(%inc)
    );
    print FH "\n);\n";

    print FH '@incarray = (' . "\n\t";
    # inner map escapes trailing backslashes
    print FH join(',', map("\n\t'$_'", map {s/\\$/\\\\/; $_} @inc));
    print FH "\n);\n";

    my @dl_bs = @dl_so;
    s/(\.so|\.dll)$/\.bs/ for @dl_bs;

    print FH '@dl_shared_objects = (' . "\n\t";
    print FH join(
        ',' => map("\n\t'$_'", grep -e, @dl_so, @dl_bs)
    );
    print FH "\n);\n";
    close FH;
}

sub _dl_shared_objects {
    if (@DynaLoader::dl_shared_objects) {
        return @DynaLoader::dl_shared_objects;
    }
    elsif (@DynaLoader::dl_modules) {
        return map { _dl_mod2filename($_) } @DynaLoader::dl_modules;
    }

    return;
}

sub _dl_mod2filename {
    my $mod = shift;

    return if $mod eq 'B';
    return unless defined &{"$mod\::bootstrap"};

    eval { require B; require Config; 1 } or return;
    my $dl_ext = $Config::Config{dlext} if %Config::Config;

    # Copied from XSLoader
    my @modparts = split(/::/, $mod);
    my $modfname = $modparts[-1];
    my $modpname = join('/', @modparts);

    foreach my $dir (@INC) {
        my $file = "$dir/auto/$modpname/$modfname.$dl_ext";
        return $file if -r $file;
    }

    return;
}

1;

=head1 SEE ALSO

L<Module::ScanDeps>

=head1 AUTHORS

Edward S. Peschko E<lt>esp5@pge.comE<gt>,
Audrey Tang E<lt>cpan@audreyt.orgE<gt>,
to a lesser degree Steffen Mueller E<lt>smueller@cpan.orgE<gt>

L<http://par.perl.org/> is the official website for this module.  You
can write to the mailing list at E<lt>par@perl.orgE<gt>, or send an empty
mail to E<lt>par-subscribe@perl.orgE<gt> to participate in the discussion.

Please submit bug reports to E<lt>bug-Module-ScanDeps@rt.cpan.orgE<gt>.

=head1 COPYRIGHT

Copyright 2004-2007 by Edward S. Peschko E<lt>esp5@pge.comE<gt>,
Audrey Tang E<lt>cpan@audreyt.orgE<gt>,
Steffen Mueller E<lt>smueller@cpan.orgE<gt>

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
