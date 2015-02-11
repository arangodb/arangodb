package CPANPLUS::Shell::Default;

use strict;


use CPANPLUS::Error;
use CPANPLUS::Backend;
use CPANPLUS::Configure::Setup;
use CPANPLUS::Internals::Constants;
use CPANPLUS::Internals::Constants::Report qw[GRADE_FAIL];

use Cwd;
use IPC::Cmd;
use Term::UI;
use Data::Dumper;
use Term::ReadLine;

use Module::Load                qw[load];
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load check_install];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

local $Params::Check::VERBOSE   = 1;
local $Data::Dumper::Indent     = 1; # for dumpering from !

BEGIN {
    use vars        qw[ $VERSION @ISA ];
    @ISA        =   qw[ CPANPLUS::Shell::_Base::ReadLine ];
    $VERSION = "0.84";
}

load CPANPLUS::Shell;


my $map = {
    'm'     => '_search_module',
    'a'     => '_search_author',
    '!'     => '_bang',
    '?'     => '_help',
    'h'     => '_help',
    'q'     => '_quit',
    'r'     => '_readme',
    'v'     => '_show_banner',
    'w'     => '__display_results',
    'd'     => '_fetch',
    'z'     => '_shell',
    'f'     => '_distributions',
    'x'     => '_reload_indices',
    'i'     => '_install',
    't'     => '_install',
    'l'     => '_details',
    'p'     => '_print',
    's'     => '_set_conf',
    'o'     => '_uptodate',
    'b'     => '_autobundle',
    'u'     => '_uninstall',
    '/'     => '_meta',         # undocumented for now
    'c'     => '_reports',
};
### free letters: e g j k n y ###


### will be filled if you have a .default-shell.rc and
### Config::Auto installed
my $rc = {};

### the shell object, scoped to the file ###
my $Shell;
my $Brand   = loc('CPAN Terminal');
my $Prompt  = $Brand . '> ';

=pod

=head1 NAME

CPANPLUS::Shell::Default

=head1 SYNOPSIS

    ### loading the shell:
    $ cpanp                     # run 'cpanp' from the command line
    $ perl -MCPANPLUS -eshell   # load the shell from the command line


    use CPANPLUS::Shell qw[Default];        # load this shell via the API
                                            # always done via CPANPLUS::Shell

    my $ui = CPANPLUS::Shell->new;
    $ui->shell;                             # run the shell
    $ui->dispatch_on_input( input => 'x');  # update the source using the
                                            # dispatch method

    ### when in the shell:
    ### Note that all commands can also take options.
    ### Look at their underlying CPANPLUS::Backend methods to see
    ### what options those are.
    cpanp> h                 # show help messages
    cpanp> ?                 # show help messages

    cpanp> m Acme            # find acme modules, allows regexes
    cpanp> a KANE            # find modules by kane, allows regexes
    cpanp> f Acme::Foo       # get a list of all releases of Acme::Foo

    cpanp> i Acme::Foo       # install Acme::Foo
    cpanp> i Acme-Foo-1.3    # install version 1.3 of Acme::Foo
    cpanp> i <URI>           # install from URI, like ftp://foo.com/X.tgz
    cpanp> i 1 3..5          # install search results 1, 3, 4 and 5
    cpanp> i *               # install all search results
    cpanp> a KANE; i *;      # find modules by kane, install all results
    cpanp> t Acme::Foo       # test Acme::Foo, without installing it
    cpanp> u Acme::Foo       # uninstall Acme::Foo
    cpanp> d Acme::Foo       # download Acme::Foo
    cpanp> z Acme::Foo       # download & extract Acme::Foo, then open a
                             # shell in the extraction directory

    cpanp> c Acme::Foo       # get a list of test results for Acme::Foo
    cpanp> l Acme::Foo       # view details about the Acme::Foo package
    cpanp> r Acme::Foo       # view Acme::Foo's README file
    cpanp> o                 # get a list of all installed modules that
                             # are out of date
    cpanp> o 1..3            # list uptodateness from a previous search 
                            
    cpanp> s conf            # show config settings
    cpanp> s conf md5 1      # enable md5 checks
    cpanp> s program         # show program settings
    cpanp> s edit            # edit config file
    cpanp> s reconfigure     # go through initial configuration again
    cpanp> s selfupdate      # update your CPANPLUS install
    cpanp> s save            # save config to disk
    cpanp> s mirrors         # show currently selected mirrors

    cpanp> ! [PERL CODE]     # execute the following perl code

    cpanp> b                 # create an autobundle for this computers
                             # perl installation
    cpanp> x                 # reload index files (purges cache)
    cpanp> x --update_source # reload index files, get fresh source files
    cpanp> p [FILE]          # print error stack (to a file)
    cpanp> v                 # show the banner
    cpanp> w                 # show last search results again

    cpanp> q                 # quit the shell

    cpanp> /plugins          # list avialable plugins
    cpanp> /? PLUGIN         # list help test of <PLUGIN>                  

    ### common options:
    cpanp> i ... --skiptest # skip tests
    cpanp> i ... --force    # force all operations
    cpanp> i ... --verbose  # run in verbose mode

=head1 DESCRIPTION

This module provides the default user interface to C<CPANPLUS>. You
can start it via the C<cpanp> binary, or as detailed in the L<SYNOPSIS>.

=cut

sub new {
    my $class   = shift;

    my $cb      = CPANPLUS::Backend->new( @_ );
    my $self    = $class->SUPER::_init(
                            brand       => $Brand,
                            term        => Term::ReadLine->new( $Brand ),
                            prompt      => $Prompt,
                            backend     => $cb,
                            format      => "%4s %-55s %8s %-10s\n",
                            dist_format => "%4s %-42s %-12s %8s %-10s\n",
                        );
    ### make it available package wide ###
    $Shell = $self;

    my $rc_file = File::Spec->catfile(
                        $cb->configure_object->get_conf('base'),
                        DOT_SHELL_DEFAULT_RC,
                    );


    if( -e $rc_file && -r _ ) {
        $rc = $self->_read_configuration_from_rc( $rc_file );
    }

    ### register install callback ###
    $cb->_register_callback(
            name    => 'install_prerequisite',
            code    => \&__ask_about_install,
    );

    ### execute any login commands specified ###
    $self->dispatch_on_input( input => $rc->{'login'} )
            if defined $rc->{'login'};

    ### register test report callbacks ###
    $cb->_register_callback(
            name    => 'edit_test_report',
            code    => \&__ask_about_edit_test_report,
    );

    $cb->_register_callback(
            name    => 'send_test_report',
            code    => \&__ask_about_send_test_report,
    );

    $cb->_register_callback(
            name    => 'proceed_on_test_failure',
            code    => \&__ask_about_test_failure,
    );

    ### load all the plugins
    $self->_plugins_init;

    return $self;
}

