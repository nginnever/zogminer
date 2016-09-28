'use strict'

const rpc = require('jsonrpc2')
const logger = require('../utils/logger')

// logger.log('info', 'This is an information message.')
//log settings

// Disable rpc library logging
rpc.Endpoint.trace = function () {
  var args = [].slice.apply(arguments)
  if (args[0] == "***") {
    args = args.slice(1)
  }
  logger.rpcdbg(args.join(" "))
}

var JsonRpcServer = exports.JsonRpcServer = function JsonRpcServer(node)
{
	this.node = node
  //logger.log('error', 'This is an information message.');
}

JsonRpcServer.prototype.enable = function () {
  if (this.node.cfg.jsonrpc.enable) {
    if (this.node.cfg.jsonrpc.password == null) {
      logger.log('error', 'JsonRpcServer(): You must set a JSON-RPC password in the settings.')
      return
    }
    this.rpc = new rpc.Server()
    this.rpc.defaultScope = this
    this.rpc.enableAuth(this.node.cfg.jsonrpc.username, this.node.cfg.jsonrpc.password)

    this.exposeMethods();
    this.startServer();
  }
}


JsonRpcServer.prototype.exposeMethods = function ()
{
  var self = this;
  var modules = ["info"];

  modules.forEach(function (name) {
    try {
      var module = require('./'+name)

      Object.keys(module).forEach(function (key) {
        self.rpc.expose(key, module[key])
      })
    } catch (e) {
      logger.error('Error loading RPC module "'+name+'":\n' +
                   (e.stack ? e.stack : e.toString()))
    }
  })
}

JsonRpcServer.prototype.startServer = function ()
{
  logger.info('Listening for JSON-RPC connections on '+
              this.node.cfg.jsonrpc.host+':'+
              this.node.cfg.jsonrpc.port);

  try {
    this.rpc.listenHybrid(this.node.cfg.jsonrpc.port, this.node.cfg.jsonrpc.host);
  } catch (e) {
    logger.warn("Could not start RPC server");
    logger.warn("Reason: "+e.message);
  }
}
