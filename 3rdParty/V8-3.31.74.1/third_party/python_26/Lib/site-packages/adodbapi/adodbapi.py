"""adodbapi v2.1 -  A python DB API 2.0 interface to Microsoft ADO
    
    Copyright (C) 2002  Henrik Ekelund
    Email: <http://sourceforge.net/sendmessage.php?touser=618411>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    version 2.1 by Vernon Cole -- update for Decimal data type
    (requires Python 2.4 or above or or Python 2.3 with "import win32com.decimal_23")
    all uses of "verbose" below added by Cole for v2.1
"""

import string
import exceptions
import time
import calendar
import types
import sys
import traceback
import datetime

try:
    import decimal
except ImportError:  #perhaps running Cpython 2.3 
    import win32com.decimal_23 as decimal

try:
    import win32com.client
    def Dispatch(dispatch):
        return win32com.client.Dispatch(dispatch)
    win32 = True
except ImportError:  #perhaps running on IronPython (someday)
    from System import Activator, Type
    def Dispatch(dispatch):
        type = Type.GetTypeFromProgID(dispatch)
        return Activator.CreateInstance(type)
    win32 = False    #implies IronPython
    
if win32:
    import pythoncom
    pythoncom.__future_currency__ = True

def standardErrorHandler(connection,cursor,errorclass,errorvalue):
    err=(errorclass,errorvalue)
    connection.messages.append(err)
    if cursor!=None:
        cursor.messages.append(err)
    raise errorclass(errorvalue)
    
   
class TimeConverter:
    def __init__(self):
        #Use self.types to compare if an input parameter is a datetime 
        self.types = (type(self.Date(2000,1,1)),
                      type(self.Time(12,01,1)),
                      type(self.Timestamp(2000,1,1,12,1,1)))
    def COMDate(self,obj):
        'Returns a ComDate from a datetime in inputformat'
        raise "Abstract class"
    
    def DateObjectFromCOMDate(self,comDate):
        'Returns an object of the wanted type from a ComDate'
        raise "Abstract class"
    
    def Date(self,year,month,day):
        "This function constructs an object holding a date value. "
        raise "Abstract class"

    def Time(self,hour,minute,second):
        "This function constructs an object holding a time value. "
        raise "Abstract class"

    def Timestamp(self,year,month,day,hour,minute,second):
        "This function constructs an object holding a time stamp value. "
        raise "Abstract class"

    def DateObjectToIsoFormatString(self,obj):
        "This function should return a string in the format 'YYYY-MM-dd HH:MM:SS:ms' (ms optional) "
        raise "Abstract class"        
    

class mxDateTimeConverter(TimeConverter):
    def COMDate(self,obj):
        return obj.COMDate()

    def DateObjectFromCOMDate(self,comDate):
        return mx.DateTime.DateTimeFromCOMDate(comDate)
    
    def Date(self,year,month,day):
        return mx.DateTime.Date(year,month,day)

    def Time(self,hour,minute,second):
        return mx.DateTime.Time(hour,minute,second)

    def Timestamp(self,year,month,day,hour,minute,second):
        return mx.DateTime.Timestamp(year,month,day,hour,minute,second)
    
    def DateObjectToIsoFormatString(self,obj):
        return obj.Format('%Y-%m-%d %H:%M:%S')

class pythonDateTimeConverter(TimeConverter): # datetime module is available in Python 2.3
    def __init__(self):       
        self._ordinal_1899_12_31=datetime.date(1899,12,31).toordinal()-1
        TimeConverter.__init__(self)
        
    def COMDate(self,obj):
        tt=obj.timetuple()
        try:
            ms=obj.microsecond
        except:
            ms=0
        YMDHMSmsTuple=tuple(tt[:6] + (ms,))
        return self.COMDateFromTuple(YMDHMSmsTuple)

    def DateObjectFromCOMDate(self,comDate):
        #ComDate is number of days since 1899-12-31
        fComDate=float(comDate)
        millisecondsperday=86400000 # 24*60*60*1000
        integerPart = int(fComDate)
        floatpart=fComDate-integerPart
        if floatpart == 0.0:
            return datetime.date.fromordinal(integerPart + self._ordinal_1899_12_31)
        dte=datetime.datetime.fromordinal(integerPart + self._ordinal_1899_12_31)
        dte=dte+datetime.timedelta(milliseconds=floatpart*millisecondsperday)
        return dte

    
    def Date(self,year,month,day):
        return datetime.date(year,month,day)

    def Time(self,hour,minute,second):
        return datetime.time(hour,minute,second)

    def Timestamp(self,year,month,day,hour,minute,second):
        return datetime.datetime(year,month,day,hour,minute,second)

    def COMDateFromTuple(self,YMDHMSmsTuple):
        t=pythoncom.MakeTime(YMDHMSmsTuple)
        return float(t)

    def DateObjectToIsoFormatString(self,obj):
        if isinstance(obj,datetime.datetime):
            s = obj.strftime('%Y-%m-%d %H:%M:%S')
        else:
            try:  #usually datetime.datetime
                s = obj.isoformat()
            except:  #but may be mxdatetime
                s = obj.Format('%Y-%m-%d %H:%M:%S')
        return s

class pythonTimeConverter(TimeConverter):
    def __init__(self):
        year,month,day,hour,minute,second,weekday,julianday,daylightsaving=time.gmtime(0)
        self.com1970=self.COMDateFromTuple((year,month,day,hour,minute,second))
        daysInMonth=[31,28,31,30,31,30,31,30,31,30,31,30]
        cumulative=0
        self.monthCumDays={}
        for i in range(12):       
            self.monthCumDays[i+1]=cumulative
            cumulative+=daysInMonth[i]
        TimeConverter.__init__(self)

    def COMDate(self,timeobj):
        return float(pythoncom.MakeTime(time.mktime(timeobj)))
        
    def COMDateFromTuple(self,YMDHMSmsTuple):
        t=pythoncom.MakeTime(YMDHMSmsTuple)
        return float(t)
        
    def DateObjectFromCOMDate(self,comDate):
        'Returns ticks since 1970'
        secondsperday=86400 # 24*60*60
        #ComDate is number of days since 1899-12-31
        t=time.gmtime(secondsperday*(float(comDate)-self.com1970 ))
        return t  #year,month,day,hour,minute,second,weekday,julianday,daylightsaving=t

    def Date(self,year,month,day):
        return self.Timestamp(year,month,day,0,0,0)

    def Time(self,hour,minute,second):
        return time.gmtime((hour*60+minute)*60 + second)

    def Timestamp(self,year,month,day,hour,minute,second):
        julian=self.monthCumDays[month] + day
        if month>2:
            julian+=calendar.isleap(year)
        weekday=calendar.weekday(year,month,day)
        daylightsaving=-1
        return time.localtime(time.mktime((year,month,day,hour,minute,second,weekday,julian,daylightsaving)))
    
    def DateObjectToIsoFormatString(self,obj):
        try:
            s = time.strftime('%Y-%m-%d %H:%M:%S',obj)
        except:
            s = obj.strftime('%Y-%m-%d')
        return s

