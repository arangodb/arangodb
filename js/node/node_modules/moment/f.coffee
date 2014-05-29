moment = require './moment'

year = parseInt(process.argv[2], 10)

m = moment([year, 0, 1])
w = m.weekday()
if w < 4
    m.add(4 - w, 'days')
else
    m.add(11 - w, 'days')

m.add(371, 'days')
while m.year() > year
    m.subtract(1, 'week')

console.log m.isoWeek()
