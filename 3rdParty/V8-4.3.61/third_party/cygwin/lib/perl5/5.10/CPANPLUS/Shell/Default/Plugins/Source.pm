package CPANPLUS::Shell::Default::Plugins::Source;

use strict;
use CPANPLUS::Error             qw[error msg];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

=head1 NAME

CPANPLUS::Shell::Default::Plugins::Source 

=head1 SYNOPSIS

    CPAN Terminal> /source /tmp/list_of_commands /tmp/more_commands

=head1 DESCRIPTION

This is a C<CPANPLUS::Shell::Default> plugin that works just like
your unix shells source(1) command; it reads in a file that has
commands in it to execute, and then executes them.

A sample file might look like this:

    # first, update all the source files
    x --update_source

    # find all of my modules that are on the CPAN 
    # test them, and store the error log
    a ^KANE$'
    t *
    p /home/kane/cpan-autotest/log
    
    # and inform us we're good to go
    ! print "Autotest complete, log stored; please enter your commands!"

Note how empty lines, and lines starting with a '#' are being skipped
in the execution.

=cut


sub plugins { return ( source => 'source' ) }

sub source {
    my $class   = shift;
    my $shell   = shift;
    my $cb      = shift;
    my $cmd     = shift;
    my $input   = shift || '';
    my $opts    = shift || {};
    my $verbose = $cb->configure_object->get_conf('verbose');
    
    for my $file ( split /\s+/, $input ) {
        my $fh = FileHandle->new("$file") or( 
            error(loc("Could not open file '%1': %2", $file, $!)),
            next
        );
        
        while( my $line = <$fh> ) {
            chomp $line;
            
            next if $line !~ /\S+/; # skip empty/whitespace only lines
            next if $line =~ /^#/;  # skip comments
            
            msg(loc("Dispatching '%1'", $line), $verbose); 
            return 1 if $shell->dispatch_on_input( input => $line );
        }
    }
}

sub source_help {
    return loc('    /source FILE [FILE ..] '.
               '# read in commands from the specified file' ),
}

1;

=pod

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-cpanplus@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

The CPAN++ interface (of which this module is a part of) is copyright (c) 
2001 - 2007, Jos Boumans E<lt>kane@cpan.orgE<gt>. All rights reserved.

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

L<CPANPLUS::Shell::Default>, L<CPANPLUS::Shell>, L<cpanp>

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