class Error(exceptions.StandardError):
    pass

class Warning(exceptions.StandardError):
    pass

class InterfaceError(Error):
    pass

class DatabaseError(Error):
    pass

class InternalError(DatabaseError):
    pass

class OperationalError(DatabaseError):
    pass

class ProgrammingError(DatabaseError):
    pass

class IntegrityError(DatabaseError):
    pass

class DataError(DatabaseError):
    pass

class NotSupportedError(DatabaseError):
    pass

verbose = False

version = __doc__.split('-',2)[0] #v2.1 Cole

def connect(connstr, timeout=30):               #v2.1 Simons
    "Connection string as in the ADO documentation, SQL timeout in seconds"
    global verbose
    try:
        conn=Dispatch('ADODB.Connection')
        if win32:
            pythoncom.CoInitialize()                 #v2.1 Paj
    except:
        raise InterfaceError #Probably COM Error
    try:
        conn.CommandTimeout=timeout             #v2.1 Simons
        conn.ConnectionString=connstr
    except:
        raise Error
    if verbose:
        print '%s attempting: "%s"' % (version,connstr)
    try:
        conn.Open()
    except (Exception), e:
        raise DatabaseError(e)
    return Connection(conn)
        
apilevel='2.0' #String constant stating the supported DB API level.

threadsafety=1 
# Integer constant stating the level of thread safety the interface supports,
# 1 = Threads may share the module, but not connections. 
# TODO: Have not tried this, maybe it is better than 1?
## 
## Possible values are:
##0 = Threads may not share the module. 
##1 = Threads may share the module, but not connections. 
##2 = Threads may share the module and connections. 
##3 = Threads may share the module, connections and cursors. 

paramstyle='qmark'
#String constant stating the type of parameter marker formatting expected by the interface. 
# 'qmark' = Question mark style, e.g. '...WHERE name=?'


adXactBrowse                  =0x100      # from enum IsolationLevelEnum
adXactChaos                   =0x10       # from enum IsolationLevelEnum
adXactCursorStability         =0x1000     # from enum IsolationLevelEnum
adXactIsolated                =0x100000   # from enum IsolationLevelEnum
adXactReadCommitted           =0x1000     # from enum IsolationLevelEnum
adXactReadUncommitted         =0x100      # from enum IsolationLevelEnum
adXactRepeatableRead          =0x10000    # from enum IsolationLevelEnum
adXactSerializable            =0x100000   # from enum IsolationLevelEnum

defaultIsolationLevel=adXactReadCommitted
#  Set defaultIsolationLevel on module level before creating the connection.
#   It may be one of the above
#   if you simply "import adodbapi", you'll say:
#   "adodbapi.adodbapi.defaultIsolationLevel=adodbapi.adodbapi.adXactBrowse", for example


adUseClient = 3     # from enum CursorLocationEnum      #v2.1 Rose
adUseServer = 2     # from enum CursorLocationEnum      #v2.1 Rose

defaultCursorLocation=adUseServer                       #v2.1 Rose
#  Set defaultCursorLocation on module level before creating the connection.
#   It may be one of the above

