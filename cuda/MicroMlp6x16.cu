
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <chrono>

#include "cubb/MicroMlp.h"


#define CUDA_SAFE_CALL(func) \
do { \
     cudaError_t err = (func); \
     if (err != cudaSuccess) { \
         fprintf(stderr, "[Error] %s (error code: %d) at %s line %d\n", cudaGetErrorString(err), err, __FILE__, __LINE__); \
         exit(1); \
     } \
} while(0)


#define MAX_NODE_SIZE	512
#define MAX_FRAME_UNIT	64
#define	N				6
#define	M				16

__constant__ int g_input_index[MAX_NODE_SIZE*N];

__global__ void kernal_MicroMlp6x16_forward(
			const float*	in_sig,
			float*			out_sig,
			const float*	hidden_W,
			const float*	hidden_b,
			const float*	output_W,
			const float*	output_b)
{
	__shared__   float hidden_buf[M*MAX_FRAME_UNIT];
	__shared__   float input_buf [N*MAX_FRAME_UNIT];

	int frame_size = blockDim.x * gridDim.x;
	int frame = blockDim.x * blockIdx.x + threadIdx.x;
	int index = threadIdx.y;
	int node  = blockIdx.y;

	// input_buf(shared memory) に コピー
	if ( index < N ) {
		int in_idx = g_input_index[node*N + index];
		input_buf[index * MAX_FRAME_UNIT + threadIdx.x] = in_sig[frame_size * in_idx + frame];
	}

	__syncthreads();

	const float*	ptr_in;
	float*			ptr_out;
	const float*	ptr_W;
	const float*	ptr_b;
	float			acc;

	// 初段計算
	ptr_in  = &input_buf[threadIdx.x];
	ptr_W = &hidden_W[(node * M + index) * N];
	ptr_b = &hidden_b[node * M + index];
	acc = ptr_b[0];
	for ( int i = 0; i < N; ++i ) {
		acc += ptr_in[i * MAX_FRAME_UNIT] * ptr_W[i];
	}
	acc = fmaxf(acc, 0);	// ReLU

	ptr_out = &hidden_buf[threadIdx.x];
	ptr_out[index * MAX_FRAME_UNIT] = acc;

	__syncthreads();

	// 出力段計算
	if ( index == 0 ) {
		ptr_in = ptr_out;
		ptr_W = &output_W[node * M];
		ptr_b = &output_b[node];
		acc = ptr_b[0];
		for ( int i = 0; i < M; ++i ) {
			acc += ptr_in[i * MAX_FRAME_UNIT] * ptr_W[i];
		}

		ptr_out = &out_sig[frame];
		ptr_out[frame_size * node] = acc;
	}
}


