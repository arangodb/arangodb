##################################################
###            CPANPLUS/Shell/Classic.pm       ###
###    Backwards compatible shell for CPAN++   ###
###      Written 08-04-2002 by Jos Boumans     ###
##################################################

package CPANPLUS::Shell::Classic;

use strict;


use CPANPLUS::Error;
use CPANPLUS::Backend;
use CPANPLUS::Configure::Setup;
use CPANPLUS::Internals::Constants;

use Cwd;
use IPC::Cmd;
use Term::UI;
use Data::Dumper;
use Term::ReadLine;

use Module::Load                qw[load];
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];

$Params::Check::VERBOSE       = 1;
$Params::Check::ALLOW_UNKNOWN = 1;

BEGIN {
    use vars        qw[ $VERSION @ISA ];
    @ISA        =   qw[ CPANPLUS::Shell::_Base::ReadLine ];
    $VERSION    =   '0.0562';
}

load CPANPLUS::Shell;


### our command set ###
my $map = {
    a           => '_author',
    b           => '_bundle',
    d           => '_distribution',
    'm'         => '_module',
    i           => '_find_all',
    r           => '_uptodate',
    u           => '_not_supported',
    ls          => '_ls',
    get         => '_fetch',
    make        => '_install',
    test        => '_install',
    install     => '_install',
    clean       => '_not_supported',
    look        => '_shell',
    readme      => '_readme',
    h           => '_help',
    '?'         => '_help',
    o           => '_set_conf',
    reload      => '_reload',
    autobundle  => '_autobundle',
    '!'         => '_bang',
    #'q'         => '_quit', # done it the loop itself
};

### the shell object, scoped to the file ###
my $Shell;
my $Brand   = 'cpan';
my $Prompt  = $Brand . '> ';

sub new {
    my $class   = shift;

    my $cb      = new CPANPLUS::Backend;
    my $self    = $class->SUPER::_init(
                            brand   => $Brand,
                            term    => Term::ReadLine->new( $Brand ),
                            prompt  => $Prompt,
                            backend => $cb,
                            format  => "%5s %-50s %8s %-10s\n",
                        );
    ### make it available package wide ###
    $Shell = $self;

    ### enable verbose, it's the cpan.pm way
    $cb->configure_object->set_conf( verbose => 1 );


    ### register install callback ###
    $cb->_register_callback(
            name    => 'install_prerequisite',
            code    => \&__ask_about_install,
    );

    ### register test report callback ###
    $cb->_register_callback(
            name    => 'edit_test_report',
            code    => \&__ask_about_test_report,
    );

    return $self;
}

sub shell {
    my $self = shift;
    my $term = $self->term;

    $self->_show_banner;
    $self->_input_loop && print "\n";
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

        last if $self->_dispatch_on_input( input => $input );

        ### flush the lib cache ###
        $cb->_flush( list => [qw|lib load|] );

    } continue {
        $self->_signals->{INT}{count}--
            if $self->_signals->{INT}{count}; # clear the sigint count
    }

    return 1;
}

sub _dispatch_on_input {
    my $self = shift;
    my $conf = $self->backend->configure_object();
    my $term = $self->term;
    my %hash = @_;

    my $string;
    my $tmpl = {
        input   => { required => 1, store => \$string }
    };

    check( $tmpl, \%hash ) or return;

    ### the original force setting;
    my $force_store = $conf->get_conf( 'force' );

    ### parse the input: the first part before the space
    ### is the command, followed by arguments.
    ### see the usage below
    my $key;
    PARSE_INPUT: {
        $string =~ s|^\s*([\w\?\!]+)\s*||;
        chomp $string;
        $key = lc($1);
    }

    ### you prefixed the input with 'force'
    ### that means we set the force flag, and
    ### reparse the input...
    ### YAY goto block :)
    if( $key eq 'force' ) {
        $conf->set_conf( force => 1 );
        goto PARSE_INPUT;
    }

    ### you want to quit
    return 1 if $key =~ /^q/;

    my $method = $map->{$key};
    unless( $self->can( $method ) ) {
        print "Unknown command '$key'. Type ? for help.\n";
        return;
    }

    ### dispatch the method call
    eval { $self->$method(
                    command => $key,
                    result  => [ split /\s+/, $string ],
                    input   => $string );
    };
    warn $@ if $@;

    return;
}

