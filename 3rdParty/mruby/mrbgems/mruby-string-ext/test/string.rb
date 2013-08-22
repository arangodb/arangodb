##
# String(Ext) Test

assert('String#getbyte') do
  str1 = "hello"
  bytes1 = [104, 101, 108, 108, 111]
  assert_equal bytes1[0], str1.getbyte(0)
  assert_equal bytes1[-1], str1.getbyte(-1)
  assert_equal bytes1[6], str1.getbyte(6)

  str2 = "\xFF"
  bytes2 = [0xFF]
  assert_equal bytes2[0], str2.getbyte(0)
end

assert('String#dump') do
  ("\1" * 100).dump     # should not raise an exception - regress #1210
  "\0".inspect == "\"\\000\"" and
  "foo".dump == "\"foo\""
end

assert('String#strip') do
  s = "  abc  " 
  s.strip
  "".strip == "" and " \t\r\n\f\v".strip == "" and
  "\0a\0".strip == "\0a" and
  "abc".strip     == "abc" and
  "  abc".strip   == "abc" and
  "abc  ".strip   == "abc" and
  "  abc  ".strip == "abc" and
  s == "  abc  "
end

assert('String#lstrip') do
  s = "  abc  " 
  s.lstrip
  "".lstrip == "" and " \t\r\n\f\v".lstrip == "" and
  "\0a\0".lstrip == "\0a\0" and
  "abc".lstrip     == "abc"   and
  "  abc".lstrip   == "abc"   and
  "abc  ".lstrip   == "abc  " and
  "  abc  ".lstrip == "abc  " and
  s == "  abc  "
end

assert('String#rstrip') do
  s = "  abc  " 
  s.rstrip
  "".rstrip == "" and " \t\r\n\f\v".rstrip == "" and
  "\0a\0".rstrip == "\0a" and
  "abc".rstrip     == "abc"   and
  "  abc".rstrip   == "  abc" and
  "abc  ".rstrip   == "abc"   and
  "  abc  ".rstrip == "  abc" and
  s == "  abc  "
end

assert('String#strip!') do
  s = "  abc  "
  t = "abc"
  s.strip! == "abc" and s == "abc" and t.strip! == nil
end

assert('String#lstrip!') do
  s = "  abc  "
  t = "abc  "
  s.lstrip! == "abc  " and s == "abc  " and t.lstrip! == nil
end

assert('String#rstrip!') do
  s = "  abc  "
  t = "  abc"
  s.rstrip! == "  abc" and s == "  abc" and t.rstrip! == nil
end

assert('String#swapcase') do
  assert_equal "hELLO", "Hello".swapcase
  assert_equal "CyBeR_pUnK11", "cYbEr_PuNk11".swapcase
end

assert('String#swapcase!') do
  s = "Hello"
  t = s.clone
  t.swapcase!
  assert_equal s.swapcase, t
end

assert('String#concat') do
  s = "Hello "
  s.concat "World!"
  t = "Hello "
  t << "World!"
  assert_equal "Hello World!", t
  assert_equal "Hello World!", s
end

assert('String#casecmp') do
  assert_equal 1, "abcdef".casecmp("abcde")
  assert_equal 0, "aBcDeF".casecmp("abcdef")
  assert_equal(-1, "abcdef".casecmp("abcdefg"))
  assert_equal 0, "abcdef".casecmp("ABCDEF")
end

assert('String#start_with?') do
  assert_true "hello".start_with?("heaven", "hell")
  assert_true !"hello".start_with?("heaven", "paradise")
end

assert('String#end_with?') do
  assert_true "string".end_with?("ing", "mng")
  assert_true !"string".end_with?("str", "tri")
end
