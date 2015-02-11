# Configure this in order to run the testcases.

import adodbapi

doAccessTest = True
doSqlServerTest = True
try: #If mx extensions are installed, use mxDateTime
    import mx.DateTime
    doMxDateTimeTest=True
except: 
    doMxDateTimeTest=False #Requires eGenixMXExtensions
doAS400Test = False
doMySqlTest = False
import sys
if float(sys.version[:3])>2.29:
    doDateTimeTest=True #Requires Python 2.3 Alpha2
else:
    doDateTimeTest=False
    print 'Error: adodbapi 2.1 requires Python 2.3'
    
iterateOverTimeTests=True

if doAccessTest:
    _accessdatasource = None  #set to None for automatic creation
    #r"C:\Program Files\Microsoft Office\Office\Samples\northwind.mdb;"
                #Typically C:\Program Files
    if _accessdatasource == None:
        # following setup code borrowed from pywin32 odbc test suite
        # kindly contributed by Frank Millman.
        import tempfile
        import os
        try:
            from win32com.client.gencache import EnsureDispatch
            from win32com.client import constants
            win32 = True
        except importError: #perhaps we are running IronPython
            win32 = False
        if not win32:
            from System import Activator, Type
        _accessdatasource = os.path.join(tempfile.gettempdir(), u"test_odbc.mdb")
        if os.path.isfile(_accessdatasource):
            os.unlink(_accessdatasource)
        # Create a brand-new database - what is the story with these?
        for suffix in (".36", ".35", ".30"):
            ###print 'trying DAO.DBEngine',suffix
            try:
                if win32:
                    dbe = EnsureDispatch("DAO.DBEngine" + suffix)
                else:
                    type= Type.GetTypeFromProgID("DAO.DBEngine" + suffix)
                    dbe =  Activator.CreateInstance(typ)
                break
            except:
                pass
        else:
            raise RuntimeError, "Can't find a DB engine"
        print '    ...Creating ACCESS db at',_accessdatasource    
        if win32:
            workspace = dbe.Workspaces(0)
            newdb = workspace.CreateDatabase(_accessdatasource, 
                                            constants.dbLangGeneral,
                                            constants.dbEncrypt)
        else:
            newdb = dbe.CreateDatabase(_accessdatasource,';LANGID=0x0409;CP=1252;COUNTRY=0')
        newdb.Close()
    connStrAccess = r"Provider=Microsoft.Jet.OLEDB.4.0;Data Source=" + _accessdatasource
    # for ODBC connection try...
    # connStrAccess = "Driver={Microsoft Access Driver (*.mdb)};db=%s;Uid=;Pwd=;" + _accessdatasource

if doSqlServerTest:
    _computername="franklin" #or name of computer with SQL Server
    _databasename="Northwind" #or something else
    #connStrSQLServer = r"Provider=SQLOLEDB.1; Integrated Security=SSPI; Initial Catalog=%s;Data Source=%s" %(_databasename, _computername)
    connStrSQLServer = r"Provider=SQLOLEDB.1; User ID=guest; Password=xxxxx; Initial Catalog=%s;Data Source=%s" %(_databasename, _computername)
    print '    ...Testing MS-SQL login...'
    try:
        s = adodbapi.connect(connStrSQLServer) #connect to server
        s.close()
    except adodbapi.DatabaseError, inst:
        print inst.args[0][2]    # should be the error message
        doSqlServerTest = False

if doMySqlTest:
    _computername='5.128.134.143'
    _databasename='test'
   
    connStrMySql = 'Driver={MySQL ODBC 3.51 Driver};Server=%s;Port=3306;Database=%s;Option=3;' % \
                   (_computername,_databasename)
    print '    ...Testing MySql login...'
    try:
        s = adodbapi.connect(connStrMySql) #connect to server
        s.close()
    except adodbapi.DatabaseError, inst:
        print inst.args[0][2]    # should be the error message
        doMySqlTest = False

if doAS400Test:
    #OLE DB -> "PROVIDER=IBMDA400; DATA SOURCE=MY_SYSTEM_NAME;USER ID=myUserName;PASSWORD=myPwd;DEFAULT COLLECTION=MY_LIBRARY;"
    connStrAS400skl = "Provider=IBMDA400; DATA SOURCE=%s;DEFAULT COLLECTION=%s;User ID=%s;Password=%s"
    # NOTE! user's PC must have OLE support installed in IBM Client Access Express
    _computername='PEPPER'
    _databasename="DPDAN"
    _username = raw_input('  AS400 User ID for data retrieval [%s]:' % defaultUser)
    _password =  getpass.getpass('  AS400 password:') #read the password
    connStrAS400 = connStrAS400skl % (_computername,_databasename,_username,_password) #build the connection string
    print '    ...Testing AS400 login...'
    try:
        s = adodbapi.connect(connStrAS400) #connect to server
        s.close()
    except adodbapi.DatabaseError, inst:
        print inst.args[0][2]    # should be the AS400 error message
        doAS400Test = False
    