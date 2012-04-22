#
#  Kernel
#
module Kernel
  # 15.3.1.2.6
  def self.lambda(&block)
    ### *** TODO *** ###
    block  # dummy
  end

  # 15.3.1.2.8
  def self.loop #(&block)
    while(true)
      yield
    end
  end

  # 15.3.1.3.4
  def __send__(symbol, *args, &block)
    ### *** TODO *** ###
  end

  # 15.3.1.3.18
  def instance_eval(string=nil, &block)
    ### *** TODO *** ###
  end

  # 15.3.1.3.27
  def lambda(&block)
    ### *** TODO *** ###
    block # dummy
  end

  # 15.3.1.3.29
  def loop #(&block)
    while(true)
      yield
    end
  end

  # 15.3.1.3.44
  def send(symbol, *args, &block)
    ### *** TODO *** ###
  end
end
