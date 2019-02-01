#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include "GSimulation.hpp"
#include <CL/cl2.hpp>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
void GSimulation :: update_accel()
{
   for (int i = 0; i < _npart; i++)// update acceleration
   {
#ifdef ASALIGN
     __assume_aligned(particles->pos_x, ALIGNMENT);
     __assume_aligned(particles->pos_y, ALIGNMENT);
     __assume_aligned(particles->pos_z, ALIGNMENT);
     __assume_aligned(particles->acc_x, ALIGNMENT);
     __assume_aligned(particles->acc_y, ALIGNMENT);
     __assume_aligned(particles->acc_z, ALIGNMENT);
     __assume_aligned(particles->mass, ALIGNMENT);
#endif
     real_type ax_i = particles->acc_x[i];
     real_type ay_i = particles->acc_y[i];
     real_type az_i = particles->acc_z[i];
#pragma omp simd reduction(+:ax_i,ay_i,az_i)
     for (int j = 0; j < _npart; j++)
     {
         real_type dx, dy, dz;
	 real_type distanceSqr = 0.0f;
	 real_type distanceInv = 0.0f;
		  
	 dx = particles->pos_x[j] - particles->pos_x[i];	//1flop
	 dy = particles->pos_y[j] - particles->pos_y[i];	//1flop	
	 dz = particles->pos_z[j] - particles->pos_z[i];	//1flop
	
 	 distanceSqr = dx*dx + dy*dy + dz*dz + softeningSquared;	//6flops
 	 distanceInv = 1.0f / sqrtf(distanceSqr);			//1div+1sqrt

	 ax_i+= dx * G * particles->mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	 ay_i += dy * G * particles->mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	 az_i += dz * G * particles->mass[j] * distanceInv * distanceInv * distanceInv; //6flops
     }
     particles->acc_x[i] = ax_i;
     particles->acc_y[i] = ay_i;
     particles->acc_z[i] = az_i;
   }

}

const char *getErrorString(cl_int error);
void GSimulation :: update_accel_cl()
{

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    printf("\nFound %d platforms:\n", platforms.size());
    for (auto &plat : platforms) // printout available platforms
        std::cout << plat.getInfo<CL_PLATFORM_NAME>() << std::endl;

    if (platforms.size() == 0) {
        std::cout << "no OpenCL platforms found!!!" << std::endl << std::endl;
        return;
    }
    auto plat = platforms[0]; // pick the first one available
    std::vector<cl::Device> devices;
    plat.getDevices(CL_DEVICE_TYPE_ALL, &devices);
    printf("\nFound %d devices:\n", devices.size());
    for (auto &dev : devices) 
        std::cout << dev.getInfo<CL_DEVICE_NAME>() << std::endl << std::endl;
    if (devices.size() == 0) {
        std::cout << "no OpenCL devices found!!!" << std::endl << std::endl;
        return;
    }
    auto dev = devices[0]; // pick the first one available

    
    cl::Context context;
    cl::CommandQueue q;
    try{
        context = cl::Context(dev);
        q = cl::CommandQueue(context, dev, 0);

    } catch (cl::Error &e) {
        std::cout << getErrorString(e.err()) << std::endl;
    }

    std::string src_str = R"CLC(
   #include <Constants.hpp>
   #include <Particle.hpp>
   __kernel void comp(__global const struct ParticleSoA* particles) {
   int i = get_global_id(0);
/*#ifdef ASALIGN
     __assume_aligned(particles->pos_x, ALIGNMENT);
     __assume_aligned(particles->pos_y, ALIGNMENT);
     __assume_aligned(particles->pos_z, ALIGNMENT);
     __assume_aligned(particles->acc_x, ALIGNMENT);
     __assume_aligned(particles->acc_y, ALIGNMENT);
     __assume_aligned(particles->acc_z, ALIGNMENT);
     __assume_aligned(particles->mass, ALIGNMENT);
#endif
*/
     real_type ax_i = particles->acc_x[i];
     real_type ay_i = particles->acc_y[i];
     real_type az_i = particles->acc_z[i];
   int j = get_global_id(1);
     real_type dx, dy, dz;
	 real_type distanceSqr = 0.0f;
	 real_type distanceInv = 0.0f;
		  
	 dx = particles->pos_x[j] - particles->pos_x[i];	//1flop
	 dy = particles->pos_y[j] - particles->pos_y[i];	//1flop	
	 dz = particles->pos_z[j] - particles->pos_z[i];	//1flop
	
 	 distanceSqr = dx*dx + dy*dy + dz*dz + softeningSquared;	//6flops
 	 distanceInv = 1.0f / sqrt(distanceSqr);			//1div+1sqrt

	 ax_i+= dx * G * particles->mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	 ay_i += dy * G * particles->mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	 az_i += dz * G * particles->mass[j] * distanceInv * distanceInv * distanceInv; //6flops

     particles->acc_x[i] = ax_i;
     particles->acc_y[i] = ay_i;
     particles->acc_z[i] = az_i;
   }
    )CLC";

    cl::Program program(src_str);

    try {
        program.build("-cl-std=CL2.0");
        auto buildInfo = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>();
        for (auto &pair : buildInfo)
            std::cerr << pair.second << std::endl << std::endl;
    } catch (cl::Error &e) {
        // Print build info for all devices
        cl_int buildErr;
        auto buildInfo = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(&buildErr);
        for (auto &pair : buildInfo) {
            std::cerr << pair.second << std::endl << std::endl;
        }
        return;
    }

//    cl::Kernel kernel;
//    try {
//        kernel = Cl:Kernel(program, "comp
//    }




}


const char *getErrorString(cl_int error) {
switch(error){
    // run-time and JIT compiler errors
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

    // compile-time errors
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

    // extension errors
    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    default: return "Unknown OpenCL error";
    }
}
