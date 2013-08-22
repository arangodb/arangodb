##
# RangeError ISO Test

assert('RangeError', '15.2.26') do
  assert_equal Class, RangeError.class
end

assert('RangeError superclass', '15.2.26.2') do
  assert_equal StandardError, RangeError.superclass
end
