""" Unit tests for adodbapi"""
"""
    adodbapi - A python DB API 2.0 interface to Microsoft ADO
    
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

    Updated for decimal data and version 2.1 by Vernon Cole
"""

import unittest
import win32com.client

import adodbapi
import adodbapitestconfig

#adodbapi.verbose = True

import types
try:
    import decimal
except ImportError:
    import win32com.decimal_23 as decimal

class CommonDBTests(unittest.TestCase):
    "Self contained super-simple tests in easy syntax, should work on everything between mySQL and Oracle"

    def setUp(self):
        self.engine = 'unknown'

    def getEngine(self):
        return self.engine
    
    def getConnection(self):
        raise "This method must be overriden by a subclass"  

    def getCursor(self):
        return self.getConnection().cursor()

    def testConnection(self):
        crsr=self.getCursor()
        assert crsr.__class__.__name__ == 'Cursor'

    def testErrorHandlerInherits(self):
        conn=self.getConnection()
        mycallable=lambda connection,cursor,errorclass,errorvalue: 1
        conn.errorhandler=mycallable
        crsr=conn.cursor()
        assert crsr.errorhandler==mycallable,"Error handler on crsr should be same as on connection"

    def testDefaultErrorHandlerConnection(self):
        conn=self.getConnection()
        del conn.messages[:]
        try:
            conn.close()
            conn.commit() #Should not be able to use connection after it is closed
        except:
            assert len(conn.messages)==1
            assert len(conn.messages[0])==2
            assert conn.messages[0][0]==adodbapi.Error
            
    def testOwnErrorHandlerConnection(self):
        mycallable=lambda connection,cursor,errorclass,errorvalue: 1 #does not raise anything
        conn=self.getConnection()        
        conn.errorhandler=mycallable
        conn.close()
        conn.commit() #Should not be able to use connection after it is closed        
        assert len(conn.messages)==0
       
        conn.errorhandler=None #This should bring back the standard error handler
        try:
            conn.close()
            conn.commit() #Should not be able to use connection after it is closed
        except:
            pass
        #The Standard errorhandler appends error to messages attribute
        assert len(conn.messages)>0,"Setting errorhandler to none  should bring back the standard error handler"


    def testDefaultErrorHandlerCursor(self):
        crsr=self.getConnection().cursor()
        del crsr.messages[:]       
        try:
            crsr.execute("SELECT abbtytddrf FROM dasdasd")
        except:
            assert len(crsr.messages)==1
            assert len(crsr.messages[0])==2
            assert crsr.messages[0][0]==adodbapi.DatabaseError
            
    def testOwnErrorHandlerCursor(self):
        mycallable=lambda connection,cursor,errorclass,errorvalue: 1 #does not raise anything
        crsr=self.getConnection().cursor()
        crsr.errorhandler=mycallable
        crsr.execute("SELECT abbtytddrf FROM dasdasd")
        assert len(crsr.messages)==0
        
        crsr.errorhandler=None #This should bring back the standard error handler
        try:
            crsr.execute("SELECT abbtytddrf FROM dasdasd")
        except:
            pass
        #The Standard errorhandler appends error to messages attribute
        assert len(crsr.messages)>0,"Setting errorhandler to none  should bring back the standard error handler"


    def testUserDefinedConversions(self):
        
        oldconverter=adodbapi.variantConversions[adodbapi.adoStringTypes]
        try:
            duplicatingConverter=lambda aStringField: aStringField*2
            assert duplicatingConverter(u'gabba') == u'gabbagabba'

            adodbapi.variantConversions[adodbapi.adoStringTypes]=duplicatingConverter

            self.helpForceDropOnTblTemp()
            conn=self.getConnection()

            crsr=conn.cursor()
            tabdef = "CREATE TABLE tblTemp (fldData VARCHAR(100) NOT NULL)"
            crsr.execute(tabdef)
            crsr.execute("INSERT INTO tblTemp(fldData) VALUES('gabba')")
            crsr.execute("INSERT INTO tblTemp(fldData) VALUES('hey')")
            crsr.execute("SELECT fldData FROM tblTemp ORDER BY fldData")
            row=crsr.fetchone()
            self.assertEquals(row[0],'gabbagabba')
            row=crsr.fetchone()
            self.assertEquals(row[0],'heyhey')                     
        finally:
            adodbapi.variantConversions[adodbapi.adoStringTypes]=oldconverter #Restore
            self.helpRollbackTblTemp()
    
    def helpTestDataType(self,sqlDataTypeString,
                         DBAPIDataTypeString,
                         pyData,
                         pyDataInputAlternatives=None,
                         compareAlmostEqual=None,
                         allowedReturnValues=None):
        self.helpForceDropOnTblTemp()
        conn=self.getConnection()       
        crsr=conn.cursor()
        tabdef= """
            CREATE TABLE tblTemp (
                fldId integer NOT NULL,
                fldData """ + sqlDataTypeString + ")\n"
            ##    """  NULL
            ##)
            ##"""
        crsr.execute(tabdef)
        
        #Test Null values mapped to None
        crsr.execute("INSERT INTO tblTemp (fldId) VALUES (1)")
        
        crsr.execute("SELECT fldId,fldData FROM tblTemp")
        rs=crsr.fetchone()
        self.assertEquals(rs[1],None) #Null should be mapped to None
        assert rs[0]==1

        #Test description related 
        descTuple=crsr.description[1]
        assert descTuple[0] == 'fldData'

        if DBAPIDataTypeString=='STRING':
            assert descTuple[1] == adodbapi.STRING
        elif DBAPIDataTypeString == 'NUMBER':
            assert descTuple[1] == adodbapi.NUMBER
        elif DBAPIDataTypeString == 'BINARY':
            assert descTuple[1] == adodbapi.BINARY
        elif DBAPIDataTypeString == 'DATETIME':
            assert descTuple[1] == adodbapi.DATETIME
        elif DBAPIDataTypeString == 'ROWID':
            assert descTuple[1] == adodbapi.ROWID
        else:
            raise "DBAPIDataTypeString not provided"

        #Test data binding
        inputs=[pyData]
        if pyDataInputAlternatives:
            inputs.append(pyDataInputAlternatives)

        fldId=1
        for inParam in inputs:
            fldId+=1
            try:
                crsr.execute("INSERT INTO tblTemp (fldId,fldData) VALUES (?,?)", (fldId,pyData))
            except:
                conn.printADOerrors()

            crsr.execute("SELECT fldData FROM tblTemp WHERE ?=fldID", [fldId])
            rs=crsr.fetchone()
            if allowedReturnValues:
                self.assertEquals(type(rs[0]),type(allowedReturnValues[0]) )
            else:
                self.assertEquals( type(rs[0]) ,type(pyData))

            if compareAlmostEqual and DBAPIDataTypeString == 'DATETIME':
                iso1=adodbapi.dateconverter.DateObjectToIsoFormatString(rs[0])
                iso2=adodbapi.dateconverter.DateObjectToIsoFormatString(pyData)
                self.assertEquals(iso1 , iso2)
            elif compareAlmostEqual:
                assert abs(rs[0]-pyData)/pyData<0.00001, \
                    "Values not almost equal rs[0]=%s, oyDta=%f" %(rs[0],pyData)
            else:
                if allowedReturnValues:
                    ok=0
                    for possibility in allowedReturnValues:
                        if rs[0]==possibility:
                            ok=1
                    assert ok
                else:                
                    self.assertEquals(rs[0] , pyData)

        self.helpRollbackTblTemp()
          
    def testDataTypeFloat(self):       
        self.helpTestDataType("real",'NUMBER',3.45,compareAlmostEqual=1)
        self.helpTestDataType("float",'NUMBER',1.79e37,compareAlmostEqual=1)

        #v2.1 Cole -- use decimal for money
        if self.getEngine() != 'MySQL':
            self.helpTestDataType("smallmoney",'NUMBER',decimal.Decimal('214748.02'))        
            self.helpTestDataType("money",'NUMBER',decimal.Decimal('-922337203685477.5808'))
        
    def testDataTypeInt(self):              
        self.helpTestDataType("tinyint",'NUMBER',115)
        self.helpTestDataType("smallint",'NUMBER',-32768)
        self.helpTestDataType("int",'NUMBER',2147483647)
        if self.getEngine() != 'ACCESS':
            self.helpTestDataType("bit",'NUMBER',1) #Does not work correctly with access        
            self.helpTestDataType("bigint",'NUMBER',3000000000L) 

    def testDataTypeChar(self):
        for sqlDataType in ("char(6)","nchar(6)"):
            self.helpTestDataType(sqlDataType,'STRING',u'spam  ',allowedReturnValues=[u'spam','spam',u'spam  ','spam  '])

    def testDataTypeVarChar(self):
        stringKinds = ["varchar(10)","nvarchar(10)","text","ntext"]
        if self.getEngine() == 'MySQL':
            stringKinds = ["varchar(10)","text"]
        for sqlDataType in stringKinds:
            self.helpTestDataType(sqlDataType,'STRING',u'spam',['spam'])
            
    def testDataTypeDate(self):
        #Does not work with pytonTimeConvertor        
        #self.helpTestDataType("smalldatetime",'DATETIME',adodbapi.Timestamp(2002,10,28,12,15,00)) #Accuracy one minute

        self.helpTestDataType("datetime",'DATETIME',adodbapi.Date(2002,10,28),compareAlmostEqual=True)
        if self.getEngine() != 'MySQL':
            self.helpTestDataType("smalldatetime",'DATETIME',adodbapi.Date(2002,10,28),compareAlmostEqual=True)
        self.helpTestDataType("datetime",'DATETIME',adodbapi.Timestamp(2002,10,28,12,15,01),compareAlmostEqual=True)

    def testDataTypeBinary(self):
        if self.getEngine() == 'MySQL':
            pass #self.helpTestDataType("BLOB",'BINARY',adodbapi.Binary('\x00\x01\xE2\x40'))
        else:
            self.helpTestDataType("binary(4)",'BINARY',adodbapi.Binary('\x00\x01\xE2\x40'))
            self.helpTestDataType("varbinary(100)",'BINARY',adodbapi.Binary('\x00\x01\xE2\x40'))
            self.helpTestDataType("image",'BINARY',adodbapi.Binary('\x00\x01\xE2\x40'))

    def helpRollbackTblTemp(self):
        try:
            self.getConnection().rollback()
        except adodbapi.NotSupportedError:
            pass
        self.helpForceDropOnTblTemp()
        
    def helpForceDropOnTblTemp(self):
        conn=self.getConnection()
        crsr=conn.cursor()       
        try:
            crsr.execute("DELETE FROM tblTemp")
            crsr.execute("DROP TABLE tblTemp")
            conn.commit()
        except:
            pass

    def helpCreateAndPopulateTableTemp(self,crsr):
        tabdef= """
            CREATE TABLE tblTemp (
                fldData INTEGER
            )
            """
        crsr.execute(tabdef)
        for i in range(9):
            crsr.execute("INSERT INTO tblTemp (fldData) VALUES (%i)" %(i,))
            
    def testFetchAll(self):      
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        crsr.execute("SELECT fldData FROM tblTemp")
        rs=crsr.fetchall()
        assert len(rs)==9
        self.helpRollbackTblTemp()

    def testExecuteMany(self):
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        values = [ (111,) , (222,) ]
        crsr.executemany("INSERT INTO tblTemp (fldData) VALUES (?)",values)
        if crsr.rowcount==-1:
            print self.getEngine(),"Provider does not support rowcount (on .executemany())"
        else:
            self.assertEquals( crsr.rowcount,2)
        crsr.execute("SELECT fldData FROM tblTemp")
        rs=crsr.fetchall()
        assert len(rs)==11
        self.helpRollbackTblTemp()
        

    def testRowCount(self):      
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        crsr.execute("SELECT fldData FROM tblTemp")
        if crsr.rowcount == -1:
            #print "provider does not support rowcount on select"
            pass
        else:
            self.assertEquals( crsr.rowcount,9)
        self.helpRollbackTblTemp()
        
    def testRowCountNoRecordset(self):      
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        crsr.execute("DELETE FROM tblTemp WHERE fldData >= 5")
        if crsr.rowcount==-1:
            print self.getEngine(), "Provider does not support rowcount (on DELETE)"
        else:
            self.assertEquals( crsr.rowcount,4)
        self.helpRollbackTblTemp()        
        
    def testFetchMany(self):
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        crsr.execute("SELECT fldData FROM tblTemp")
        rs=crsr.fetchmany(3)
        assert len(rs)==3
        rs=crsr.fetchmany(5)
        assert len(rs)==5
        rs=crsr.fetchmany(5)
        assert len(rs)==1 #Ask for five, but there is only one left
        self.helpRollbackTblTemp()        

    def testFetchManyWithArraySize(self):
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        crsr.execute("SELECT fldData FROM tblTemp")
        rs=crsr.fetchmany()
        assert len(rs)==1 #arraysize Defaults to one
        crsr.arraysize=4
        rs=crsr.fetchmany()
        assert len(rs)==4
        rs=crsr.fetchmany()
        assert len(rs)==4
        rs=crsr.fetchmany()
        assert len(rs)==0 
        self.helpRollbackTblTemp()


    def testCurrencyDataType(self):
        if self.getEngine() != 'MySQL':
            tabdef= """
                CREATE TABLE tblTemp (
                fldCurr MONEY 
                )
                """
        else:
            tabdef= """
                CREATE TABLE tblTemp (
                fldCurr DECIMAL(19,4) 
                )
                """

        conn=self.getConnection()
        crsr=conn.cursor()
        crsr.execute(tabdef)
        
        for multiplier in (1,decimal.Decimal('2.5'),78,9999,999999,7007):
            crsr.execute("DELETE FROM tblTemp")
            crsr.execute("INSERT INTO tblTemp(fldCurr) VALUES (12.50*%f)" % multiplier)

            sql="SELECT fldCurr FROM tblTemp "
            try:
                crsr.execute(sql)
            except:
                conn.printADOerrors()
                print sql
            fldcurr=crsr.fetchone()[0]
            correct = decimal.Decimal('12.50') * multiplier
            self.assertEquals( fldcurr,correct)

    def testErrorConnect(self):
        self.assertRaises(adodbapi.DatabaseError,adodbapi.connect,'not a valid connect string')

