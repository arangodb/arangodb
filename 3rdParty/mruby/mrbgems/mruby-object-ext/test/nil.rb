assert('NilClass#to_a') do
  assert_equal [], nil.to_a
end

assert('NilClass#to_f') do
  assert_equal 0.0, nil.to_f
end

assert('NilClass#to_i') do
  assert_equal 0, nil.to_i
end
