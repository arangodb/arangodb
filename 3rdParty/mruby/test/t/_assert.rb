$ok_test = 0
$ko_test = 0
$asserts  = []

##
# Verify a code block.
#
# str : A remark which will be printed in case
#       this assertion fails
# iso : The ISO reference code of the feature
#       which will be tested by this
#       assertion
def assert(str = 'Assertion failed', iso = '')
 if(!yield)
    $asserts.push([str, iso])
    $ko_test += 1
    print "F"
 else
    $ok_test += 1
    print "."
  end
end

##
# Report the test result and print all assertions
# which were reported broken.
def report()
  print "\n"
  $asserts.each do |str, iso|
    print("Test Failed: #{str} [#{iso}]\n");
  end

  $total_test = $ok_test + $ko_test
  print 'Total tests:'
  print $total_test
  print "\n"

  print '  OK: '
  print $ok_test
  print "\n"
  print '  KO: '
  print $ko_test
  print "\n"
end
