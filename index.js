'use strict'

const nooocl = require('nooocl')
const Server = require('./lib/rpc/jsonrpcserver').JsonRpcServer
const Connection = require('./lib/connection').Connection
const RpcClient = require('./lib/rpc/jsonrpcclient').JsonRpcClient

const CLHost = nooocl.CLHost

var host = CLHost.createV12()
host = new CLHost(1.2)

var hostVersion = host.cl.version
var someOpenCLValue = host.cl.defs.CL_MEM_COPY_HOST_PTR
var platform = host.getPlatforms()[0]
//var allPlatforms = host.getPlatforms()
console.log(platform)

// configure hardcoded node
var node = {
  cfg: {
    jsonrpc: {
      enable: true,
      username: 'voxelot',
      password: 'bushido',
      host: '127.0.0.1',
      port: 18232//8333//29001
    }
  }
}

//var server = new Server(node)
//var connection = new Connection()

//server.enable()

var client = new RpcClient(node)
//client.enable()
//client.call()
