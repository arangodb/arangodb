package App::Prove;

use strict;
use TAP::Harness;
use TAP::Parser::Utils qw( split_shell );
use File::Spec;
use Getopt::Long;
use App::Prove::State;
use Carp;

use vars qw($VERSION);

=head1 NAME

App::Prove - Implements the C<prove> command.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

L<Test::Harness> provides a command, C<prove>, which runs a TAP based
test suite and prints a report. The C<prove> command is a minimal
wrapper around an instance of this module.

=head1 SYNOPSIS

    use App::Prove;

    my $app = App::Prove->new;
    $app->process_args(@ARGV);
    $app->run;

=cut

use constant IS_WIN32 => ( $^O =~ /^(MS)?Win32$/ );
use constant IS_VMS => $^O eq 'VMS';
use constant IS_UNIXY => !( IS_VMS || IS_WIN32 );

use constant STATE_FILE => IS_UNIXY ? '.prove'   : '_prove';
use constant RC_FILE    => IS_UNIXY ? '.proverc' : '_proverc';

use constant PLUGINS => 'App::Prove::Plugin';

my @ATTR;

BEGIN {
    @ATTR = qw(
      archive argv blib color directives exec failures fork formatter
      harness includes modules plugins jobs lib merge parse quiet
      really_quiet recurse backwards shuffle taint_fail taint_warn timer
      verbose warnings_fail warnings_warn show_help show_man
      show_version test_args state dry
    );
    for my $attr (@ATTR) {
        no strict 'refs';
        *$attr = sub {
            my $self = shift;
            croak "$attr is read-only" if @_;
            $self->{$attr};
        };
    }
}

=head1 METHODS

=head2 Class Methods

=head3 C<new>

Create a new C<App::Prove>. Optionally a hash ref of attribute
initializers may be passed.

=cut

sub new {
    my $class = shift;
    my $args = shift || {};

    my $self = bless {
        argv          => [],
        rc_opts       => [],
        includes      => [],
        modules       => [],
        state         => [],
        plugins       => [],
        harness_class => 'TAP::Harness',
        _state        => App::Prove::State->new( { store => STATE_FILE } ),
    }, $class;

    for my $attr (@ATTR) {
        if ( exists $args->{$attr} ) {

            # TODO: Some validation here
            $self->{$attr} = $args->{$attr};
        }
    }
    return $self;
}

=head3 C<add_rc_file>

    $prove->add_rc_file('myproj/.proverc');

Called before C<process_args> to prepend the contents of an rc file to
the options.

=cut

sub add_rc_file {
    my ( $self, $rc_file ) = @_;

    local *RC;
    open RC, "<$rc_file" or croak "Can't read $rc_file ($!)";
    while ( defined( my $line = <RC> ) ) {
        push @{ $self->{rc_opts} }, grep $_ && $_ !~ /^#/,
          $line =~ m{ ' ([^']*) ' | " ([^"]*) " | (\#.*) | (\S*) }xg;
    }
    close RC;
}

=head3 C<process_args>

    $prove->process_args(@args);

Processes the command-line arguments. Attributes will be set
appropriately. Any filenames may be found in the C<argv> attribute.

Dies on invalid arguments.

=cut

sub process_args {
    my $self = shift;

    my @rc = RC_FILE;
    unshift @rc, glob '~/' . RC_FILE if IS_UNIXY;

    # Preprocess meta-args.
    my @args;
    while ( defined( my $arg = shift ) ) {
        if ( $arg eq '--norc' ) {
            @rc = ();
        }
        elsif ( $arg eq '--rc' ) {
            defined( my $rc = shift )
              or croak "Missing argument to --rc";
            push @rc, $rc;
        }
        elsif ( $arg =~ m{^--rc=(.+)$} ) {
            push @rc, $1;
        }
        else {
            push @args, $arg;
        }
    }

    # Everything after the arisdottle '::' gets passed as args to
    # test programs.
    if ( defined( my $stop_at = _first_pos( '::', @args ) ) ) {
        my @test_args = splice @args, $stop_at;
        shift @test_args;
        $self->{test_args} = \@test_args;
    }

    # Grab options from RC files
    $self->add_rc_file($_) for grep -f, @rc;
    unshift @args, @{ $self->{rc_opts} };

    if ( my @bad = map {"-$_"} grep {/^-(man|help)$/} @args ) {
        die "Long options should be written with two dashes: ",
          join( ', ', @bad ), "\n";
    }

    # And finally...

    {
        local @ARGV = @args;
        Getopt::Long::Configure( 'no_ignore_case', 'bundling' );

        # Don't add coderefs to GetOptions
        GetOptions(
            'v|verbose'   => \$self->{verbose},
            'f|failures'  => \$self->{failures},
            'l|lib'       => \$self->{lib},
            'b|blib'      => \$self->{blib},
            's|shuffle'   => \$self->{shuffle},
            'color!'      => \$self->{color},
            'colour!'     => \$self->{color},
            'c'           => \$self->{color},
            'D|dry'       => \$self->{dry},
            'harness=s'   => \$self->{harness},
            'formatter=s' => \$self->{formatter},
            'r|recurse'   => \$self->{recurse},
            'reverse'     => \$self->{backwards},
            'fork'        => \$self->{fork},
            'p|parse'     => \$self->{parse},
            'q|quiet'     => \$self->{quiet},
            'Q|QUIET'     => \$self->{really_quiet},
            'e|exec=s'    => \$self->{exec},
            'm|merge'     => \$self->{merge},
            'I=s@'        => $self->{includes},
            'M=s@'        => $self->{modules},
            'P=s@'        => $self->{plugins},
            'state=s@'    => $self->{state},
            'directives'  => \$self->{directives},
            'h|help|?'    => \$self->{show_help},
            'H|man'       => \$self->{show_man},
            'V|version'   => \$self->{show_version},
            'a|archive=s' => \$self->{archive},
            'j|jobs=i'    => \$self->{jobs},
            'timer'       => \$self->{timer},
            'T'           => \$self->{taint_fail},
            't'           => \$self->{taint_warn},
            'W'           => \$self->{warnings_fail},
            'w'           => \$self->{warnings_warn},
        ) or croak('Unable to continue');

        # Stash the remainder of argv for later
        $self->{argv} = [@ARGV];
    }

    return;
}

