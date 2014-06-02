!CHAPTER Accessing the Web Interface

ArangoDB comes with a built-in web interface for administration. The web 
interface can be accessed via the URL

    http://localhost:8529

assuming you are using the standard port and no user routings. If you
have any application installed, the home page might point to that
application instead. In this case use

  http://localhost:8529/_admin/aardvark/index.html

(note: _aardvark_ is the web interface's internal name).

If no database name is specified in the URL, you will in most cases get
routed to the web interface for the `_system` database. To access the web 
interface for any other ArangoDB database, put the database name into the
request URI path as follows:
  
  http://localhost:8529/_db/mydb/

The above will load the web interface for the database `mydb`.

To restrict access to the web interface, use 
[ArangoDB's authentication feature](../GeneralHttp/README.html#Authentication).