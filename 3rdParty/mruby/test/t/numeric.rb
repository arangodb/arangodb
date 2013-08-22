##
# Numeric ISO Test

assert('Numeric', '15.2.7') do
  assert_equal Class, Numeric.class
end

assert('Numeric superclass', '15.2.7.2') do
  assert_equal Object, Numeric.superclass
end

assert('Numeric#+@', '15.2.7.4.1') do
  assert_equal(+1, +1)
end

assert('Numeric#-@', '15.2.7.4.2') do
  assert_equal(-1, -1)
end

assert('Numeric#abs', '15.2.7.4.3') do
  assert_equal(1, 1.abs)
  assert_equal(1.0, -1.abs)
end

# Not ISO specified

assert('Numeric#**') do
  assert_equal 8.0, 2.0**3
end