### displays quit message
sub _quit {

    ### well, that's what CPAN.pm says...
    print "Lockfile removed\n";
}

sub _not_supported {
    my $self = shift;
    my %hash = @_;

    my $cmd;
    my $tmpl = {
        command => { required => 1, store => \$cmd }
    };

    check( $tmpl, \%hash ) or return;

    print "Sorry, the command '$cmd' is not supported\n";

    return;
}

sub _fetch {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $input);
    my $tmpl = {
        result  => { store => \$aref, default => [] },
        input   => { default => 'all', store => \$input },
    };

    check( $tmpl, \%hash ) or return;

    for my $mod (@$aref) {
        my $obj;

        unless( $obj = $cb->module_tree($mod) ) {
            print "Warning: Cannot get $input, don't know what it is\n";
            print "Try the command\n\n";
            print "\ti /$mod/\n\n";
            print "to find objects with matching identifiers.\n";

            next;
        }

        $obj->fetch && $obj->extract;
    }

    return $aref;
}

sub _install {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my $mapping = {
        make        => { target => TARGET_CREATE, skiptest => 1 },
        test        => { target => TARGET_CREATE },
        install     => { target => TARGET_INSTALL },
    };

    my($aref,$cmd);
    my $tmpl = {
        result  => { store => \$aref, default => [] },
        command => { required => 1, store => \$cmd, allow => [keys %$mapping] },
    };

    check( $tmpl, \%hash ) or return;

    for my $mod (@$aref) {
        my $obj = $cb->module_tree( $mod );

        unless( $obj ) {
            print "No such module '$mod'\n";
            next;
        }

        my $opts = $mapping->{$cmd};
        $obj->install( %$opts );
    }

    return $aref;
}

sub _shell {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my($aref, $cmd);
    my $tmpl = {
        result  => { store => \$aref, default => [] },
        command => { required => 1, store => \$cmd },

    };

    check( $tmpl, \%hash ) or return;


    my $shell = $conf->get_program('shell');
    unless( $shell ) {
        print "Your configuration does not define a value for subshells.\n".
              qq[Please define it with "o conf shell <your shell>"\n];
        return;
    }

    my $cwd = Cwd::cwd();

    for my $mod (@$aref) {
        print "Running $cmd for $mod\n";

        my $obj = $cb->module_tree( $mod )  or next;
        $obj->fetch                         or next;
        $obj->extract                       or next;

        $cb->_chdir( dir => $obj->status->extract )   or next;

        local $ENV{PERL5OPT} = CPANPLUS::inc->original_perl5opt;
        if( system($shell) and $! ) {
            print "Error executing your subshell '$shell': $!\n";
            next;
        }
    }
    $cb->_chdir( dir => $cwd );

    return $aref;
}

sub _readme {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my($aref, $cmd);
    my $tmpl = {
        result  => { store => \$aref, default => [] },
        command => { required => 1, store => \$cmd },

    };

    check( $tmpl, \%hash ) or return;

    for my $mod (@$aref) {
        my $obj = $cb->module_tree( $mod ) or next;

        if( my $readme = $obj->readme ) {

            $self->_pager_open;
            print $readme;
            $self->_pager_close;
        }
    }

    return 1;
}

sub _reload {
    my $self    = shift;
    my $cb      = $self->backend;
    my $conf    = $cb->configure_object;
    my %hash    = @_;

    my($input, $cmd);
    my $tmpl = {
        input   => { default => 'all', store => \$input },
        command => { required => 1, store => \$cmd },

    };

    check( $tmpl, \%hash ) or return;

    if ( $input =~ /cpan/i ) {
        print qq[You want to reload the CPAN code\n];
        print qq[Just type 'q' and then restart... ] .
              qq[Trust me, it is MUCH safer\n];

    } elsif ( $input =~ /index/i ) {
        $cb->reload_indices(update_source => 1);

    } else {
        print qq[cpan     re-evals the CPANPLUS.pm file\n];
        print qq[index    re-reads the index files\n];
    }

    return 1;
}

sub _autobundle {
    my $self    = shift;
    my $cb      = $self->backend;

    print qq[Writing bundle file... This may take a while\n];

    my $where = $cb->autobundle();

    print $where
        ? qq[\nWrote autobundle to $where\n]
        : qq[\nCould not create autobundle\n];

    return 1;
}

