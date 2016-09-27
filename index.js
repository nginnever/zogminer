'use strict'

const nooocl = require('nooocl')
const Server = require('./jsonrpcserver')

const CLHost = nooocl.CLHost

var host = CLHost.createV12()
host = new CLHost(1.2)

// configure hardcoded node
var node = {
	cfg: {
		jsonrpc: {
			enable: true,
			password: null
		}
	}
}

var server = new Server.JsonRpcServer(node)

server.enable()