class Connection:
    def __init__(self,adoConn):       
        self.adoConn=adoConn
        self.supportsTransactions=False
        for indx in range(adoConn.Properties.Count):
            if adoConn.Properties(indx).Name == 'Transaction DDL' \
            and adoConn.Properties(indx).Value != 0:        #v2.1 Albrecht
                self.supportsTransactions=True
        self.adoConn.CursorLocation = defaultCursorLocation #v2.1 Rose
        if self.supportsTransactions:
            self.adoConn.IsolationLevel=defaultIsolationLevel
            self.adoConn.BeginTrans() #Disables autocommit
        self.errorhandler=None
        self.messages=[]
        global verbose
        if verbose:
            print 'adodbapi New connection at %X' % id(self)

    def _raiseConnectionError(self,errorclass,errorvalue):
        eh=self.errorhandler
        if eh==None:
            eh=standardErrorHandler
        eh(self,None,errorclass,errorvalue)

    def _closeAdoConnection(self):                  #all v2.1 Rose
        """close the underlying ADO Connection object,
           rolling it back first if it supports transactions."""
        if self.supportsTransactions:
            self.adoConn.RollbackTrans()
        self.adoConn.Close()
        if verbose:
            print 'adodbapi Closed connection at %X' % id(self)

    def close(self):
        """Close the connection now (rather than whenever __del__ is called).
        
        The connection will be unusable from this point forward;
        an Error (or subclass) exception will be raised if any operation is attempted with the connection.
        The same applies to all cursor objects trying to use the connection. 
        """
        self.messages=[]
        
        try:
            self._closeAdoConnection()                      #v2.1 Rose
        except (Exception), e:
            self._raiseConnectionError(InternalError,e)
        if win32:
            pythoncom.CoUninitialize()                             #v2.1 Paj

    def commit(self):
        """Commit any pending transaction to the database.

        Note that if the database supports an auto-commit feature,
        this must be initially off. An interface method may be provided to turn it back on. 
        Database modules that do not support transactions should implement this method with void functionality. 
        """
        self.messages=[]        
        try:
            if self.supportsTransactions:
                self.adoConn.CommitTrans()
                if not(self.adoConn.Attributes & adXactCommitRetaining):
                    #If attributes has adXactCommitRetaining it performs retaining commits that is,
                    #calling CommitTrans automatically starts a new transaction. Not all providers support this.
                    #If not, we will have to start a new transaction by this command:                
                    self.adoConn.BeginTrans()
        except (Exception), e:
            self._raiseConnectionError(Error,e)        

    def rollback(self):
        """In case a database does provide transactions this method causes the the database to roll back to
        the start of any pending transaction. Closing a connection without committing the changes first will
        cause an implicit rollback to be performed.

        If the database does not support the functionality required by the method, the interface should
        throw an exception in case the method is used. 
        The preferred approach is to not implement the method and thus have Python generate
        an AttributeError in case the method is requested. This allows the programmer to check for database
        capabilities using the standard hasattr() function. 

        For some dynamically configured interfaces it may not be appropriate to require dynamically making
        the method available. These interfaces should then raise a NotSupportedError to indicate the
        non-ability to perform the roll back when the method is invoked. 
        """
        self.messages=[]        
        if self.supportsTransactions:
            self.adoConn.RollbackTrans()
            if not(self.adoConn.Attributes & adXactAbortRetaining):
                #If attributes has adXactAbortRetaining it performs retaining aborts that is,
                #calling RollbackTrans automatically starts a new transaction. Not all providers support this.
                #If not, we will have to start a new transaction by this command:
                self.adoConn.BeginTrans()
        else:
            self._raiseConnectionError(NotSupportedError,None)
        
        #TODO: Could implement the prefered method by havins two classes,
        # one with trans and one without, and make the connect function choose which one.
        # the one without transactions should not implement rollback

    def cursor(self):
        "Return a new Cursor Object using the connection."
        self.messages=[]        
        return Cursor(self)
    
    def printADOerrors(self):
        j=self.adoConn.Errors.Count
        print 'ADO Errors:(%i)' % j
        for i in range(j):
            e=self.adoConn.Errors(i)
            print 'Description: %s' % e.Description
            print 'Number: %s ' % e.Number
            try:
                print 'Number constant:' % adoErrors[e.Number]
            except:
                pass
            print 'Source: %s' %e.Source
            print 'NativeError: %s' %e.NativeError
            print 'SQL State: %s' %e.SQLState
            if e.Number == -2147217871:
                print ' Error means that the Query timed out.\n Try using adodbpi.connect(constr,timeout=Nseconds)'

    def __del__(self):
        try:
            self._closeAdoConnection()                  #v2.1 Rose
        except:
            pass
        self.adoConn=None

    
class Cursor:
    description=None
##    This read-only attribute is a sequence of 7-item sequences.
##    Each of these sequences contains information describing one result column:
##        (name, type_code, display_size, internal_size, precision, scale, null_ok).
##    This attribute will be None for operations that do not return rows or if the
##    cursor has not had an operation invoked via the executeXXX() method yet. 
##    The type_code can be interpreted by comparing it to the Type Objects specified in the section below. 

    rowcount = -1
##    This read-only attribute specifies the number of rows that the last executeXXX() produced
##    (for DQL statements like select) or affected (for DML statements like update or insert). 
##    The attribute is -1 in case no executeXXX() has been performed on the cursor or
##    the rowcount of the last operation is not determinable by the interface.[7] 
##    N.O.T.E. -- adodbapi returns "-1" by default for all select statements
    
    arraysize=1
##    This read/write attribute specifies the number of rows to fetch at a time with fetchmany().
##    It defaults to 1 meaning to fetch a single row at a time. 
##    Implementations must observe this value with respect to the fetchmany() method,
##    but are free to interact with the database a single row at a time.
##    It may also be used in the implementation of executemany(). 

    def __init__(self,connection):
        self.messages=[]        
        self.conn=connection
        self.rs=None
        self.description=None
        self.errorhandler=connection.errorhandler
        if verbose:
            print 'adodbapi New cursor at %X on conn %X' % (id(self),id(self.conn))

    def __iter__(self):                   # [2.1 Zamarev]
        return iter(self.fetchone, None)  # [2.1 Zamarev]
       
    def _raiseCursorError(self,errorclass,errorvalue):
        eh=self.errorhandler
        if eh==None:
            eh=standardErrorHandler
        eh(self.conn,self,errorclass,errorvalue)

    def callproc(self, procname, parameters=None):
        """Call a stored database procedure with the given name.

            The sequence of parameters must contain one entry for each argument that the procedure expects.
            The result of the call is returned as modified copy of the input sequence.
            Input parameters are left untouched, output and input/output parameters replaced
            with possibly new values. 

            The procedure may also provide a result set as output, which is
            then available through the standard fetchXXX() methods.
        """
        self.messages=[]                
        return self._executeHelper(procname,True,parameters)

    def _returnADOCommandParameters(self,adoCommand):
        retLst=[]
        for i in range(adoCommand.Parameters.Count):                
            p=adoCommand.Parameters(i)
            if verbose > 2:
                   'return', p.Name, p.Type, p.Direction, repr(p.Value)
            type_code=p.Type 
            pyObject=convertVariantToPython(p.Value,type_code)
            if p.Direction == adParamReturnValue:
                self.returnValue=pyObject
            else:
                retLst.append(pyObject)
        return retLst

    def _makeDescriptionFromRS(self,rs):        
        if (rs == None) or (rs.State == adStateClosed): 
            self.rs=None
            self.description=None
        else:
            #self.rowcount=rs.RecordCount
            # Since the current implementation has a forward-only cursor, RecordCount will always return -1
            # The ADO documentation hints that obtaining the recordcount may be timeconsuming
            #   "If the Recordset object does not support approximate positioning, this property
            #    may be a significant drain on resources # [ekelund]
            # Therefore, COM will not return rowcount in most cases. [Cole]
            # Switching to client-side cursors will force a static cursor,
            # and rowcount will then be set accurately [Cole]
            self.rowcount = -1
            self.rs=rs
            nOfFields=rs.Fields.Count
            self.description=[]
            for i in range(nOfFields):
                f=rs.Fields(i)
                name=f.Name
                type_code=f.Type 
                if not(rs.EOF or rs.BOF):
                    display_size=f.ActualSize #TODO: Is this the correct defintion according to the DB API 2 Spec ?
                else:
                    display_size=None
                internal_size=f.DefinedSize
                precision=f.Precision
                scale=f.NumericScale
                null_ok= bool(f.Attributes & adFldMayBeNull)          #v2.1 Cole 
                self.description.append((name, type_code, display_size, internal_size, precision, scale, null_ok))

    def close(self):        
        """Close the cursor now (rather than whenever __del__ is called).
            The cursor will be unusable from this point forward; an Error (or subclass)
            exception will be raised if any operation is attempted with the cursor.
        """
        self.messages=[]                
        self.conn = None    #this will make all future method calls on me throw an exception
        if self.rs and self.rs.State != adStateClosed: # rs exists and is open      #v2.1 Rose
            self.rs.Close()                                                         #v2.1 Rose
            self.rs = None  #let go of the recordset os ADO will let it be disposed #v2.1 Rose

