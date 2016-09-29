'use strict'

const nooocl = require('nooocl')
const Server = require('./lib/rpc/jsonrpcserver').JsonRpcServer
const Connection = require('./lib/connection').Connection
const RpcClient = require('./lib/rpc/jsonrpcclient').JsonRpcClient
const ArrayType = require("ref-array")
const FloatArray = new ArrayType("float")
const ref = require("ref")
var float = ref.types.float

const CLHost = nooocl.CLHost
const CLContext = nooocl.CLContext
const CLCommandQueue = nooocl.CLCommandQueue
const clCreateProgramWithSource = nooocl.clCreateProgramWithSource
const NDRange = nooocl.NDRange
const CLBuffer = nooocl.CLBuffer

var host = CLHost.createV12()
host = new CLHost(1.2)

var hostVersion = host.cl.version
var someOpenCLValue = host.cl.defs.CL_MEM_COPY_HOST_PTR
var platform = host.getPlatforms()[0]
var count = host.platformsCount
var allPlatforms = host.getPlatforms()
var gpuDevice = platform.gpuDevices()[0]
var cpuDevice = platform.cpuDevices()[0]
var maxComputeUnits = gpuDevice.maxComputeUnits
console.log('GPU max compute threads ' + maxComputeUnits)

maxComputeUnits = cpuDevice.maxComputeUnits
console.log('CPU max compute threads ' + maxComputeUnits)

var context = new CLContext(gpuDevice)
var queue = new CLCommandQueue(context, gpuDevice)

var kernelSource = "kernel void copy(global float* src, global float* dst, uint begin)" +
    "{" +
    "uint idx = get_global_id(0);" +
    "dst[idx - 1] = src[idx + begin];" +
    "}"

var program = context.createProgram(kernelSource)

program.build('-cl-fast-relaxed-math').then(
    function() {
        // At this point you don't know that the build succeeded or failed. 
        // Since one context can hold multiple devices, 
        // and a build could succeeded on a device, but could failed on the other, 
        // NOOOCL won't raise build errors, you should asks for it per device basis: 
 
        // can be either: CL_BUILD_SUCCESS, CL_BUILD_ERROR 
        var buildStatus = program.getBuildStatus(gpuDevice);
 
        // Compiler output: 
        var buildLog = program.getBuildLog(gpuDevice);

        console.log(buildLog)

        var kernel = program.createKernel('copy')
        var kernels = program.createAllKernels()

        // kernel.setArg(1.45)

        // var r1 = new NDRange(10)

        // queue.enqueueNDRangeKernel(
        //     kernel,
        //     new NDRange(3), // global size 
        //     null, // local size 
        //     new NDRange(1) // offset 
        // )

        var srcArray = new FloatArray(5);
        var dstArray = new FloatArray(4);
        for (var i = 0; i < srcArray.length; i++) {
            srcArray[i] = (i + 1) * 1.1;
        }
        for (var i = 0; i < dstArray.length; i++) {
            dstArray[i] = 0.0;
        }
        
        var src = CLBuffer.wrapReadOnly(context, srcArray)
        var dst = CLBuffer.wrap(context, ref.reinterpret(dstArray.buffer, 3 * float.size, 0))

        var assertValues = function () {
          var out = {};
          return queue.waitable().enqueueMapBuffer(dst, host.cl.defs.CL_MAP_READ | host.cl.defs.CL_MAP_WRITE, 0, dst.size, out).promise
            .then(function () {
                var buffer = ref.reinterpret(out.ptr, dst.size + float.size, 0);
                var v1 = float.get(buffer, 0).toFixed(2);
                var v2 = dstArray[0].toFixed(2);

                float.set(buffer, 0, 0.0);

                v1 = float.get(buffer, 1 * float.size).toFixed(2);
                v2 = dstArray[1].toFixed(2);

                float.set(buffer, 1 * float.size, 0.0);

                v1 = float.get(buffer, 2 * float.size).toFixed(2);
                v2 = dstArray[2].toFixed(2);

                float.set(buffer, 2 * float.size, 0.0);

                v1 = float.get(buffer, 3 * float.size).toFixed(2);
                v2 = dstArray[3].toFixed(2);


                return queue.waitable().enqueueUnmapMemory(dst, out.ptr).promise;
            });
        }

        var func = kernel.bind(queue, new NDRange(3), null, new NDRange(1));
        func(src, dst, { "uint": 1 })

        assertValues()
          .then(function () {
            // Test direct call:
            kernels[0].setArg(0, src);
            kernels[0].setArg(1, dst);
            kernels[0].setArg(2, 1, "uint");
            queue.enqueueNDRangeKernel(kernels[0], new NDRange(3), null, new NDRange(1));
            console.log('weee')
            return assertValues();
          })
    })

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