sub shell {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->backend->configure_object;

    $self->_show_banner;
    $self->__print( "*** Type 'p' now to show start up log\n" ); # XXX add to banner?
    $self->_show_random_tip if $conf->get_conf('show_startup_tip');
    $self->_input_loop && $self->__print( "\n" );
    $self->_quit;
}

sub _input_loop {
    my $self    = shift;
    my $term    = $self->term;
    my $cb      = $self->backend;

    my $normal_quit = 0;
    while (
        defined (my $input = eval { $term->readline($self->prompt) } )
        or $self->_signals->{INT}{count} == 1
    ) {
        ### re-initiate all signal handlers
        while (my ($sig, $entry) = each %{$self->_signals} ) {
            $SIG{$sig} = $entry->{handler} if exists($entry->{handler});
        }

        $self->__print( "\n" );
        last if $self->dispatch_on_input( input => $input );

        ### flush the lib cache ###
        $cb->_flush( list => [qw|lib load|] );

    } continue {
        $self->_signals->{INT}{count}--
            if $self->_signals->{INT}{count}; # clear the sigint count
    }

    return 1;
}

### return 1 to quit ###
sub dispatch_on_input {
    my $self = shift;
    my $conf = $self->backend->configure_object();
    my $term = $self->term;
    my %hash = @_;

    my($string, $noninteractive);
    my $tmpl = {
        input          => { required => 1, store => \$string },
        noninteractive => { required => 0, store => \$noninteractive },
    };

    check( $tmpl, \%hash ) or return;

    ### indicates whether or not the user will receive a shell
    ### prompt after the command has finished.
    $self->noninteractive($noninteractive) if defined $noninteractive;

    my @cmds =  split ';', $string;
    while( my $input = shift @cmds ) {

        ### to send over the socket ###
        my $org_input = $input;

        my $key; my $options;
        {   ### make whitespace not count when using special chars
            { $input =~ s|^\s*([!?/])|$1 |; }

            ### get the first letter of the input
            $input =~ s|^\s*([\w\?\!/])\w*||;

            chomp $input;
            $key =  lc($1);

            ### we figured out what the command was...
            ### if we have more input, that DOES NOT start with a white
            ### space char, we misparsed.. like 'Test::Foo::Bar', which
            ### would turn into 't', '::Foo::Bar'...
            if( $input and $input !~ s/^\s+// ) {
                $self->__print( loc("Could not understand command: %1\n".
                          "Possibly missing command before argument(s)?\n",
                          $org_input) ); 
                return;
            }     

            ### allow overrides from the config file ###
            if( defined $rc->{$key} ) {
                $input = $rc->{$key} . $input;
            }

            ### grab command line options like --no-force and --verbose ###
            ($options,$input) = $term->parse_options($input)
                unless $key eq '!';
        }

        ### emtpy line? ###
        return unless $key;

        ### time to quit ###
        return 1 if $key eq 'q';

        my $method = $map->{$key};

        ### dispatch meta locally at all times ###
        $self->$method(input => $input, options => $options), next
            if $key eq '/';

        ### flush unless we're trying to print the stack
        CPANPLUS::Error->flush unless $key eq 'p';

        ### connected over a socket? ###
        if( $self->remote ) {

            ### unsupported commands ###
            if( $key eq 'z' or
                ($key eq 's' and $input =~ /^\s*edit/)
            ) {
                $self->__print( "\n", 
                      loc(  "Command '%1' not supported over remote connection",
                            join ' ', $key, $input 
                      ), "\n\n" );

            } else {
                my($status,$buff) = $self->__send_remote_command($org_input);

                $self->__print( "\n", loc("Command failed!"), "\n\n" )
                    unless $status;

                $self->_pager_open if $buff =~ tr/\n// > $self->_term_rowcount;
                $self->__print( $buff );
                $self->_pager_close;
            }

        ### or just a plain local shell? ###
        } else {

            unless( $self->can($method) ) {
                $self->__print(loc("Unknown command '%1'. Usage:", $key), "\n");
                $self->_help;

            } else {

                ### some methods don't need modules ###
                my @mods;
                @mods = $self->_select_modules($input)
                        unless grep {$key eq $_} qw[! m a v w x p s b / ? h];

                eval { $self->$method(  modules => \@mods,
                                        options => $options,
                                        input   => $input,
                                        choice  => $key )
                };
                error( $@ ) if $@;
            }
        }
    }

    return;
}

