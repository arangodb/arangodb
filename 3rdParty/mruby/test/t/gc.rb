# Not ISO specified

assert('GC.enable') do
  GC.disable == false
  GC.enable == true
  GC.enable == false
end

assert('GC.disable') do
  begin
    GC.disable == false
    GC.disable == true
  ensure
    GC.enable
  end
end

assert('GC.interval_ratio=') do
  origin = GC.interval_ratio
  begin
    (GC.interval_ratio = 150) == 150
  ensure
    GC.interval_ratio = origin
  end
end

assert('GC.step_ratio=') do
  origin = GC.step_ratio
  begin
    (GC.step_ratio = 150) == 150
  ensure
    GC.step_ratio = origin
  end
end

assert('GC.generational_mode=') do
  origin = GC.generational_mode
  begin
    (GC.generational_mode = false) == false
    (GC.generational_mode = true) == true
    (GC.generational_mode = true) == true
  ensure
    GC.generational_mode = origin
  end
end
