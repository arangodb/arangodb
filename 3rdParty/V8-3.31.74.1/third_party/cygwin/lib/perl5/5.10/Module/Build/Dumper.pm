package Module::Build::Dumper;

# This is just a split-out of a wrapper function to do Data::Dumper
# stuff "the right way".  See:
# http://groups.google.com/group/perl.module.build/browse_thread/thread/c8065052b2e0d741

use Data::Dumper;

sub _data_dump {
  my ($self, $data) = @_;
  return ("do{ my "
	  . Data::Dumper->new([$data],['x'])->Purity(1)->Dump()
	  . '$x; }')
}

1;
