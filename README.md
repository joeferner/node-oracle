# Oracle driver for Node.js

A driver to connect to an Oracle database from node.js, leveraging the "Oracle C++ Call Interface" (OCCI)
for connectivity.  This is most commonly obtained as part of the Oracle Instant Client.

It is known to work with Oracle 10, 11, and 12, and has been mostly tested on Linux, but should also work on OS X and
Windows 7+


# Basic installation

(See INSTALL.md for complete instructions for your platform.)

* Download the latest Oracle Instant Client Basic and SDK, and extract to the same directory.
* Set environment variables:

	```
OCI_LIB_DIR=/path/to/instant_client
OCI_INCLUDE_DIR=/path/to/instant_client/sdk/include
OCI_VERSION=<10, 11, or 12> # Integer. Optional, defaults to '11'
NLS_LANG=.UTF8 # Optional, but required to support international characters
	```
* Create symlinks for libclntsh and libocci in the Instant Client directory (see INSTALL.md)
* (Linux) Install libaio
* Configure the dynamic library path on your platform to include $OCI_LIB_DIR (see INSTALL.md)
* `npm install oracle` to get the latest from npmjs.org


# Examples

### Basic example

```javascript
var oracle = require('oracle');

var connectData = {
	hostname: "localhost",
	port: 1521,
	database: "xe", // System ID (SID)
	user: "oracle",
	password: "oracle"
}

oracle.connect(connectData, function(err, connection) {
	if (err) { console.log("Error connecting to db:", err); return; }

	connection.execute("SELECT systimestamp FROM dual", [], function(err, results) {
		if (err) { console.log("Error executing query:", err); return; }

		console.log(results);
		connection.close(); // call only when query is finished executing
	});
});
```

### Alternative connection using TNS
Replace the `connectData` object above with one of the following.

Without tnsnames.ora file:

```javascript
var connString = "(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=localhost)(PORT=1521))(CONNECT_DATA=(SERVER=DEDICATED)(SERVICE_NAME=xe)))";
var connectData = { "tns": connString, "user": "test", "password": "test" };
```
With tnsnames.ora file:

```text
DEV =
	(DESCRIPTION =
		(ADDRESS = (PROTOCOL = TCP)(HOST = localhost)(PORT = 1521))
		(CONNECT_DATA =
			(SERVER = DEDICATED)
			(SERVICE_NAME = orcl)
		)
	)
```

```javascript
var connectData = { "tns": "DEV", "user": "test", "password": "test" };
```

### Connection options

The following options can be set on the connection:

* `connection.setAutoCommit(true/false);`
* `connection.setPrefetchRowCount(count);` Should improve performance with large result sets

### Out Params
Following the basic example above, a query using a return parameter looks like this:

```javascript
	...
	connection.execute(
		"INSERT INTO person (name) VALUES (:1) RETURNING id INTO :2",
		['joe ferner', new oracle.OutParam()],
		function(err, results) {
			if ( err ) { ... } 
			// results.updateCount = 1
			// results.returnParam = the id of the person just inserted
			connection.close();
		}
	);
	...
```

The following OUT Params are supported in Stored Procedures:

* `OCCIINT`
* `OCCISTRING`
* `OCCIDOUBLE`
* `OCCIFLOAT`
* `OCCICURSOR`
* `OCCICLOB`
* `OCCIDATE`
* `OCCITIMESTAMP`
* `OCCINUMBER`
* `OCCIBLOB`

Specify the return type in the OutParam() constructor:

```javascript
connection.execute("call myProc(:1,:2)", ["nodejs", new oracle.OutParam(oracle.OCCISTRING)], ...
```

When using `OCCISTRING`, the size can optionally be specified (default is 200 chars):

```javascript
connection.execute("call myProc(:1,:2)", ["nodejs", new oracle.OutParam(oracle.OCCISTRING, {size: 1000})], ...
```
See tests for more examples.

### In/Out Params
The following INOUT param types are supported:

* `OCCIINT`
* `OCCISTRING`
* `OCCIDOUBLE`
* `OCCIFLOAT`
* `OCCINUMBER`

INOUT params are used like normal OUT params, with the optional 'in' paramater value being passed in the options object:

```javascript
connection.execute("call myProc(:1)", [new oracle.OutParam(oracle.OCCIINT, {in: 42})], ...
```

### Validate connection
To validate whether the connection is still established after some time:

```javascript
if (!connection.isConnected()) {
	// Do something like retire this connection from a pool
}
```

### Dates
For DATE and TIMESTAMP types, the driver uses the UTC methods from the Javascript Date object. This means the DATE
value stored will match the value of `new Date().toISOString()` on your client machine.  Consider this example
for a client machine in "GMT-0700":

Table schema:

```sql
CREATE TABLE date_test (mydate DATE)
```

Javascript code:

```javascript
...
	var date = new Date(2013, 11, 24, 18, 0, 1);  // Client timezone dependent
	console.log(date.toString());      // Tue Dec 24 2013 18:00:01 GMT-0700 (MST)
	console.log(date.toISOString());   // 2013-12-25T01:00:01.000Z

	connection.execute(
		"INSERT INTO date_test (mydate) VALUES (:1) " +
			"RETURNING mydate, to_char(mydate, 'YYYY-MM-DD HH24:MI:SS') INTO :2, :3",
		[date, new oracle.OutParam(oracle.OCCIDATE), new oracle.OutParam(oracle.OCCISTRING)],
		function(err, results) {
			console.log(results.returnParam.toString());  // Tue Dec 24 2013 18:00:01 GMT-0700 (MST)
			console.log(results.returnParam1);            // 2013-12-25 01:00:01
		}
	);
...
```

# Limitations/Caveats

* Currently no native support for connection pooling (forthcoming; use generic-pool for now.)
* Currently no support for column type "Timestamp With Timezone" (Issue #67)
* While the Oracle TIMESTAMP type provides fractional seconds up to 9 digits (nanoseconds), this will be rounded
  to the nearest millisecond when converted to a Javascript date (a _data loss_).

# Development
* Clone the source repo
* Follow the installation instructions to prepare your environment (using Oracle Instant Client)
* Run `npm install` or `npm test` in the root of the source directory
* Point to an Oracle instance of your choice.  The free Oracle Express edition works well:
  * Oracle Express 11g: http://www.oracle.com/technetwork/database/database-technologies/express-edition/downloads/index.htmlDownload
* Debugging:
  * Compile node with debug symbols
  * Use gdb/ddd or another C++ debugger to step through
