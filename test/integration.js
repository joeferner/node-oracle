
/*
 tests-settings.json:
   {
    "hostname": "localhost",
    "user": "test",
    "password": "test"
   }

 Database:
   CREATE TABLE person (id INTEGER PRIMARY KEY, name VARCHAR(255));
   CREATE SEQUENCE person_seq START WITH 1 INCREMENT BY 1 NOMAXVALUE;
   CREATE TRIGGER person_pk_trigger BEFORE INSERT ON person FOR EACH row
     BEGIN
       SELECT person_seq.nextval INTO :new.id FROM dual;
     END;
   /
   CREATE TABLE datatype_test (
    id INTEGER PRIMARY KEY,
    tvarchar2 VARCHAR2(255),
    tnvarchar2 NVARCHAR2(255),
    tchar CHAR(255),
    tnchar NCHAR(255),
    tnumber NUMBER(10,5),
    tdate DATE,
    ttimestamp TIMESTAMP,
    tclob CLOB,
    tnclob NCLOB,
    tblob BLOB);
   CREATE SEQUENCE datatype_test_seq START WITH 1 INCREMENT BY 1 NOMAXVALUE;
   CREATE TRIGGER datatype_test_pk_trigger BEFORE INSERT ON datatype_test FOR EACH row
     BEGIN
       SELECT datatype_test_seq.nextval INTO :new.id FROM dual;
     END;
   /
*/

var nodeunit = require("nodeunit");
var oracle = require("../");

var settings = JSON.parse(require('fs').readFileSync('./tests-settings.json','utf8'));

exports['IntegrationTest'] = nodeunit.testCase({
  setUp: function(callback) {
    var self = this;
    //console.log("connecting: ", settings);
    oracle.connect(settings, function(err, connection) {
      if(err) { callback(err); return; }
      //console.log("connected");
      self.connection = connection;
      self.connection.execute("DELETE FROM person", [], function(err, results) {
        if(err) { callback(err); return; }
        self.connection.execute("DELETE FROM datatype_test", [], function(err, results) {
          if(err) { callback(err); return; }
          //console.log("rows deleted: ", results);
          callback();
        });
      });
    });
  },

  tearDown: function(callback) {
    if(this.connection) {
      this.connection.close();
    }
    callback();
  },

  "select with single quote": function(test) {
    var self = this;
    self.connection.execute("INSERT INTO person (name) VALUES (:1)", ["Bill O'Neil"], function(err, results) {
      if(err) { console.error(err); return; }
      self.connection.execute("INSERT INTO person (name) VALUES (:1)", ["Bob Johnson"], function(err, results) {
        if(err) { console.error(err); return; }
        self.connection.execute("SELECT * FROM person WHERE name = :1", ["Bill O'Neil"], function(err, results) {
          if(err) { console.error(err); return; }
          //console.log(results);
          test.equal(results.length, 1);
          test.equal(results[0]['NAME'], "Bill O'Neil");
          test.done();
        });
      });
    });
  },

  "insert with returning value": function(test) {
    var self = this;
    self.connection.execute("INSERT INTO person (name) VALUES (:1) RETURNING id INTO :2", ["Bill O'Neil", new oracle.OutParam()], function(err, results) {
      if(err) { console.error(err); return; }
      test.ok(results.returnParam > 0);
      test.done();
    });
  },

  "datatypes": function(test) {
    var self = this;
    var date1 = new Date(2011, 10, 30, 1, 2, 3);
    var date2 = new Date(2011, 11, 1, 1, 2, 3);
    self.connection.execute(
      "INSERT INTO datatype_test "
        + "(tvarchar2, tnvarchar2, tchar, tnchar, tnumber, tdate, ttimestamp, tclob, tnclob, tblob) VALUES "
        + "(:1, :2, :3, :4, :5, :6, :7, :8, :9, :10) RETURNING id INTO :11",
      [
        "tvarchar2 value",
        "tnvarchar2 value",
        "tchar value",
        "tnchar value",
        42.5,
        date1,
        date2,
        "tclob value",
        "tnclob value",
        null, //new Buffer("tblob value"),
        new oracle.OutParam()
      ],
      function(err, results) {
        if(err) { console.error(err); return; }
        test.ok(results.returnParam > 0);

        self.connection.execute("SELECT * FROM datatype_test", [], function(err, results) {
          if(err) { console.error(err); return; }
          test.equal(results.length, 1);
          test.equal(results[0]['TVARCHAR2'], "tvarchar2 value");
          test.equal(results[0]['TNVARCHAR2'], "tnvarchar2 value");
          test.equal(results[0]['TCHAR'], "tchar value                                                                                                                                                                                                                                                    ");
          test.equal(results[0]['TNCHAR'], "tnchar value                                                                                                                                                                                                                                                   ");
          test.equal(results[0]['TNUMBER'], 42.5);
          test.equal(results[0]['TDATE'].getTime(), date1.getTime());
          var date2Timestamp = new Date(2011, 11, 1, 0, 0, 0); // same as date2 but without time
          test.equal(results[0]['TTIMESTAMP'].getTime(), date2Timestamp.getTime());
          test.equal(results[0]['TCLOB'], "tclob value");
          test.equal(results[0]['TNCLOB'], "tnclob value");
          // todo: test.equal(results[0]['TBLOB'], null);
          test.done();
        });
      });
  },

  "datatypes null": function(test) {
    var self = this;
    self.connection.execute(
      "INSERT INTO datatype_test "
        + "(tvarchar2, tnvarchar2, tchar, tnchar, tnumber, tdate, ttimestamp, tclob, tnclob, tblob) VALUES "
        + "(:1, :2, :3, :4, :5, :6, :7, :8, :9, :10) RETURNING id INTO :11",
      [
        null,
        null,
        null,
        null,
        null,
        null,
        null,
        null,
        null,
        null,
        new oracle.OutParam()
      ],
      function(err, results) {
        if(err) { console.error(err); return; }
        test.ok(results.returnParam > 0);

        self.connection.execute("SELECT * FROM datatype_test", [], function(err, results) {
          if(err) { console.error(err); return; }
          test.equal(results.length, 1);
          test.equal(results[0]['TVARCHAR2'], null);
          test.equal(results[0]['TNVARCHAR2'], null);
          test.equal(results[0]['TCHAR'], null);
          test.equal(results[0]['TNCHAR'], null);
          test.equal(results[0]['TNUMBER'], null);
          test.equal(results[0]['TDATE'], null);
          test.equal(results[0]['TTIMESTAMP'], null);
          test.equal(results[0]['TCLOB'], null);
          test.equal(results[0]['TNCLOB'], null);
          test.equal(results[0]['TBLOB'], null);
          test.done();
        });
      });
  } 
});