sub _select_modules {
    my $self    = shift;
    my $input   = shift or return;
    my $cache   = $self->cache;
    my $cb      = $self->backend;

    ### expand .. in $input
    $input =~ s{\b(\d+)\s*\.\.\s*(\d+)\b}
               {join(' ', ($1 < 1 ? 1 : $1) .. ($2 > $#{$cache} ? $#{$cache} : $2))}eg;

    $input = join(' ', 1 .. $#{$cache}) if $input eq '*';
    $input =~ s/'/::/g; # perl 4 convention

    my @rv;
    for my $mod (split /\s+/, $input) {

        ### it's a cache look up ###
        if( $mod =~ /^\d+/ and $mod > 0 ) {
            unless( scalar @$cache ) {
                $self->__print( loc("No search was done yet!"), "\n" );

            } elsif ( my $obj = $cache->[$mod] ) {
                push @rv, $obj;

            } else {
                $self->__print( loc("No such module: %1", $mod), "\n" );
            }

        } else {
            my $obj = $cb->parse_module( module => $mod );

            unless( $obj ) {
                $self->__print( loc("No such module: %1", $mod), "\n" );

            } else {
                push @rv, $obj;
            }
        }
    }

    unless( scalar @rv ) {
        $self->__print( loc("No modules found to operate on!\n") );
        return;
    } else {
        return @rv;
    }
}

sub _format_version {
    my $self    = shift;
    my $version = shift;

    ### fudge $version into the 'optimal' format
    $version = 0 if $version eq 'undef';
    $version =~ s/_//g; # everything after gets stripped off otherwise

    ### allow 6 digits after the dot, as that's how perl stringifies
    ### x.y.z numbers.
    $version = sprintf('%3.6f', $version);
    $version = '' if $version == '0.00';
    $version =~ s/(00{0,3})$/' ' x (length $1)/e;

    return $version;
}

sub __display_results {
    my $self    = shift;
    my $cache   = $self->cache;

    my @rv = @$cache;

    if( scalar @rv ) {

        $self->_pager_open if $#{$cache} >= $self->_term_rowcount;

        my $i = 1;
        for my $mod (@rv) {
            next unless $mod;   # first one is undef
                                # humans start counting at 1

            ### for dists only -- we have checksum info
            if( $mod->mtime ) {
                $self->__printf(
                    $self->dist_format,
                    $i,
                    $mod->module,
                    $mod->mtime,
                    $self->_format_version( $mod->version ),
                    $mod->author->cpanid
                );

            } else {
                $self->__printf(
                    $self->format,
                    $i,
                    $mod->module,
                    $self->_format_version( $mod->version ),
                    $mod->author->cpanid
                );
            }
            $i++;
        }

        $self->_pager_close;

    } else {
        $self->__print( loc("No results to display"), "\n" );
    }
}


sub _quit {
    my $self = shift;

    $self->dispatch_on_input( input => $rc->{'logout'} )
            if defined $rc->{'logout'};

    $self->__print( loc("Exiting CPANPLUS shell"), "\n" );
}

###########################
### actual command subs ###
###########################


### print out the help message ###
### perhaps, '?' should be a slightly different version ###
{   my @help;
    sub _help {
        my $self = shift;
        my %hash    = @_;
    
        my $input;
        {   local $Params::Check::ALLOW_UNKNOWN = 1;
    
            my $tmpl = {
                input   => { required => 0, store => \$input }
            };
    
            my $args = check( $tmpl, \%hash ) or return;
        }
    
        @help = (
loc('[General]'                                                                     ),
loc('    h | ?                  # display help'                                     ),
loc('    q                      # exit'                                             ),
loc('    v                      # version information'                              ),
loc('[Search]'                                                                      ),
loc('    a AUTHOR ...           # search by author(s)'                              ),
loc('    m MODULE ...           # search by module(s)'                              ),
loc('    f MODULE ...           # list all releases of a module'                    ),
loc("    o [ MODULE ... ]       # list installed module(s) that aren't up to date"  ),
loc('    w                      # display the result of your last search again'     ),
loc('[Operations]'                                                                  ),
loc('    i MODULE | NUMBER ...  # install module(s), by name or by search number'   ),
loc('    i URI | ...            # install module(s), by URI (ie http://foo.com/X.tgz)'   ),
loc('    t MODULE | NUMBER ...  # test module(s), by name or by search number'      ),
loc('    u MODULE | NUMBER ...  # uninstall module(s), by name or by search number' ),
loc('    d MODULE | NUMBER ...  # download module(s)'                               ),
loc('    l MODULE | NUMBER ...  # display detailed information about module(s)'     ),
loc('    r MODULE | NUMBER ...  # display README files of module(s)'                ),
loc('    c MODULE | NUMBER ...  # check for module report(s) from cpan-testers'     ),
loc('    z MODULE | NUMBER ...  # extract module(s) and open command prompt in it'  ),
loc('[Local Administration]'                                                        ),
loc('    b                      # write a bundle file for your configuration'       ),
loc('    s program [OPT VALUE]  # set program locations for this session'           ),
loc('    s conf    [OPT VALUE]  # set config options for this session'              ),
loc('    s mirrors              # show currently selected mirrors' ),
loc('    s reconfigure          # reconfigure settings ' ),
loc('    s selfupdate           # update your CPANPLUS install '),
loc('    s save [user|system]   # save settings for this user or systemwide' ),
loc('    s edit [user|system]   # open configuration file in editor and reload'     ),
loc('    ! EXPR                 # evaluate a perl statement'                        ),
loc('    p [FILE]               # print the error stack (optionally to a file)'     ),
loc('    x                      # reload CPAN indices (purges cache)'                              ),
loc('    x --update_source      # reload CPAN indices, get fresh source files' ),
loc('[Common Options]'                                  ),
loc('   i ... --skiptest        # skip tests'           ),
loc('   i ... --force           # force all operations' ),
loc('   i ... --verbose         # run in verbose mode'  ),
loc('[Plugins]'                                                             ),
loc('   /plugins                # list available plugins'                   ),
loc('   /? [PLUGIN NAME]        # show usage for (a particular) plugin(s)'  ),

        ) unless @help;
    
        $self->_pager_open if (@help >= $self->_term_rowcount);
        ### XXX: functional placeholder for actual 'detailed' help.
        $self->__print( "Detailed help for the command '$input' is " .
                        "not available.\n\n" ) if length $input;
        $self->__print( map {"$_\n"} @help );
        $self->__print( $/ );
        $self->_pager_close;
    }
}

### eval some code ###
sub _bang {
    my $self    = shift;
    my $cb      = $self->backend;
    my %hash    = @_;


    my $input;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            input   => { required => 1, store => \$input }
        };

        my $args = check( $tmpl, \%hash ) or return;
    }

    local $Data::Dumper::Indent     = 1; # for dumpering from !
    eval $input;
    error( $@ ) if $@;
    $self->__print( "\n" );
    return;
}

sub _search_module {
    my $self    = shift;
    my $cb      = $self->backend;
    my %hash    = @_;

    my $args;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            input   => { required => 1, },
            options => { default => { } },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my @regexes = map { qr/$_/i } split /\s+/, $args->{'input'};

    ### XXX this is rather slow, because (probably)
    ### of the many method calls
    ### XXX need to profile to speed it up =/

    ### find the modules ###
    my @rv = sort { $a->module cmp $b->module }
                    $cb->search(
                        %{$args->{'options'}},
                        type    => 'module',
                        allow   => \@regexes,
                    );

    ### store the result in the cache ###
    $self->cache([undef,@rv]);

    $self->__display_results;

    return 1;
}

sub _search_author {
    my $self    = shift;
    my $cb      = $self->backend;
    my %hash    = @_;

    my $args;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            input   => { required => 1, },
            options => { default => { } },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my @regexes = map { qr/$_/i } split /\s+/, $args->{'input'};

    my @rv;
    for my $type (qw[author cpanid]) {
        push @rv, $cb->search(
                        %{$args->{'options'}},
                        type    => $type,
                        allow   => \@regexes,
                    );
    }

    my %seen;
    my @list =  sort { $a->module cmp $b->module }
                grep { defined }
                map  { $_->modules }
                grep { not $seen{$_}++ } @rv;

    $self->cache([undef,@list]);

    $self->__display_results;
    return 1;
}

sub _readme {
    my $self    = shift;
    my $cb      = $self->backend;
    my %hash    = @_;

    my $args; my $mods; my $opts;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            modules => { required => 1,  store => \$mods },
            options => { default => { }, store => \$opts },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    return unless scalar @$mods;

    $self->_pager_open;
    for my $mod ( @$mods ) {
        $self->__print( $mod->readme( %$opts ) );
    }

    $self->_pager_close;

    return 1;
}

sub _fetch {
    my $self    = shift;
    my $cb      = $self->backend;
    my %hash    = @_;

    my $args; my $mods; my $opts;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            modules => { required => 1,  store => \$mods },
            options => { default => { }, store => \$opts },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    $self->_pager_open if @$mods >= $self->_term_rowcount;
    for my $mod (@$mods) {
        my $where = $mod->fetch( %$opts );

        $self->__print(
            $where
                ? loc("Successfully fetched '%1' to '%2'",
                        $mod->module, $where )
                : loc("Failed to fetch '%1'", $mod->module)
        );
        $self->__print( "\n" );
    }
    $self->_pager_close;

}

sub _shell {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my $shell = $conf->get_program('shell');
    unless( $shell ) {
        $self->__print(
                loc("Your config does not specify a subshell!"), "\n",
                loc("Perhaps you need to re-run your setup?"), "\n"
        );
        return;
    }

    my $args; my $mods; my $opts;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            modules => { required => 1,  store => \$mods },
            options => { default => { }, store => \$opts },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my $cwd = Cwd::cwd();
    for my $mod (@$mods) {
        $mod->fetch(    %$opts )    or next;
        $mod->extract(  %$opts )    or next;

        $cb->_chdir( dir => $mod->status->extract() )   or next;

        #local $ENV{PERL5OPT} = CPANPLUS::inc->original_perl5opt;

        if( system($shell) and $! ) {
            $self->__print(
                loc("Error executing your subshell '%1': %2",
                        $shell, $!),"\n"
            );
            next;
        }
    }
    $cb->_chdir( dir => $cwd );

    return 1;
}

sub _distributions {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my $args; my $mods; my $opts;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            modules => { required => 1,  store => \$mods },
            options => { default => { }, store => \$opts },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my @list;
    for my $mod (@$mods) {
        push @list, sort { $a->version <=> $b->version }
                    grep { defined } $mod->distributions( %$opts );
    }

    my @rv = sort { $a->module cmp $b->module } @list;

    $self->cache([undef,@rv]);
    $self->__display_results;

    return; 1;
}

sub _reload_indices {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my $args; my $opts;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my $rv = $cb->reload_indices( %$opts );
    
    ### so the update failed, but you didnt give it any options either
    if( !$rv and !(keys %$opts) ) {
        $self->__print(
                "\nFailure may be due to corrupt source files\n" .
                "Try this:\n\tx --update_source\n\n" );
    }
    
    return $rv;
    
}

sub _install {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my $args; my $mods; my $opts; my $choice;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            modules => { required => 1,     store => \$mods },
            options => { default  => { },   store => \$opts },
            choice  => { required => 1,     store => \$choice,
                         allow    => [qw|i t|] },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    unless( scalar @$mods ) {
        $self->__print( loc("Nothing done\n") );
        return;
    }

    my $target = $choice eq 'i' ? TARGET_INSTALL : TARGET_CREATE;
    my $prompt = $choice eq 'i' ? loc('Installing ') : loc('Testing ');
    my $action = $choice eq 'i' ? 'install' : 'test';

    my $status = {};
    ### first loop over the mods to install them ###
    for my $mod (@$mods) {
        $self->__print( $prompt, $mod->module, " (".$mod->version.")", "\n" );

        my $log_length = length CPANPLUS::Error->stack_as_string;
    
        ### store the status for look up when we're done with all
        ### install calls
        $status->{$mod} = $mod->install( %$opts, target => $target );
        
        ### would you like a log file of what happened?
        if( $conf->get_conf('write_install_logs') ) {

            my $dir = File::Spec->catdir(
                            $conf->get_conf('base'),
                            $conf->_get_build('install_log_dir'),
                        );
            ### create the dir if it doesn't exit yet
            $cb->_mkdir( dir => $dir ) unless -d $dir;

            my $file = File::Spec->catfile( 
                            $dir,
                            INSTALL_LOG_FILE->( $mod ) 
                        );
            if ( open my $fh, ">$file" ) {
                my $stack = CPANPLUS::Error->stack_as_string;
                ### remove everything in the log that was there *before*
                ### we started this install
                substr( $stack, 0, $log_length, '' );
                
                print $fh $stack;
                close $fh;
                
                $self->__print( 
                    loc("*** Install log written to:\n  %1\n\n", $file)
                );
            } else {                
                warn "Could not open '$file': $!\n";
                next;
            }                
        }
    }

    my $flag;
    ### then report whether all this went ok or not ###
    for my $mod (@$mods) {
    #    if( $mod->status->installed ) {
        if( $status->{$mod} ) {
            $self->__print(
                loc("Module '%1' %tense(%2,past) successfully\n",
                $mod->module, $action)
            );                
        } else {
            $flag++;
            $self->__print(
                loc("Error %tense(%1,present) '%2'\n", $action, $mod->module)
            );
        }
    }



    if( !$flag ) {
        $self->__print(
            loc("No errors %tense(%1,present) all modules", $action), "\n"
        );
    } else {
        $self->__print(
            loc("Problem %tense(%1,present) one or more modules", $action)
        );
        $self->__print( "\n" );
        
        $self->__print( 
            loc("*** You can view the complete error buffer by pressing ".
                "'%1' ***\n", 'p')
        ) unless $conf->get_conf('verbose') || $self->noninteractive;
    }
    $self->__print( "\n" );

    return !$flag;
}

sub __ask_about_install {
    my $mod     = shift or return;
    my $prereq  = shift or return;
    my $term    = $Shell->term;

    $Shell->__print( "\n" );
    $Shell->__print( loc("Module '%1' requires '%2' to be installed",
                         $mod->module, $prereq->module ) );
    $Shell->__print( "\n\n" );
    $Shell->__print( 
        loc(    "If you don't wish to see this question anymore\n".
                "you can disable it by entering the following ".
                "commands on the prompt:\n    '%1'",
                's conf prereqs 1; s save' ) );
    $Shell->__print("\n\n");

    my $bool =  $term->ask_yn(
                    prompt  => loc("Should I install this module?"),
                    default => 'y'
                );

    return $bool;
}

sub __ask_about_send_test_report {
    my($mod, $grade) = @_;
    return 1 unless $grade eq GRADE_FAIL;

    my $term    = $Shell->term;

    $Shell->__print( "\n" );
    $Shell->__print(
        loc("Test report prepared for module '%1'.\n Would you like to ".
            "send it? (You can edit it if you like)", $mod->module ) );
    $Shell->__print( "\n\n" );
    my $bool =  $term->ask_yn(
                    prompt  => loc("Would you like to send the test report?"),
                    default => 'n'
                );

    return $bool;
}

sub __ask_about_edit_test_report {
    my($mod, $grade) = @_;
    return 0 unless $grade eq GRADE_FAIL;

    my $term    = $Shell->term;

    $Shell->__print( "\n" );
    $Shell->__print( 
        loc("Test report prepared for module '%1'. You can edit this ".
            "report if you would like", $mod->module ) );
    $Shell->__print("\n\n");
    my $bool =  $term->ask_yn(
                    prompt  => loc("Would you like to edit the test report?"),
                    default => 'y'
                );

    return $bool;
}

sub __ask_about_test_failure {
    my $mod         = shift;
    my $captured    = shift || '';
    my $term        = $Shell->term;

    $Shell->__print( "\n" );
    $Shell->__print( 
        loc(    "The tests for '%1' failed. Would you like me to proceed ".
                "anyway or should we abort?", $mod->module ) );
    $Shell->__print( "\n\n" );
    
    my $bool =  $term->ask_yn(
                    prompt  => loc("Proceed anyway?"),
                    default => 'n',
                );

    return $bool;
}


sub _details {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my $args; my $mods; my $opts;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            modules => { required => 1,  store => \$mods },
            options => { default => { }, store => \$opts },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    ### every module has about 10 lines of details
    ### maybe more later with Module::CPANTS etc
    $self->_pager_open if scalar @$mods * 10 > $self->_term_rowcount;


    my $format = "%-30s %-30s\n";
    for my $mod (@$mods) {
        my $href = $mod->details( %$opts );
        my @list = sort { $a->module cmp $b->module } $mod->contains;

        unless( $href ) {
            $self->__print( 
                loc("No details for %1 - it might be outdated.",
                    $mod->module), "\n" );
            next;

        } else {
            $self->__print( loc( "Details for '%1'\n", $mod->module ) );
            for my $item ( sort keys %$href ) {
                $self->__printf( $format, $item, $href->{$item} );
            }
            
            my $showed;
            for my $item ( @list ) {
                $self->__printf(
                    $format, ($showed ? '' : 'Contains:'), $item->module
                );
                $showed++;
            }
            $self->__print( "\n" );
        }
    }
    $self->_pager_close;
    $self->__print( "\n" );

    return 1;
}

sub _print {
    my $self = shift;
    my %hash = @_;

    my $args; my $opts; my $file;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
            input   => { default => '',  store => \$file },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my $old; my $fh;
    if( $file ) {
        $fh = FileHandle->new( ">$file" )
                    or( warn loc("Could not open '%1': '%2'", $file, $!),
                        return
                    );
        $old = select $fh;
    }


    $self->_pager_open if !$file;

    $self->__print( CPANPLUS::Error->stack_as_string );

    $self->_pager_close;

    select $old if $old;
    $self->__print( "\n" );

    return 1;
}

sub _set_conf {
    my $self    = shift;
    my %hash    = @_;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;

    ### possible options
    ### XXX hard coded, not optimal :(
    my %types   = (
        reconfigure => '', 
        save        => q([user | system | boxed]),
        edit        => '',
        program     => q([key => val]),
        conf        => q([key => val]),
        mirrors     => '',
        selfupdate  => '',  # XXX add all opts here?
    );


    my $args; my $opts; my $input;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
            input   => { default => '',  store => \$input },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    my ($type,$key,$value) = $input =~ m/(\w+)\s*(\w*)\s*(.*?)\s*$/;
    $type = lc $type;

    if( $type eq 'reconfigure' ) {
        my $setup = CPANPLUS::Configure::Setup->new(
                        configure_object    => $conf,
                        term                => $self->term,
                        backend             => $cb,
                    );
        return $setup->init;

    } elsif ( $type eq 'save' ) {
        my $where = {
            user    => CONFIG_USER,
            system  => CONFIG_SYSTEM,
            boxed   => CONFIG_BOXED,
        }->{ $key } || CONFIG_USER;      
        
        ### boxed is special, so let's get it's value from %INC
        ### so we can tell it where to save
        ### XXX perhaps this logic should be generic for all
        ### types, and put in the ->save() routine
        my $dir;
        if( $where eq CONFIG_BOXED ) {
            my $file    = join( '/', split( '::', CONFIG_BOXED ) ) . '.pm';
            my $file_re = quotemeta($file);
            
            my $path    = $INC{$file} || '';
            $path       =~ s/$file_re$//;        
            $dir        = $path;
        }     
        
        my $rv = $cb->configure_object->save( $where => $dir );

        $self->__print( 
            $rv
                ? loc("Configuration successfully saved to %1\n    (%2)\n",
                       $where, $rv)
                : loc("Failed to save configuration\n" )
        );
        return $rv;

    } elsif ( $type eq 'edit' ) {

        my $editor  = $conf->get_program('editor')
                        or( print(loc("No editor specified")), return );

        my $where = {
            user    => CONFIG_USER,
            system  => CONFIG_SYSTEM,
        }->{ $key } || CONFIG_USER;      
        
        my $file = $conf->_config_pm_to_file( $where );
        system("$editor $file");

        ### now reload it
        ### disable warnings for this
        {   require Module::Loaded;
            Module::Loaded::mark_as_unloaded( $_ ) for $conf->configs;

            ### reinitialize the config
            local $^W;
            $conf->init;
        }

        return 1;

    } elsif ( $type eq 'mirrors' ) {
    
        $self->__print( 
            loc("Readonly list of mirrors (in order of preference):\n\n" ) );
        
        my $i;
        for my $host ( @{$conf->get_conf('hosts')} ) {
            my $uri = $cb->_host_to_uri( %$host );
            
            $i++;
            $self->__print( "\t[$i] $uri\n" );
        }

    } elsif ( $type eq 'selfupdate' ) {
        my %valid = map { $_ => $_ } 
                        $cb->selfupdate_object->list_categories;    

        unless( $valid{$key} ) {
            $self->__print(
                loc( "To update your current CPANPLUS installation, ".
                        "choose one of the these options:\n%1",
                        ( join $/, map { 
                             sprintf "\ts selfupdate %-17s " .
                                     "[--latest=0] [--dryrun]", $_ 
                          } sort keys %valid ) 
                    )
            );          
        } else {
            my %update_args = (
                update  => $key,
                latest  => 1,
                %$opts
            );


            my %list = $cb->selfupdate_object
                            ->list_modules_to_update( %update_args );

            $self->__print(loc("The following updates will take place:"),$/.$/);
            
            for my $feature ( sort keys %list ) {
                my $aref = $list{$feature};
                
                ### is it a 'feature' or a built in?
                $self->__print(
                    $valid{$feature} 
                        ? "  " . ucfirst($feature) . ":\n"
                        : "  Modules for '$feature' support:\n"
                );
                    
                ### show what modules would be installed    
                $self->__print(
                    scalar @$aref
                        ? map { sprintf "    %-42s %-6s -> %-6s \n", 
                                $_->name, $_->installed_version, $_->version
                          } @$aref      
                        : "    No upgrades required\n"
                );                                                  
                $self->__print( $/ );
            }
            
        
            unless( $opts->{'dryrun'} ) { 
                $self->__print( loc("Updating your CPANPLUS installation\n") );
                $cb->selfupdate_object->selfupdate( %update_args );
            }
        }
        
    } else {

        if ( $type eq 'program' or $type eq 'conf' ) {

            my $format = {
                conf    => '%-25s %s',
                program => '%-12s %s',
            }->{ $type };      

            unless( $key ) {
                my @list =  grep { $_ ne 'hosts' }
                            $conf->options( type => $type );

                my $method = 'get_' . $type;

                local $Data::Dumper::Indent = 0;
                for my $name ( @list ) {
                    my $val = $conf->$method($name) || '';
                    ($val)  = ref($val)
                                ? (Data::Dumper::Dumper($val) =~ /= (.*);$/)
                                : "'$val'";

                    $self->__printf( "    $format\n", $name, $val );
                }

            } elsif ( $key eq 'hosts' ) {
                $self->__print( 
                    loc(  "Setting hosts is not trivial.\n" .
                          "It is suggested you use '%1' and edit the " .
                          "configuration file manually", 's edit')
                );
            } else {
                my $method = 'set_' . $type;
                $conf->$method( $key => defined $value ? $value : '' )
                    and $self->__print( loc("Key '%1' was set to '%2'", $key,
                                  defined $value ? $value : 'EMPTY STRING') );
            }

        } else {
            $self->__print( loc("Unknown type '%1'",$type || 'EMPTY' ) );
            $self->__print( $/ );
            $self->__print( loc("Try one of the following:") );
            $self->__print( $/, join $/, 
                      map { sprintf "\t%-11s %s", $_, $types{$_} } 
                      sort keys %types );
        }
    }
    $self->__print( "\n" );
    return 1;
}

sub _uptodate {
    my $self = shift;
    my %hash = @_;
    my $cb   = $self->backend;
    my $conf = $cb->configure_object;

    my $opts; my $mods;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
            modules => { required => 1,  store => \$mods },
        };

        check( $tmpl, \%hash ) or return;
    }

    ### long listing? short is default ###
    my $long = $opts->{'long'} ? 1 : 0;

    my @list = scalar @$mods ? @$mods : @{$cb->_all_installed};

    my @rv; my %seen;
    for my $mod (@list) {
        ### skip this mod if it's up to date ###
        next if $mod->is_uptodate;
        ### skip this mod if it's core ###
        next if $mod->package_is_perl_core;

        if( $long or !$seen{$mod->package}++ ) {
            push @rv, $mod;
        }
    }

    @rv = sort { $a->module cmp $b->module } @rv;

    $self->cache([undef,@rv]);

    $self->_pager_open if scalar @rv >= $self->_term_rowcount;

    my $format = "%5s %12s %12s %-36s %-10s\n";

    my $i = 1;
    for my $mod ( @rv ) {
        $self->__printf(
            $format,
            $i,
            $self->_format_version($mod->installed_version) || 'Unparsable',
            $self->_format_version( $mod->version ),
            $mod->module,
            $mod->author->cpanid
        );
        $i++;
    }
    $self->_pager_close;

    return 1;
}