class TestADOwithSQLServer(CommonDBTests):
    def setUp(self):
        try:
            conn=win32com.client.Dispatch("ADODB.Connection")
        except:
            self.fail('SetUpError: Is MDAC installed?')
        try:
            conn.Open(adodbapitestconfig.connStrSQLServer)
        except:
            self.fail('SetUpError: Can not connect to the testdatabase, all other tests will fail...\nAdo error:%s' % conn.Errors(0))
        self.conn=adodbapi.connect(adodbapitestconfig.connStrSQLServer)
        self.engine = 'MSSQL'

    def tearDown(self):
        try:
            self.conn.rollback()
        except:
            pass
        try:
            self.conn.close()
        except:
            pass
        self.conn=None
            
    def getConnection(self):
        return self.conn

    def testSQLServerDataTypes(self):
        self.helpTestDataType("decimal(18,2)",'NUMBER',3.45, allowedReturnValues=[u'3.45',u'3,45'])
        self.helpTestDataType("numeric(18,2)",'NUMBER',3.45, allowedReturnValues=[u'3.45',u'3,45'])

    def testUserDefinedConversionForExactNumericTypes(self):
        # By default decimal and numbers are returned as strings.
        # (see testSQLServerDataTypes method)
        # Instead, make them return as  floats
        oldconverter=adodbapi.variantConversions[adodbapi.adoExactNumericTypes]
        
        adodbapi.variantConversions[adodbapi.adoExactNumericTypes]=adodbapi.cvtFloat
        self.helpTestDataType("decimal(18,2)",'NUMBER',3.45,compareAlmostEqual=1)
        self.helpTestDataType("numeric(18,2)",'NUMBER',3.45,compareAlmostEqual=1)        
     
        adodbapi.variantConversions[adodbapi.adoExactNumericTypes]=oldconverter #Restore
        

    def testVariableReturningStoredProcedure(self):
        crsr=self.conn.cursor()
        spdef= """
            CREATE PROCEDURE sp_DeleteMeOnlyForTesting
                @theInput varchar(50),
                @theOtherInput varchar(50),
                @theOutput varchar(100) OUTPUT
            AS
                SET @theOutput=@theInput+@theOtherInput
                    """
        try:
            crsr.execute("DROP PROCEDURE sp_DeleteMeOnlyForTesting")
            self.conn.commit()
        except: #Make sure it is empty
            pass
        crsr.execute(spdef)

        retvalues=crsr.callproc('sp_DeleteMeOnlyForTesting',('Dodsworth','Anne','              '))

        assert retvalues[0]=='Dodsworth'
        assert retvalues[1]=='Anne'
        assert retvalues[2]=='DodsworthAnne'
        self.conn.rollback()

        
    def testMultipleSetReturn(self):
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        
        spdef= """
            CREATE PROCEDURE sp_DeleteMeOnlyForTesting
            AS
                SELECT fldData FROM tblTemp ORDER BY fldData ASC
                SELECT fldData FROM tblTemp ORDER BY fldData DESC
                    """
        try:
            crsr.execute("DROP PROCEDURE sp_DeleteMeOnlyForTesting")
            self.conn.commit()
        except: #Make sure it is empty
            pass
        crsr.execute(spdef)

        retvalues=crsr.callproc('sp_DeleteMeOnlyForTesting')
        row=crsr.fetchone()
        self.assertEquals(row[0], 0) 
        assert crsr.nextset()
        rowdesc=crsr.fetchall()
        self.assertEquals(rowdesc[0][0],8) 
        s=crsr.nextset()
        assert s== None,'No more return sets, should return None'

        self.helpRollbackTblTemp()



    def testRollBack(self):
        crsr=self.getCursor()
        self.helpCreateAndPopulateTableTemp(crsr)
        self.conn.commit()

        crsr.execute("INSERT INTO tblTemp (fldData) VALUES(100)")

        selectSql="SELECT fldData FROM tblTemp WHERE fldData=100"
        crsr.execute(selectSql)
        rs=crsr.fetchall()
        assert len(rs)==1
        self.conn.rollback()
        crsr.execute(selectSql)
        assert crsr.fetchone()==None, 'cursor.fetchone should return None if a query retrieves no rows'
        self.helpRollbackTblTemp()
        
  
 
