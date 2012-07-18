##
# Module ISO Test

assert('Module', '15.2.2') do
  Module.class == Class
end

assert('Module superclass', '15.2.2.2') do
  Module.superclass == Object
end

assert('Module#const_defined?', '15.2.2.4.20') do
  module Test4ConstDefined
    Const4Test4ConstDefined = true
  end

  Test4ConstDefined.const_defined?(:Const4Test4ConstDefined) and
    not Test4ConstDefined.const_defined?(:NotExisting)
end

assert('Module#const_get', '15.2.2.4.21') do
  module Test4ConstGet
    Const4Test4ConstGet = 42
  end

  Test4ConstGet.const_get(:Const4Test4ConstGet) == 42
end

assert('Module.const_missing', '15.2.2.4.22') do
  e1 = nil

  module Test4ConstMissing
    def const_missing(sym)
      # ATM this redirect doesn't work
      puts "PLEASE GO TO TEST CASE Module.const_missing!"
      puts "IT IS WORKING NOW!! PLEASE FINALIZE."
      puts "Thanks :)"
    end
  end

  begin
    Test4ConstMissing.const_get(:ConstDoesntExist)
  rescue => e2
    e1 = e2
  end

  e1.class == NameError
end

assert('Module#const_get', '15.2.2.4.23') do
  module Test4ConstSet
    Const4Test4ConstSet = 42
  end

  Test4ConstSet.const_set(:Const4Test4ConstSet, 23)
  Test4ConstSet.const_get(:Const4Test4ConstSet) == 23
end

# TODO not implemented ATM assert('Module.constants', '15.2.2') do

# TODO not implemented ATM assert('Module.nesting', '15.2.2') do