# ------------------------
# a note about Strategies:
# Ekelund struggled to find ways to keep ADO happy with passed parameters.
# He finally decided to try four "strategies" of parameter passing:
# 1. Let COM define the parameter list & pass date-times in COMdate format
# 2. Build a parameter list with python & pass date-times in COMdate format
# 3. Let COM define the parameter list & pass date-times as ISO date strings.
# 4. Build a parameter list with python & pass date-times as ISO date strings.
# For each "execute" call, he would try each way, then pass all four sets of error
#  messages back if all four attempts failed.
# My optimization is as follows:
#  1. Try to use the COM parameter list.
#  2. If the Refresh() call blows up, then build a perameter list
#  3. If the python data is a date but the ADO is a string,
#      or we built the parameter list,
#     then pass date-times as ISO dates,
#     otherwise, convert to a COM date.
# -- Vernon Cole v2.1
# (code in executeHelper freely reformatted by me)
#---------------------
    def _executeHelper(self,operation,isStoredProcedureCall,parameters=None):        
        if self.conn == None:
            self._raiseCursorError(Error,None)
            return
        returnValueIndex=None

        if verbose > 1 and parameters:
            print 'adodbapi parameters=',repr(parameters)

        parmIndx=-1
        try:                
            self.cmd=Dispatch("ADODB.Command")
            self.cmd.ActiveConnection=self.conn.adoConn
            self.cmd.CommandTimeout = self.conn.adoConn.CommandTimeout  #v2.1 Simons
            self.cmd.CommandText=operation
            if isStoredProcedureCall:
                self.cmd.CommandType=adCmdStoredProc
            else:
                self.cmd.CommandType=adCmdText

            returnValueIndex=-1            # Initialize to impossible value
            if parameters != None:
                try: # attempt to use ADO's parameter list
                    self.cmd.Parameters.Refresh()
                    defaultParameterList = False
                except: # if it blows up
                    #-- build own parameter list
                    if verbose:
                        print 'error in COM Refresh(). adodbapi building default parameter list'
                    defaultParameterList = True # we don't know whether db is expecting dates
                    for i,elem in enumerate(parameters):
                        name='p%i' % i
                        adotype = pyTypeToADOType(elem)
                        p=self.cmd.CreateParameter(name,adotype) # Name, Type, Direction, Size, Value
                        if isStoredProcedureCall:
                            p.Direction=adParamUnknown
                        self.cmd.Parameters.Append(p)  
                if verbose > 2:
                       for i in range(self.cmd.Parameters.Count):
                            P = self.cmd.Parameters(i)
                            print 'adodbapi parameter attributes before=', P.Name, P.Type, P.Direction, P.Size
                if isStoredProcedureCall:
                    cnt=self.cmd.Parameters.Count
                    if cnt<>len(parameters):
                        for i in range(cnt):
                            if self.cmd.Parameters(i).Direction == adParamReturnValue:
                                returnValueIndex=i
                                break
                for elem in parameters:
                    parmIndx+=1
                    if parmIndx == returnValueIndex:
                        parmIndx+=1
                    p=self.cmd.Parameters(parmIndx)
                    if p.Direction in [adParamInput,adParamInputOutput,adParamUnknown]:
                        tp = type(elem)
                        if tp in dateconverter.types:
                            if not defaultParameterList and p.Type in adoDateTimeTypes:
                                p.Value=dateconverter.COMDate(elem)
                            else: #probably a string
                    #Known problem with JET Provider. Date can not be specified as a COM date.
                    # See for example..http://support.microsoft.com/default.aspx?scid=kb%3ben-us%3b284843
                    # One workaround is to provide the date as a string in the format 'YYYY-MM-dd'
                                s = dateconverter.DateObjectToIsoFormatString(elem)
                                p.Value = s
                                p.Size = len(s)
                        elif tp in types.StringTypes:                  #v2.1 Jevon
                            if defaultParameterList:
                                p.Value = elem
                                L = len(elem)
                            else:
                                L = min(len(elem),p.Size) #v2.1 Cole limit data to defined size
                                p.Value = elem[:L]       #v2.1 Jevon & v2.1 Cole
                            if L>0:   #v2.1 Cole something does not like p.Size as Zero
                                p.Size = L           #v2.1 Jevon
                        elif tp == types.BufferType: #v2.1 Cole -- ADO BINARY
                            p.Size = len(elem)
                            p.AppendChunk(elem)
                        else:
                            p.Value=elem
                        if verbose > 2:
                            print 'Parameter %d type %d stored as: %s' % (parmIndx,p.Type,repr(p.Value))
            parmIndx = -2 # kludge to prevent exception handler picking out one parameter
            
            # ----- the actual SQL is executed here --- 
            adoRetVal=self.cmd.Execute()
            # ----- ------------------------------- ---
        except (Exception), e:
            tbk = u'\n--ADODBAPI\n'
            if parmIndx >= 0:
                tbk += u'-- Trying parameter %d = %s\n' \
                    %(parmIndx, repr(parameters[parmIndx]))
            tblist=(traceback.format_exception(sys.exc_type,
                                              sys.exc_value,
                                              sys.exc_traceback,
                                              8))
            tb=string.join(tblist)
            tracebackhistory = tbk + tb + u'-- on command: "%s"\n-- with parameters: %s' \
                               %(operation,parameters)
            self._raiseCursorError(DatabaseError,tracebackhistory)
            return
            
        rs=adoRetVal[0]
        ##self.rowcount=adoRetVal[1]    # NOTE: usually will be -1 for SELECT  :-(
        recs=adoRetVal[1]   #[begin Jorgensen ] ## but it breaks unit test
        ## skip over empty record sets to get to one that isn't empty
        ##while rs and rs.State == adStateClosed:
        ##    try:
        ##        rs, recs = rs.NextRecordset()
        ##    except:
        ##        break
        self.rowcount=recs  #[end Jorgensen]
        
        self._makeDescriptionFromRS(rs)

        if isStoredProcedureCall and parameters != None:
            return self._returnADOCommandParameters(self.cmd)
        
    def execute(self, operation, parameters=None):
        """Prepare and execute a database operation (query or command).
        
            Parameters may be provided as sequence or mapping and will be bound to variables in the operation.
            Variables are specified in a database-specific notation
            (see the module's paramstyle attribute for details). [5] 
            A reference to the operation will be retained by the cursor.
            If the same operation object is passed in again, then the cursor
            can optimize its behavior. This is most effective for algorithms
            where the same operation is used, but different parameters are bound to it (many times). 

            For maximum efficiency when reusing an operation, it is best to use
            the setinputsizes() method to specify the parameter types and sizes ahead of time.
            It is legal for a parameter to not match the predefined information;
            the implementation should compensate, possibly with a loss of efficiency. 

            The parameters may also be specified as list of tuples to e.g. insert multiple rows in
            a single operation, but this kind of usage is depreciated: executemany() should be used instead. 

            Return values are not defined.

            [5] The module will use the __getitem__ method of the parameters object to map either positions
            (integers) or names (strings) to parameter values. This allows for both sequences and mappings
            to be used as input. 
            The term "bound" refers to the process of binding an input value to a database execution buffer.
            In practical terms, this means that the input value is directly used as a value in the operation.
            The client should not be required to "escape" the value so that it can be used -- the value
            should be equal to the actual database value. 
        """
        self.messages=[]                
        self._executeHelper(operation,False,parameters)
            
    def executemany(self, operation, seq_of_parameters):
        """Prepare a database operation (query or command) and then execute it against all parameter sequences or mappings found in the sequence seq_of_parameters.
        
            Return values are not defined. 
        """
        self.messages=[]                
        totrecordcount = 0
        canCount=True
        for params in seq_of_parameters:
            self.execute(operation, params)
            if self.rowcount == -1:
                canCount=False
            elif canCount:
                totrecordcount+=self.rowcount
                
        if canCount:                  
            self.rowcount=totrecordcount
        else:
            self.rowcount=-1

    def _fetch(self, rows=None):
        """ Fetch rows from the recordset.
        rows is None gets all (for fetchall).
        """
        rs=self.rs
        if self.conn == None:
            self._raiseCursorError(Error,None)
            return
        if rs == None:
            self._raiseCursorError(Error,None)
            return
        else:
            if rs.State == adStateClosed or rs.BOF or rs.EOF:
                if rows == 1: return None # fetchone can return None
                else: return [] # fetchall and fetchmany return empty lists

            if rows:
                ado_results = self.rs.GetRows(rows)
            else:
                ado_results = self.rs.GetRows()

            d=self.description
            returnList=[]
            i=0
            for descTuple in d:
                # Desctuple =(name, type_code, display_size, internal_size, precision, scale, null_ok).
                type_code=descTuple[1]
                returnList.append([convertVariantToPython(r,type_code) for r in ado_results[i]])
                i+=1
            return tuple(zip(*returnList))
        

    def fetchone(self):
        """ Fetch the next row of a query result set, returning a single sequence,
            or None when no more data is available.

            An Error (or subclass) exception is raised if the previous call to executeXXX()
            did not produce any result set or no call was issued yet. 
        """
        self.messages=[]                
        ret = self._fetch(1)
        if ret: # return record (not list of records)
            return ret[0]
        else:
            return ret # (in case of None)

                    
    def fetchmany(self, size=None):
        """Fetch the next set of rows of a query result, returning a list of tuples. An empty sequence is returned when no more rows are available.
        
        The number of rows to fetch per call is specified by the parameter.
        If it is not given, the cursor's arraysize determines the number of rows to be fetched.
        The method should try to fetch as many rows as indicated by the size parameter.
        If this is not possible due to the specified number of rows not being available,
        fewer rows may be returned. 

        An Error (or subclass) exception is raised if the previous call to executeXXX()
        did not produce any result set or no call was issued yet. 

        Note there are performance considerations involved with the size parameter.
        For optimal performance, it is usually best to use the arraysize attribute.
        If the size parameter is used, then it is best for it to retain the same value from
        one fetchmany() call to the next. 
        """
        self.messages=[]                
        if size == None:
            size = self.arraysize
        return self._fetch(size)

    def fetchall(self):
        """Fetch all (remaining) rows of a query result, returning them as a sequence of sequences (e.g. a list of tuples).

            Note that the cursor's arraysize attribute
            can affect the performance of this operation. 
            An Error (or subclass) exception is raised if the previous call to executeXXX()
            did not produce any result set or no call was issued yet. 
        """
        self.messages=[]                
        return self._fetch()

    def nextset(self):
        """Make the cursor skip to the next available set, discarding any remaining rows from the current set. 

            If there are no more sets, the method returns None. Otherwise, it returns a true
            value and subsequent calls to the fetch methods will return rows from the next result set. 

            An Error (or subclass) exception is raised if the previous call to executeXXX()
            did not produce any result set or no call was issued yet.
        """
        self.messages=[]                
        if self.conn == None:
            self._raiseCursorError(Error,None)
            return
        if self.rs == None:
            self._raiseCursorError(Error,None)
            return
        else:
            try:                                                   #[begin 2.1 ekelund]
                rsTuple=self.rs.NextRecordset()                    # 
            except pywintypes.com_error, exc:                      # return appropriate error
                self._raiseCursorError(NotSupportedError, exc.args)#[end 2.1 ekelund]
            rs=rsTuple[0]
            if rs==None:
                return None
            self._makeDescriptionFromRS(rs)
            return 1

    def setinputsizes(self,sizes):
        pass

    def setoutputsize(self, size, column=None):
        pass

