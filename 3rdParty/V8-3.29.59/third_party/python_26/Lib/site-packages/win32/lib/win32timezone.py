# -*- coding: UTF-8 -*-

"""
win32timezone:
	Module for handling datetime.tzinfo time zones using the windows
registry for time zone information.  The time zone names are dependent
on the registry entries defined by the operating system.

	Currently, this module only supports the Windows NT line of products
and not Windows 95/98/Me.

	This module may be tested using the doctest module.

	Written by Jason R. Coombs (jaraco@jaraco.com).
	Copyright © 2003-2007.
	All Rights Reserved.

	This module is licenced for use in Mark Hammond's pywin32
library under the same terms as the pywin32 library.

	To use this time zone module with the datetime module, simply pass
the TimeZoneInfo object to the datetime constructor.  For example,

>>> import win32timezone, datetime
>>> assert 'Mountain Standard Time' in win32timezone.GetTimeZoneNames()
>>> tzi = win32timezone.TimeZoneInfo( 'Mountain Standard Time' )
>>> now = datetime.datetime.now( tzi )

	The now object is now a time-zone aware object, and daylight savings-
aware methods may be called on it.

>>> now.utcoffset() in ( datetime.timedelta(-1, 61200), datetime.timedelta(-1, 64800) )
True

(note that the result of utcoffset call will be different based on when now was
generated, unless standard time is always used)

>>> now = datetime.datetime.now( TimeZoneInfo( 'Mountain Standard Time', True ) )
>>> now.utcoffset()
datetime.timedelta(-1, 61200)

>>> aug2 = datetime.datetime( 2003, 8, 2, tzinfo = tzi )
>>> tuple(aug2.utctimetuple())
(2003, 8, 2, 6, 0, 0, 5, 214, 0)
>>> nov2 = datetime.datetime( 2003, 11, 25, tzinfo = tzi )
>>> tuple(nov2.utctimetuple())
(2003, 11, 25, 7, 0, 0, 1, 329, 0)

To convert from one timezone to another, just use the astimezone method.

>>> aug2.isoformat()
'2003-08-02T00:00:00-06:00'
>>> aug2est = aug2.astimezone( win32timezone.TimeZoneInfo( 'Eastern Standard Time' ) )
>>> aug2est.isoformat()
'2003-08-02T02:00:00-04:00'

calling the displayName member will return the display name as set in the
registry.

>>> est = win32timezone.TimeZoneInfo( 'Eastern Standard Time' )
>>> est.displayName
u'(GMT-05:00) Eastern Time (US & Canada)'

>>> gmt = win32timezone.TimeZoneInfo( 'GMT Standard Time', True )
>>> gmt.displayName
u'(GMT) Greenwich Mean Time : Dublin, Edinburgh, Lisbon, London'

TimeZoneInfo now supports being pickled and comparison
>>> import pickle
>>> tz = win32timezone.TimeZoneInfo( 'China Standard Time' )
>>> tz == pickle.loads( pickle.dumps( tz ) )
True

>>> aest = win32timezone.TimeZoneInfo( 'AUS Eastern Standard Time' )
>>> est = win32timezone.TimeZoneInfo( 'E. Australia Standard Time' )
>>> dt = datetime.datetime( 2006, 11, 11, 1, 0, 0, tzinfo = aest )
>>> estdt = dt.astimezone( est )
>>> estdt.strftime( '%Y-%m-%d %H:%M:%S' )
'2006-11-11 00:00:00'

>>> dt = datetime.datetime( 2007, 1, 12, 1, 0, 0, tzinfo = aest )
>>> estdt = dt.astimezone( est )
>>> estdt.strftime( '%Y-%m-%d %H:%M:%S' )
'2007-01-12 00:00:00'

>>> dt = datetime.datetime( 2007, 6, 13, 1, 0, 0, tzinfo = aest )
>>> estdt = dt.astimezone( est )
>>> estdt.strftime( '%Y-%m-%d %H:%M:%S' )
'2007-06-13 01:00:00'

Microsoft now has a patch for handling time zones in 2007 (see
http://support.microsoft.com/gp/cp_dst)

As a result, the following test will fail in machines with the patch
except for Vista and its succssors, which have dynamic time
zone support.
#>>> nov2 = datetime.datetime( 2003, 11, 2, tzinfo = tzi )
#>>> nov2.utctimetuple()
(2003, 11, 2, 7, 0, 0, 6, 306, 0)

Note that is the correct response beginning in 2007
This test will fail in Windows versions prior to Vista
#>>> nov2 = datetime.datetime( 2007, 11, 2, tzinfo = tzi )
#>>> nov2.utctimetuple()
(2007, 11, 2, 6, 0, 0, 4, 306, 0)

There is a function you can call to get some capabilities of the time
zone data.
>>> caps = GetTZCapabilities()
>>> isinstance( caps, dict )
True
>>> caps.has_key( 'MissingTZPatch' )
True
>>> caps.has_key( 'DynamicTZSupport' )
True
"""
from __future__ import generators