sub _first_pos {
    my $want = shift;
    for ( 0 .. $#_ ) {
        return $_ if $_[$_] eq $want;
    }
    return;
}

sub _exit { exit( $_[1] || 0 ) }

sub _help {
    my ( $self, $verbosity ) = @_;

    eval('use Pod::Usage 1.12 ()');
    if ( my $err = $@ ) {
        die 'Please install Pod::Usage for the --help option '
          . '(or try `perldoc prove`.)'
          . "\n ($@)";
    }

    Pod::Usage::pod2usage( { -verbose => $verbosity } );

    return;
}

sub _color_default {
    my $self = shift;

    return -t STDOUT && !IS_WIN32;
}

sub _get_args {
    my $self = shift;

    my %args;

    if ( defined $self->color ? $self->color : $self->_color_default ) {
        $args{color} = 1;
    }

    if ( $self->archive ) {
        $self->require_harness( archive => 'TAP::Harness::Archive' );
        $args{archive} = $self->archive;
    }

    if ( my $jobs = $self->jobs ) {
        $args{jobs} = $jobs;
    }

    if ( my $fork = $self->fork ) {
        $args{fork} = $fork;
    }

    if ( my $harness_opt = $self->harness ) {
        $self->require_harness( harness => $harness_opt );
    }

    if ( my $formatter = $self->formatter ) {
        $args{formatter_class} = $formatter;
    }

    if ( $self->taint_fail && $self->taint_warn ) {
        die '-t and -T are mutually exclusive';
    }

    if ( $self->warnings_fail && $self->warnings_warn ) {
        die '-w and -W are mutually exclusive';
    }

    for my $a (qw( lib switches )) {
        my $method = "_get_$a";
        my $val    = $self->$method();
        $args{$a} = $val if defined $val;
    }

    # Handle verbose, quiet, really_quiet flags
    my %verb_map = ( verbose => 1, quiet => -1, really_quiet => -2, );

    my @verb_adj = grep {$_} map { $self->$_() ? $verb_map{$_} : 0 }
      keys %verb_map;

    die "Only one of verbose, quiet or really_quiet should be specified\n"
      if @verb_adj > 1;

    $args{verbosity} = shift @verb_adj || 0;

    for my $a (qw( merge failures timer directives )) {
        $args{$a} = 1 if $self->$a();
    }

    $args{errors} = 1 if $self->parse;

    # defined but zero-length exec runs test files as binaries
    $args{exec} = [ split( /\s+/, $self->exec ) ]
      if ( defined( $self->exec ) );

    if ( defined( my $test_args = $self->test_args ) ) {
        $args{test_args} = $test_args;
    }

    return ( \%args, $self->{harness_class} );
}

sub _find_module {
    my ( $self, $class, @search ) = @_;

    croak "Bad module name $class"
      unless $class =~ /^ \w+ (?: :: \w+ ) *$/x;

    for my $pfx (@search) {
        my $name = join( '::', $pfx, $class );
        print "$name\n";
        eval "require $name";
        return $name unless $@;
    }

    eval "require $class";
    return $class unless $@;
    return;
}

sub _load_extension {
    my ( $self, $class, @search ) = @_;

    my @args = ();
    if ( $class =~ /^(.*?)=(.*)/ ) {
        $class = $1;
        @args = split( /,/, $2 );
    }

    if ( my $name = $self->_find_module( $class, @search ) ) {
        $name->import(@args);
    }
    else {
        croak "Can't load module $class";
    }
}