#Type Objects and Constructors
#Many databases need to have the input in a particular format for binding to an operation's input parameters.
#For example, if an input is destined for a DATE column, then it must be bound to the database in a particular
#string format. Similar problems exist for "Row ID" columns or large binary items (e.g. blobs or RAW columns).
#This presents problems for Python since the parameters to the executeXXX() method are untyped.
#When the database module sees a Python string object, it doesn't know if it should be bound as a simple CHAR column,
#as a raw BINARY item, or as a DATE. 
#
#To overcome this problem, a module must provide the constructors defined below to create objects that can
#hold special values. When passed to the cursor methods, the module can then detect the proper type of
#the input parameter and bind it accordingly. 

#A Cursor Object's description attribute returns information about each of the result columns of a query.
#The type_code must compare equal to one of Type Objects defined below. Type Objects may be equal to more than
#one type code (e.g. DATETIME could be equal to the type codes for date, time and timestamp columns;
#see the Implementation Hints below for details). 

def Date(year,month,day):
    "This function constructs an object holding a date value. "
    return dateconverter.Date(year,month,day)

def Time(hour,minute,second):
    "This function constructs an object holding a time value. "
    return dateconverter.Time(hour,minute,second)

def Timestamp(year,month,day,hour,minute,second):
    "This function constructs an object holding a time stamp value. "
    return dateconverter.Timestamp(year,month,day,hour,minute,second)

