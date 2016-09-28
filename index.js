'use strict'

const nooocl = require('nooocl')
const Server = require('./lib/rpc/jsonrpcserver').JsonRpcServer
const Connection = require('./lib/connection').Connection

const CLHost = nooocl.CLHost

var host = CLHost.createV12()
host = new CLHost(1.2)

// configure hardcoded node
var node = {
  cfg: {
    jsonrpc: {
      enable: true,
      username: 'voxelot',
      password: 'kittens',
      host: '127.0.0.1',
      port: 8332 //29001
    }
  }
}

console.log('tesrt')
var server = new Server(node)
var connection = new Connection()

server.enable()
