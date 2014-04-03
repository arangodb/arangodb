#!/usr/bin/perl -w

use strict;

use IO::Select;
use IPC::Open2;

my @files = `find . -type f -name "*.txt"`;

chdir "..";

my @protos = `find bson -name "*.[ch]" | grep -v win32 | xargs cproto -Ibson -Iyajl -I. -DBSON_COMPILATION -i | grep -v "^/" | sort | uniq | indent -l1000 -linux`;

chdir "doc";

my %protos = map {
   my $v = extract_function_name($_);

   if ($v) {
      @$v;
   } else {
      ();
   }
} @protos;

foreach my $file (@files) {
   chomp $file;
   open FILE, "< $file" or die "Couldn't find $file: $!";

   my $in_source;
   my $in_synopsis;
   my $in_block;

   my @lines;

   while (my $line = <FILE>) {
      if ($in_block) {
         if ($line =~ /^-+$/) {
            last;
         } else {
            push @lines, $line;
         }
         next;
      }

      if ($in_source) {
         if ($line =~ /^-+$/) {
            $in_block = 1;
         } else {
            push @lines, $line;
            last;
         }
         next;
      }

      if ($in_synopsis) {
         if ($line =~ /^\[source/) {
            $in_source = 1;
         }
         next;
      }

      if ($line =~ /^SYNOPSIS/) {
         $in_synopsis = 1;
      }
   }

   my $buf = join('', @lines);

   @lines = map { "$_\n" } split /\n/, pipe_return(join('', @lines), "indent", "-linux", "-l1000");

   foreach my $line (@lines) {
      my $v = extract_function_name($line);

      if ($v) {
         if ($protos{$v->[0]}) {
            if ($protos{$v->[0]} ne $v->[1]) {
               print "$file: $v->[0]\n  $protos{$v->[0]}\n  $v->[1]\n";
            }
         }
      }
   }
}

sub extract_function_name {
   my $line = shift;

   chomp($line);

   if ($line =~ /=/) {
      return undef;
   }

   if ($line =~ /^(.+?)(\w+)\(.*\);$/) {
      if ($1 =~ /^\s+$/) {
         return undef;
      }
      my $name = $2;
      return ([$2, $line]);
   }

   return undef;
}

sub pipe_return {
   my ($buf, $cmd, @args) = @_;

   my ($out_fh, $in_fh);

   my $pid = open2($out_fh, $in_fh, $cmd, @args);

   my $out = '';

   my $bytes_written = 0;
   my $bytes_read = 0;
   my $length = length $buf;

   my $s = IO::Select->new($out_fh, $in_fh);

   while ($bytes_written < $length) {
      if ($s->can_write(0)) {
         my $r = syswrite($in_fh, $buf, $length - $bytes_written, $bytes_written);

         if (! defined($r)) {
            die "error in write: $!";
         }

         $bytes_written += $r;
      } elsif ($s->can_read(0)) {
         my $r = sysread($out_fh, $out, 4096, $bytes_read);

         if (! defined($r)) {
            die "error in read: $!";
         }

         $bytes_read += $r;
      }
   }

   close $in_fh;

   while (1) {
      my $r = sysread($out_fh, $out, 4096, $bytes_read);

      if (! defined($r)) {
         die "error in read: $!";
      }

      $bytes_read += $r;

      if ($r == 0) {
         last;
      }
   }

   close $out_fh;

   waitpid ($pid, 0);

   return $out;
}