def DateFromTicks(ticks):
    """This function constructs an object holding a date value from the given ticks value
    (number of seconds since the epoch; see the documentation of the standard Python time module for details). """
    return apply(Date,time.localtime(ticks)[:3])


def TimeFromTicks(ticks):
    """This function constructs an object holding a time value from the given ticks value
    (number of seconds since the epoch; see the documentation of the standard Python time module for details). """
    return apply(Time,time.localtime(ticks)[3:6])

def TimestampFromTicks(ticks):
    """This function constructs an object holding a time stamp value from the given
    ticks value (number of seconds since the epoch;
    see the documentation of the standard Python time module for details). """
    return apply(Timestamp,time.localtime(ticks)[:6])

def Binary(aString):
    """This function constructs an object capable of holding a binary (long) string value. """
    return buffer(aString)

#v2.1 Cole comment out: BinaryType = Binary('a')

#SQL NULL values are represented by the Python None singleton on input and output. 

#Note: Usage of Unix ticks for database interfacing can cause troubles because of the limited date range they cover. 

def pyTypeToADOType(d):
    tp=type(d)
    try:
        return typeMap[tp]
    except KeyError:
        if tp in dateconverter.types:
            return adDate
    raise DataError
        
def cvtCurrency((hi, lo), decimal=2): #special for type adCurrency
    if lo < 0:
        lo += (2L ** 32)
    # was: return round((float((long(hi) << 32) + lo))/10000.0,decimal)
    return decimal.Decimal((long(hi) << 32) + lo)/decimal.Decimal(1000) #v2.1 Cole

def cvtNumeric(variant):     #all v2.1 Cole
    try:
        return decimal.Decimal(variant)
    except (ValueError,TypeError):
        pass
    try:
        europeVsUS=str(variant).replace(",",".")
        return decimal.Decimal(europeVsUS)
    except (ValueError,TypeError):
        pass    

def cvtFloat(variant):
    try:
        return float(variant)
    except (ValueError,TypeError):
        pass
#    if type(variant)==types.TupleType: #v2.1 Cole (should be unnecessary now)
#        return cvtCurrency(variant)
    try:
        europeVsUS=str(variant).replace(",",".")   #v2.1 Cole
        return float(europeVsUS)
    except (ValueError,TypeError):
        pass    

def identity(x): return x

class VariantConversionMap(dict):
    def __init__(self, aDict):
        for k, v in aDict.iteritems():
            self[k] = v # we must call __setitem__

    def __setitem__(self, adoType, cvtFn):
        "don't make adoType a string :-)"
        try: # user passed us a tuple, set them individually
           for type in adoType:
               dict.__setitem__(self, type, cvtFn)
        except TypeError: # single value
           dict.__setitem__(self, adoType, cvtFn)

    def __getitem__(self, fromType):
        try:
            return dict.__getitem__(self, fromType)
        except KeyError:
            return identity

def convertVariantToPython(variant, adType):
    if variant is None:
        return None
    return variantConversions[adType](variant)


adCmdText = 1 
adCmdStoredProc = 4

adParamInput                  =0x1        # from enum ParameterDirectionEnum
adParamInputOutput            =0x3        # from enum ParameterDirectionEnum
adParamOutput                 =0x2        # from enum ParameterDirectionEnum
adParamReturnValue            =0x4        # from enum ParameterDirectionEnum
adParamUnknown                =0x0        # from enum ParameterDirectionEnum


adStateClosed =0

adFldMayBeNull=0x40 

adModeShareExclusive=0xc

adXactCommitRetaining= 131072
adXactAbortRetaining = 262144 


