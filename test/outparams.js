
/*
 tests-settings.json:
   {
    "hostname": "localhost",
    "user": "test",
    "password": "test"
   }

 Database:
  CREATE OR REPLACE PROCEDURE procNumericOutParam(param1 IN VARCHAR2, outParam1 OUT NUMBER)
  IS
  BEGIN
    DBMS_OUTPUT.PUT_LINE('Hello '|| param1);
    outParam1 := 42;
  END;
  /
  CREATE OR REPLACE PROCEDURE procStringOutParam(param1 IN VARCHAR2, outParam1 OUT STRING)
  IS
  BEGIN
    DBMS_OUTPUT.PUT_LINE('Hello '|| param1);
    outParam1 := 'Hello ' || param1;
  END;
  /
  CREATE OR REPLACE PROCEDURE procVarChar2OutParam(param1 IN VARCHAR2, outParam1 OUT VARCHAR2)
  IS
  BEGIN
    DBMS_OUTPUT.PUT_LINE('Hello '|| param1);
    outParam1 := 'Hello ' || param1;
  END;
  /
  CREATE OR REPLACE PROCEDURE procDoubleOutParam(param1 IN VARCHAR2, outParam1 OUT DOUBLE PRECISION)
  IS
  BEGIN
    outParam1 := -43.123456789012;
  END;
  /
  CREATE OR REPLACE PROCEDURE procFloatOutParam(param1 IN VARCHAR2, outParam1 OUT FLOAT)
  IS
  BEGIN
    outParam1 := 43;
  END;
  /

  CREATE OR REPLACE PROCEDURE procTwoOutParams(param1 IN VARCHAR2, outParam1 OUT NUMBER, outParam2 OUT STRING)
  IS
  BEGIN
    outParam1 := 42;
    outParam2 := 'Hello ' || param1;
  END;
  /
  CREATE OR REPLACE PROCEDURE procCursorOutParam(outParam OUT SYS_REFCURSOR)
  IS
  BEGIN
    open outParam for
    select * from person;
  END;
  /
  CREATE OR REPLACE PROCEDURE procCLOBOutParam(outParam OUT CLOB)
  IS
  BEGIN
    outParam := 'IAMCLOB';
  END;
  /

  BEGIN
     EXECUTE IMMEDIATE 'DROP TABLE basic_lob_table';
  EXCEPTION
     WHEN OTHERS THEN
        IF SQLCODE != -942 THEN
           RAISE;
        END IF;
  END;
  /

  create table basic_lob_table (x varchar2 (30), b blob, c clob);
  insert into basic_lob_table values('one', '010101010101010101010101010101', 'onetwothreefour');
  select * from basic_lob_table where x='one' and ROWNUM = 1;

  CREATE OR REPLACE PROCEDURE ReadBasicBLOB (outBlob OUT BLOB)
  IS
  BEGIN
      SELECT b INTO outBlob FROM basic_lob_table where x='one' and ROWNUM = 1;
  END;
  /

  CREATE OR REPLACE procedure doSquareInteger(z IN OUT Integer)
  is
  begin
    z := z * z;
  end;
  /
*/

var nodeunit = require("nodeunit");
var oracle = require("../");

var settings = JSON.parse(require('fs').readFileSync('./tests-settings.json','utf8'));

exports['OutParamsTest'] = nodeunit.testCase({
  setUp: function(callback) {
    var self = this;
    oracle.connect(settings, function(err, connection) {
      if(err) { callback(err); return; }
      self.connection = connection;
      callback();
    });
  },

  tearDown: function(callback) {
    if(this.connection) {
      this.connection.close();
    }
    callback();
  },

  "stored procedures - multiple out params": function(test){
    var self = this;

    self.connection.execute("call procTwoOutParams(:1,:2,:3)", ["node", new oracle.OutParam(oracle.OCCIINT), new oracle.OutParam(oracle.OCCISTRING)], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, 42);
      test.equal(results.returnParam1, "Hello node");
      test.done();
    });
  },

  "stored procedures - numeric out param - occiint": function(test){
    var self = this;
    self.connection.execute("call procNumericOutParam(:1,:2)", ["node", new oracle.OutParam()], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, 42);
      test.done();
    });
  },

  "stored procedures - numeric out param - occinumber": function(test){
    var self = this;
    self.connection.execute("call procNumericOutParam(:1,:2)", ["node", new oracle.OutParam(oracle.OCCINUMBER)], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, 42);
      test.done();
    });
  },

  "stored procedures - string out param": function(test){
    var self = this;
    self.connection.execute("call procStringOutParam(:1,:2)", ["node", new oracle.OutParam(oracle.OCCISTRING)], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, "Hello node");
      test.done();
    });
  },

  "stored procedures - varchar2 out param": function(test){
    var self = this;
    self.connection.execute("call procVarChar2OutParam(:1,:2)", ["node", new oracle.OutParam(oracle.OCCISTRING, {size: 40})], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, "Hello node");
      test.done();
    });
  },


  "stored procedures - double out param": function(test){
    var self = this;
    self.connection.execute("call procDoubleOutParam(:1,:2)", ["node", new oracle.OutParam(oracle.OCCIDOUBLE)], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, -43.123456789012);
      test.done();
    });
  },

  "stored procedures - float out param": function(test){
    var self = this;
    self.connection.execute("call procFloatOutParam(:1,:2)", ["node", new oracle.OutParam(oracle.OCCIFLOAT)], function(err, results){
      if(err) { console.error(err); return; }
      // purposely commented, gotta love floats in javasctipt: http://stackoverflow.com/questions/588004/is-javascripts-floating-point-math-broken
      // test.equal(results.returnParam, 43);

      test.done();
    });
  },

  "stored procedures - cursor out param": function(test){
    var self = this;
    self.connection.execute("call procCursorOutParam(:1)", [new oracle.OutParam(oracle.OCCICURSOR)], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam.length, 0);
      test.done();
    });
  },

  "stored procedures - clob out param": function(test){
    var self = this;
    self.connection.execute("call procCLOBOutParam(:1)", [new oracle.OutParam(oracle.OCCICLOB)], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, "IAMCLOB");
      test.done();
    });
  },

  "stored procedures - date timestamp out param": function(test){
    var self = this;
    self.connection.execute("call procDateTimeOutParam(:1, :2)", [new oracle.OutParam(oracle.OCCIDATE), new oracle.OutParam(oracle.OCCITIMESTAMP)], function(err, results){
      if(err) { console.error(err); return; }
      var d = new Date();
      test.equal(results.returnParam.getFullYear(), d.getFullYear());
      test.done();
    });
  },

  "stored procedures - blob out param": function(test){
    var self = this;
    self.connection.execute("call ReadBasicBLOB(:1)", [new oracle.OutParam(oracle.OCCIBLOB)], function(err, results){
      if(err) { console.error(err); return; }
      test.done();
    });
  },

  "stored procedures - in out params": function(test){
    var self = this;
    self.connection.execute("call doSquareInteger(:1)", [new oracle.OutParam(oracle.OCCIINT, {in: 5})], function(err, results){
      if(err) { console.error(err); return; }
      test.equal(results.returnParam, 25);
      test.done();
    });
  }

});