sub _load_extensions {
    my ( $self, $ext, @search ) = @_;
    $self->_load_extension( $_, @search ) for @$ext;
}

=head3 C<run>

Perform whatever actions the command line args specified. The C<prove>
command line tool consists of the following code:

    use App::Prove;

    my $app = App::Prove->new;
    $app->process_args(@ARGV);
    $app->run;

=cut

sub run {
    my $self = shift;

    if ( $self->show_help ) {
        $self->_help(1);
    }
    elsif ( $self->show_man ) {
        $self->_help(2);
    }
    elsif ( $self->show_version ) {
        $self->print_version;
    }
    elsif ( $self->dry ) {
        print "$_\n" for $self->_get_tests;
    }
    else {

        $self->_load_extensions( $self->modules );
        $self->_load_extensions( $self->plugins, PLUGINS );

        local $ENV{TEST_VERBOSE} = 1 if $self->verbose;

        $self->_runtests( $self->_get_args, $self->_get_tests );
    }

    return;
}

sub _get_tests {
    my $self = shift;

    my $state = $self->{_state};
    if ( defined( my $state_switch = $self->state ) ) {
        $state->apply_switch(@$state_switch);
    }

    my @tests = $state->get_tests( $self->recurse, @{ $self->argv } );

    $self->_shuffle(@tests) if $self->shuffle;
    @tests = reverse @tests if $self->backwards;

    return @tests;
}

sub _runtests {
    my ( $self, $args, $harness_class, @tests ) = @_;
    my $harness = $harness_class->new($args);

    $harness->callback(
        after_test => sub {
            $self->{_state}->observe_test(@_);
        }
    );

    my $aggregator = $harness->runtests(@tests);

    $self->_exit( $aggregator->has_problems ? 1 : 0 );

    return;
}

sub _get_switches {
    my $self = shift;
    my @switches;

    # notes that -T or -t must be at the front of the switches!
    if ( $self->taint_fail ) {
        push @switches, '-T';
    }
    elsif ( $self->taint_warn ) {
        push @switches, '-t';
    }
    if ( $self->warnings_fail ) {
        push @switches, '-W';
    }
    elsif ( $self->warnings_warn ) {
        push @switches, '-w';
    }

    push @switches, split_shell( $ENV{HARNESS_PERL_SWITCHES} );

    return @switches ? \@switches : ();
}

sub _get_lib {
    my $self = shift;
    my @libs;
    if ( $self->lib ) {
        push @libs, 'lib';
    }
    if ( $self->blib ) {
        push @libs, 'blib/lib', 'blib/arch';
    }
    if ( @{ $self->includes } ) {
        push @libs, @{ $self->includes };
    }

    #24926
    @libs = map { File::Spec->rel2abs($_) } @libs;

    # Huh?
    return @libs ? \@libs : ();
}

sub _shuffle {
    my $self = shift;

    # Fisher-Yates shuffle
    my $i = @_;
    while ($i) {
        my $j = rand $i--;
        @_[ $i, $j ] = @_[ $j, $i ];
    }
    return;
}

=head3 C<require_harness>

Load a harness replacement class.

  $prove->require_harness($for => $class_name);

=cut

sub require_harness {
    my ( $self, $for, $class ) = @_;

    eval("require $class");
    die "$class is required to use the --$for feature: $@" if $@;

    $self->{harness_class} = $class;

    return;
}

=head3 C<print_version>

Display the version numbers of the loaded L<TAP::Harness> and the
current Perl.

=cut

sub print_version {
    my $self = shift;
    printf(
        "TAP::Harness v%s and Perl v%vd\n",
        $TAP::Harness::VERSION, $^V
    );

    return;
}

1;

# vim:ts=4:sw=4:et:sta

__END__

=head2 Attributes

After command line parsing the following attributes reflect the values
of the corresponding command line switches. They may be altered before
calling C<run>.

=over

=item C<archive>

=item C<argv>

=item C<backwards>

=item C<blib>

=item C<color>

=item C<directives>

=item C<dry>

=item C<exec>

=item C<failures>

=item C<fork>

=item C<formatter>

=item C<harness>

=item C<includes>

=item C<jobs>

=item C<lib>

=item C<merge>

=item C<modules>

=item C<parse>

=item C<plugins>

=item C<quiet>

=item C<really_quiet>

=item C<recurse>

=item C<show_help>

=item C<show_man>

=item C<show_version>

=item C<shuffle>

=item C<state>

=item C<taint_fail>

=item C<taint_warn>

=item C<test_args>

=item C<timer>

=item C<verbose>

=item C<warnings_fail>

=item C<warnings_warn>

=back