class TestADOwithAccessDB(CommonDBTests):
    def setUp(self):
        try:
            adoConn=win32com.client.Dispatch("ADODB.Connection")
        except:
            self.fail('SetUpError: Is MDAC installed?')

        try:
            adoConn.Open(adodbapitestconfig.connStrAccess)
        except:
            self.fail('SetUpError: Can not connect to the testdatabase, all other tests will fail...\nAdo error:%s' % adoConn.Errors(0))
        self.engine = 'ACCESS'

    def getConnection(self):
        return adodbapi.connect(adodbapitestconfig.connStrAccess)

    def testOkConnect(self):
        c=adodbapi.connect(adodbapitestconfig.connStrAccess)
        assert c != None
        
class TestADOwithMySql(CommonDBTests):
    def setUp(self):
        try:
            adoConn=win32com.client.Dispatch("ADODB.Connection")
        except:
            self.fail('SetUpError: Is MDAC installed?')
        try:
            adoConn.Open(adodbapitestconfig.connStrMySql)
        except:
            self.fail('SetUpError: Can not connect to the testdatabase, all other tests will fail...\nAdo error:%s' % adoConn.Errors(0))
        self.engine = 'MySQL'

    def getConnection(self):
        return adodbapi.connect(adodbapitestconfig.connStrMySql)

    def testOkConnect(self):
        c=adodbapi.connect(adodbapitestconfig.connStrMySql)
        assert c != None
        
