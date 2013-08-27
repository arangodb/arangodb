##
# Exception ISO Test

assert('Exception', '15.2.22') do
  assert_equal Class, Exception.class
end

assert('Exception superclass', '15.2.22.2') do
  assert_equal Object, Exception.superclass
end

assert('Exception.exception', '15.2.22.4.1') do
  e = Exception.exception('a')

  assert_equal Exception, e.class
end

assert('Exception#exception', '15.2.22.5.1') do
  e1 = Exception.exception()
  e2 = Exception.exception('b')

  assert_equal Exception, e1.class
  assert_equal Exception, e2.class
end

assert('Exception#message', '15.2.22.5.2') do
  e = Exception.exception('a')

  assert_equal 'a', e.message
end

assert('Exception#to_s', '15.2.22.5.3') do
  e = Exception.exception('a')

  assert_equal 'a', e.to_s
end

assert('Exception.exception', '15.2.22.4.1') do
  e = Exception.exception()
  e.initialize('a')

  assert_equal 'a', e.message
end

assert('ScriptError', '15.2.37') do
  assert_raise(ScriptError) do
    raise ScriptError.new
  end
end

assert('SyntaxError', '15.2.38') do
  assert_raise(SyntaxError) do
    raise SyntaxError.new
  end
end

# Not ISO specified

assert('Exception 1') do
  begin
    1+1
  ensure
    2+2
  end == 2
end

assert('Exception 2') do
  begin
    1+1
    begin
      2+2
    ensure
      3+3
    end
  ensure
    4+4
  end == 4
end

assert('Exception 3') do
  begin
    1+1
    begin
      2+2
    ensure
      3+3
    end
  ensure
    4+4
    begin
      5+5
    ensure
      6+6
    end
  end == 4
end

assert('Exception 4') do
  a = nil
  1.times{|e|
    begin
    rescue => err
    end
    a = err.class
  }
  assert_equal NilClass, a
end

assert('Exception 5') do
  $ans = []
  def m
    $!
  end
  def m2
    1.times{
      begin
        return
      ensure
        $ans << m
      end
    }
  end
  m2
  assert_equal [nil], $ans
end

assert('Exception 6') do
  $i = 0
  def m
    iter{
      begin
        $i += 1
        begin
          $i += 2
          break
        ensure

        end
      ensure
        $i += 4
      end
      $i = 0
    }
  end

  def iter
    yield
  end
  m
  assert_equal 7, $i
end

assert('Exception 7') do
  $i = 0
  def m
    begin
      $i += 1
      begin
        $i += 2
        return
      ensure
        $i += 3
      end
    ensure
      $i += 4
    end
    p :end
  end
  m
  assert_equal 10, $i
end

assert('Exception 8') do
  begin
    1
  rescue
    2
  else
    3
  end == 3
end

assert('Exception 9') do
  begin
    1+1
  rescue
    2+2
  else
    3+3
  ensure
    4+4
  end == 6
end

assert('Exception 10') do
  begin
    1+1
    begin
      2+2
    rescue
      3+3
    else
      4+4
    end
  rescue
    5+5
  else
    6+6
  ensure
    7+7
  end == 12
end

assert('Exception 11') do
  a = :ok
  begin
    begin
      raise Exception
    rescue
      a = :ng
    end
  rescue Exception
  end
  assert_equal :ok, a
end

assert('Exception 12') do
  a = :ok
  begin
    raise Exception rescue a = :ng
  rescue Exception
  end
  assert_equal :ok, a
end

assert('Exception 13') do
  a = :ng
  begin
    raise StandardError
  rescue TypeError, ArgumentError
    a = :ng
  rescue
    a = :ok
  else
    a = :ng
  end
  assert_equal :ok, a
end

assert('Exception 14') do
  def exception_test14; UnknownConstant; end
  a = :ng
  begin
    send(:exception_test14)
  rescue
    a = :ok
  end

  assert_equal :ok, a
end

assert('Exception 15') do
  a = begin
        :ok
      rescue
        :ko
      end
  assert_equal :ok, a
end

assert('Exception 16') do
  begin
    raise "foo"
    false
  rescue => e
    e.message == "foo"
  end
end

assert('Exception 17') do
  begin
    raise "a"  # StandardError
  rescue ArgumentError
    1
  rescue StandardError
    2
  else
    3
  ensure
    4
  end == 2
end

assert('Exception 18') do
  begin
    0
  rescue ArgumentError
    1
  rescue StandardError
    2
  else
    3
  ensure
    4
  end == 3
end

assert('Exception 19') do
  class Class4Exception19
    def a
      r = @e = false
      begin
        b
      rescue TypeError
        r = self.z
      end
      [ r, @e ]
    end

    def b
      begin
        1 * "b"
      ensure
        @e = self.z
      end
    end

    def z
      true
    end
  end
  assert_equal [true, true], Class4Exception19.new.a
end

assert('Exception#inspect without message') do
  Exception.new.inspect
end
