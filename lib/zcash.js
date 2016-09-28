require('buffertools')

// Load version from package.json
require('pkginfo')(module, 'version')

exports.logger = require('./utils/logger');

exports.Settings = require('./settings').Settings;
exports.Connection = require('./connection').Connection;
exports.Node = require('./node').Node;
exports.Storage = require('./storage').Storage;
exports.Script = require('./script').Script;
exports.ScriptInterpreter = require('./scriptinterpreter').ScriptInterpreter;
//exports.Util = require('./util');

var txSchema = require('./schema/transaction');
exports.schema = {
  Transaction: txSchema.Transaction,
  TransactionIn: txSchema.TransactionIn,
  TransactionOut: txSchema.TransactionOut,
  Block: require('./schema/block').Block
};

// Multiple instances of bigint are incompatible (instanceof doesn't work etc.),
// so we export our instance so libraries downstream can use the same one.
exports.bignum = exports.bigint = require('bignum')