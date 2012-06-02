##
# Time ISO Test

assert('Time', '15.2.19') do
  Time.class == Class
end

assert('Time.at', '15.2.19.6.1') do
  Time.at(1300000000.0)
end

assert('Time.gm', '15.2.19.6.2') do
  Time.gm(2012, 12, 23)
end

assert('Time#asctime', '15.2.19.7.4') do
  Time.at(1300000000.0).utc.asctime == "Sun Mar 13 07:06:40 UTC 2011"
end

assert('Time#initialize_copy', '15.2.19.7.17') do
  time_tmp_2 = Time.at(7.0e6)
  time_tmp_2.clone == time_tmp_2
end

assert('Time#mday', '15.2.19.7.19') do
  Time.gm(2012, 12, 23).mday == 23
end

assert('Time#month', '15.2.19.7.22') do
  Time.gm(2012, 12, 23).month == 12
end

assert('Time#to_f', '15.2.19.7.24') do
  Time.at(1300000000.0).to_f == 1300000000.0
end

assert('Time#to_i', '15.2.19.7.25') do
  Time.at(1300000000.0).to_i == 1300000000
end

assert('Time#usec', '15.2.19.7.26') do
  Time.at(1300000000.0).usec == 0
end

assert('Time#utc', '15.2.19.7.27') do
  Time.at(1300000000.0).utc
end

assert('Time#utc?', '15.2.19.7.28') do
  Time.at(1300000000.0).utc.utc?
end

assert('Time#wday', '15.2.19.7.30') do
  Time.at(1300000000.0).utc.wday == 0
end

assert('Time#yday', '15.2.19.7.31') do
  Time.at(1300000000.0).utc.yday == 71
end

assert('Time#year', '15.2.19.7.32') do
  Time.gm(2012, 12, 23).year == 2012
end

assert('Time#zone', '15.2.19.7.33') do
  Time.at(1300000000.0).utc.zone == 'UTC'
end

# Not ISO specified

assert('Time#new') do
  Time.new.class == Time
end