__author__ = 'Jason R. Coombs <jaraco@jaraco.com>'
__version__ = '$Revision: 1.9 $'[11:-2]
__sccauthor__ = '$Author: mhammond $'[9:-2]
__date__ = '$Date: 2008/04/11 03:15:15 $'[10:-2]

import os, _winreg, struct, datetime, win32api, re, sys, operator

import logging
log = logging.getLogger( __file__ )

class WinTZI( object ):
	format = '3l8h8h'

	def __init__( self, key, name = None ):
		if( not name and len( key ) == struct.calcsize( self.format ) ):
			self.__init_from_bytes__( key )
		else:
			self.__init_from_reg_key__( key, name )
		
	def __init_from_reg_key__( self, key, name = None ):
		if not name:
			key, name = os.path.split( key )
		value, type = _winreg.QueryValueEx( key, name ) 
		self.__init_from_bytes__( value )
		
	def __init_from_bytes__( self, bytes ):
		components = struct.unpack( self.format, bytes )
		makeMinuteTimeDelta = lambda x: datetime.timedelta( minutes = x )
		self.bias, self.standardBiasOffset, self.daylightBiasOffset = \
				   map( makeMinuteTimeDelta, components[:3] )
		# daylightEnd and daylightStart are 8-tuples representing a Win32 SYSTEMTIME structure
		self.daylightEnd, self.daylightStart = components[3:11], components[11:19]

	def LocateStartDay( self, year ):
		return self._LocateDay( year, self.daylightStart )

	def LocateEndDay( self, year ):
		return self._LocateDay( year, self.daylightEnd )

	def _LocateDay( self, year, win32SystemTime ):
		"""
		Takes a SYSTEMTIME structure as retrieved from a TIME_ZONE_INFORMATION
		structure and interprets it based on the given year to identify the actual day.

		This method is necessary because the SYSTEMTIME structure refers to a day by its
		day of the week or week of the month (e.g. 4th saturday in April).

		Refer to the Windows Platform SDK for more information on the SYSTEMTIME
		and TIME_ZONE_INFORMATION structures.
		"""
		month = win32SystemTime[ 1 ]
		# MS stores Sunday as 0, Python datetime stores Monday as zero
		targetWeekday = ( win32SystemTime[ 2 ] + 6 ) % 7
		# win32SystemTime[3] is the week of the month, so the following
		#  is the first day of that week
		day = ( win32SystemTime[ 3 ] - 1 ) * 7 + 1
		hour, min, sec, msec = win32SystemTime[4:]
		result = datetime.datetime( year, month, day, hour, min, sec, msec )
		# now the result is the correct week, but not necessarily the correct day of the week
		daysToGo = targetWeekday - result.weekday()
		result += datetime.timedelta( daysToGo )
		# if we selected a day in the month following the target month,
		#  move back a week or two.
		# This is necessary because Microsoft defines the fifth week in a month
		#  to be the last week in a month and adding the time delta might have
		#  pushed the result into the next month.
		while result.month == month + 1:
			result -= datetime.timedelta( weeks = 1 )
		return result

	def __cmp__( self, other ):
		return cmp( self.__dict__, other.__dict__ )

