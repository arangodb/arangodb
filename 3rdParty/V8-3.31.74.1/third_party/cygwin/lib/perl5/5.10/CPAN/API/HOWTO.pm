=head1 NAME

CPAN::API::HOWTO - a recipe book for programming with CPAN.pm

=head1 RECIPES

All of these recipes assume that you have put "use CPAN" at the top of
your program.

=head2 What distribution contains a particular module?

    my $distribution = CPAN::Shell->expand(
        "Module", "Data::UUID"
    )->distribution()->pretty_id();

This returns a string of the form "AUTHORID/TARBALL".  If you want the
full path and filename to this distribution on a CPAN mirror, then it is
C<.../authors/id/A/AU/AUTHORID/TARBALL>.

=head2 What modules does a particular distribution contain?

    CPAN::Index->reload();
    my @modules = CPAN::Shell->expand(
        "Distribution", "JHI/Graph-0.83.tar.gz"
    )->containsmods();

You may also refer to a distribution in the form A/AU/AUTHORID/TARBALL.

=head1 SEE ALSO

the main CPAN.pm documentation

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=head1 AUTHOR

David Cantrell

=cut