class TestADOwithAS400DB(CommonDBTests):
    def setUp(self):
        try:
            adoConn=win32com.client.Dispatch("ADODB.Connection")
        except:
            self.fail('SetUpError: Is MDAC installed?')
        try:
            adoConn.Open(adodbapitestconfig.connStrAS400)
        except:
            self.fail('SetUpError: Can not connect to the testdatabase, all other tests will fail...\nAdo error:%s' % adoConn.Errors(0))
        self.engine = 'IBMDA400'

    def getConnection(self):
        return adodbapi.connect(adodbapitestconfig.connStrAS400)

    def testOkConnect(self):
        c=adodbapi.connect(adodbapitestconfig.connStrAS400)
        assert c != None


class TimeConverterInterfaceTest(unittest.TestCase):
    def testIDate(self):
        assert self.tc.Date(1990,2,2)

    def testITime(self):
        assert self.tc.Time(13,2,2)

    def testITimestamp(self):
        assert self.tc.Timestamp(1990,2,2,13,2,1)

    def testIDateObjectFromCOMDate(self):
        assert self.tc.DateObjectFromCOMDate(37435.7604282)

    def testICOMDate(self):
        assert hasattr(self.tc,'COMDate')

    def testExactDate(self):
        d=self.tc.Date(1994,11,15)
        comDate=self.tc.COMDate(d)
        correct=34653.0
        assert comDate == correct,comDate
        
    def testExactTimestamp(self):
        d=self.tc.Timestamp(1994,11,15,12,0,0)
        comDate=self.tc.COMDate(d)
        correct=34653.5
        self.assertEquals( comDate ,correct)
        
        d=self.tc.Timestamp(2003,05,06,14,15,17)
        comDate=self.tc.COMDate(d)
        correct=37747.593946759262
        self.assertEquals( comDate ,correct)

    def testIsoFormat(self):
        d=self.tc.Timestamp(1994,11,15,12,3,10)
        iso=self.tc.DateObjectToIsoFormatString(d)
        self.assertEquals(str(iso[:19]) , '1994-11-15 12:03:10')
        
        dt=self.tc.Date(2003,5,2)
        iso=self.tc.DateObjectToIsoFormatString(dt)
        self.assertEquals(str(iso[:10]), '2003-05-02')
        
