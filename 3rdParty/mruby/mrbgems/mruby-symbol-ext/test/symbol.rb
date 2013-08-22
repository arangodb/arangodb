##
# Symbol(Ext) Test

assert('Symbol#to_proc') do
  assert_equal 5, :abs.to_proc[-5]
end

assert('Symbol.all_symbols') do
  foo = [:__symbol_test_1, :__symbol_test_2, :__symbol_test_3].sort
  symbols = Symbol.all_symbols.select{|sym|sym.to_s.include? '__symbol_test'}.sort
  assert_equal foo, symbols
end