sub _autobundle {
    my $self = shift;
    my %hash = @_;
    my $cb   = $self->backend;
    my $conf = $cb->configure_object;

    my $opts; my $input;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
            input   => { default => '',  store => \$input },
        };

         check( $tmpl, \%hash ) or return;
    }

    $opts->{'path'} = $input if $input;

    my $where = $cb->autobundle( %$opts );

    $self->__print( 
        $where
            ? loc("Wrote autobundle to '%1'", $where)
            : loc("Could not create autobundle" )
    );
    $self->__print( "\n" );

    return $where ? 1 : 0;
}

sub _uninstall {
    my $self = shift;
    my %hash = @_;
    my $cb   = $self->backend;
    my $term = $self->term;
    my $conf = $cb->configure_object;

    my $opts; my $mods;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
            modules => { default => [],  store => \$mods },
        };

         check( $tmpl, \%hash ) or return;
    }

    my $force = $opts->{'force'} || $conf->get_conf('force');

    unless( $force ) {
        my $list = join "\n", map { '    ' . $_->module } @$mods;

        $self->__print( loc("
This will uninstall the following modules:
%1

Note that if you installed them via a package manager, you probably
should use the same package manager to uninstall them

", $list) );

        return unless $term->ask_yn(
                        prompt  => loc("Are you sure you want to continue?"),
                        default => 'n',
                    );
    }

    ### first loop over all the modules to uninstall them ###
    for my $mod (@$mods) {
        $self->__print( loc("Uninstalling '%1'", $mod->module), "\n" );

        $mod->uninstall( %$opts );
    }

    my $flag;
    ### then report whether all this went ok or not ###
    for my $mod (@$mods) {
        if( $mod->status->uninstall ) {
            $self->__print( 
                loc("Module '%1' %tense(uninstall,past) successfully\n",
                    $mod->module ) );
        } else {
            $flag++;
            $self->__print( 
                loc("Error %tense(uninstall,present) '%1'\n", $mod->module) );
        }
    }

    if( !$flag ) {
        $self->__print( 
            loc("All modules %tense(uninstall,past) successfully"), "\n" );
    } else {
        $self->__print( 
            loc("Problem %tense(uninstalling,present) one or more modules" ),
            "\n" );
            
        $self->__print( 
            loc("*** You can view the complete error buffer by pressing '%1'".
                "***\n", 'p') ) unless $conf->get_conf('verbose');
    }
    $self->__print( "\n" );

    return !$flag;
}

sub _reports {
   my $self = shift;
    my %hash = @_;
    my $cb   = $self->backend;
    my $term = $self->term;
    my $conf = $cb->configure_object;

    my $opts; my $mods;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            options => { default => { }, store => \$opts },
            modules => { default => '',  store => \$mods },
        };

         check( $tmpl, \%hash ) or return;
    }

    ### XXX might need to be conditional ###
    $self->_pager_open;

    for my $mod (@$mods) {
        my @list = $mod->fetch_report( %$opts )
                    or( print(loc("No reports available for this distribution.")),
                        next
                    );

        @list = reverse
                map  { $_->[0] }
                sort { $a->[1] cmp $b->[1] }
                map  { [$_, $_->{'dist'}.':'.$_->{'platform'}] } @list;



        ### XXX this may need to be sorted better somehow ###
        my $url;
        my $format = "%8s %s %s\n";

        my %seen;
        for my $href (@list ) {
            $self->__print( 
                "[" . $mod->author->cpanid .'/'. $href->{'dist'} . "]\n"
            ) unless $seen{ $href->{'dist'} }++;

            $self->__printf( 
                $format, 
                $href->{'grade'}, 
                $href->{'platform'},
                ($href->{'details'} ? '(*)' : '')
            );

            $url ||= $href->{'details'};
        }

        $self->__print( "\n==> $url\n" ) if $url;
        $self->__print( "\n" );
    }
    $self->_pager_close;

    return 1;
}


