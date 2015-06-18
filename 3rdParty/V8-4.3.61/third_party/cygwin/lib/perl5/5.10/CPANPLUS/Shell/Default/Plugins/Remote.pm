package CPANPLUS::Shell::Default::Plugins::Remote;

use strict;

use Module::Load;
use Params::Check               qw[check];
use CPANPLUS::Error             qw[error msg];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

=head1 NAME

CPANPLUS::Shell::Default::Plugins::Remote

=head1 SYNOPSIS

    CPAN Terminal> /connect localhost 1337 --user=foo --pass=bar
    ...
    CPAN Terminal@localhost> /disconnect

=head1 DESCRIPTION

This is a C<CPANPLUS::Shell::Default> plugin that allows you to connect
to a machine running an instance of C<CPANPLUS::Daemon>, allowing remote
usage of the C<CPANPLUS Shell>.

A sample session, updating all modules on a remote machine, might look
like this:

    CPAN Terminal> /connect --user=my_user --pass=secret localhost 1337

    Connection accepted
    
    Successfully connected to 'localhost' on port '11337'
    
    Note that no output will appear until a command has completed
    -- this may take a while


    CPAN Terminal@localhost> o; i *
    
    [....]
    
    CPAN Terminal@localhost> /disconnect

    CPAN Terminal>

=cut

### store the original prompt here, so we can restore it on disconnect
my $Saved_Prompt;

sub plugins { ( connect => 'connect', disconnect => 'disconnect' ) }

sub connect {
    my $class   = shift;
    my $shell   = shift;
    my $cb      = shift;
    my $cmd     = shift;
    my $input   = shift || '';
    my $opts    = shift || {};
    my $conf = $cb->configure_object;

    my $user; my $pass;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            user => { default   => 'cpanpd',    store => \$user },
            pass => { required  => 1,           store => \$pass },
        };

         check( $tmpl, $opts ) or return;
    }

    my @parts = split /\s+/, $input;
    my $host = shift @parts || 'localhost';
    my $port = shift @parts || '1337';

    load IO::Socket;

    my $remote = IO::Socket::INET->new(
                        Proto       => "tcp",
                        PeerAddr    => $host,
                        PeerPort    => $port,
                    ) or (
                        error( loc( "Cannot connect to port '%1' ".
                                    "on host '%2'", $port, $host ) ),
                        return
                    );

    my $con = {
        connection  => $remote,
        username    => $user,
        password    => $pass,
    };

    ### store the connection
    $shell->remote( $con );

    my($status,$buffer) = $shell->__send_remote_command(
                            "VERSION=$CPANPLUS::Shell::Default::VERSION");

    if( $status ) {
        print "\n$buffer\n\n";

        print loc(  "Successfully connected to '%1' on port '%2'",
                    $host, $port );
        print "\n\n";
        print loc(  "Note that no output will appear until a command ".
                    "has completed\n-- this may take a while" );
        print "\n\n";

        ### save the original prompt
        $Saved_Prompt = $shell->prompt;

        $shell->prompt( $shell->brand .'@'. $host .':'. $port .'> ' );

    } else {
        print "\n$buffer\n\n";

        print loc(  "Failed to connect to '%1' on port '%2'",
                    $host, $port );
        print "\n\n";

        $shell->remote( undef );
    }
}

sub disconnect {
    my $class   = shift;
    my $shell   = shift;

    print "\n", ( $shell->remote
                    ? loc( "Disconnecting from remote host" )
                    : loc( "Not connected to remote host" )
            ), "\n\n";

    $shell->remote( undef );
    $shell->prompt( $Saved_Prompt );
}

sub connect_help {
    return loc( 
            "    /connect [HOST PORT]   # Connect to the remote machine,\n" .
            "                           # defaults taken from your config\n" .
            "        --user=USER        # Optional username\n" .
            "        --pass=PASS        # Optional password" );
}

sub disconnect_help {
    return loc(
            "    /disconnect            # Disconnect from the remote server" );
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

