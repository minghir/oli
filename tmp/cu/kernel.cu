extern "C" __global__ void double_elements(long long* data, int count) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < count) {
        data[idx] = data[idx] * 2;
    }
}