#!/usr/bin/perl

use Proc::ProcessTable;

$ref = new Proc::ProcessTable;

foreach $proc (@{$ref->table}) {
  if(@ARGV) {
    next unless grep {$_ == $proc->{pid}} @ARGV;
  }

  print "--------------------------------\n";
  foreach $field ($ref->fields){
    print $field, ":  ", $proc->{$field}, "\n";
  }
}
