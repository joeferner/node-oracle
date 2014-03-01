
var bindings = require("../build/Release/oracle_bindings");
var oracle = new bindings.OracleClient();

function getSettings(settings) {
  settings = settings || {
      hostname: '127.0.0.1',
      database: 'XE'
  };
  settings.hostname = settings.hostname || settings.host;
  settings.user = settings.user || settings.username;
  return settings;
}

exports.connect = function(settings, callback) {
  settings = getSettings(settings);
  oracle.connect(settings, callback);
}

exports.connectSync = function(settings) {
  settings = getSettings(settings);
  return oracle.connectSync(settings);
}

exports.OutParam = bindings.OutParam;

exports.OCCIINT = 0;
exports.OCCISTRING = 1;
exports.OCCIDOUBLE = 2;
exports.OCCIFLOAT = 3;
exports.OCCICURSOR = 4;
exports.OCCICLOB = 5;
exports.OCCIDATE = 6;
exports.OCCITIMESTAMP = 7;
exports.OCCINUMBER = 8;
exports.OCCIBLOB = 9;

// Reader is implemented in JS around a C++ handle
// This is easier and also more efficient because we don't cross the JS/C++ boundary 
// every time we read a record.
function Reader(handle) {
  this._handle = handle;
  this._error = null;
  this._rows = [];
}

Reader.prototype.nextRows = function() {
  this._handle.nextRows.apply(this._handle, arguments);
}

Reader.prototype.nextRow = function(cb) {
  var self = this;
  if (!self._handle || self._error || (self._rows && self._rows.length > 0)) {
    process.nextTick(function() {
      cb(self._error, self._rows && self._rows.shift());
    });
  } else {
    // nextRows willl use the prefetch row count as window size
    self._handle.nextRows(function(err, result) {
      self._error = err || self._error;
      self._rows = result;
      if (err || result.length === 0) self._handle = null;
      cb(self._error, self._rows && self._rows.shift());
    });
  }
};

bindings.Connection.prototype.reader = function(sql, args) {
  return new Reader(this.readerHandle(sql, args));
}