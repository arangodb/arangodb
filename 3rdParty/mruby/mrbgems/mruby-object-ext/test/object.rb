assert('Object#instance_exec') do
  class KlassWithSecret
    def initialize
      @secret = 99
    end
  end
  k = KlassWithSecret.new
  assert_equal 104, k.instance_exec(5) {|x| @secret+x }
end
