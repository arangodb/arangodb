package blib;

=head1 NAME

blib - Use MakeMaker's uninstalled version of a package

=head1 SYNOPSIS

 perl -Mblib script [args...]

 perl -Mblib=dir script [args...]

=head1 DESCRIPTION

Looks for MakeMaker-like I<'blib'> directory structure starting in 
I<dir> (or current directory) and working back up to five levels of '..'.

Intended for use on command line with B<-M> option as a way of testing
arbitrary scripts against an uninstalled version of a package.

However it is possible to : 

 use blib; 
 or 
 use blib '..';

etc. if you really must.

=head1 BUGS

Pollutes global name space for development only task.

=head1 AUTHOR

Nick Ing-Simmons nik@tiuk.ti.com

=cut

use Cwd;
use File::Spec;

use vars qw($VERSION $Verbose);
$VERSION = '1.03';
$Verbose = 0;

sub import
{
 my $package = shift;
 my $dir;
 if ($^O eq "MSWin32" && -f "Win32.xs") {
     # We don't use getcwd() on Windows because it will internally
     # call Win32::GetCwd(), which will get the Win32 module loaded.
     # That means that it would not be possible to run `make test`
     # for the Win32 module because blib.pm would always load the
     # installed version before @INC gets updated with the blib path.
     chomp($dir = `cd`);
 }
 else {
     $dir = getcwd;
 }
 if ($^O eq 'VMS') { ($dir = VMS::Filespec::unixify($dir)) =~ s-/\z--; }
 if (@_)
  {
   $dir = shift;
   $dir =~ s/blib\z//;
   $dir =~ s,/+\z,,;
   $dir = File::Spec->curdir unless ($dir);
   die "$dir is not a directory\n" unless (-d $dir);
  }
 my $i = 5;
 my($blib, $blib_lib, $blib_arch);
 while ($i--)
  {
   $blib = File::Spec->catdir($dir, "blib");
   $blib_lib = File::Spec->catdir($blib, "lib");

   if ($^O eq 'MacOS')
    {
     $blib_arch = File::Spec->catdir($blib_lib, $MacPerl::Architecture);
    }
   else
    {
     $blib_arch = File::Spec->catdir($blib, "arch");
    }

   if (-d $blib && -d $blib_arch && -d $blib_lib)
    {
     unshift(@INC,$blib_arch,$blib_lib);
     warn "Using $blib\n" if $Verbose;
     return;
    }
   $dir = File::Spec->catdir($dir, File::Spec->updir);
  }
 die "Cannot find blib even in $dir\n";
}

1;
