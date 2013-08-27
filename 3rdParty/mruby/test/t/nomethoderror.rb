##
# NoMethodError ISO Test

assert('NoMethodError', '15.2.32') do
  NoMethodError.class == Class
  assert_raise NoMethodError do
    doesNotExistAsAMethodNameForVerySure("")
  end
end

assert('NoMethodError superclass', '15.2.32.2') do
  assert_equal NameError, NoMethodError.superclass
end
