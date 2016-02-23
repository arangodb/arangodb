package CPANPLUS::Shell;

use strict;

use CPANPLUS::Error;
use CPANPLUS::Configure;
use CPANPLUS::Internals::Constants;

use Module::Load                qw[load];
use Params::Check               qw[check];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

$Params::Check::VERBOSE = 1;

use vars qw[@ISA $SHELL $DEFAULT];

$DEFAULT    = SHELL_DEFAULT;

=pod

=head1 NAME

CPANPLUS::Shell

=head1 SYNOPSIS

    use CPANPLUS::Shell;             # load the shell indicated by your
                                     # config -- defaults to
                                     # CPANPLUS::Shell::Default

    use CPANPLUS::Shell qw[Classic]  # load CPANPLUS::Shell::Classic;

    my $ui      = CPANPLUS::Shell->new();
    my $name    = $ui->which;        # Find out what shell you loaded

    $ui->shell;                      # run the ui shell


=head1 DESCRIPTION

This module is the generic loading (and base class) for all C<CPANPLUS>
shells. Through this module you can load any installed C<CPANPLUS>
shell.

Just about all the functionality is provided by the shell that you have
loaded, and not by this class (which merely functions as a generic
loading class), so please consult the documentation of your shell of
choice.

=cut

sub import {
    my $class   = shift;
    my $option  = shift;

    ### find out what shell we're supposed to load ###
    $SHELL      = $option
                    ? $class . '::' . $option
                    : do {  ### XXX this should offer to reconfigure 
                            ### CPANPLUS, somehow.  --rs
                            ### XXX load Configure only if we really have to
                            ### as that means any $Conf passed later on will
                            ### be ignored in favour of the one that was 
                            ### retrieved via ->new --kane
                        my $conf = CPANPLUS::Configure->new() or 
                        die loc("No configuration available -- aborting") . $/;
                        $conf->get_conf('shell') || $DEFAULT;
                    };
                    
    ### load the shell, fall back to the default if required
    ### and die if even that doesn't work
    EVAL: {
        eval { load $SHELL };

        if( $@ ) {
            my $err = $@;

            die loc("Your default shell '%1' is not available: %2",
                    $DEFAULT, $err) .
                loc("Check your installation!") . "\n"
                    if $SHELL eq $DEFAULT;

            warn loc("Failed to use '%1': %2", $SHELL, $err),
                 loc("Switching back to the default shell '%1'", $DEFAULT),
                 "\n";

            $SHELL = $DEFAULT;
            redo EVAL;
        }
    }
    @ISA = ($SHELL);
}

sub which { return $SHELL }

1;

###########################################################################
### abstracted out subroutines available to programmers of other shells ###
###########################################################################

package CPANPLUS::Shell::_Base::ReadLine;

use strict;
use vars qw($AUTOLOAD $TMPL);

use FileHandle;
use CPANPLUS::Error;
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

$Params::Check::VERBOSE = 1;


$TMPL = {
    brand           => { default => '', strict_type => 1 },
    prompt          => { default => '> ', strict_type => 1 },
    pager           => { default => '' },
    backend         => { default => '' },
    term            => { default => '' },
    format          => { default => '' },
    dist_format     => { default => '' },
    remote          => { default => undef },
    noninteractive  => { default => '' },
    cache           => { default => [ ] },
    _old_sigpipe    => { default => '', no_override => 1 },
    _old_outfh      => { default => '', no_override => 1 },
    _signals        => { default => { INT => { } }, no_override => 1 },
};

### autogenerate accessors ###
for my $key ( keys %$TMPL ) {
    no strict 'refs';
    *{__PACKAGE__."::$key"} = sub {
        my $self = shift;
        $self->{$key} = $_[0] if @_;
        return $self->{$key};
    }
}

sub _init {
    my $class   = shift;
    my %hash    = @_;

    my $self    = check( $TMPL, \%hash ) or return;

    bless $self, $class;

    ### signal handler ###
    $SIG{INT} = $self->_signals->{INT}->{handler} =
        sub {
            unless ( $self->_signals->{INT}->{count}++ ) {
                warn loc("Caught SIGINT"), "\n";
            } else {
                warn loc("Got another SIGINT"), "\n"; die;
            }
        };
    ### end sig handler ###

    return $self;
}

