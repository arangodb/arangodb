##
# StandardError ISO Test

assert('StandardError', '15.2.23') do
  assert_equal Class, StandardError.class
end

assert('StandardError superclass', '15.2.23.2') do
  assert_equal Exception, StandardError.superclass
end
