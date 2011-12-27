# Install

 * Install [Instant Client Package - Basic Lite](http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html)
 * Install [Instant Client Package - SDK](http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html)
 * Finally install using Node Package Manager (npm):

    npm install oracle

# Example

```javascript
var oracle = require("oracle");

oracle.connect({ "hostname": "localhost", "user": "test", "password": "test" }, function(err, connection) {
  // selecting rows
  connection.execute("SELECT * FROM person WHERE name = :1", ['bob smith'], function(err, results) {
    // results will be an array of objects
  });

  // inserting with return value
  connection.execute(
    "INSERT INTO person (name) VALUES (:1) RETURNING id INTO :2",
    ['joe ferner', new oracle.OutParam()],
    function(err, results) {
      // results.updateCount = 1
      // results.returnParam = the id of the person just inserted
    });

  connection.setAutoCommit(true);

  connection.commit(function(err) {
    // transaction committed
  });

  connection.rollback(function(err) {
    // transaction rolledback
  });

  connection.close(); // call this when you are done with the connection
});
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
node-waf configure
node-waf build
nodeunit tests/*
```
