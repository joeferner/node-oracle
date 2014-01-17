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

function initDb(connection, max, cb) {
  connection.setPrefetchRowCount(50);
  connection.execute("DROP TABLE test_table", [], function(err) {
    // ignore error
    connection.execute("CREATE TABLE test_table (X INT)", [], function(err) {
      if (err) throw err;

      function insert(i, cb) {
        connection.execute("INSERT INTO test_table (X) VALUES (:1)", [i], function(err) {
          if (err) throw err;
          if (i < max) insert(i + 1, cb);
          else cb();
        });
      }
      insert(1, cb);
    });
  });
}

function doRead(reader, fn, count, cb) {
  if (count === 0) return cb();
  reader.nextRow(function(err, row) {
    if (err) return cb(err);
    if (row) {
      fn(row);
      return doRead(reader, fn, count - 1, cb)
    } else {
      return cb();
    }
  });
}

function testNextRow(test, connection, prefetch, requested, expected) {
  connection.setPrefetchRowCount(prefetch);
  var reader = connection.reader("SELECT X FROM test_table ORDER BY X", []);
  var total = 0;
  doRead(reader, function(row) {
    total += row.X;
  }, requested, function(err) {
    if (err) return console.error(err);
    test.equal(total, expected * (expected + 1) / 2);
    test.done();
  });
}

function testNextRows(test, connection, requested, len1, len2) {
  var reader = connection.reader("SELECT X FROM test_table ORDER BY X", []);
  reader.nextRows(requested, function(err, rows) {
    if (err) return console.error(err);
    test.equal(rows.length, len1);
    reader.nextRows(requested, function(err, rows) {
      if (err) return console.error(err);
      test.equal(rows.length, len2);
      test.done();
    });
  });
}

exports['reader'] = nodeunit.testCase({
  setUp: function(callback) {
    var self = this;
    oracle.connect(settings, function(err, connection) {
      if (err) return callback(err);
      self.connection = connection;
      initDb(self.connection, 100, callback);
    });
  },

  tearDown: function(callback) {
    if (this.connection) {
      this.connection.close();
    }
    callback();
  },

  "reader.nextRow - request 20 with prefetch of 5": function(test) {
    testNextRow(test, this.connection, 5, 20, 20);
  },

  "reader.nextRow - request 20 with prefetch of 200": function(test) {
    testNextRow(test, this.connection, 200, 20, 20);
  },

  "reader.nextRow - request 200 with prefetch of 5": function(test) {
    testNextRow(test, this.connection, 5, 200, 100);
  },

  "reader.nextRow - request 200 with prefetch of 200": function(test) {
    testNextRow(test, this.connection, 200, 200, 100);
  },

  "reader.nextRows - request 20": function(test) {
    testNextRows(test, this.connection, 20, 20, 20);
  },

  "reader.nextRows - request 70": function(test) {
    testNextRows(test, this.connection, 70, 70, 30);
  },

  "reader.nextRows - request 200": function(test) {
    testNextRows(test, this.connection, 200, 100, 0);
  },

});