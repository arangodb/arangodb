##
# RuntimeError ISO Test

assert('RuntimeError', '15.2.28') do
  e2 = nil
  begin
    # this will cause an exception due to the wrong location
    retry
  rescue => e1
    e2 = e1
  end

  RuntimeError.class == Class and e2.class == RuntimeError
end
