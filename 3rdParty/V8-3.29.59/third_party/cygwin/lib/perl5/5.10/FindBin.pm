# FindBin.pm
#
# Copyright (c) 1995 Graham Barr & Nick Ing-Simmons. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

=head1 NAME

FindBin - Locate directory of original perl script

=head1 SYNOPSIS

 use FindBin;
 use lib "$FindBin::Bin/../lib";

 or

 use FindBin qw($Bin);
 use lib "$Bin/../lib";

=head1 DESCRIPTION

Locates the full path to the script bin directory to allow the use
of paths relative to the bin directory.

This allows a user to setup a directory tree for some software with
directories C<< <root>/bin >> and C<< <root>/lib >>, and then the above
example will allow the use of modules in the lib directory without knowing
where the software tree is installed.

If perl is invoked using the B<-e> option or the perl script is read from
C<STDIN> then FindBin sets both C<$Bin> and C<$RealBin> to the current
directory.

=head1 EXPORTABLE VARIABLES

 $Bin         - path to bin directory from where script was invoked
 $Script      - basename of script from which perl was invoked
 $RealBin     - $Bin with all links resolved
 $RealScript  - $Script with all links resolved

=head1 KNOWN ISSUES

If there are two modules using C<FindBin> from different directories
under the same interpreter, this won't work. Since C<FindBin> uses a
C<BEGIN> block, it'll be executed only once, and only the first caller
will get it right. This is a problem under mod_perl and other persistent
Perl environments, where you shouldn't use this module. Which also means
that you should avoid using C<FindBin> in modules that you plan to put
on CPAN. To make sure that C<FindBin> will work is to call the C<again>
function:

  use FindBin;
  FindBin::again(); # or FindBin->again;

In former versions of FindBin there was no C<again> function. The
workaround was to force the C<BEGIN> block to be executed again:

  delete $INC{'FindBin.pm'};
  require FindBin;

=head1 KNOWN BUGS

If perl is invoked as

   perl filename

and I<filename> does not have executable rights and a program called
I<filename> exists in the users C<$ENV{PATH}> which satisfies both B<-x>
and B<-T> then FindBin assumes that it was invoked via the
C<$ENV{PATH}>.

Workaround is to invoke perl as

 perl ./filename

=head1 AUTHORS

FindBin is supported as part of the core perl distribution. Please send bug
reports to E<lt>F<perlbug@perl.org>E<gt> using the perlbug program
included with perl.

Graham Barr E<lt>F<gbarr@pobox.com>E<gt>
Nick Ing-Simmons E<lt>F<nik@tiuk.ti.com>E<gt>

=head1 COPYRIGHT

Copyright (c) 1995 Graham Barr & Nick Ing-Simmons. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

package FindBin;
use Carp;
require 5.000;
require Exporter;
use Cwd qw(getcwd cwd abs_path);
use Config;
use File::Basename;
use File::Spec;

@EXPORT_OK = qw($Bin $Script $RealBin $RealScript $Dir $RealDir);
%EXPORT_TAGS = (ALL => [qw($Bin $Script $RealBin $RealScript $Dir $RealDir)]);
@ISA = qw(Exporter);

$VERSION = "1.49";


# needed for VMS-specific filename translation
if( $^O eq 'VMS' ) {
    require VMS::Filespec;
    VMS::Filespec->import;
}

sub cwd2 {
   my $cwd = getcwd();
   # getcwd might fail if it hasn't access to the current directory.
   # try harder.
   defined $cwd or $cwd = cwd();
   $cwd;
}

sub init
{
 *Dir = \$Bin;
 *RealDir = \$RealBin;

 if($0 eq '-e' || $0 eq '-')
  {
   # perl invoked with -e or script is on C<STDIN>
   $Script = $RealScript = $0;
   $Bin    = $RealBin    = cwd2();
   $Bin = VMS::Filespec::unixify($Bin) if $^O eq 'VMS';
  }
 else
  {
   my $script = $0;

   if ($^O eq 'VMS')
    {
     ($Bin,$Script) = VMS::Filespec::rmsexpand($0) =~ /(.*[\]>\/]+)(.*)/s;
     # C<use disk:[dev]/lib> isn't going to work, so unixify first
     ($Bin = VMS::Filespec::unixify($Bin)) =~ s/\/\z//;
     ($RealBin,$RealScript) = ($Bin,$Script);
    }
   else
    {
     my $dosish = ($^O eq 'MSWin32' or $^O eq 'os2');
     unless(($script =~ m#/# || ($dosish && $script =~ m#\\#))
            && -f $script)
      {
       my $dir;
       foreach $dir (File::Spec->path)
        {
        my $scr = File::Spec->catfile($dir, $script);

        # $script can been found via PATH but perl could have
        # been invoked as 'perl file'. Do a dumb check to see
        # if $script is a perl program, if not then keep $script = $0
        #
        # well we actually only check that it is an ASCII file
        # we know its executable so it is probably a script
        # of some sort.
        if(-f $scr && -r _ && ($dosish || -x _) && -s _ && -T _)
         {
          $script = $scr;
          last;
         }
       }
     }

     croak("Cannot find current script '$0'") unless(-f $script);

     # Ensure $script contains the complete path in case we C<chdir>

     $script = File::Spec->catfile(cwd2(), $script)
       unless File::Spec->file_name_is_absolute($script);

     ($Script,$Bin) = fileparse($script);

     # Resolve $script if it is a link
     while(1)
      {
       my $linktext = readlink($script);

       ($RealScript,$RealBin) = fileparse($script);
       last unless defined $linktext;

       $script = (File::Spec->file_name_is_absolute($linktext))
                  ? $linktext
                  : File::Spec->catfile($RealBin, $linktext);
      }

     # Get absolute paths to directories
     if ($Bin) {
      my $BinOld = $Bin;
      $Bin = abs_path($Bin);
      defined $Bin or $Bin = File::Spec->canonpath($BinOld);
     }
     $RealBin = abs_path($RealBin) if($RealBin);
    }
  }
}

BEGIN { init }

*again = \&init;

1; # Keep require happy
