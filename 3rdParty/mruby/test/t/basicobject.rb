##
# BasicObject

assert('BasicObject') do
  assert_equal(Class, BasicObject.class)
end

assert('BasicObject superclass') do
  assert_nil(BasicObject.superclass)
end

