package CPANPLUS;

use strict;
use Carp;

use CPANPLUS::Error;
use CPANPLUS::Backend;

use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

BEGIN {
    use Exporter    ();
    use vars        qw( @EXPORT @ISA $VERSION );
    @EXPORT     =   qw( shell fetch get install );
    @ISA        =   qw( Exporter );
    $VERSION = "0.84";     #have to hardcode or cpan.org gets unhappy
}

### purely for backward compatibility, so we can call it from the commandline:
### perl -MCPANPLUS -e 'install Net::SMTP'
sub install {
    my $cpan = CPANPLUS::Backend->new;
    my $mod = shift or (
                    error(loc("No module specified!")), return
                );

    if ( ref $mod ) {
        error( loc( "You passed an object. Use %1 for OO style interaction",
                    'CPANPLUS::Backend' ));
        return;

    } else {
        my $obj = $cpan->module_tree($mod) or (
                        error(loc("No such module '%1'", $mod)),
                        return
                    );

        my $ok = $obj->install;

        $ok
            ? msg(loc("Installing of %1 successful", $mod),1)
            : msg(loc("Installing of %1 failed", $mod),1);

        return $ok;
    }
}

### simply downloads a module and stores it
sub fetch {
    my $cpan = CPANPLUS::Backend->new;

    my $mod = shift or (
                    error(loc("No module specified!")), return
                );

    if ( ref $mod ) {
        error( loc( "You passed an object. Use %1 for OO style interaction",
                    'CPANPLUS::Backend' ));
        return;

    } else {
        my $obj = $cpan->module_tree($mod) or (
                        error(loc("No such module '%1'", $mod)),
                        return
                    );

        my $ok = $obj->fetch( fetchdir => '.' );

        $ok
            ? msg(loc("Fetching of %1 successful", $mod),1)
            : msg(loc("Fetching of %1 failed", $mod),1);

        return $ok;
    }
}

### alias to fetch() due to compatibility with cpan.pm ###
sub get { fetch(@_) }


### purely for backwards compatibility, so we can call it from the commandline:
### perl -MCPANPLUS -e 'shell'
sub shell {
    my $option  = shift;

    ### since the user can specify the type of shell they wish to start
    ### when they call the shell() function, we have to eval the usage
    ### of CPANPLUS::Shell so we can set up all the checks properly
    eval { require CPANPLUS::Shell; CPANPLUS::Shell->import($option) };
    die $@ if $@;

    my $cpan = CPANPLUS::Shell->new();

    $cpan->shell();
}

1;

__END__

=pod

=head1 NAME

CPANPLUS - API & CLI access to the CPAN mirrors

=head1 SYNOPSIS

    ### standard invocation from the command line
    $ cpanp
    $ cpanp -i Some::Module

    $ perl -MCPANPLUS -eshell
    $ perl -MCPANPLUS -e'fetch Some::Module'

    
=head1 DESCRIPTION

The C<CPANPLUS> library is an API to the C<CPAN> mirrors and a
collection of interactive shells, commandline programs, etc,
that use this API.

=head1 GUIDE TO DOCUMENTATION

=head2 GENERAL USAGE

This is the document you are currently reading. It describes 
basic usage and background information. Its main purpose is to 
assist the user who wants to learn how to invoke CPANPLUS
and install modules from the commandline and to point you
to more indepth reading if required.

=head2 API REFERENCE

The C<CPANPLUS> API is meant to let you programmatically 
interact with the C<CPAN> mirrors. The documentation in
L<CPANPLUS::Backend> shows you how to create an object
capable of interacting with those mirrors, letting you
create & retrieve module objects.
L<CPANPLUS::Module> shows you how you can use these module
objects to perform actions like installing and testing. 

The default shell, documented in L<CPANPLUS::Shell::Default>
is also scriptable. You can use its API to dispatch calls
from your script to the CPANPLUS Shell.

=cut

=head1 COMMANDLINE TOOLS

=head2 STARTING AN INTERACTIVE SHELL

You can start an interactive shell by running either of 
the two following commands:

    $ cpanp

    $ perl -MCPANPLUS -eshell

All commans available are listed in the interactive shells
help menu. See C<cpanp -h> or L<CPANPLUS::Shell::Default> 
for instructions on using the default shell.  
    
=head2 CHOOSE A SHELL

By running C<cpanp> without arguments, you will start up
the shell specified in your config, which defaults to 
L<CPANPLUS::Shell::Default>. There are more shells available.
C<CPANPLUS> itself ships with an emulation shell called 
L<CPANPLUS::Shell::Classic> that looks and feels just like 
the old C<CPAN.pm> shell.

You can start this shell by typing:

    $ perl -MCPANPLUS -e'shell Classic'
    
Even more shells may be available from C<CPAN>.    

Note that if you have changed your default shell in your
configuration, that shell will be used instead. If for 
some reason there was an error with your specified shell, 
you will be given the default shell.

=head2 BUILDING PACKAGES

C<cpan2dist> is a commandline tool to convert any distribution 
from C<CPAN> into a package in the format of your choice, like
for example C<.deb> or C<FreeBSD ports>. 

See C<cpan2dist -h> for details.
    
    
=head1 FUNCTIONS

For quick access to common commands, you may use this module,
C<CPANPLUS> rather than the full programmatic API situated in
C<CPANPLUS::Backend>. This module offers the following functions:

=head2 $bool = install( Module::Name | /A/AU/AUTHOR/Module-Name-1.tgz )

This function requires the full name of the module, which is case
sensitive.  The module name can also be provided as a fully
qualified file name, beginning with a I</>, relative to
the /authors/id directory on a CPAN mirror.

It will download, extract and install the module.

=head2 $where = fetch( Module::Name | /A/AU/AUTHOR/Module-Name-1.tgz )

Like install, fetch needs the full name of a module or the fully
qualified file name, and is case sensitive.

It will download the specified module to the current directory.

=head2 $where = get( Module::Name | /A/AU/AUTHOR/Module-Name-1.tgz )

Get is provided as an alias for fetch for compatibility with
CPAN.pm.

=head2 shell()

Shell starts the default CPAN shell.  You can also start the shell
by using the C<cpanp> command, which will be installed in your
perl bin.

=head1 FAQ

For frequently asked questions and answers, please consult the
C<CPANPLUS::FAQ> manual.

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

L<CPANPLUS::Shell::Default>, L<CPANPLUS::FAQ>, L<CPANPLUS::Backend>, L<CPANPLUS::Module>, L<cpanp>, L<cpan2dist>

=head1 CONTACT INFORMATION

=over 4

=item * Bug reporting:
I<bug-cpanplus@rt.cpan.org>

=item * Questions & suggestions:
I<cpanplus-devel@lists.sourceforge.net>

=back


=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