if adodbapitestconfig.doMxDateTimeTest:
    import mx.DateTime    
class TestMXDateTimeConverter(TimeConverterInterfaceTest):
    def setUp(self):     
        self.tc=adodbapi.mxDateTimeConverter()
  
    def testCOMDate(self):       
        t=mx.DateTime.DateTime(2002,06,28,18,15,02)       
        cmd=self.tc.COMDate(t)       
        assert cmd == t.COMDate()
    
    def testDateObjectFromCOMDate(self):
        cmd=self.tc.DateObjectFromCOMDate(37435.7604282)
        t=mx.DateTime.DateTime(2002,06,28,18,15,00)
        t2=mx.DateTime.DateTime(2002,06,28,18,15,02)
        assert t2>cmd>t
    
    def testDate(self):
        assert mx.DateTime.Date(1980,11,4)==self.tc.Date(1980,11,4)

    def testTime(self):
        assert mx.DateTime.Time(13,11,4)==self.tc.Time(13,11,4)

    def testTimestamp(self):
        t=mx.DateTime.DateTime(2002,6,28,18,15,01)   
        obj=self.tc.Timestamp(2002,6,28,18,15,01)
        assert t == obj

import time
class TestPythonTimeConverter(TimeConverterInterfaceTest):
    def setUp(self):
        self.tc=adodbapi.pythonTimeConverter()
    
    def testCOMDate(self):
        t=time.localtime(time.mktime((2002,6,28,18,15,01, 4,31+28+31+30+31+28,-1)))
        # Fri, 28 Jun 2002 18:15:01 +0000
        cmd=self.tc.COMDate(t)
        assert abs(cmd - 37435.7604282) < 1.0/24,"more than an hour wrong"

    
    def testDateObjectFromCOMDate(self):
        cmd=self.tc.DateObjectFromCOMDate(37435.7604282)
        t1=time.localtime(time.mktime((2002,6,28,18,14,01, 4,31+28+31+30+31+28,-1)))
        t2=time.localtime(time.mktime((2002,6,28,18,16,01, 4,31+28+31+30+31+28,-1)))
        assert t1<cmd<t2,cmd
    
    def testDate(self):
        t1=time.mktime((2002,6,28,18,15,01, 4,31+28+31+30+31+30,0))
        t2=time.mktime((2002,6,30,18,15,01, 4,31+28+31+30+31+28,0))
        obj=self.tc.Date(2002,6,29)
        assert t1< time.mktime(obj)<t2,obj

    def testTime(self):
        self.assertEquals( self.tc.Time(18,15,2),time.gmtime(18*60*60+15*60+2))

    def testTimestamp(self):
        t1=time.localtime(time.mktime((2002,6,28,18,14,01, 4,31+28+31+30+31+28,-1)))
        t2=time.localtime(time.mktime((2002,6,28,18,16,01, 4,31+28+31+30+31+28,-1)))
        obj=self.tc.Timestamp(2002,6,28,18,15,02)
        assert t1< obj <t2,obj

