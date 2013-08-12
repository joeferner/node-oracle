
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
