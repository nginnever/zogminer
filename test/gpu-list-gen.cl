__kernel void add(__global float *inputA, __global float *inputB, __global float *output) {
    
    size_t id = get_global_id(0);
    output[id] = inputA[id] + inputB[id];

}
