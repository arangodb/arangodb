# By John Bazik
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Cwd
#
# Usage: $cwd = &fastcwd;
#
# This is a faster version of getcwd.  It's also more dangerous because
# you might chdir out of a directory that you can't chdir back into.

sub fastcwd {
	local($odev, $oino, $cdev, $cino, $tdev, $tino);
	local(@path, $path);
	local(*DIR);

	($cdev, $cino) = stat('.');
	for (;;) {
		($odev, $oino) = ($cdev, $cino);
		chdir('..');
		($cdev, $cino) = stat('.');
		last if $odev == $cdev && $oino == $cino;
		opendir(DIR, '.');
		for (;;) {
			$_ = readdir(DIR);
			next if $_ eq '.';
			next if $_ eq '..';

			last unless $_;
			($tdev, $tino) = lstat($_);
			last unless $tdev != $odev || $tino != $oino;
		}
		closedir(DIR);
		unshift(@path, $_);
	}
	chdir($path = '/' . join('/', @path));
	$path;
}
1;