if adodbapitestconfig.doDateTimeTest:
    import datetime
class TestPythonDateTimeConverter(TimeConverterInterfaceTest):
    def setUp(self):
        self.tc=adodbapi.pythonDateTimeConverter()
    
    def testCOMDate(self):
        t=datetime.datetime( 2002,6,28,18,15,01)
        # Fri, 28 Jun 2002 18:15:01 +0000
        cmd=self.tc.COMDate(t)
        assert abs(cmd - 37435.7604282) < 1.0/24,"more than an hour wrong"

    
    def testDateObjectFromCOMDate(self):
        cmd=self.tc.DateObjectFromCOMDate(37435.7604282)
        t1=datetime.datetime(2002,6,28,18,14,01)
        t2=datetime.datetime(2002,6,28,18,16,01)
        assert t1<cmd<t2,cmd
    
    def testDate(self):
        t1=datetime.date(2002,6,28)
        t2=datetime.date(2002,6,30)
        obj=self.tc.Date(2002,6,29)
        assert t1< obj <t2,obj

    def testTime(self):
        self.assertEquals( self.tc.Time(18,15,2).isoformat()[:8],'18:15:02')

    def testTimestamp(self):
        t1=datetime.datetime(2002,6,28,18,14,01)
        t2=datetime.datetime(2002,6,28,18,16,01)
        obj=self.tc.Timestamp(2002,6,28,18,15,02)
        assert t1< obj <t2,obj

        