### display shell's banner, takes the Backend object as argument
sub _show_banner {
    my $self = shift;
    my $cpan = $self->backend;
    my $term = $self->term;

    ### Tries to probe for our ReadLine support status
    # a) under an interactive shell?
    my $rl_avail = (!$term->isa('CPANPLUS::Shell::_Faked'))
        # b) do we have a tty terminal?
        ? (-t STDIN)
            # c) should we enable the term?
            ? (!$self->__is_bad_terminal($term))
                # d) external modules available?
                ? ($term->ReadLine ne "Term::ReadLine::Stub")
                    # a+b+c+d => "Smart" terminal
                    ? loc("enabled")
                    # a+b+c => "Stub" terminal
                    : loc("available (try 'i Term::ReadLine::Perl')")
                # a+b => "Bad" terminal
                : loc("disabled")
            # a => "Dumb" terminal
            : loc("suppressed")
        # none    => "Faked" terminal
        : loc("suppressed in batch mode");

    $rl_avail = loc("ReadLine support %1.", $rl_avail);
    $rl_avail = "\n*** $rl_avail" if (length($rl_avail) > 45);

    $self->__print(
          loc("%1 -- CPAN exploration and module installation (v%2)",
                $self->which, $self->which->VERSION()), "\n",
          loc("*** Please report bugs to <bug-cpanplus\@rt.cpan.org>."), "\n",
          loc("*** Using CPANPLUS::Backend v%1.  %2",
                $cpan->VERSION, $rl_avail), "\n\n"
    );
}

### checks whether the Term::ReadLine is broken and needs to fallback to Stub
sub __is_bad_terminal {
    my $self = shift;
    my $term = $self->term;

    return unless $^O eq 'MSWin32';

    ### replace the term with the default (stub) one
    return $self->term(Term::ReadLine::Stub->new( $self->brand ) );
}

### open a pager handle
sub _pager_open {
    my $self  = shift;
    my $cpan  = $self->backend;
    my $cmd   = $cpan->configure_object->get_program('pager') or return;

    $self->_old_sigpipe( $SIG{PIPE} );
    $SIG{PIPE} = 'IGNORE';

    my $fh = new FileHandle;
    unless ( $fh->open("| $cmd") ) {
        error(loc("could not pipe to %1: %2\n", $cmd, $!) );
        return;
    }

    $fh->autoflush(1);

    $self->pager( $fh );
    $self->_old_outfh( select $fh );

    return $fh;
}

### print to the current pager handle, or STDOUT if it's not opened
sub _pager_close {
    my $self  = shift;
    my $pager = $self->pager or return;

    $pager->close if (ref($pager) and $pager->can('close'));

    $self->pager( undef );

    select $self->_old_outfh;
    $SIG{PIPE} = $self->_old_sigpipe;

    return 1;
}



{
    my $win32_console;

    ### determines row count of current terminal; defaults to 25.
    ### used by the pager functions
    sub _term_rowcount {
        my $self = shift;
        my $cpan = $self->backend;
        my %hash = @_;

        my $default;
        my $tmpl = {
            default => { default => 25, allow => qr/^\d$/,
                         store => \$default }
        };

        check( $tmpl, \%hash ) or return;

        if ( $^O eq 'MSWin32' ) {
            if ( can_load( modules => { 'Win32::Console' => '0.0' } ) ) {
                $win32_console ||= Win32::Console->new();
                my $rows = ($win32_console->Info)[-1];
                return $rows;
            }

        } else {
            local $Module::Load::Conditional::VERBOSE = 0;
            if ( can_load(modules => {'Term::Size' => '0.0'}) ) {
                my ($cols, $rows) = Term::Size::chars();
                return $rows;
            }
        }
        return $default;
    }
}

### Custom print routines, mainly to be able to catch output
### in test cases, or redirect it if need be
{   sub __print {
        my $self = shift;
        print @_;
    }
    
    sub __printf {
        my $self = shift;
        my $fmt  = shift;
        
        ### MUST specify $fmt as a seperate param, and not as part
        ### of @_, as it will then miss the $fmt and return the 
        ### number of elements in the list... =/ --kane
        $self->__print( sprintf( $fmt, @_ ) );
    }
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

L<CPANPLUS::Shell::Default>, L<CPANPLUS::Shell::Classic>, L<cpanp>

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

