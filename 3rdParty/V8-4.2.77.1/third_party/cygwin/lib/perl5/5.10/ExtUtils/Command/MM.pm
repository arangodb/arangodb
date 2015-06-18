package ExtUtils::Command::MM;

require 5.006;

use strict;
use warnings;

require Exporter;
our @ISA = qw(Exporter);

our @EXPORT  = qw(test_harness pod2man perllocal_install uninstall 
                  warn_if_old_packlist);
our $VERSION = '6.44';

my $Is_VMS = $^O eq 'VMS';


=head1 NAME

ExtUtils::Command::MM - Commands for the MM's to use in Makefiles

=head1 SYNOPSIS

  perl "-MExtUtils::Command::MM" -e "function" "--" arguments...


=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>  The interface is not stable.

ExtUtils::Command::MM encapsulates code which would otherwise have to
be done with large "one" liners.

Any $(FOO) used in the examples are make variables, not Perl.

=over 4

=item B<test_harness>

  test_harness($verbose, @test_libs);

Runs the tests on @ARGV via Test::Harness passing through the $verbose
flag.  Any @test_libs will be unshifted onto the test's @INC.

@test_libs are run in alphabetical order.

=cut

sub test_harness {
    require Test::Harness;
    require File::Spec;

    $Test::Harness::verbose = shift;

    # Because Windows doesn't do this for us and listing all the *.t files
    # out on the command line can blow over its exec limit.
    require ExtUtils::Command;
    my @argv = ExtUtils::Command::expand_wildcards(@ARGV);

    local @INC = @INC;
    unshift @INC, map { File::Spec->rel2abs($_) } @_;
    Test::Harness::runtests(sort { lc $a cmp lc $b } @argv);
}



=item B<pod2man>

  pod2man( '--option=value',
           $podfile1 => $manpage1,
           $podfile2 => $manpage2,
           ...
         );

  # or args on @ARGV

pod2man() is a function performing most of the duties of the pod2man
program.  Its arguments are exactly the same as pod2man as of 5.8.0
with the addition of:

    --perm_rw   octal permission to set the resulting manpage to

And the removal of:

    --verbose/-v
    --help/-h

If no arguments are given to pod2man it will read from @ARGV.

=cut

sub pod2man {
    local @ARGV = @_ ? @_ : @ARGV;

    require Pod::Man;
    require Getopt::Long;

    # We will cheat and just use Getopt::Long.  We fool it by putting
    # our arguments into @ARGV.  Should be safe.
    my %options = ();
    Getopt::Long::config ('bundling_override');
    Getopt::Long::GetOptions (\%options, 
                'section|s=s', 'release|r=s', 'center|c=s',
                'date|d=s', 'fixed=s', 'fixedbold=s', 'fixeditalic=s',
                'fixedbolditalic=s', 'official|o', 'quotes|q=s', 'lax|l',
                'name|n=s', 'perm_rw:i'
    );

    # If there's no files, don't bother going further.
    return 0 unless @ARGV;

    # Official sets --center, but don't override things explicitly set.
    if ($options{official} && !defined $options{center}) {
        $options{center} = q[Perl Programmer's Reference Guide];
    }

    # This isn't a valid Pod::Man option and is only accepted for backwards
    # compatibility.
    delete $options{lax};

    do {{  # so 'next' works
        my ($pod, $man) = splice(@ARGV, 0, 2);

        next if ((-e $man) &&
                 (-M $man < -M $pod) &&
                 (-M $man < -M "Makefile"));

        print "Manifying $man\n";

        my $parser = Pod::Man->new(%options);
        $parser->parse_from_file($pod, $man)
          or do { warn("Could not install $man\n");  next };

        if (length $options{perm_rw}) {
            chmod(oct($options{perm_rw}), $man)
              or do { warn("chmod $options{perm_rw} $man: $!\n"); next };
        }
    }} while @ARGV;

    return 1;
}


=item B<warn_if_old_packlist>

  perl "-MExtUtils::Command::MM" -e warn_if_old_packlist <somefile>

Displays a warning that an old packlist file was found.  Reads the
filename from @ARGV.

=cut

sub warn_if_old_packlist {
    my $packlist = $ARGV[0];

    return unless -f $packlist;
    print <<"PACKLIST_WARNING";
WARNING: I have found an old package in
    $packlist.
Please make sure the two installations are not conflicting
PACKLIST_WARNING

}


=item B<perllocal_install>

    perl "-MExtUtils::Command::MM" -e perllocal_install 
        <type> <module name> <key> <value> ...

    # VMS only, key|value pairs come on STDIN
    perl "-MExtUtils::Command::MM" -e perllocal_install
        <type> <module name> < <key>|<value> ...

Prints a fragment of POD suitable for appending to perllocal.pod.
Arguments are read from @ARGV.

'type' is the type of what you're installing.  Usually 'Module'.

'module name' is simply the name of your module.  (Foo::Bar)

Key/value pairs are extra information about the module.  Fields include:

    installed into      which directory your module was out into
    LINKTYPE            dynamic or static linking
    VERSION             module version number
    EXE_FILES           any executables installed in a space seperated 
                        list

=cut

sub perllocal_install {
    my($type, $name) = splice(@ARGV, 0, 2);

    # VMS feeds args as a piped file on STDIN since it usually can't
    # fit all the args on a single command line.
    my @mod_info = $Is_VMS ? split /\|/, <STDIN>
                           : @ARGV;

    my $pod;
    $pod = sprintf <<POD, scalar localtime;
 =head2 %s: C<$type> L<$name|$name>
 
 =over 4
 
POD

    do {
        my($key, $val) = splice(@mod_info, 0, 2);

        $pod .= <<POD
 =item *
 
 C<$key: $val>
 
POD

    } while(@mod_info);

    $pod .= "=back\n\n";
    $pod =~ s/^ //mg;
    print $pod;

    return 1;
}

=item B<uninstall>

    perl "-MExtUtils::Command::MM" -e uninstall <packlist>

A wrapper around ExtUtils::Install::uninstall().  Warns that
uninstallation is deprecated and doesn't actually perform the
uninstallation.

=cut

sub uninstall {
    my($packlist) = shift @ARGV;

    require ExtUtils::Install;

    print <<'WARNING';

Uninstall is unsafe and deprecated, the uninstallation was not performed.
We will show what would have been done.

WARNING

    ExtUtils::Install::uninstall($packlist, 1, 1);

    print <<'WARNING';

Uninstall is unsafe and deprecated, the uninstallation was not performed.
Please check the list above carefully, there may be errors.
Remove the appropriate files manually.
Sorry for the inconvenience.

WARNING

}

=back

=cut

1;
