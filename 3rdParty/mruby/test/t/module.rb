##
# Module ISO Test

assert('Module', '15.2.2') do
  Module.class == Class
end

assert('Module superclass', '15.2.2.2') do
  Module.superclass == Object
end

# TODO not implemented ATM assert('Module.constants', '15.2.2.3.1') do

# TODO not implemented ATM assert('Module.nesting', '15.2.2.3.2') do

assert('Module#ancestors', '15.2.2.4.9') do
  r = String.ancestors
  r.class == Array and r.include?(String) and r.include?(Object)
end

assert('Module#append_features', '15.2.2.4.10') do
  module Test4AppendFeatures
    def self.append_features(mod)
      Test4AppendFeatures2.const_set(:Const4AppendFeatures2, mod)
    end
  end
  module Test4AppendFeatures2
    include Test4AppendFeatures
  end

  Test4AppendFeatures2.const_get(:Const4AppendFeatures2) == Test4AppendFeatures2
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
    def self.const_missing(sym)
      42 # the answer to everything
    end
  end

  Test4ConstMissing.const_get(:ConstDoesntExist) == 42
end

assert('Module#const_get', '15.2.2.4.23') do
  module Test4ConstSet
    Const4Test4ConstSet = 42
  end

  Test4ConstSet.const_set(:Const4Test4ConstSet, 23)
  Test4ConstSet.const_get(:Const4Test4ConstSet) == 23
end

assert('Module#include', '15.2.2.4.27') do
  module Test4Include
    Const4Include = 42
  end
  module Test4Include2
    include Test4Include
  end

  Test4Include2.const_get(:Const4Include) == 42
end

assert('Module#included', '15.2.2.4.29') do
  module Test4Included
    Const4Included = 42
    def self.included mod
      Test4Included.const_set(:Const4Included2, mod)
    end
  end
  module Test4Included2
    include Test4Included
  end

  Test4Included2.const_get(:Const4Included) == 42 and
    Test4Included2.const_get(:Const4Included2) == Test4Included2
end

assert('Module#included_modules', '15.2.2.4.30') do
  module Test4includedModules
  end
  module Test4includedModules2
    include Test4includedModules
  end

  r = Test4includedModules2.included_modules
  r.class == Array and r.include?(Test4includedModules)
end

# Not ISO specified

assert('Module#to_s') do
  module Test4to_sModules
  end

  Test4to_sModules.to_s == 'Test4to_sModules'
end

assert('Module#inspect') do
  module Test4to_sModules
  end

  Test4to_sModules.inspect == 'Test4to_sModules'
end
