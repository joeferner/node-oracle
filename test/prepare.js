/*
 tests-settings.json:
   {
    "hostname": "localhost",
    "user": "test",
    "password": "test"
   }
*/

var nodeunit = require("nodeunit");
var oracle = require("../");

var settings;

try {
  settings = JSON.parse(require('fs').readFileSync('./tests-settings-custom.json', 'utf8'));
} catch (ex) {}
settings = settings || JSON.parse(require('fs').readFileSync('./tests-settings.json', 'utf8'));

function initDb(connection, cb) {
  connection.setPrefetchRowCount(50);
  connection.execute("DROP TABLE test_table", [], function(err) {
    // ignore error
    connection.execute("CREATE TABLE test_table (X INT)", [], function(err) {
      if (err) throw err;
      return cb();
    });
  });
}

function doInsert(stmt, i, max, cb) {
  if (i < max) {
    stmt.execute([i], function(err, result) {
      if (err) return cb(err);
      if (result.updateCount !== 1) return cb(new Error("bad count: " + result.updateCount));
      doInsert(stmt, i + 1, max, cb);
    });
  } else {
    return cb();
  }
}

exports['reader'] = nodeunit.testCase({
  setUp: function(callback) {
    var self = this;
    oracle.connect(settings, function(err, connection) {
      if (err) return callback(err);
      self.connection = connection;
      initDb(self.connection, callback);
    });
  },

  tearDown: function(callback) {
    if (this.connection) {
      this.connection.close();
    }
    callback();
  },

  "prepared insert": function(test) {
    var self = this;
    var stmt = self.connection.prepare("INSERT INTO test_table (X) values (:1)");
    doInsert(stmt, 0, 100, function(err) {
      test.equal(err, null);
      self.connection.execute("SELECT COUNT(*) from test_table", [], function(err, result) {
        test.equal(err, null);
        test.ok(Array.isArray(result));
        test.equal(result.length, 1);
        test.equal(result[0]['COUNT(*)'], 100);
        test.done();
      });
    });
  },
});