sub _set_conf {
    my $self = shift;
    my $cb   = $self->backend;
    my $conf = $cb->configure_object;
    my %hash = @_;

    my($aref, $input);
    my $tmpl = {
        result  => { store => \$aref, default => [] },
        input   => { default => 'all', store => \$input },
    };

    check( $tmpl, \%hash ) or return;

    my $type = shift @$aref;

    if( $type eq 'debug' ) {
        print   qq[Sorry you cannot set debug options through ] .
                qq[this shell in CPANPLUS\n];
        return;

    } elsif ( $type eq 'conf' ) {

        ### from CPAN.pm :o)
        # CPAN::Shell::o and CPAN::Config::edit are closely related. 'o conf'
        # should have been called set and 'o debug' maybe 'set debug'

        #    commit             Commit changes to disk
        #    defaults           Reload defaults from disk
        #    init               Interactive setting of all options

        my $name    = shift @$aref;
        my $value   = "@$aref";

        if( $name eq 'init' ) {
            my $setup = CPANPLUS::Configure::Setup->new(
                        conf    => $cb->configure_object,
                        term    => $self->term,
                        backend => $cb,
                    );
            return $setup->init;

        } elsif ($name eq 'commit' ) {;
            $cb->configure_object->save;
            print "Your CPAN++ configuration info has been saved!\n\n";
            return;

        } elsif ($name eq 'defaults' ) {
            print   qq[Sorry, CPANPLUS cannot restore default for you.\n] .
                    qq[Perhaps you should run the interactive setup again.\n] .
                    qq[\ttry running 'o conf init'\n];
            return;

        ### we're just supplying things in the 'conf' section now,
        ### not the program section.. it's a bit of a hassle to make that
        ### work cleanly with the original CPAN.pm interface, so we'll fix
        ### it when people start complaining, which is hopefully never.
        } else {
            unless( $name ) {
                my @list =  grep { $_ ne 'hosts' }
                            $conf->options( type => $type );

                my $method = 'get_' . $type;

                local $Data::Dumper::Indent = 0;
                for my $name ( @list ) {
                    my $val = $conf->$method($name);
                    ($val)  = ref($val)
                                ? (Data::Dumper::Dumper($val) =~ /= (.*);$/)
                                : "'$val'";
                    printf  "    %-25s %s\n", $name, $val;
                }

            } elsif ( $name eq 'hosts' ) {
                print   "Setting hosts is not trivial.\n" .
                        "It is suggested you edit the " .
                        "configuration file manually";

            } else {
                my $method = 'set_' . $type;
                if( $conf->$method($name => defined $value ? $value : '') ) {
                    my $set_to = defined $value ? $value : 'EMPTY STRING';
                    print "Key '$name' was set to '$set_to'\n";
                }
            }
        }
    } else {
        print   qq[Known options:\n] .
                qq[  conf    set or get configuration variables\n] .
                qq[  debug   set or get debugging options\n];
    }

    return;
}

########################
### search functions ###
########################

sub _author {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $short, $input, $class);
    my $tmpl = {
        result  => { store => \$aref, default => ['/./'] },
        short   => { default => 0, store => \$short },
        input   => { default => 'all', store => \$input },
        class   => { default => 'Author', no_override => 1,
                    store => \$class },
    };

    check( $tmpl, \%hash ) or return;

    my @regexes = map { m|/(.+)/| ? qr/$1/ : $_ } @$aref;


    my @rv;
    for my $type (qw[author cpanid]) {
        push @rv, $cb->search( type => $type, allow => \@regexes );
    }

    unless( @rv ) {
        print "No object of type $class found for argument $input\n"
            unless $short;
        return;
    }

    return $self->_pp_author(
                result  => \@rv,
                class   => $class,
                short   => $short,
                input   => $input );

}

### find all modules matching a query ###
sub _module {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $short, $input, $class);
    my $tmpl = {
        result  => { store => \$aref, default => ['/./'] },
        short   => { default => 0, store => \$short },
        input   => { default => 'all', store => \$input },
        class   => { default => 'Module', no_override => 1,
                    store => \$class },
    };

    check( $tmpl, \%hash ) or return;

    my @rv;
    for my $module (@$aref) {
        if( $module =~ m|/(.+)/| ) {
            push @rv, $cb->search(  type    => 'module',
                                    allow   => [qr/$1/i] );
        } else {
            my $obj = $cb->module_tree( $module ) or next;
            push @rv, $obj;
        }
    }

    return $self->_pp_module(
                result  => \@rv,
                class   => $class,
                short   => $short,
                input   => $input );
}

