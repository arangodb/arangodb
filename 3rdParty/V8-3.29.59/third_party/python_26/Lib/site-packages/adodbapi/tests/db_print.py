""" db_print.py -- a simple demo for ADO database reads."""

import adodbapi
adodbapi.adodbapi.verbose = True # adds details to the sample printout

_computername="franklin" #or name of computer with SQL Server
_databasename="Northwind" #or something else
_username="guest"
_password='xxxxx'

_table_name= 'Products'

# connection string templates from http://www.connectionstrings.com

# Switch test providers by changing the "if False" below

# connection string for an Access data table:
if False:
    _databasename = "C:/adodbapi-2.1/tests/Test.mdb"
    # generic -> 'Provider=Microsoft.Jet.OLEDB.4.0;Data Source=%s; User Id=%s; Password=%s;' % (_databasename, _username, _password)
    constr = 'Provider=Microsoft.Jet.OLEDB.4.0;Data Source=%s' \
             % _databasename
    _table_name= 'Test_tbl'

if False:
    # this will open a MS-SQL table with Windows authentication
    constr = r"Initial Catalog=%s; Data Source=%s; Provider=SQLOLEDB.1; Integrated Security=SSPI" %(_databasename, _computername)

# this set opens a MS-SQL table with SQL authentication
if True:
    constr = r"Provider=SQLOLEDB.1; Initial Catalog=%s; Data Source=%s; user ID=%s; Password=%s; " \
             % (_databasename, _computername, _username, _password)

# connection string for MySQL
if False:
    # this will open a MySQL table (assuming you have the ODBC driver from MySQL.org
    _computername = 'star.vernonscomputershop.com'
    _databasename = 'test'
    constr = 'Driver={MySQL ODBC 3.51 Driver};Server=%s;Port=3306;Database=%s;Option=3;' \
        % (_computername,_databasename)
    _table_name= 'Test_tbl'
    
# connection string for AS400
if False:
    _computername = "PEPPER"
    _databasename = 'CMSDTA00'
    constr = "Provider=IBMDA400; DATA SOURCE=%s;DEFAULT COLLECTION=%s;User ID=%s;Password=%s" \
                      %  (_computername, _databasename, _username, _password)
    # NOTE! user's PC must have OLE support installed in IBM Client Access Express
    _table_name= 'CSPCM'

#tell the server  we are not planning to update...
adodbapi.adodbapi.defaultIsolationLevel = adodbapi.adodbapi.adXactBrowse
#and we want a local cursor
adodbapi.adodbapi.defaultCursorLocation = adodbapi.adodbapi.adUseClient

#create the connection
con = adodbapi.connect(constr)

#make a cursor on the connection
c = con.cursor()

#run an SQL statement on the cursor
sql = 'select * from %s' % _table_name
print 'Executing the command: "%s"' % sql
c.execute(sql)

#check the results
print 'result rowcount shows as= %d. (Note: -1 means "not known")' \
      % (c.rowcount,)
print
print 'result data description is:'
print '            NAME TypeCd DispSize IntrnlSz Prec Scale Null?'
for d in c.description:
    print ('%16s %6d %8d %8d %4d %5d %5d') % d
print
print 'result first ten records are:' 

#get the results
db = c.fetchmany(10)

#print them
for rec in db:
    print repr(rec)

c.close()
con.close()
