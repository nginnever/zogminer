'use strict'

const opencl = require('./lib/opencl')
// const Server = require('./lib/rpc/jsonrpcserver').JsonRpcServer
// const Connection = require('./lib/connection').Connection
// const RpcClient = require('./lib/rpc/jsonrpcclient').JsonRpcClient
// const ArrayType = require("ref-array")
// const FloatArray = new ArrayType("float")
// const ref = require("ref")
// var float = ref.types.float

var device = opencl()
device.connect('gpu')
//device.test()

var kernelSource = "__kernel void copy(global float* src, global float* dst, uint begin)" +
    "{" +
    "uint idx = get_global_id(0);" +
    "dst[idx - 1] = src[idx + begin];" +
    "}"

device.build(kernelSource)


// configure hardcoded node
// var node = {
//   cfg: {
//     jsonrpc: {
//       enable: true,
//       username: 'voxelot',
//       password: 'bushido',
//       host: '127.0.0.1',
//       port: 18232//8333//29001
//     }
//   }
// }

//var server = new Server(node)
//var connection = new Connection()

//server.enable()

//var client = new RpcClient(node)
//client.enable()
//client.call()