### find all bundles matching a query ###
sub _bundle {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $short, $input, $class);
    my $tmpl = {
        result  => { store => \$aref, default => ['/./'] },
        short   => { default => 0, store => \$short },
        input   => { default => 'all', store => \$input },
        class   => { default => 'Bundle', no_override => 1,
                    store => \$class },
    };

    check( $tmpl, \%hash ) or return;

    my @rv;
    for my $bundle (@$aref) {
        if( $bundle =~ m|/(.+)/| ) {
            push @rv, $cb->search(  type    => 'module',
                                    allow   => [qr/Bundle::.*?$1/i] );
        } else {
            my $obj = $cb->module_tree( "Bundle::${bundle}" ) or next;
            push @rv, $obj;
        }
    }

    return $self->_pp_module(
                result  => \@rv,
                class   => $class,
                short   => $short,
                input   => $input );
}

sub _distribution {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $short, $input, $class);
    my $tmpl = {
        result  => { store => \$aref, default => ['/./'] },
        short   => { default => 0, store => \$short },
        input   => { default => 'all', store => \$input },
        class   => { default => 'Distribution', no_override => 1,
                    store => \$class },
    };

    check( $tmpl, \%hash ) or return;

    my @rv;
    for my $module (@$aref) {
        ### if it's a regex... ###
        if ( my ($match) = $module =~ m|^/(.+)/$|) {

            ### something like /FOO/Bar.tar.gz/ was entered
            if (my ($path,$package) = $match =~ m|^/?(.+)/(.+)$|) {
                my $seen;

                my @data = $cb->search( type    => 'package',
                                        allow   => [qr/$package/i] );

                my @list = $cb->search( type    => 'path',
                                        allow   => [qr/$path/i],
                                        data    => \@data );

                ### make sure we dont list the same dist twice
                for my $val ( @list ) {
                    next if $seen->{$val->package}++;

                    push @rv, $val;
                }

            ### something like /FOO/ or /Bar.tgz/ was entered
            ### so we look both in the path, as well as in the package name
            } else {
                my $seen;
                {   my @list = $cb->search( type    => 'package',
                                            allow   => [qr/$match/i] );

                    ### make sure we dont list the same dist twice
                    for my $val ( @list ) {
                        next if $seen->{$val->package}++;

                        push @rv, $val;
                    }
                }

                {   my @list = $cb->search( type    => 'path',
                                            allow   => [qr/$match/i] );

                    ### make sure we dont list the same dist twice
                    for my $val ( @list ) {
                        next if $seen->{$val->package}++;

                        push @rv, $val;
                    }

                }
            }

        } else {

            ### user entered a full dist, like: R/RC/RCAPUTO/POE-0.19.tar.gz
            if (my ($path,$package) = $module =~ m|^/?(.+)/(.+)$|) {
                my @data = $cb->search( type    => 'package',
                                        allow   => [qr/^$package$/] );
                my @list = $cb->search( type    => 'path',
                                        allow   => [qr/$path$/i],
                                        data    => \@data);

                ### make sure we dont list the same dist twice
                my $seen;
                for my $val ( @list ) {
                    next if $seen->{$val->package}++;

                    push @rv, $val;
                }
            }
        }
    }

    return $self->_pp_distribution(
                result  => \@rv,
                class   => $class,
                short   => $short,
                input   => $input );
}

sub _find_all {
    my $self = shift;

    my @rv;
    for my $method (qw[_author _bundle _module _distribution]) {
        my $aref = $self->$method( @_, short => 1 );

        push @rv, @$aref if $aref;
    }

    print scalar(@rv). " items found\n"
}

