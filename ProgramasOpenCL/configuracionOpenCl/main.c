

#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include "timing.h"
#include "cl-helper.h"

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "need two arguments!\n");
    abort();
 }

  const cl_long n = atol(argv[1]);
  const int ntrips = atoi(argv[2]);
  cl_context ctx;
  cl_command_queue queue;
  create_context_on(CHOOSE_INTERACTIVELY, CHOOSE_INTERACTIVELY, 0, &ctx, &queue, 0);

  print_device_info_from_queue(queue);

  // --------------------------------------------------------------------------
  // load kernels 
  // --------------------------------------------------------------------------
  char *knl_text = read_file("vec-add-soln.cl");
  cl_kernel knl = kernel_from_string(ctx, knl_text, "sum", NULL);
  free(knl_text);

  // --------------------------------------------------------------------------
  // allocate and initialize CPU memory
  // --------------------------------------------------------------------------
  float *a = (float *) malloc(sizeof(float) * n);
  if (!a) { perror("alloc x"); abort(); }
  float *b = (float *) malloc(sizeof(float) * n);
  if (!b) { perror("alloc y"); abort(); }
  float *c = (float *) malloc(sizeof(float) * n);
  if (!c) { perror("alloc z"); abort(); }
  size_t i = 0;
  for (i = 0; i < n; ++i)
  {
    a[i] = i;
    b[i] = 2*i;
  }

//  // --------------------------------------------------------------------------
//  // allocate device memory
//  // --------------------------------------------------------------------------
  cl_int status;
  cl_mem buf_a = clCreateBuffer(ctx, CL_MEM_READ_WRITE, 
      sizeof(float) * n, 0, &status);
  CHECK_CL_ERROR(status, "clCreateBuffer");

  cl_mem buf_b = clCreateBuffer(ctx, CL_MEM_READ_WRITE,
      sizeof(float) * n, 0, &status);
  CHECK_CL_ERROR(status, "clCreateBuffer");
//
  cl_mem buf_c = clCreateBuffer(ctx, CL_MEM_READ_WRITE,
      sizeof(float) * n, 0, &status);
  CHECK_CL_ERROR(status, "clCreateBuffer");
//
//  // --------------------------------------------------------------------------
//  // transfer to device
//  // --------------------------------------------------------------------------
  CALL_CL_GUARDED(clEnqueueWriteBuffer, (
        queue, buf_a, /*blocking*/ CL_TRUE, /*offset*/ 0,
        n * sizeof(float), a,
        0, NULL, NULL));
//
  CALL_CL_GUARDED(clEnqueueWriteBuffer, (
        queue, buf_b, /*blocking*/ CL_TRUE, /*offset*/ 0,
        n * sizeof(float), b,
        0, NULL, NULL));

//  // --------------------------------------------------------------------------
//  // run code on device
//  // --------------------------------------------------------------------------
//
  CALL_CL_GUARDED(clFinish, (queue));
//
  timestamp_type time1, time2;
  get_timestamp(&time1);
int trip = 0;
  for (trip = 0; trip < ntrips; ++trip)
  {
    SET_4_KERNEL_ARGS(knl, buf_a, buf_b, buf_c, n);
    size_t ldim[] = { 32 };
    size_t gdim[] = { ((n + ldim[0] - 1)/ldim[0])*ldim[0] };
    CALL_CL_GUARDED(clEnqueueNDRangeKernel,
        (queue, knl,
         /*dimensions*/ 1, NULL, gdim, ldim,
         0, NULL, NULL));
  }
//
  CALL_CL_GUARDED(clFinish, (queue));
//
  get_timestamp(&time2);
  double elapsed = timestamp_diff_in_seconds(time1,time2)/ntrips;
  printf("%f s\n", elapsed);
  printf("%f GB/s\n",
      3*n*sizeof(float)/1e9/elapsed);
//
//  // --------------------------------------------------------------------------
//  // transfer back & check
//  // --------------------------------------------------------------------------
  CALL_CL_GUARDED(clEnqueueReadBuffer, (
        queue, buf_c, /*blocking*/ CL_TRUE, /*offset*/ 0,
        n * sizeof(float), c,
        0, NULL, NULL));
   size_t j = 0;
  for (j = 0; j < n; ++j)
    if (c[j] != 3*j)
    {
      printf("BAD %ld %f %f!\n", j, c[j], c[j] - 3*j);
      abort();
    }
  puts("GOOD");
//
//  // --------------------------------------------------------------------------
//  // clean up
//  // --------------------------------------------------------------------------
  CALL_CL_GUARDED(clReleaseMemObject, (buf_a));

  CALL_CL_GUARDED(clReleaseMemObject, (buf_b));  
  CALL_CL_GUARDED(clReleaseMemObject, (buf_c));
  CALL_CL_GUARDED(clReleaseKernel, (knl));
  CALL_CL_GUARDED(clReleaseCommandQueue, (queue));
  CALL_CL_GUARDED(clReleaseContext, (ctx));
//
  return 0;
}
