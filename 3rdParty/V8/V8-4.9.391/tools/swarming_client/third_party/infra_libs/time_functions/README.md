# A README ON TIME

[TOC]

## ABOUT THIS DOCUMENT

This is a document intended to persuade you, in the course of your computer
doings, to use a particular representation of time called *stiptime*. Why a
particular representation of time, and why so staunch about it? The main reason
is that time is not handled well by libraries in any programming language, and
misuse leads to subtle bugs. Like memory allocators, representations of time are
not something application developers give much thought to. Unlike memory
allocators, you cannot expect your time libraries to "just work." This leads to
a wide class of bugs that are unintuitive, difficult to anticipate, and
difficult to test for. stiptime has been designed to avoid many of these
pitfalls. For a large number of time-related tasks, stiptime just works.

If reading this document causes your head to spin, it has achieved its goal. Use
stiptime and go on with your day.

## STIPTIME VS BIZARRO TIME

### stiptime

stiptime is a contemporary terrestrial time format meant to reduce the number of
time-related bugs in computer programs. It makes certain compromises to be
easier to implement on most operating systems and programming languages circa
2015-07-10T22:54:32.0Z.

- stiptime is for absolute times: stiptime is meant to represent the absolute
  (instead of relative) time an event happened. It is not intended for durations
  or for local representations of time (it will not tell you where the sun is in
  the sky).
