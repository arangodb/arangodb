##
# Integer
#
# ISO 15.2.8
class Integer

  ##
  # Returns the receiver simply.
  #
  # ISO 15.2.8.3.14
  def ceil
    self
  end

  ##
  # Calls the given block once for each Integer
  # from +self+ downto +num+.
  #
  # ISO 15.2.8.3.15
  def downto(num, &block)
    i = self
    while(i >= num)
      block.call(i)
      i -= 1
    end
    self
  end

  ##
  # Returns the receiver simply.
  #
  # ISO 15.2.8.3.17
  def floor
    self
  end

  ##
  # Calls the given block +self+ times.
  #
  # ISO 15.2.8.3.22
  def times(&block)
    i = 0
    while(i < self)
      block.call(i)
      i += 1
    end
    self
  end

  ##
  # Returns the receiver simply.
  #
  # ISO 15.2.8.3.24
  def round
    self
  end

  ##
  # Returns the receiver simply.
  #
  # ISO 15.2.8.3.26
  def truncate
    self
  end

  ##
  # Calls the given block once for each Integer
  # from +self+ upto +num+.
  #
  # ISO 15.2.8.3.27
  def upto(num, &block)
    i = self
    while(i <= num)
      block.call(i)
      i += 1
    end
    self
  end

  ##
  # Calls the given block from +self+ to +num+
  # incremented by +step+ (default 1).
  #
  def step(num, step=1, &block)
    i = if num.kind_of? Float then self.to_f else self end
    while(i <= num)
      block.call(i)
      i += step
    end
    self
  end
end

##
# Numeric is comparable
#
# ISO 15.2.7.3
module Comparable; end
class Numeric
  include Comparable
end
