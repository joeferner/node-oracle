# Install
 * Install the Oracle Instant Client:
  * Download the [Instant Client Package](http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html): both "Basic Lite" and "SDK".
  * Extract both the Basic Lite and the SDK zip files to `/opt/instantclient/` (Linux/OS X) or `C:\oracle\instantclient\` (Windows)
   * or put them wherever you want and set the environment variables `OCI_INCLUDE_DIR` (to the sdk/include) and `OCI_LIB_DIR` (to the directory containing libocci.so.11.1 on Linux/OS X, or oraocci11.lib on Windows).
  * Linux: add the shared object files to the ld cache:
 
  	```bash
	# Replace /opt/instantclient/ with wherever you extracted the Basic Lite files to
	echo '/opt/instantclient/' | sudo tee -a /etc/ld.so.conf.d/moo.conf
	sudo ldconfig
	```
	* Create the symbolic links:
	
	```bash
		cd /opt/instantclient/
		sudo ln -s libclntsh.so.11.1 libclntsh.so 
		sudo ln -s libocci.so.11.1 libocci.so 
	```
 * Finally install using Node Package Manager (npm):

    `npm install oracle`

# Examples

The simplest way to connect to the database uses the following code:

```javascript
var oracle = require("oracle");

var connectData = { "hostname": "localhost", "user": "test", "password": "test", "database": "ORCL" };

oracle.connect(connectData, function(err, connection) {
    ...
    connection.close(); // call this when you are done with the connection
  });
});
```
The `database` parameter contains the "service name" or "SID" of the database. If you have an Oracle RAC or some other kind of setup, or if you prefer to use "TNS", you can do so as well. You can either provide a complete connection string, like:

```javascript
var connString = 
      "(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=localhost)(PORT=1521))(CONNECT_DATA=(SERVER=DEDICATED)(SERVICE_NAME=orcl)))";
```

or just the shortcut as declared in your `tnsnames.ora`:

    DEV =
      (DESCRIPTION =
        (ADDRESS = (PROTOCOL = TCP)(HOST = localhost)(PORT = 1521))
        (CONNECT_DATA =
          (SERVER = DEDICATED)
          (SERVICE_NAME = orcl)
        )
      )

The connection parameter would then be:

```javascript
var connectData = { "tns": "DEV", "user": "test", "password": "test" };
// or: var connectData = { "tns": connString, "user": "test", "password": "test" };
```

To access a table you could then use:

```javascript
oracle.connect(connData, function(err, connection) {

  connection.setAutoCommit(true);

  // selecting rows
  connection.execute("SELECT * FROM person", [], function(err, results) {
    if ( err ) {
      console.log(err);
    } else {
      console.log(results);

      // inserting with return value
      connection.execute(
        "INSERT INTO person (name) VALUES (:1) RETURNING id INTO :2",
        ['joe ferner', new oracle.OutParam()],
        function(err, results) {
          if ( err ) { console.log(err) } else {
            console.log(results);
          }
          // results.updateCount = 1
          // results.returnParam = the id of the person just inserted
          connection.close(); // call this when you are done with the connection
        }
      );
    }
  
  });
});
```



## Out Params

The following Out Params are supported in Stored Procedures:

```

OCCIINT
OCCISTRING
OCCIDOUBLE
OCCIFLOAT
OCCICURSOR
OCCICLOB
OCCIDATE
OCCITIMESTAMP
OCCINUMBER
OCCIBLOB

```

And can be used as follows:

```

connection.execute("call myProc(:1,:2)", ["nodejs", new oracle.OutParam(oracle.OCCISTRING)], function(err, results){
  console.dir(results);
};

```

When using Strings as Out Params, the size can be optionally specified as follows:

```

connection.execute("call myProc(:1,:2)", ["nodejs", new oracle.OutParam(oracle.OCCISTRING, {size: 1000})], function(err, results){

```

If no size is specified, a default size of 200 chars is used.

See tests for more examples.

## In/Out Params

The following INOUT param types are supported:

```

OCCIINT
OCCISTRING
OCCIDOUBLE
OCCIFLOAT
OCCINUMBER

```

INOUT params are used like normal OUT prams, with the optional 'in' paramater value being passed in the options object:

```

connection.execute("call myProc(:1)", [new oracle.OutParam(oracle.OCCIINT, {in: 42})], function(err, results){
  console.dir(results);
};

```


# Develop

## Install Oracle/Oracle Express

 * Download [Oracle Express 10g](http://www.oracle.com/technetwork/database/express-edition/database10gxe-459378.html)
 * Download [Instant Client](http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html)
  * Instant Client Package - Basic Lite
  * Instant Client Package - SQL*Plus
  * Instant Client Package - SDK
 * Install Oracle Express (Ubuntu)

```bash
sudo dpkg -i oracle-xe_11.2.0.3.0-1.0_i386.deb
sudo apt-get install alien
sudo alien oracle-instantclient11.2-*
sudo dpkg -i oracle-instantclient11.2-*.deb
sudo /etc/init.d/oracle-xe configure
```

 * Open http://localhost:9999/apex/ change 9999 to the port you configured. Log-in with "sys" and the password.
 * Create a user called "test" with password "test" and give all accesses.

```bash
sudo vi /etc/ld.so.conf.d/oracle.conf -- add this line /usr/lib/oracle/11.2/client/lib/
sudo ldconfig

export ORACLE_SID=test
export ORACLE_HOME=/usr/lib/oracle/xe/app/oracle/product/11.2/server
export OCI_INCLUDE_DIR=/usr/include/oracle/11.2/client/
export OCI_LIB_DIR=/usr/lib/oracle/11.2/client/lib/
sqlplus test@XE
```

## Build

```bash
npm install
npm test
```