adArray                       =0x2000     # from enum DataTypeEnum
adBSTR                        =0x8        # from enum DataTypeEnum
adBigInt                      =0x14       # from enum DataTypeEnum
adBinary                      =0x80       # from enum DataTypeEnum
adBoolean                     =0xb        # from enum DataTypeEnum
adChapter                     =0x88       # from enum DataTypeEnum
adChar                        =0x81       # from enum DataTypeEnum
adCurrency                    =0x6        # from enum DataTypeEnum
adDBDate                      =0x85       # from enum DataTypeEnum
adDBTime                      =0x86       # from enum DataTypeEnum
adDBTimeStamp                 =0x87       # from enum DataTypeEnum
adDate                        =0x7        # from enum DataTypeEnum
adDecimal                     =0xe        # from enum DataTypeEnum
adDouble                      =0x5        # from enum DataTypeEnum
adEmpty                       =0x0        # from enum DataTypeEnum
adError                       =0xa        # from enum DataTypeEnum
adFileTime                    =0x40       # from enum DataTypeEnum
adGUID                        =0x48       # from enum DataTypeEnum
adIDispatch                   =0x9        # from enum DataTypeEnum
adIUnknown                    =0xd        # from enum DataTypeEnum
adInteger                     =0x3        # from enum DataTypeEnum
adLongVarBinary               =0xcd       # from enum DataTypeEnum
adLongVarChar                 =0xc9       # from enum DataTypeEnum
adLongVarWChar                =0xcb       # from enum DataTypeEnum
adNumeric                     =0x83       # from enum DataTypeEnum
adPropVariant                 =0x8a       # from enum DataTypeEnum
adSingle                      =0x4        # from enum DataTypeEnum
adSmallInt                    =0x2        # from enum DataTypeEnum
adTinyInt                     =0x10       # from enum DataTypeEnum
adUnsignedBigInt              =0x15       # from enum DataTypeEnum
adUnsignedInt                 =0x13       # from enum DataTypeEnum
adUnsignedSmallInt            =0x12       # from enum DataTypeEnum
adUnsignedTinyInt             =0x11       # from enum DataTypeEnum
adUserDefined                 =0x84       # from enum DataTypeEnum
adVarBinary                   =0xcc       # from enum DataTypeEnum
adVarChar                     =0xc8       # from enum DataTypeEnum
adVarNumeric                  =0x8b       # from enum DataTypeEnum
adVarWChar                    =0xca       # from enum DataTypeEnum
adVariant                     =0xc        # from enum DataTypeEnum
adWChar                       =0x82       # from enum DataTypeEnum

#ado DataTypeEnum -- copy of above, here in decimal, sorted by value
#adEmpty 0 Specifies no value (DBTYPE_EMPTY). 
#adSmallInt 2 Indicates a two-byte signed integer (DBTYPE_I2). 
#adInteger 3 Indicates a four-byte signed integer (DBTYPE_I4). 
#adSingle 4 Indicates a single-precision floating-point value (DBTYPE_R4). 
#adDouble 5 Indicates a double-precision floating-point value (DBTYPE_R8). 
#adCurrency 6 Indicates a currency value (DBTYPE_CY). Currency is a fixed-point number with four digits to the right of the decimal point. It is stored in an eight-byte signed integer scaled by 10,000. 
#adDate 7 Indicates a date value (DBTYPE_DATE). A date is stored as a double, the whole part of which is the number of days since December 30, 1899, and the fractional part of which is the fraction of a day. 
#adBSTR 8 Indicates a null-terminated character string (Unicode) (DBTYPE_BSTR). 
#adIDispatch 9 Indicates a pointer to an IDispatch interface on a COM object (DBTYPE_IDISPATCH). 
#adError 10 Indicates a 32-bit error code (DBTYPE_ERROR). 
#adBoolean 11 Indicates a boolean value (DBTYPE_BOOL). 
#adVariant 12 Indicates an Automation Variant (DBTYPE_VARIANT). 
#adIUnknown 13 Indicates a pointer to an IUnknown interface on a COM object (DBTYPE_IUNKNOWN). 
#adDecimal 14 Indicates an exact numeric value with a fixed precision and scale (DBTYPE_DECIMAL). 
#adTinyInt 16 Indicates a one-byte signed integer (DBTYPE_I1). 
#adUnsignedTinyInt 17 Indicates a one-byte unsigned integer (DBTYPE_UI1). 
#adUnsignedSmallInt 18 Indicates a two-byte unsigned integer (DBTYPE_UI2). 
#adUnsignedInt 19 Indicates a four-byte unsigned integer (DBTYPE_UI4). 
#adBigInt 20 Indicates an eight-byte signed integer (DBTYPE_I8). 
#adUnsignedBigInt 21 Indicates an eight-byte unsigned integer (DBTYPE_UI8). 
#adFileTime 64 Indicates a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (DBTYPE_FILETIME). 
#adGUID 72 Indicates a globally unique identifier (GUID) (DBTYPE_GUID). 
#adBinary 128 Indicates a binary value (DBTYPE_BYTES). 
#adChar 129 Indicates a string value (DBTYPE_STR). 
#adWChar 130 Indicates a null-terminated Unicode character string (DBTYPE_WSTR). 
#adNumeric 131 Indicates an exact numeric value with a fixed precision and scale (DBTYPE_NUMERIC). adUserDefined 132 Indicates a user-defined variable (DBTYPE_UDT). 
#adUserDefined 132 Indicates a user-defined variable (DBTYPE_UDT). 
#adDBDate 133 Indicates a date value (yyyymmdd) (DBTYPE_DBDATE). 
#adDBTime 134 Indicates a time value (hhmmss) (DBTYPE_DBTIME). 
#adDBTimeStamp 135 Indicates a date/time stamp (yyyymmddhhmmss plus a fraction in billionths) (DBTYPE_DBTIMESTAMP). 
#adChapter 136 Indicates a four-byte chapter value that identifies rows in a child rowset (DBTYPE_HCHAPTER). 
#adPropVariant 138 Indicates an Automation PROPVARIANT (DBTYPE_PROP_VARIANT). 
#adVarNumeric 139 Indicates a numeric value (Parameter object only). 
#adVarChar 200 Indicates a string value (Parameter object only). 
#adLongVarChar 201 Indicates a long string value (Parameter object only). 
#adVarWChar 202 Indicates a null-terminated Unicode character string (Parameter object only). 
#adLongVarWChar 203 Indicates a long null-terminated Unicode string value (Parameter object only). 
#adVarBinary 204 Indicates a binary value (Parameter object only). 
#adLongVarBinary 205 Indicates a long binary value (Parameter object only). 
#adArray (Does not apply to ADOX.) 0x2000 A flag value, always combined with another data type constant, that indicates an array of that other data type.  