sub _uptodate {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $short, $input, $class);
    my $tmpl = {
        result  => { store => \$aref, default => ['/./'] },
        short   => { default => 0, store => \$short },
        input   => { default => 'all', store => \$input },
        class   => { default => 'Uptodate', no_override => 1,
                    store => \$class },
    };

    check( $tmpl, \%hash ) or return;


    my @rv;
    if( @$aref) {
        for my $module (@$aref) {
            if( $module =~ m|/(.+)/| ) {
                my @list = $cb->search( type    => 'module',
                                        allow   => [qr/$1/i] );

                ### only add those that are installed and not core
                push @rv, grep { not $_->package_is_perl_core }
                          grep { $_->installed_file }
                          @list;

            } else {
                my $obj = $cb->module_tree( $module ) or next;
                push @rv, $obj;
            }
        }
    } else {
        @rv = @{$cb->_all_installed};
    }

    return $self->_pp_uptodate(
            result  => \@rv,
            class   => $class,
            short   => $short,
            input   => $input );
}

sub _ls {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my($aref, $short, $input, $class);
    my $tmpl = {
        result  => { store => \$aref, default => [] },
        short   => { default => 0, store => \$short },
        input   => { default => 'all', store => \$input },
        class   => { default => 'Uptodate', no_override => 1,
                    store => \$class },
    };

    check( $tmpl, \%hash ) or return;

    my @rv;
    for my $name (@$aref) {
        my $auth = $cb->author_tree( uc $name );

        unless( $auth ) {
            print qq[ls command rejects argument $name: not an author\n];
            next;
        }

        push @rv, $auth->distributions;
    }

    return $self->_pp_ls(
            result  => \@rv,
            class   => $class,
            short   => $short,
            input   => $input );
}

############################
### pretty printing subs ###
############################


sub _pp_author {
    my $self = shift;
    my %hash = @_;

    my( $aref, $short, $class, $input );
    my $tmpl = {
        result  => { required => 1, default => [], strict_type => 1,
                        store => \$aref },
        short   => { default => 0, store => \$short },
        class   => { required => 1, store => \$class },
        input   => { required => 1, store => \$input },
    };

    check( $tmpl, \%hash ) or return;

    ### no results
    if( !@$aref ) {
        print "No objects of type $class found for argument $input\n"
            unless $short;

    ### one result, long output desired;
    } elsif( @$aref == 1 and !$short ) {

        ### should look like this:
        #cpan> a KANE
        #Author id = KANE
        #    EMAIL        boumans@frg.eur.nl
        #    FULLNAME     Jos Boumans

        my $obj = shift @$aref;

        print "$class id = ",                   $obj->cpanid(), "\n";
        printf "    %-12s %s\n", 'EMAIL',       $obj->email();
        printf "    %-12s %s%s\n", 'FULLNAME',  $obj->author();

    } else {

        ### should look like this:
        #Author          KANE (Jos Boumans)
        #Author          LBROCARD (Leon Brocard)
        #2 items found

        for my $obj ( @$aref ) {
            printf qq[%-15s %s ("%s" (%s))\n],
                $class, $obj->cpanid, $obj->author, $obj->email;
        }
        print scalar(@$aref)." items found\n" unless $short;
    }

    return $aref;
}

