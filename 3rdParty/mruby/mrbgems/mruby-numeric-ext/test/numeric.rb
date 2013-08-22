##
# Numeric(Ext) Test

assert('Integer#chr') do
  assert_equal("A", 65.chr)
  assert_equal("B", 0x42.chr)

  # multibyte encoding (not support yet)
  assert_raise(RangeError) { 12345.chr }
end