adoErrors= {
    0xe7b      :'adErrBoundToCommand           ', 
    0xe94      :'adErrCannotComplete           ', 
    0xea4      :'adErrCantChangeConnection     ', 
    0xc94      :'adErrCantChangeProvider       ', 
    0xe8c      :'adErrCantConvertvalue         ', 
    0xe8d      :'adErrCantCreate               ', 
    0xea3      :'adErrCatalogNotSet            ', 
    0xe8e      :'adErrColumnNotOnThisRow       ', 
    0xd5d      :'adErrDataConversion           ', 
    0xe89      :'adErrDataOverflow             ', 
    0xe9a      :'adErrDelResOutOfScope         ', 
    0xea6      :'adErrDenyNotSupported         ', 
    0xea7      :'adErrDenyTypeNotSupported     ', 
    0xcb3      :'adErrFeatureNotAvailable      ', 
    0xea5      :'adErrFieldsUpdateFailed       ', 
    0xc93      :'adErrIllegalOperation         ', 
    0xcae      :'adErrInTransaction            ', 
    0xe87      :'adErrIntegrityViolation       ', 
    0xbb9      :'adErrInvalidArgument          ', 
    0xe7d      :'adErrInvalidConnection        ', 
    0xe7c      :'adErrInvalidParamInfo         ', 
    0xe82      :'adErrInvalidTransaction       ', 
    0xe91      :'adErrInvalidURL               ', 
    0xcc1      :'adErrItemNotFound             ', 
    0xbcd      :'adErrNoCurrentRecord          ', 
    0xe83      :'adErrNotExecuting             ', 
    0xe7e      :'adErrNotReentrant             ', 
    0xe78      :'adErrObjectClosed             ', 
    0xd27      :'adErrObjectInCollection       ', 
    0xd5c      :'adErrObjectNotSet             ', 
    0xe79      :'adErrObjectOpen               ', 
    0xbba      :'adErrOpeningFile              ', 
    0xe80      :'adErrOperationCancelled       ', 
    0xe96      :'adErrOutOfSpace               ', 
    0xe88      :'adErrPermissionDenied         ', 
    0xe9e      :'adErrPropConflicting          ', 
    0xe9b      :'adErrPropInvalidColumn        ', 
    0xe9c      :'adErrPropInvalidOption        ', 
    0xe9d      :'adErrPropInvalidValue         ', 
    0xe9f      :'adErrPropNotAllSettable       ', 
    0xea0      :'adErrPropNotSet               ', 
    0xea1      :'adErrPropNotSettable          ', 
    0xea2      :'adErrPropNotSupported         ', 
    0xbb8      :'adErrProviderFailed           ', 
    0xe7a      :'adErrProviderNotFound         ', 
    0xbbb      :'adErrReadFile                 ', 
    0xe93      :'adErrResourceExists           ', 
    0xe92      :'adErrResourceLocked           ', 
    0xe97      :'adErrResourceOutOfScope       ', 
    0xe8a      :'adErrSchemaViolation          ', 
    0xe8b      :'adErrSignMismatch             ', 
    0xe81      :'adErrStillConnecting          ', 
    0xe7f      :'adErrStillExecuting           ', 
    0xe90      :'adErrTreePermissionDenied     ', 
    0xe8f      :'adErrURLDoesNotExist          ', 
    0xe99      :'adErrURLNamedRowDoesNotExist  ', 
    0xe98      :'adErrUnavailable              ', 
    0xe84      :'adErrUnsafeOperation          ', 
    0xe95      :'adErrVolumeNotFound           ', 
    0xbbc      :'adErrWriteFile                ' 
    }


class DBAPITypeObject:
  def __init__(self,valuesTuple):
    self.values = valuesTuple

  def __cmp__(self,other):
    if other in self.values:
      return 0
    if other < self.values:
      return 1
    else:
      return -1

adoIntegerTypes=(adInteger,adSmallInt,adTinyInt,adUnsignedInt,
                 adUnsignedSmallInt,adUnsignedTinyInt,
                 adBoolean,adError) #max 32 bits
adoRowIdTypes=(adChapter,)          #v2.1 Rose
adoLongTypes=(adBigInt,adFileTime,adUnsignedBigInt)
adoExactNumericTypes=(adDecimal,adNumeric,adVarNumeric,adCurrency)      #v2.1 Cole     
adoApproximateNumericTypes=(adDouble,adSingle)                          #v2.1 Cole
adoStringTypes=(adBSTR,adChar,adLongVarChar,adLongVarWChar,
                adVarChar,adVarWChar,adWChar,adGUID)
adoBinaryTypes=(adBinary,adLongVarBinary,adVarBinary)
adoDateTimeTypes=(adDBTime, adDBTimeStamp, adDate, adDBDate)            
adoRemainingTypes=(adEmpty,adIDispatch,adIUnknown,
                   adPropVariant,adArray,adUserDefined,
                   adVariant)
                   
"""This type object is used to describe columns in a database that are string-based (e.g. CHAR). """
STRING   = DBAPITypeObject(adoStringTypes)

"""This type object is used to describe (long) binary columns in a database (e.g. LONG, RAW, BLOBs). """
BINARY   = DBAPITypeObject(adoBinaryTypes)

"""This type object is used to describe numeric columns in a database. """
NUMBER   = DBAPITypeObject(adoIntegerTypes + adoLongTypes + \
                           adoExactNumericTypes + adoApproximateNumericTypes)

"""This type object is used to describe date/time columns in a database. """

DATETIME = DBAPITypeObject(adoDateTimeTypes)
"""This type object is used to describe the "Row ID" column in a database. """
ROWID    = DBAPITypeObject(adoRowIdTypes)

typeMap= { 
           types.BufferType: adBinary,
           types.FloatType: adNumeric,
           types.IntType: adInteger,
           types.LongType: adBigInt,
           types.StringType: adBSTR,
           types.NoneType: adEmpty,
           types.UnicodeType: adBSTR,
           types.BooleanType:adBoolean,          #v2.1 Cole
           type(decimal.Decimal): adNumeric       #v2.1 Cole
           }

try: #If mx extensions are installed, use mxDateTime
    import mx.DateTime
    dateconverter=mxDateTimeConverter()
except: #Python 2.3 or later, use datetimelibrary by default
    dateconverter=pythonDateTimeConverter()

# variant type : function converting variant to Python value

def variantConvertDate(v):
    return dateconverter.DateObjectFromCOMDate(v)

variantConversions = VariantConversionMap( {
    adoDateTimeTypes : variantConvertDate,
    adoApproximateNumericTypes: cvtFloat,
    adCurrency: cvtCurrency,
    adoExactNumericTypes: identity, # use cvtNumeric to force decimal rather than unicode
    adoLongTypes : long,
    adoIntegerTypes: int,
    adoRowIdTypes: int,
    adoStringTypes: identity,
    adoBinaryTypes: identity,
    adoRemainingTypes: identity
})