class TimeZoneInfo( datetime.tzinfo ):
	"""
	Main class for handling win32 time zones.
	Usage:
		TimeZoneInfo( <Time Zone Standard Name>, [<Fix Standard Time>] )
	If <Fix Standard Time> evaluates to True, daylight savings time is calculated in the same
		way as standard time.
	"""

	# this key works for WinNT+, but not for the Win95 line.
	tzRegKey = r'SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones'
		
	def __init__( self, timeZoneName, fixedStandardTime=False ):
		self.timeZoneName = timeZoneName
		key = self._FindTimeZoneKey()
		self._LoadInfoFromKey( key )
		self.fixedStandardTime = fixedStandardTime

	def _FindTimeZoneKey( self ):
		"""Find the registry key for the time zone name (self.timeZoneName)."""
		# for multi-language compatability, match the time zone name in the
		# "Std" key of the time zone key.
		zoneNames = dict( GetIndexedTimeZoneNames( 'Std' ) )
		# Also match the time zone key name itself, to be compatible with
		# English-based hard-coded time zones.
		timeZoneName = zoneNames.get( self.timeZoneName, self.timeZoneName )
		tzRegKeyPath = os.path.join( self.tzRegKey, timeZoneName )
		try:
			key = _winreg.OpenKeyEx( _winreg.HKEY_LOCAL_MACHINE, tzRegKeyPath )
		except:
			raise ValueError, 'Timezone Name %s not found.' % timeZoneName
		return key

	def __getinitargs__( self ):
		return ( self.timeZoneName, )

	def _LoadInfoFromKey( self, key ):
		"""Loads the information from an opened time zone registry key
		into relevant fields of this TZI object"""
		self.displayName = _winreg.QueryValueEx( key, "Display" )[0]
		self.standardName = _winreg.QueryValueEx( key, "Std" )[0]
		self.daylightName = _winreg.QueryValueEx( key, "Dlt" )[0]
		self.staticInfo = WinTZI( key, "TZI" )
		self._LoadDynamicInfoFromKey( key )

	def _LoadDynamicInfoFromKey( self, key ):
		try:
			dkey = _winreg.OpenKeyEx( key, 'Dynamic DST' )
		except WindowsError:
			return
		info = _RegKeyDict( dkey )
		del info['FirstEntry']
		del info['LastEntry']
		years = map( int, info.keys() )
		values = map( WinTZI, info.values() )
		# create a range mapping that searches by descending year and matches
		# if the target year is greater or equal.
		self.dynamicInfo = RangeMap( zip( years, values ), descending, operator.ge )

	def __repr__( self ):
		result = '%s( %s' % ( self.__class__.__name__, repr( self.timeZoneName ) )
		if self.fixedStandardTime:
			result += ', True'
		result += ' )'
		return result

	def __str__( self ):
		return self.displayName

	def tzname( self, dt ):
		winInfo = self.getWinInfo( dt )
		if self.dst( dt ) == winInfo.daylightBiasOffset:
			result = self.daylightName
		elif self.dst( dt ) == winInfo.standardBiasOffset:
			result = self.standardName
		return result

	def getWinInfo( self, targetYear ):
		if not hasattr( self, 'dynamicInfo' ) or not self.dynamicInfo:
			return self.staticInfo
		# Find the greatest year entry in self.dynamicInfo which is for
		#  a year greater than or equal to our targetYear. If not found,
		#  default to the earliest year.
		return self.dynamicInfo.get( targetYear, self.dynamicInfo[ RangeItemLast() ] )
		
	def _getStandardBias( self, dt ):
		winInfo = self.getWinInfo( dt.year )
		return winInfo.bias + winInfo.standardBiasOffset

	def _getDaylightBias( self, dt ):
		winInfo = self.getWinInfo( dt.year )
		return winInfo.bias + winInfo.daylightBiasOffset

	def utcoffset( self, dt ):
		"Calculates the utcoffset according to the datetime.tzinfo spec"
		if dt is None: return
		winInfo = self.getWinInfo( dt.year )
		return -( winInfo.bias + self.dst( dt ) )

	def dst( self, dt ):
		"Calculates the daylight savings offset according to the datetime.tzinfo spec"
		if dt is None: return
		assert dt.tzinfo is self

		winInfo = self.getWinInfo( dt.year )
		if not self.fixedStandardTime and self._inDaylightSavings( dt ):
			result = winInfo.daylightBiasOffset
		else:
			result = winInfo.standardBiasOffset
		return result

	def _inDaylightSavings( self, dt ):
		try:
			dstStart = self.GetDSTStartTime( dt.year )
			dstEnd = self.GetDSTEndTime( dt.year )
			
			if dstStart < dstEnd:
				inDaylightSavings = dstStart <= dt.replace( tzinfo=None ) < dstEnd
			else:
				# in the southern hemisphere, daylight savings time
				#  typically ends before it begins in a given year.
				inDaylightSavings = not ( dstEnd < dt.replace( tzinfo=None ) <= dstStart )
		except ValueError:
			# there was an error parsing the time zone, which is normal when a
			#  start and end time are not specified.
			inDaylightSavings = False

		return inDaylightSavings

	def GetDSTStartTime( self, year ):
		"Given a year, determines the time when daylight savings time starts"
		return self.getWinInfo( year ).LocateStartDay( year )

	def GetDSTEndTime( self, year ):
		"Given a year, determines the time when daylight savings ends."
		return self.getWinInfo( year ).LocateEndDay( year )
	
	def __cmp__( self, other ):
		return cmp( self.__dict__, other.__dict__ )

