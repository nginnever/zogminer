'use strict'

const rpc = require('jsonrpc2')
const logger = require('./logger')

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
  console.log(node)
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

    //this.exposeMethods();
    //this.startServer();
  }
} 