sub _pp_module {
    my $self = shift;
    my %hash = @_;

    my( $aref, $short, $class, $input );
    my $tmpl = {
        result  => { required => 1, default => [], strict_type => 1,
                        store => \$aref },
        short   => { default => 0, store => \$short },
        class   => { required => 1, store => \$class },
        input   => { required => 1, store => \$input },
    };

    check( $tmpl, \%hash ) or return;


    ### no results
    if( !@$aref ) {
        print "No objects of type $class found for argument $input\n"
            unless $short;

    ### one result, long output desired;
    } elsif( @$aref == 1 and !$short ) {


        ### should look like this:
        #Module id = LWP
        #    DESCRIPTION  Libwww-perl
        #    CPAN_USERID  GAAS (Gisle Aas <gisle@ActiveState.com>)
        #    CPAN_VERSION 5.64
        #    CPAN_FILE    G/GA/GAAS/libwww-perl-5.64.tar.gz
        #    DSLI_STATUS  RmpO (released,mailing-list,perl,object-oriented)
        #    MANPAGE      LWP - The World-Wide Web library for Perl
        #    INST_FILE    C:\Perl\site\lib\LWP.pm
        #    INST_VERSION 5.62

        my $obj     = shift @$aref;
        my $aut_obj = $obj->author;
        my $format  = "    %-12s %s%s\n";

        print "$class id = ",           $obj->module(), "\n";
        printf $format, 'DESCRIPTION',  $obj->description()
            if $obj->description();

        printf $format, 'CPAN_USERID',  $aut_obj->cpanid() . " (" .
            $aut_obj->author() . " <" . $aut_obj->email() . ">)";

        printf $format, 'CPAN_VERSION', $obj->version();
        printf $format, 'CPAN_FILE',    $obj->path() . '/' . $obj->package();

        printf $format, 'DSLI_STATUS',  $self->_pp_dslip($obj->dslip)
            if $obj->dslip() =~ /\w/;

        #printf $format, 'MANPAGE',      $obj->foo();
        ### this is for bundles... CPAN.pm downloads them,
        #printf $format, 'CONATAINS,
        # parses and goes from there...

        printf $format, 'INST_FILE',    $obj->installed_file ||
            '(not installed)';
        printf $format, 'INST_VERSION', $obj->installed_version;



    } else {

        ### should look like this:
        #Module          LWP             (G/GA/GAAS/libwww-perl-5.64.tar.gz)
        #Module          POE             (R/RC/RCAPUTO/POE-0.19.tar.gz)
        #2 items found

        for my $obj ( @$aref ) {
            printf "%-15s %-15s (%s)\n", $class, $obj->module(),
                $obj->path() .'/'. $obj->package();
        }
        print scalar(@$aref). " items found\n" unless $short;
    }

    return $aref;
}

sub _pp_dslip {
    my $self    = shift;
    my $dslip   = shift or return;

    my (%_statusD, %_statusS, %_statusL, %_statusI);

    @_statusD{qw(? i c a b R M S)} =
        qw(unknown idea pre-alpha alpha beta released mature standard);

    @_statusS{qw(? m d u n)}       =
        qw(unknown mailing-list developer comp.lang.perl.* none);

    @_statusL{qw(? p c + o h)}     = qw(unknown perl C C++ other hybrid);
    @_statusI{qw(? f r O h)}       =
        qw(unknown functions references+ties object-oriented hybrid);

    my @status = split("", $dslip);

    my $results = sprintf( "%s (%s,%s,%s,%s)",
        $dslip,
        $_statusD{$status[0]},
        $_statusS{$status[1]},
        $_statusL{$status[2]},
        $_statusI{$status[3]}
    );

    return $results;
}

sub _pp_distribution {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my( $aref, $short, $class, $input );
    my $tmpl = {
        result  => { required => 1, default => [], strict_type => 1,
                        store => \$aref },
        short   => { default => 0, store => \$short },
        class   => { required => 1, store => \$class },
        input   => { required => 1, store => \$input },
    };

    check( $tmpl, \%hash ) or return;


    ### no results
    if( !@$aref ) {
        print "No objects of type $class found for argument $input\n"
            unless $short;

    ### one result, long output desired;
    } elsif( @$aref == 1 and !$short ) {


        ### should look like this:
        #Distribution id = S/SA/SABECK/POE-Component-Client-POP3-0.02.tar.gz
        #    CPAN_USERID  SABECK (Scott Beck <scott@gossamer-threads.com>)
        #    CONTAINSMODS POE::Component::Client::POP3

        my $obj     = shift @$aref;
        my $aut_obj = $obj->author;
        my $pkg     = $obj->package;
        my $format  = "    %-12s %s\n";

        my @list    = $cb->search(  type    => 'package',
                                    allow   => [qr/^$pkg$/] );


        print "$class id = ", $obj->path(), '/', $obj->package(), "\n";
        printf $format, 'CPAN_USERID',
                    $aut_obj->cpanid .' ('. $aut_obj->author .
                    ' '. $aut_obj->email .')';

        ### yes i know it's ugly, but it's what cpan.pm does
        printf $format, 'CONTAINSMODS', join (' ', map { $_->module } @list);

    } else {

        ### should look like this:
        #Distribution    LWP             (G/GA/GAAS/libwww-perl-5.64.tar.gz)
        #Distribution    POE             (R/RC/RCAPUTO/POE-0.19.tar.gz)
        #2 items found

        for my $obj ( @$aref ) {
            printf "%-15s %s\n", $class, $obj->path() .'/'. $obj->package();
        }

        print scalar(@$aref). " items found\n" unless $short;
    }

    return $aref;
}