### Load plugins
{   my @PluginModules;
    my %Dispatch = ( 
        showtip => [ __PACKAGE__, '_show_random_tip'], 
        plugins => [ __PACKAGE__, '_list_plugins'   ], 
        '?'     => [ __PACKAGE__, '_plugins_usage'  ],
    );        

    sub plugin_modules  { return @PluginModules }
    sub plugin_table    { return %Dispatch }
    
    my $init_done;
    sub _plugins_init {
        ### only initialize once
        return if $init_done++;
        
        ### find all plugins first
        if( check_install( module  => 'Module::Pluggable', version => '2.4') ) {
            require Module::Pluggable;
    
            my $only_re = __PACKAGE__ . '::Plugins::\w+$';
    
            Module::Pluggable->import(
                            sub_name    => '_plugins',
                            search_path => __PACKAGE__,
                            only        => qr/$only_re/,
                            #except      => [ INSTALLER_MM, INSTALLER_SAMPLE ]
                        );
                        
            push @PluginModules, __PACKAGE__->_plugins;
        }
    
        ### now try to load them
        for my $p ( __PACKAGE__->plugin_modules ) {
            my %map = eval { load $p; $p->import; $p->plugins };
            error(loc("Could not load plugin '$p': $@")), next if $@;
        
            ### register each plugin
            while( my($name, $func) = each %map ) {
                
                if( not length $name or not length $func ) {
                    error(loc("Empty plugin name or dispatch function detected"));
                    next;
                }                
                
                if( exists( $Dispatch{$name} ) ) {
                    error(loc("'%1' is already registered by '%2'", 
                        $name, $Dispatch{$name}->[0]));
                    next;                    
                }
        
                ### register name, package and function
                $Dispatch{$name} = [ $p, $func ];
            }
        }
    }
    
    ### dispatch a plugin command to it's function
    sub _meta {
        my $self = shift;
        my %hash = @_;
        my $cb   = $self->backend;
        my $term = $self->term;
        my $conf = $cb->configure_object;
    
        my $opts; my $input;
        {   local $Params::Check::ALLOW_UNKNOWN = 1;
    
            my $tmpl = {
                options => { default => { }, store => \$opts },
                input   => { default => '',  store => \$input },
            };
    
             check( $tmpl, \%hash ) or return;
        }
    
        $input =~ s/\s*(\S+)\s*//;
        my $cmd = $1;
    
        ### look up the command, or go to the default
        my $aref = $Dispatch{ $cmd } || [ __PACKAGE__, '_plugin_default' ];
        
        my($pkg,$func) = @$aref;
        
        my $rv = eval { $pkg->$func( $self, $cb, $cmd, $input, $opts ) };
        
        error( $@ ) if $@;

        ### return $rv instead, so input loop can be terminated?
        return 1;
    }
    
    sub _plugin_default { error(loc("No such plugin command")) }
}