def _RegKeyEnumerator( key ):
	return _RegEnumerator( key, _winreg.EnumKey )

def _RegValueEnumerator( key ):
	return _RegEnumerator( key, _winreg.EnumValue )

def _RegEnumerator( key, func ):
	"Enumerates an open registry key as an iterable generator"
	index = 0
	try:
		while 1:
			yield func( key, index )
			index += 1
	except WindowsError: pass
	
def _RegKeyDict( key ):
	values = _RegValueEnumerator( key )
	values = tuple( values )
	return dict( map( lambda (name,value,type): (name,value), values ) )

def GetTimeZoneNames( ):
	"Returns the names of the time zones as defined in the registry"
	key = _winreg.OpenKeyEx( _winreg.HKEY_LOCAL_MACHINE, TimeZoneInfo.tzRegKey )
	return _RegKeyEnumerator( key )

def GetIndexedTimeZoneNames( index_key = 'Index' ):
	"""Returns the names of the time zones as defined in the registry, but
	includes an index by which they may be sorted.  Default index is "Index"
	by which they may be sorted longitudinally."""
	for timeZoneName in GetTimeZoneNames():
		tzRegKeyPath = os.path.join( TimeZoneInfo.tzRegKey, timeZoneName )
		key = _winreg.OpenKeyEx( _winreg.HKEY_LOCAL_MACHINE, tzRegKeyPath )
		tzIndex, type = _winreg.QueryValueEx( key, index_key )
		yield ( tzIndex, timeZoneName )

def GetSortedTimeZoneNames( ):
	""" Uses GetIndexedTimeZoneNames to return the time zone names sorted
	longitudinally."""
	tzs = list( GetIndexedTimeZoneNames() )
	tzs.sort()
	return zip( *tzs )[1]

def GetLocalTimeZone( ):
	"""Returns the local time zone as defined by the operating system in the
	registry.
	Note that this will only work if the TimeZone in the registry has not been
	customized.  It should have been selected from the Windows interface.
	>>> localTZ = GetLocalTimeZone()
	>>> nowLoc = datetime.datetime.now( localTZ )
	>>> nowUTC = datetime.datetime.utcnow( )
	>>> ( nowUTC - nowLoc ) < datetime.timedelta( seconds = 5 )
	Traceback (most recent call last):
	  ...
	TypeError: can't subtract offset-naive and offset-aware datetimes

	>>> nowUTC = nowUTC.replace( tzinfo = TimeZoneInfo( 'GMT Standard Time', True ) )

	Now one can compare the results of the two offset aware values	
	>>> ( nowUTC - nowLoc ) < datetime.timedelta( seconds = 5 )
	True
	"""
	tzRegKey = r'SYSTEM\CurrentControlSet\Control\TimeZoneInformation'
	key = _winreg.OpenKeyEx( _winreg.HKEY_LOCAL_MACHINE, tzRegKey )
	local = _RegKeyDict( key )
	# if the user has not checked "Automatically adjust clock for daylight
	# saving changes" in the Date and Time Properties control, the standard
	# and daylight values will be the same.  If this is the case, create a
	# timezone object fixed to standard time.
	fixStandardTime = local['StandardName'] == local['DaylightName'] and \
					local['StandardBias'] == local['DaylightBias']
	keyName = [ 'StandardName', 'TimeZoneKeyName' ][ sys.getwindowsversion() >= (6,) ]
	standardName = local[ keyName ]
	standardName = __TimeZoneKeyNameWorkaround( standardName )
	return TimeZoneInfo( standardName, fixStandardTime )

def __TimeZoneKeyNameWorkaround( name ):
	"""It may be a bug in Vista, but in standard Windows Vista install
	(both 32-bit and 64-bit), it appears the TimeZoneKeyName returns a
	string with extraneous characters."""
	try:
		return name[:name.index('\x00')]
	except ValueError:
		#null character not found
		return name