int MicroMlp6x16_Forward
		(
			int				input_node_size,
			int				output_node_size,
			int				frame_size,
			const float*	in_sig,
			float*			out_sig,
			const int*		input_index,
			const float*	hidden_W,
			const float*	hidden_b,
			const float*	output_W,
			const float*	output_b
		)
{
	cudaError_t cudaStatus0 = cudaGetLastError();
    if (cudaStatus0 != cudaSuccess) {
        fprintf(stderr, "start failed: %s\n", cudaGetErrorString(cudaStatus0));
		exit(1);
    }

	cudaDeviceSynchronize();
	auto time0 = std::chrono::system_clock::now();

	float* dev_in_sig;
	float* dev_out_sig;
	float* dev_hidden_W;
	float* dev_hidden_b;
	float* dev_output_W;
	float* dev_output_b;

	CUDA_SAFE_CALL(cudaMalloc((void**)&dev_in_sig,   input_node_size * frame_size * sizeof(float)));
	CUDA_SAFE_CALL(cudaMalloc((void**)&dev_out_sig,  output_node_size * frame_size * sizeof(float)));
	CUDA_SAFE_CALL(cudaMalloc((void**)&dev_hidden_W, output_node_size * M * N * sizeof(float)));
	CUDA_SAFE_CALL(cudaMalloc((void**)&dev_hidden_b, output_node_size * M * sizeof(float)));
	CUDA_SAFE_CALL(cudaMalloc((void**)&dev_output_W, output_node_size * M * sizeof(float)));
	CUDA_SAFE_CALL(cudaMalloc((void**)&dev_output_b, output_node_size * sizeof(float)));
	
	cudaDeviceSynchronize();
	auto time1 = std::chrono::system_clock::now();

	CUDA_SAFE_CALL(cudaMemcpyToSymbol(g_input_index, input_index, output_node_size * N * sizeof(int)));
	CUDA_SAFE_CALL(cudaMemcpy(dev_hidden_W, hidden_W, output_node_size * M * N * sizeof(float), cudaMemcpyHostToDevice));
	CUDA_SAFE_CALL(cudaMemcpy(dev_hidden_b, hidden_b, output_node_size * M * sizeof(float), cudaMemcpyHostToDevice));
	CUDA_SAFE_CALL(cudaMemcpy(dev_output_W, output_W, output_node_size * M * sizeof(float), cudaMemcpyHostToDevice));
	CUDA_SAFE_CALL(cudaMemcpy(dev_output_b, output_b, output_node_size * sizeof(float), cudaMemcpyHostToDevice));

	cudaDeviceSynchronize();
	auto time2 = std::chrono::system_clock::now();

	CUDA_SAFE_CALL(cudaMemcpy(dev_in_sig, in_sig, input_node_size * frame_size * sizeof(float), cudaMemcpyHostToDevice));

	cudaDeviceSynchronize();
	auto time3 = std::chrono::system_clock::now();

	int		frame_unit = frame_size;
	if ( frame_unit > MAX_FRAME_UNIT ) { frame_unit = MAX_FRAME_UNIT; }

	dim3	grid(frame_size / frame_unit, output_node_size);
	dim3	block(frame_unit, M);

	kernal_MicroMlp6x16_forward<<<grid, block>>>(
			dev_in_sig,
			dev_out_sig,
			dev_hidden_W,
			dev_hidden_b,
			dev_output_W,
			dev_output_b
		);
	cudaError_t cudaStatus = cudaGetLastError();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "kernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
		exit(1);
    }


	cudaDeviceSynchronize();
	auto time4 = std::chrono::system_clock::now();

	CUDA_SAFE_CALL(cudaMemcpy(out_sig, dev_out_sig, output_node_size * frame_size * sizeof(float), cudaMemcpyDeviceToHost));

	cudaDeviceSynchronize();
	auto time5 = std::chrono::system_clock::now();

	CUDA_SAFE_CALL(cudaFree(dev_in_sig));
	CUDA_SAFE_CALL(cudaFree(dev_out_sig));
	CUDA_SAFE_CALL(cudaFree(dev_hidden_W));
	CUDA_SAFE_CALL(cudaFree(dev_hidden_b));
	CUDA_SAFE_CALL(cudaFree(dev_output_W));
	CUDA_SAFE_CALL(cudaFree(dev_output_b));

	cudaDeviceSynchronize();
	auto time6 = std::chrono::system_clock::now();

	double elapsed_malloc       = std::chrono::duration_cast<std::chrono::milliseconds>(time1-time0).count();
	double elapsed_cpu_to_gpu_p = std::chrono::duration_cast<std::chrono::milliseconds>(time2-time1).count();
	double elapsed_cpu_to_gpu   = std::chrono::duration_cast<std::chrono::milliseconds>(time3-time2).count();
	double elapsed_kernel       = std::chrono::duration_cast<std::chrono::milliseconds>(time4-time3).count();
	double elapsed_gpu_to_cpu   = std::chrono::duration_cast<std::chrono::milliseconds>(time5-time4).count();
	double elapsed_free         = std::chrono::duration_cast<std::chrono::milliseconds>(time6-time5).count();
	std::cout << "malloc               : " << elapsed_malloc       << " [msec]" << std::endl;
	std::cout << "param copy(cpu->gpu) : " << elapsed_cpu_to_gpu_p << " [msec]" << std::endl;
	std::cout << "data copy(cpu->gpu)  : " << elapsed_cpu_to_gpu   << " [msec]" << std::endl;
	std::cout << "kernel               : " << elapsed_kernel       << " [msec]" << std::endl;
	std::cout << "data copy(gpu->cpu)  : " << elapsed_gpu_to_cpu   << " [msec]" << std::endl;
	std::cout << "free                 : " << elapsed_free         << " [msec]" << std::endl;

	return 0;
}