sub _pp_uptodate {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my( $aref, $short, $class, $input );
    my $tmpl = {
        result  => { required => 1, default => [], strict_type => 1,
                        store => \$aref },
        short   => { default => 0, store => \$short },
        class   => { required => 1, store => \$class },
        input   => { required => 1, store => \$input },
    };

    check( $tmpl, \%hash ) or return;

    my $format  = "%-25s %9s %9s  %s\n";

    my @not_uptodate;
    my $no_version;

    my %seen;
    for my $mod (@$aref) {
        next if $mod->package_is_perl_core;
        next if $seen{ $mod->package }++;


        if( $mod->installed_file and not $mod->installed_version ) {
            $no_version++;
            next;
        }

        push @not_uptodate, $mod unless $mod->is_uptodate;
    }

    unless( @not_uptodate ) {
        my $string = $input
                        ? "for $input"
                        : '';
        print "All modules are up to date $string\n";
        return;

    } else {
        printf $format, (   'Package namespace',
                            'installed',
                            'latest',
                            'in CPAN file'
                        );
    }

    for my $mod ( sort { $a->module cmp $b->module } @not_uptodate ) {
        printf $format, (   $mod->module,
                            $mod->installed_version,
                            $mod->version,
                            $mod->path .'/'. $mod->package,
                        );
    }

    print "$no_version installed modules have no (parsable) version number\n"
        if $no_version;

    return \@not_uptodate;
}

sub _pp_ls {
    my $self = shift;
    my $cb   = $self->backend;
    my %hash = @_;

    my( $aref, $short, $class, $input );
    my $tmpl = {
        result  => { required => 1, default => [], strict_type => 1,
                        store => \$aref },
        short   => { default => 0, store => \$short },
        class   => { required => 1, store => \$class },
        input   => { required => 1, store => \$input },
    };

    check( $tmpl, \%hash ) or return;

    ### should look something like this:
    #6272 2002-05-12 KANE/Acme-Comment-1.00.tar.gz
    #8171 2002-08-13 KANE/Acme-Comment-1.01.zip
    #7110 2002-09-04 KANE/Acme-Comment-1.02.tar.gz
    #7571 2002-09-08 KANE/Acme-Intraweb-1.01.tar.gz
    #6625 2001-08-23 KANE/Acme-POE-Knee-1.10.zip
    #3058 2003-10-05 KANE/Acme-Test-0.02.tar.gz

    ### don't know size or mtime
    #my $format = "%8d %10s %s/%s\n";

    for my $mod ( sort { $a->package cmp $b->package } @$aref ) {
        print "\t" . $mod->package . "\n";
    }

    return $aref;
}


#############################
### end pretty print subs ###
#############################


sub _bang {
    my $self = shift;
    my %hash = @_;

    my( $input );
    my $tmpl = {
        input   => { required => 1, store => \$input },
    };

    check( $tmpl, \%hash ) or return;

    eval $input;
    warn $@ if $@;

    print "\n";

    return;
}

sub _help {
    print qq[
Display Information
 a                                    authors
 b         string           display   bundles
 d         or               info      distributions
 m         /regex/          about     modules
 i         or                         anything of above
 r         none             reinstall recommendations
 u                          uninstalled distributions

Download, Test, Make, Install...
 get                        download
 make                       make (implies get)
 test      modules,         make test (implies make)
 install   dists, bundles   make install (implies test)
 clean                      make clean
 look                       open subshell in these dists' directories
 readme                     display these dists' README files

Other
 h,?           display this menu       ! perl-code   eval a perl command
 o conf [opt]  set and query options   q             quit the cpan shell
 reload cpan   load CPAN.pm again      reload index  load newer indices
 autobundle    Snapshot                force cmd     unconditionally do cmd
];

}



1;
__END__

=pod

=head1 NAME

CPANPLUS::Shell::Classic - CPAN.pm emulation for CPANPLUS

=head1 DESCRIPTION

The Classic shell is designed to provide the feel of the CPAN.pm shell
using CPANPLUS underneath.

For detailed documentation, refer to L<CPAN>.

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

L<CPANPLUS::Configure>, L<CPANPLUS::Module>, L<CPANPLUS::Module::Author>

=cut


=head1 SEE ALSO

L<CPAN>

=cut



# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