def GetTZCapabilities():
	"""Run a few known tests to determine the capabilities of the time zone database
	on this machine.
	Note Dynamic Time Zone support is not available on any platform at this time; this
	is a limitation of this library, not the platform."""
	tzi = TimeZoneInfo( 'Mountain Standard Time' )
	MissingTZPatch = datetime.datetime( 2007,11,2,tzinfo=tzi ).utctimetuple() != (2007,11,2,6,0,0,4,306,0)
	DynamicTZSupport = not MissingTZPatch and datetime.datetime( 2003,11,2,tzinfo=tzi).utctimetuple() == (2003,11,2,7,0,0,6,306,0)
	del tzi
	return vars()
	

class DLLHandleCache( object ):
	def __init__( self ):
		self.__cache = {}

	def __getitem__( self, filename ):
		key = filename.lower()
		return self.__cache.setdefault( key, win32api.LoadLibrary( key ) )

DLLCache = DLLHandleCache()	

def resolveMUITimeZone( spec ):
	"""Resolve a multilingual user interface resource for the time zone name
	>>> result = resolveMUITimeZone( '@tzres.dll,-110' )
	>>> expectedResultType = [type(None),unicode][sys.getwindowsversion() >= (6,)]
	>>> type( result ) is expectedResultType
	True
	
	spec should be of the format @path,-stringID[;comment]
	see http://msdn2.microsoft.com/en-us/library/ms725481.aspx for details
	"""
	pattern = re.compile( '@(?P<dllname>.*),-(?P<index>\d+)(?:;(?P<comment>.*))?' )
	matcher = pattern.match( spec )
	assert matcher, 'Could not parse MUI spec'

	try:
		handle = DLLCache[ matcher.groupdict()[ 'dllname' ] ]
		result = win32api.LoadString( handle, int( matcher.groupdict()[ 'index' ] ) )
	except win32api.error, e:
		result = None
	return result

# the following code implements a RangeMap and its support classes

ascending = cmp
def descending( a, b ):
	return -ascending( a, b )

class RangeMap( dict ):
	"""A dictionary-like object that uses the keys as bounds for a range.
	Inclusion of the value for that range is determined by the
	keyMatchComparator, which defaults to greater-than-or-equal.
	A value is returned for a key if it is the first key that matches in
	the sorted list of keys.  By default, keys are sorted in ascending
	order, but can be sorted in any other order using the keySortComparator.

	Let's create a map that maps 1-3 -> 'a', 4-6 -> 'b'
	>>> r = RangeMap( { 3: 'a', 6: 'b' } )  # boy, that was easy
	>>> r[1], r[2], r[3], r[4], r[5], r[6]
	('a', 'a', 'a', 'b', 'b', 'b')

	But you'll notice that the way rangemap is defined, it must be open-ended on one side.
	>>> r[0]
	'a'
	>>> r[-1]
	'a'

	One can close the open-end of the RangeMap by using RangeValueUndefined
	>>> r = RangeMap( { 0: RangeValueUndefined(), 3: 'a', 6: 'b' } )
	>>> r[0]
	Traceback (most recent call last):
	  ...
	KeyError: 0

	One can get the first or last elements in the range by using RangeItem
	>>> last_item = RangeItem( -1 )
	>>> r[last_item]
	'b'

	>>> r[RangeItemLast()]
	'b'

	>>> r.bounds()
	(0, 6)
	
	"""
	def __init__( self, source, keySortComparator = ascending, keyMatchComparator = operator.le ):
		dict.__init__( self, source )
		self.sort = keySortComparator
		self.match = keyMatchComparator

	def __getitem__( self, item ):
		sortedKeys = self.keys()
		sortedKeys.sort( self.sort )
		if isinstance( item, RangeItem ):
			result = self.__getitem__( sortedKeys[ item ] )
		else:
			key = self._find_first_match_( sortedKeys, item )
			result = dict.__getitem__( self, key )
			if isinstance( result, RangeValueUndefined ): raise KeyError, key
		return result

	def _find_first_match_( self, keys, item ):
		is_match = lambda k: self.match( item, k )
		# use of ifilter here would be more efficent
		matches = filter( is_match, keys )
		if matches:
			return matches[0]
		raise KeyError( item )

	def bounds( self ):
		sortedKeys = self.keys()
		sortedKeys.sort( self.sort )
		return sortedKeys[ RangeItemFirst() ], sortedKeys[ RangeItemLast() ]

class RangeValueUndefined( object ): pass
class RangeItem( int ):
	def __new__( cls, value ):
		return int.__new__( cls, value )
class RangeItemFirst( RangeItem ):
	def __new__( cls ):
		return RangeItem.__new__( cls, 0 )
class RangeItemLast( RangeItem ):
	def __new__( cls ):
		return RangeItem.__new__( cls, -1 )
