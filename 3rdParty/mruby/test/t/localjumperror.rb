##
# LocalJumpError ISO Test

assert('LocalJumpError', '15.2.25') do
  assert_equal Class, LocalJumpError.class
  assert_raise LocalJumpError do
    # this will cause an exception due to the wrong location
    retry
  end
end

# TODO 15.2.25.2.1 LocalJumpError#exit_value
# TODO 15.2.25.2.2 LocalJumpError#reason