### plugin commands 
{   my $help_format = "    /%-21s # %s\n"; 
    
    sub _list_plugins   {
        my $self = shift;
        
        $self->__print( loc("Available plugins:\n") );
        $self->__print( loc("    List usage by using: /? PLUGIN_NAME\n" ) );
        $self->__print( $/ );
        
        my %table = __PACKAGE__->plugin_table;
        for my $name( sort keys %table ) {
            my $pkg     = $table{$name}->[0];
            my $this    = __PACKAGE__;
            
            my $who = $pkg eq $this
                ? "Standard Plugin"
                : do { $pkg =~ s/^$this/../; "Provided by: $pkg" };
            
            $self->__printf( $help_format, $name, $who );
        }          
    
        $self->__print( $/.$/ );
        
        $self->__print(
            "    Write your own plugins? Read the documentation of:\n" .
            "        CPANPLUS::Shell::Default::Plugins::HOWTO\n" );
                
        $self->__print( $/ );        
    }

    sub _list_plugins_help {
        return sprintf $help_format, 'plugins', loc("lists available plugins");
    }

    ### registered as a plugin too
    sub _show_random_tip_help {
        return sprintf $help_format, 'showtip', loc("show usage tips" );
    }   

    sub _plugins_usage {
        my $self    = shift;
        my $shell   = shift;
        my $cb      = shift;
        my $cmd     = shift;
        my $input   = shift;
        my %table   = $self->plugin_table;
        
        my @list = length $input ? split /\s+/, $input : sort keys %table;
        
        for my $name( @list ) {

            ### no such plugin? skip
            error(loc("No such plugin '$name'")), next unless $table{$name};

            my $pkg     = $table{$name}->[0];
            my $func    = $table{$name}->[1] . '_help';
            
            if ( my $sub = $pkg->can( $func ) ) {
                eval { $self->__print( $sub->() ) };
                error( $@ ) if $@;
            
            } else {
                $self->__print("    No usage for '$name' -- try perldoc $pkg");
            }
            
            $self->__print( $/ );
        }          
    
        $self->__print( $/.$/ );      
    }
    
    sub _plugins_usage_help {
        return sprintf $help_format, '? [NAME ...]',
                                     loc("show usage for plugins");
    }
}

