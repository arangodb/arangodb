##
# Random Test

assert("Random#srand") do
  r1 = Random.new(123)
  r2 = Random.new(123)
  r1.rand == r2.rand
end

assert("Kernel::srand") do
  srand(234)
  r1 = rand
  srand(234)
  r2 = rand
  r1 == r2
end

assert("Random::srand") do
  Random.srand(345)
  r1 = rand
  srand(345)
  r2 = Random.rand
  r1 == r2
end

assert("fixnum") do
  rand(3).class == Fixnum
end

assert("float") do
  rand.class == Float
end
