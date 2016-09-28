'use strict'
//const rpc = require('jsonrpc2')
//const rpc = require('node-json-rpc')
const zcash = require('bitcoin')
const logger = require('../utils/logger')

/*// Disable rpc library logging
rpc.Endpoint.trace = function () {
  var args = [].slice.apply(arguments)
  if (args[0] == "***") {
    args = args.slice(1)
  }
  logger.rpcdbg(args.join(" "))
}*/

var JsonRpcClient = exports.JsonRpcClient= function JsonRpcClient(node)
{
	this.node = node
  //logger.log('error', 'This is an information message.');
}

JsonRpcClient.prototype.enable = function () {
	//this.rpc = new rpc.Client(this.node.cfg.jsonrpc.port, this.node.cfg.jsonrpc.host)
	//this.exposeMethods();
	//this.call()
/*	var options = {
	  // int port of rpc server, default 5080 for http or 5433 for https 
	  port: this.node.cfg.jsonrpc.port,
	  // string domain name or ip of rpc server, default '127.0.0.1' 
	  host: this.node.cfg.jsonrpc.host,
	  // string with default path, default '/' 
	  path: '/',
	  method: 'GET',
	  username: this.node.cfg.jsonrpc.username,
	  password: this.node.cfg.jsonrpc.password,
	  headers: {
	    'Content-Type': 'application/json; charset=utf-8'
	  },
	  // boolean false to turn rpc checks off, default true 
	  strict: true
  }

  var client = new rpc.Client(options)

  client.call(
	  {"jsonrpc": "2.0", "method": "z_listaddresses"},
	  function (err, res) {
	    // Did it all work ? 
	    if (err) { console.log(err) }
	    else { console.log(res) }
	  }
	)*/
	var client = new zcash.Client({
	  host: this.node.cfg.jsonrpc.host,
	  port: this.node.cfg.jsonrpc.port,
	  user: this.node.cfg.jsonrpc.username,
	  pass: this.node.cfg.jsonrpc.password,
	  timeout: 30000
	})

	client.getBlockTemplate((err, accs, resHeaders) => {
	  if (err) return console.log(err);
	  console.log('Accounts:', accs);
	})

}

JsonRpcClient.prototype.call = function (req)
{
  try {

		this.rpc.call('z_listaddresses', (err, result) => {
			if (err) {
				console.log(err)
			}
	    console.log(result)
	  })

  } catch (e) {
    logger.warn("Could not start RPC server")
    logger.warn("Reason: "+e.message)
  }
}
