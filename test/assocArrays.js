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
	// Create the TEST_PKG for the test (first spec then body).

	var spec = '\
	CREATE OR REPLACE PACKAGE "TEST_PKG" IS \
		TYPE Y_STRINGS_TABLE IS TABLE OF NVARCHAR2(4000) INDEX BY PLS_INTEGER; \
		TYPE Y_NUMBERS_TABLE IS TABLE OF NUMBER INDEX BY PLS_INTEGER; \
		PROCEDURE sp_get_numbers(i_arr Y_NUMBERS_TABLE, o_out OUT sys_refcursor); \
		PROCEDURE sp_get_strings(i_arr Y_STRINGS_TABLE, o_out OUT sys_refcursor); \
	END test_pkg;'

	var body = '\
	CREATE OR REPLACE PACKAGE BODY "TEST_PKG" IS \
		PROCEDURE sp_get_numbers(i_arr Y_NUMBERS_TABLE, o_out OUT sys_refcursor) IS \
			vals sys.ODCINumberList := sys.ODCINumberList(); \
		BEGIN \
			FOR i IN i_arr.first..i_arr.last LOOP \
				vals.extend(1); \
				vals(i) := i_arr(i); \
			END LOOP; \
			OPEN o_out FOR SELECT * FROM TABLE(vals); \
			END; \
		PROCEDURE sp_get_strings(i_arr Y_STRINGS_TABLE, o_out OUT sys_refcursor) IS \
			vals sys.ODCIVarchar2List := sys.ODCIVarchar2List(); \
		BEGIN \
			FOR i IN i_arr.first..i_arr.last LOOP \
				vals.extend(1); \
				vals(i) := i_arr(i); \
			END LOOP; \
			OPEN o_out FOR SELECT * FROM TABLE(vals); \
		END; \
	END test_pkg;';

	connection.execute(spec, [], function(err) {
		if (err) throw err;

		connection.execute(body, [], function(err) {
			if (err) throw err;
			cb();
		});
	});
}


exports['AssocArrays'] = nodeunit.testCase({
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

	"AssocArrays - Select using a numbers array": function(test) {
		var out = new oracle.OutParam(oracle.OCCICURSOR);
		var arr = [12.453, -98.31, -5, 5, 3.876e123, -3.876e123];
		this.connection.execute('Begin TEST_PKG.sp_get_numbers(:1, :2); End;', [arr, out], function(err, results) {
			if(err) { console.error(err); return; }
			test.equal(results.returnParam.length, arr.length);

			// Loop thru all the values we passed and check that we got them back.
			// Due to floating point precision (and exponential notation) we are testing
			// that they are "close enough".
			arr.forEach(function(val, i) {
				var res = results.returnParam[i]['COLUMN_VALUE'];
				test.ok(Math.abs(res/val - 1) < 0.001, 'Expected: ' + val + ' Got: ' + res);
			});
			test.done();
		});
	},

	"AssocArrays - Select using too big positive number": function(test) {
		var out = new oracle.OutParam(oracle.OCCICURSOR);
		var arr = [12.453, Number.MAX_VALUE, -5, 5, 0];
		var self = this;
		test.throws(function(){
			self.connection.execute('Begin TEST_PKG.sp_get_numbers(:1, :2); End;', [arr, out], function(err, results) {});
		});
		test.done();
	},

	"AssocArrays - Select using too big negative number": function(test) {
		var out = new oracle.OutParam(oracle.OCCICURSOR);
		var arr = [12.453, -1*Number.MAX_VALUE, -5, 5, 0];
		var self = this;
		test.throws(function(){
			self.connection.execute('Begin TEST_PKG.sp_get_numbers(:1, :2); End;', [arr, out], function(err, results) {});
		});
		test.done();
	},

	"AssocArrays - Select using strings": function(test) {
		var out = new oracle.OutParam(oracle.OCCICURSOR);
		var arr = ['1234567890', 'ThE ', 'quick  ', ' BrOwN  ','fox' ,'Ju m p s', '!!!', 'noo ??', '~!@#$%^&*()_+', ''];
		this.connection.execute('Begin TEST_PKG.sp_get_strings(:1, :2); End;', [arr, out], function(err, results) {
			if(err) { console.error(err); return; }
			test.equal(results.returnParam.length, arr.length);
			arr.forEach(function(val, i) {
				var res = results.returnParam[i]['COLUMN_VALUE'] || '';
				test.equal(res, val);
			});
			test.done();
		});
	},

	"AssocArrays - Select using UTF8 strings": function(test) {
		var out = new oracle.OutParam(oracle.OCCICURSOR);
		var arr = ['тест', ' тест ', '12тест34', 'AB тест'];
		this.connection.execute('Begin TEST_PKG.sp_get_strings(:1, :2); End;', [arr, out], function(err, results) {
			if(err) { console.error(err); return; }
			test.equal(results.returnParam.length, arr.length);
			arr.forEach(function(val, i) {
				var res = results.returnParam[i]['COLUMN_VALUE'] || '';
				test.equal(res, val);
			});
			test.done();
		});
	}
});
