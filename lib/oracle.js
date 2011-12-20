
var bindings = require("../build/Release/oracle_bindings");
var oracle = new bindings.OracleClient();

exports.connect = function(settings, callback) {
  oracle.connect(settings, callback);
}

exports.OutParam = bindings.OutParam;