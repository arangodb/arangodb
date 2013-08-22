class Symbol

  def to_proc
    Proc.new do |obj, *args|
      obj.send(self, *args)
    end
  end

end