- stiptime is terrestrial: it is not suitable for astronomical calculations or
  events happening on Mars. It does not account for [relativistic time
  dilation](https://en.wikipedia.org/wiki/Barycentric_Dynamical_Time) while
  traveling through the solar system. It is not suitable for comparing clocks
  across astronomically vast distances.
- stiptime is contemporary: it is not particularly useful for describing dates
  in antiquity, such as anything before the invention of Greenwich Mean Time.
- stiptime is based on UTC, and uses UTC's concept of leap seconds. Many OSes
  and date libraries [handle leap
  seconds](https://tools.ietf.org/html/rfc7164#page-3) in a way that make
  duration computations inaccurate across them. Unfortunately, continuous
  timescales like [TAI](https://en.wikipedia.org/wiki/International_Atomic_Time)
  are not easily available on modern OSes. In light of the difficulty of
  obtaining TAI, stiptime compromises by being based on UTC and urges caution
  when making duration computations.

#### definition

stiptime's format is UTC represented as follows:

	YYYY-MM-DDThh:mm:ssZ

where

	YYYY: four digit year
	MM: zero padded month
	DD: zero padded day
	hh: zero padded 24hr hour
	mm: zero padded minute
	ss: zero padded second, with a required fractional part

You may notice that this is exactly the [ISO 8601 date
format](http://www.w3.org/TR/NOTE-datetime) with Z used to represent UTC. That's
because it is! Z is the [nautical
timezone](https://en.wikipedia.org/wiki/Nautical_time) for UTC. Since 'Zulu' is
the [NATO phonetic](https://en.wikipedia.org/wiki/NATO_phonetic_alphabet)
representation of Z, UTC (and by extension stiptime) can also be referred to as
Zulu time. Z is used instead of +00:00 as it unambiguously signals UTC and not
"put in any arbitrary timezone here." It is also short and compact.

#### examples of stiptime

- 2015-06-30T18:50:50.0Z    *typical representation*
- 2015-06-30T18:50:50.123Z    *fractional seconds*
- 2015-06-30T23:59:60.0Z    *leap second*
- 2015-06-30T23:59:60.123Z    *fractional leap second*

#### lesser stiptime

Lesser stiptime is to be used only when necessary. Lesser stiptime is Unix time
or POSIX time, "fractional seconds since the epoch, defined as
1970-01-01T00:00:00Z. Seconds are corrected such that days are exactly 86400
seconds long." The reason stiptime is preferred over lesser stiptime is due to
that last correction. Since UTC occasionally contains days longer than 86400
seconds, lesser stiptime cannot encode positive leap seconds unambiguously. The
consequences of this will be described in a later section. stiptime is preferred
over lesser stiptime, but lesser stiptime is definitely preferred over bizarro
time.

### bizarro time

Bizarro time is any time format that is used for absolute time that isn't
stiptime. Some of these may seem obvious and good, but a later section will show
their pitfalls.

#### examples of bizarro time

- Tomorrow, 3pm
- Tuesday 14, July 2015 4:38PM PST
- 2015-06-30T06:50:50.0PMZ    *not 24hr time*
- 2015/06/30 18:50:50.0Z    *incorrect separators*
- 2015-06-30T18:50:50.0    *no Z at the end, unknown timezone*
- 2015-06-30T18:50:50.0 PST    *PST instead of Z*
- 2015-06-30T18:50:50.0
  [America/Los_Angeles](https://en.wikipedia.org/wiki/America/Los_Angeles)
- 2015-06-30T18:50:50.0+00:00    *using +00:00 instead of Z for UTC*
- 2015-06-30T18:50:50.0-07:00    *not using UTC*
- 2015-06-30T18:50:50Z    *no fractional seconds*

## STUCK IN TIME JAIL (THE PITFALLS OF BIZARRO TIME)

Using bizarro time will eventually lead to "fun" and subtle bugs in your
programs. The inevitability of dealing with all of the contingencies of bizarro
time (and the libraries that handle it) leads to a profound frustration and
ennui — this is when you are stuck in *time jail*. The entrances to time jail
are many, but can be broadly classified into implementation-induced time jail
and timezone-induced time jail.

### implementation-induced time jail

Writing proper time-handling libraries is hard, which means that most time
libraries have quirks, unexpected behavior or outright bugs. Some of these even
occur at the operating system level. This section mostly describes the
[datetime](https://docs.python.org/2/library/datetime.html) module included in
the python standard library, but other quirks are documented as well.

#### python 2.7 datetime

- python 2.7 datetime lets you print a date that itself cannot parse


	>>> import datetime
	>>> from dateutil import tz
	>>> offset = tz.tzoffset(None, -7*60*60)
	>>> dt = datetime.datetime(2015, 1, 1, 0, 0, 0, tzinfo=offset)
	>>> a = dt.strftime('%Y-%m-%dT%H:%M:%S %z')
	>>> a
	'2015-01-01T00:00:00 -0700'
	>>> datetime.datetime.strptime(a, '%Y-%m-%dT%H:%M:%S %z')
	ValueError: 'z' is a bad directive in format '%Y-%m-%dT%H:%M:%S %z'


- it lets you print a date that it can parse most of the time, except that one
  time when you have zero microseconds and then it can't

  See https://bugs.python.org/issue19475 for the problem, and see
  [zulu.py](https://chromium.googlesource.com/infra/infra/+/master/infra_libs/time_functions/zulu.py)
  for the 'solution.'
    

	>>> import datetime
	>>> a = datetime.datetime(2015, 1, 1, 0, 0, 0, 0).isoformat()
	>>> b = datetime.datetime(2015, 1, 1, 0, 0, 0, 123).isoformat()
	>>> a
	'2015-01-01T00:00:00'
	>>> b
	'2015-01-01T00:00:00.000123'
	>>> datetime.datetime.strptime(a, '%Y-%m-%dT%H:%M:%S')
	datetime.datetime(2015, 1, 1, 0, 0)
	>>> datetime.datetime.strptime(b, '%Y-%m-%dT%H:%M:%S')
	ValueError: unconverted data remains: .000123
	# okay, let's try with .%f at the end
	>>> datetime.datetime.strptime(b, '%Y-%m-%dT%H:%M:%S.%f')
	datetime.datetime(2015, 1, 1, 0, 0, 0, 123)
	>>> datetime.datetime.strptime(a, '%Y-%m-%dT%H:%M:%S.%f')
	ValueError: time data '2015-01-01T00:00:00' does not match format '%Y-%m-%dT%H:%M:%S.%f'

- it can't understand leap seconds


	>>> import datetime
	>>> datetime.datetime(2015, 6, 30, 23, 59, 60)
	ValueError: second must be in 0..59

- it can't tell you what timezone datetime.now() is


	>>> import datetime
	>>> datetime.datetime.now().tzinfo is None
	true
	>>> datetime.datetime.now().utcoffset() is None
	true
	>>> datetime.datetime.utcnow().tzinfo is None
	true
	>>> datetime.datetime.utcnow().utcoffset() is None
	true
	>>> datetime.datetime.now().isoformat()
	'2015-07-15T15:36:23.591431'
	>>> datetime.datetime.utcnow().isoformat()
	'2015-07-15T22:36:28.431225'

- it can't mix timezone aware datetimes with naive datetimes (why have naive
  datetimes to begin with?)


	>>> import datetime
	>>> from dateutil import tz
	>>> a = datetime.datetime(2015, 1, 1, 0, 0, 0, tzinfo=tz.tzoffset(None, -7*60*60))
	>>> b = datetime.datetime(2015, 1, 1, 0, 0, 0)
	>>> a == b
	TypeError: can't compare offset-naive and offset-aware datetimes

#### that's okay, I'll just use dateutil

- [dateutil](http://labix.org/python-dateutil) is not part of the standard
  library

  You'll have to venv or wheel it wherever you go. A (non-leap second aware)
  stiptime parser is a single line of python and requires nothing more than the
  standard library. A stiptime formatter is 4 lines, and also requires nothing
  more than the standard library.

- dateutil can't understand leap seconds


	>>> import dateutil.parser
	>>> dateutil.parser.parse('2015-06-30T23:59:60')
	ValueError: second must be in 0..59

- parser.parse works great, except when it silently doesn't

  (note, running this example will yield different results depending on your
  local timezone. lol.)


	>>> import dateutil.parser
	>>> a = dateutil.parser.parse('2015-01-01T00:00:00 PST')
	>>> b = dateutil.parser.parse('2015-01-01T00:00:00 PDT')
	>>> c = dateutil.parser.parse('2015-01-01T00:00:00 EST')
	>>> a.isoformat()
	'2015-01-01T00:00:00-08:00'		# great!
	>>> b.isoformat()
	'2015-01-01T00:00:00-08:00'		# uh...
	>>> c.isoformat()
	'2015-01-01T00:00:00'					# uhhhhhhh
	>>> c == a
	TypeError: can't compare offset-naive and offset-aware datetimes

#### python 2.7 time

- time.time()'s definition is incorrect

  According to [the python docs](https://docs.python.org/2/library/time.html),
  time.time() "[returns] the time in seconds since the epoch as a floating point
  number." Except it doesn't, as (at least on unix) days are corrected to be
  86400 seconds long. Thus the true definition of time.time() should be "the
  time in seconds since the epoch minus any UTC leap seconds added since the
  epoch."

### timezone-induced time jail

These are a collection of gotchas that occur even if your timezone-handling
libraries are perfect. They arise purely out of not using UTC for internal
computations.

- dates which represent the same moment in time can have different weekdays or
  other attributes


	>>> import dateutil.parser
	>>> a = dateutil.parser.parse('2015-06-17T23:00:00 PDT')
	>>> b = dateutil.parser.parse('2015-06-18T06:00:00 UTC')
	>>> a == b
	True
	>>> a.weekday()
	2
	>>> b.weekday()
	3

- it's easy to write code thinking it's in one timezone when it's really in
  another

  - pop quiz: what timezone does the AppEngine datastore store times in?
  - pop quiz: what months does daylight savings take effect in Australia? North
    America?
  - pop quiz: what timezone does buildbot write twistd.log in? What timezone
    does it write http.log in?


- you can have timezone un-aware code in a timezone that shifts (PST -> PDT).

  Now you have graphs wrapping back on themselves, systems restarting repeatedly
  for an hour, or silent data corruption. This is the classic timezone bug.

- ambiguous encoding

  If you're not careful, you can encode dates which refer to two instances in
  time. The date '2015-11-01T01:30:00 Pacific' or '2015-11-01T01:30:00
  America/Los_Angeles' refers to *two* distinct times:
  '2015-11-01T01:30:00-0800' and '2015-11-01T01:30:00-0700'. Imagine an alarm
  clock or cron job which triggers on that.

- illegal dates that aren't obviously illegal

  The opposite of ambiguous encoding: did you know that '2015-03-08T02:30:00
  Pacific' doesn't exist? It jumped from 2015-03-08T01:59:59 immediately to
  2015-03-08T03:00:00.

- what does a configuration file look like where any timezone is allowed?

  You've now required everyone to convert every timezone into every timezone,
  instead of every timezone into one (UTC):


	['2015-06-18T05:00:00+00:00'
	 '2015-06-17T23:00:00-07:00',
	 '2015-06-18T06:00:00-10:00',
	]

### leap second time jail

Finally, there is a small jail associated with errors occurring due to leap
seconds themselves. Unfortunately, stiptime is *not* immune to these.

- ambiguous unix time


	2015-06-30T23:59:59.0Z -> 1435708799.0
	2015-06-30T23:59:60.0Z -> 1435708799.0

- illegal unix time

  This hasn't happened yet, but if a negative leap second ever occurs, there
  will be a floating point time which 'never happened.'

- calculating durations using start/stop times

  Of course, calculating durations across leap seconds can cause slight errors,
  mistriggers or even retriggers. Since they happen infrequently (and TAI is not
  commonly available), stiptime has chosen to be susceptible.

## CONCLUSION

It is my hope that after reading this document, you will consider stiptime and,
occasionally, lesser stiptime to be the proper way to represent time in stored
formats. I firmly believe time should be displayed to humans in their local
format — but only in ephemeral displays. A local absolute time should never
touch a disk. Join me in using stiptime and get back some sanity in your life.
