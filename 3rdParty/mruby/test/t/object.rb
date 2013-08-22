##
# Object ISO Test

assert('Object', '15.2.1') do
  assert_equal Class, Object.class
end

assert('Object superclass', '15.2.1.2') do
  assert_equal BasicObject, Object.superclass
end

