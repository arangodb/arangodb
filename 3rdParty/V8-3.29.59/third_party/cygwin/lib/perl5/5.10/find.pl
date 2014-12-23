# Usage:
#	require "find.pl";
#
#	&find('/foo','/bar');
#
#	sub wanted { ... }
#		where wanted does whatever you want.  $dir contains the
#		current directory name, and $_ the current filename within
#		that directory.  $name contains "$dir/$_".  You are cd'ed
#		to $dir when the function is called.  The function may
#		set $prune to prune the tree.
#
# This library is primarily for find2perl, which, when fed
#
#   find2perl / -name .nfs\* -mtime +7 -exec rm -f {} \; -o -fstype nfs -prune
#
# spits out something like this
#
#	sub wanted {
#	    /^\.nfs.*$/ &&
#	    (($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_)) &&
#	    int(-M _) > 7 &&
#	    unlink($_)
#	    ||
#	    ($nlink || (($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_))) &&
#	    $dev < 0 &&
#	    ($prune = 1);
#	}
#
# Set the variable $dont_use_nlink if you're using AFS, since AFS cheats.

use File::Find ();

*name		= *File::Find::name;
*prune		= *File::Find::prune;
*dir		= *File::Find::dir;
*topdir		= *File::Find::topdir;
*topdev		= *File::Find::topdev;
*topino		= *File::Find::topino;
*topmode	= *File::Find::topmode;
*topnlink	= *File::Find::topnlink;

sub find {
    &File::Find::find(\&wanted, @_);
}

1;