suites=[]
if adodbapitestconfig.doMxDateTimeTest:
    suites.append( unittest.makeSuite(TestMXDateTimeConverter,'test'))
if adodbapitestconfig.doDateTimeTest:
    suites.append( unittest.makeSuite(TestPythonDateTimeConverter,'test'))
suites.append( unittest.makeSuite(TestPythonTimeConverter,'test'))

if adodbapitestconfig.doAccessTest:
    suites.append( unittest.makeSuite(TestADOwithAccessDB,'test'))
if adodbapitestconfig.doSqlServerTest:    
    suites.append( unittest.makeSuite(TestADOwithSQLServer,'test'))
if adodbapitestconfig.doMySqlTest:
    suites.append( unittest.makeSuite(TestADOwithMySql,'test'))
if adodbapitestconfig.doAS400Test:
    suites.append( unittest.makeSuite(TestADOwithAS400DB,'test'))
suite=unittest.TestSuite(suites)
if __name__ == '__main__':       
    defaultDateConverter=adodbapi.dateconverter
    print __doc__
    try:
        print adodbapi.version # show version
    except:
        print '"adodbapi.version()" not present or not working.'
    print "Default Date Converter is %s" %(defaultDateConverter,)
    unittest.TextTestRunner().run(suite)
    if adodbapitestconfig.iterateOverTimeTests:
        for test,dateconverter in (
                    (1,adodbapi.pythonTimeConverter),
                    (adodbapitestconfig.doMxDateTimeTest,adodbapi.mxDateTimeConverter),
                    (adodbapitestconfig.doDateTimeTest,adodbapi.pythonDateTimeConverter)
                                    ):
            if test and not isinstance(defaultDateConverter,dateconverter):
                adodbapi.dateconverter=dateconverter()
                print "Changed dateconverter to "
                print adodbapi.dateconverter
                unittest.TextTestRunner().run(suite)
                