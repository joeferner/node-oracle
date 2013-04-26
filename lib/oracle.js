
var bindings = require("../build/Release/oracle_bindings");
var oracle = new bindings.OracleClient();

exports.connect = function(settings, callback) {
  oracle.connect(settings, callback);
}

exports.OutParam = bindings.OutParam;

exports.OCCIINT = 0;
exports.OCCISTRING = 1;
exports.OCCIDOUBLE = 2;
exports.OCCIFLOAT = 3;
exports.OCCICURSOR = 4;
exports.OCCICLOB = 5;