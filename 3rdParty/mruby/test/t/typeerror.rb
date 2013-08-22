##
# TypeError ISO Test

assert('TypeError', '15.2.29') do
  assert_equal Class, TypeError.class
end

assert('TypeError superclass', '15.2.29.2') do
  assert_equal StandardError, TypeError.superclass
end

