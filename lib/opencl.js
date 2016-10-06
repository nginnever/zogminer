'use strict'

const nooocl = require('nooocl')
const CLHost = nooocl.CLHost
const CLContext = nooocl.CLContext
const CLCommandQueue = nooocl.CLCommandQueue
const clCreateProgramWithSource = nooocl.clCreateProgramWithSource
const NDRange = nooocl.NDRange
const CLBuffer = nooocl.CLBuffer

let device
let host
let platform
let context
let queue
let program

module.exports = function opencl () {
  return {
    connect: (device_type) => {
      if (device_type === undefined) {
        device_type = 'gpu'
      }

      host = CLHost.createV12()
      host = new CLHost(1.2)
      
      platform = host.getPlatforms()[0]

      if(device_type === 'gpu') {
        device = platform.gpuDevices()[0]
        var label = 'GPU'
        var maxComputeUnits = device.maxComputeUnits
        console.log('GPU max compute threads ' + maxComputeUnits)
      } else {
        device = platform.cpuDevices()[0]
        var label = 'CPU'
        var maxComputeUnits = device.maxComputeUnits
        console.log('CPU max compute threads ' + maxComputeUnits)
      }

      // set the context
      context = new CLContext(device)
      // tie the context and device to command que
      queue = new CLCommandQueue(context, device)
      console.log(label + ' connected')

    },
    build: (kernel_source) => {
      console.log(typeof kernel_source)
      if(!kernel_source) {
        console.log('must provide kernel source')
        return
      }

      program = context.createProgram(kernel_source)

      program.build('-cl-fast-relaxed-math').then(() => {
        // At this point you don't know that the build succeeded or failed. 
        // Since one context can hold multiple devices, 
        // and a build could succeeded on a device, but could failed on the other, 
        // NOOOCL won't raise build errors, you should asks for it per device basis: 
 
        // can be either: CL_BUILD_SUCCESS, CL_BUILD_ERROR 

        var build_status = program.getBuildStatus(device)
        console.log(build_status)
 
        // Compiler output: 
        var build_log = program.getBuildLog(device);
        console.log(build_log)

        // var kernel = program.createKernel('copy')
        // var kernels = program.createAllKernels()

        // var srcArray = new FloatArray(5);
        // var dstArray = new FloatArray(4);
        // for (var i = 0; i < srcArray.length; i++) {
        //     srcArray[i] = (i + 1) * 1.1;
        // }
        // for (var i = 0; i < dstArray.length; i++) {
        //     dstArray[i] = 0.0;
        // }
        
        // var src = CLBuffer.wrapReadOnly(context, srcArray)
        // var dst = CLBuffer.wrap(context, ref.reinterpret(dstArray.buffer, 3 * float.size, 0))

        // var assertValues = function () {
        //   var out = {};
        //   return queue.waitable().enqueueMapBuffer(dst, host.cl.defs.CL_MAP_READ | host.cl.defs.CL_MAP_WRITE, 0, dst.size, out).promise
        //     .then(function () {
        //         var buffer = ref.reinterpret(out.ptr, dst.size + float.size, 0);
        //         var v1 = float.get(buffer, 0).toFixed(2);
        //         var v2 = dstArray[0].toFixed(2);

        //         float.set(buffer, 0, 0.0);

        //         v1 = float.get(buffer, 1 * float.size).toFixed(2);
        //         v2 = dstArray[1].toFixed(2);

        //         float.set(buffer, 1 * float.size, 0.0);

        //         v1 = float.get(buffer, 2 * float.size).toFixed(2);
        //         v2 = dstArray[2].toFixed(2);

        //         float.set(buffer, 2 * float.size, 0.0);

        //         v1 = float.get(buffer, 3 * float.size).toFixed(2);
        //         v2 = dstArray[3].toFixed(2);


        //         return queue.waitable().enqueueUnmapMemory(dst, out.ptr).promise;
        //     });
        // }

    //         var func = kernel.bind(queue, new NDRange(3), null, new NDRange(1));
    //         func(src, dst, { "uint": 1 })

    //         assertValues()
    //           .then(function () {
    //             // Test direct call:
    //             kernels[0].setArg(0, src);
    //             kernels[0].setArg(1, dst);
    //             kernels[0].setArg(2, 1, "uint");
    //             queue.enqueueNDRangeKernel(kernels[0], new NDRange(3), null, new NDRange(1));
    //             console.log('weee')
    //             return assertValues();
    //           })
    //     })
      })
    },
    test: () => {
      console.log(queue)
    }
  }
}