### send a command to a remote host, retrieve the answer;
sub __send_remote_command {
    my $self    = shift;
    my $cmd     = shift;
    my $remote  = $self->remote or return;
    my $user    = $remote->{'username'};
    my $pass    = $remote->{'password'};
    my $conn    = $remote->{'connection'};
    my $end     = "\015\012";
    my $answer;

    my $send = join "\0", $user, $pass, $cmd;

    print $conn $send . $end;

    ### XXX why doesn't something like this just work?
    #1 while recv($conn, $answer, 1024, 0);
    while(1) {
        my $buff;
        $conn->recv( $buff, 1024, 0 );
        $answer .= $buff;
        last if $buff =~ /$end$/;
    }

    my($status,$buffer) = split "\0", $answer;

    return ($status, $buffer);
}


sub _read_configuration_from_rc {
    my $self    = shift;
    my $rc_file = shift;

    my $href;
    if( can_load( modules => { 'Config::Auto' => '0.0' } ) ) {
        $Config::Auto::DisablePerl = 1;

        eval { $href = Config::Auto::parse( $rc_file, format => 'space' ) };

        $self->__print( 
            loc( "Unable to read in config file '%1': %2", $rc_file, $@ ) 
        ) if $@;
    }

    return $href || {};
}

{   my @tips = (
        loc( "You can update CPANPLUS by running: '%1'", 's selfupdate' ),
        loc( "You can install modules by URL using '%1'", 'i URL' ),
        loc( "You can turn off these tips using '%1'", 
             's conf show_startup_tip 0' ),
        loc( "You can use wildcards like '%1' and '%2' on search results",
             '*', '2..5' ) ,
        loc( "You can use plugins. Type '%1' to list available plugins",
             '/plugins' ),
        loc( "You can show all your out of date modules using '%1'", 'o' ),  
        loc( "Many operations take options, like '%1', '%2' or '%3'",
             '--verbose', '--force', '--skiptest' ),
        loc( "The documentation in %1 and %2 is very useful",
             "CPANPLUS::Module", "CPANPLUS::Backend" ),
        loc( "You can type '%1' for help and '%2' to exit", 'h', 'q' ),
        loc( "You can run an interactive setup using '%1'", 's reconfigure' ),    
        loc( "You can add custom sources to your index. See '%1' for details",
             '/cs --help' ),
    );
    
    sub _show_random_tip {
        my $self = shift;
        $self->__print( $/, "Did you know...\n    ", 
                        $tips[ int rand scalar @tips ], $/ );
        return 1;
    }
}    

1;

__END__

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

L<CPANPLUS::Shell::Classic>, L<CPANPLUS::Shell>, L<cpanp>

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

__END__

TODO:
    e   => "_expand_inc", # scratch it, imho -- not used enough

### free letters: g j k n y ###
