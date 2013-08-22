##
# Toplevel Self(Ext) Test

assert('Toplevel#include') do
  module ToplevelTestModule1
    def method_foo
      :foo
    end

    CONST_BAR = :bar
  end

  module ToplevelTestModule2
    CONST_BAR = :bar2
  end

  self.include ToplevelTestModule2, ToplevelTestModule1
  
  assert_true self.class.included_modules.include?( ToplevelTestModule1 )
  assert_true self.class.included_modules.include?( ToplevelTestModule2 )
  assert_equal :foo, method_foo
  assert_equal :bar2, CONST_BAR
end

