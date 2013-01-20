$ok_test = 0
$ko_test = 0
$kill_test = 0
$asserts  = []
$test_start = Time.now if Object.const_defined?(:Time)

##
# Create the assertion in a readable way
def assertion_string(err, str, iso=nil, e=nil)
  msg = "#{err}#{str}"
  msg += " [#{iso}]" if iso && iso != ''
  msg += " => #{e.message}" if e
  msg += " (mrbgems: #{GEMNAME})" if Object.const_defined?(:GEMNAME)
  msg
end

##
# Verify a code block.
#
# str : A remark which will be printed in case
#       this assertion fails
# iso : The ISO reference code of the feature
#       which will be tested by this
#       assertion
def assert(str = 'Assertion failed', iso = '')
  begin
    if(!yield)
      $asserts.push(assertion_string('Fail: ', str, iso))
      $ko_test += 1
      print('F')
    else
      $ok_test += 1
      print('.')
    end
  rescue Exception => e
    $asserts.push(assertion_string('Error: ', str, iso, e))
    $kill_test += 1
    print('X')
  end
end

##
# Report the test result and print all assertions
# which were reported broken.
def report()
  print "\n"

  $asserts.each do |msg|
    puts msg
  end

  $total_test = $ok_test.+($ko_test)
  print('Total: ')
  print($total_test)
  print("\n")

  print('   OK: ')
  print($ok_test)
  print("\n")
  print('   KO: ')
  print($ko_test)
  print("\n")
  print('Crash: ')
  print($kill_test)
  print("\n")

  if Object.const_defined?(:Time)
    print(' Time: ')
    print(Time.now - $test_start)
    print(" seconds\n")
  end
end

##
# Performs fuzzy check for equality on methods returning floats
# on the basis of the Math::TOLERANCE constant.
def check_float(a, b)
  tolerance = Math::TOLERANCE
  a = a.to_f
  b = b.to_f
  if a.finite? and b.finite?
    (a-b).abs < tolerance
  else
    true
  end
end
