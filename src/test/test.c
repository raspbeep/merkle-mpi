/**
 * @file       test.c
 *
 * @author    Pavel Kratochvil \n
 *            Faculty of Information Technology \n
 *            Brno University of Technology \n
 *            xkrato61@fit.vutbr.cz
 *
 * @brief     SCO project - Merkle tree implementation with parallel hash calculation using OpenMP.
 *
 * @date      10 November  2024 \n
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "xxhash.h"
#include "mpi_interface.h"

#define MIN_BUFFER_SIZE (32)
#define MAX_BUFFER_SIZE (1024 * 1024 * 256)
#define BENCHMARK_ITERATIONS 100

typedef struct {
    double min_time;
    double max_time;
    double avg_time;
    size_t msg_size;
    int iterations;
} benchmark_result_t;

void print_results(const char *operation, const benchmark_result_t *result) {
  double bandwidth = (result->msg_size * sizeof(int)) / (1024.0 * 1024.0) / result->avg_time; // MB/s
  printf("%-20s Size: %8zu bytes  Time: min=%8.2f μs avg=%8.2f μs max=%8.2fμs  BW=%8.2f MB/s\n",
    operation,
    result->msg_size * sizeof(int),
    result->min_time * 1e6,
    result->avg_time * 1e6,
    result->max_time * 1e6,
    bandwidth);
}

benchmark_result_t run_benchmark(void (*benchmark_fn)(void *buffer, size_t size), size_t buffer_size) {
  benchmark_result_t result = {
    .min_time = 1e9,
    .max_time = 0,
    .avg_time = 0,
    .msg_size = buffer_size,
    .iterations = BENCHMARK_ITERATIONS
  };
  
  int* buffer = (int *) malloc(buffer_size * sizeof(int));
  if (buffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
  
  // Measurement phase
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    double start_time = MPI_Wtime();
    benchmark_fn(buffer, buffer_size);
    double end_time = MPI_Wtime();
    
    double elapsed = end_time - start_time;
    result.min_time = (elapsed < result.min_time) ? elapsed : result.min_time;
    result.max_time = (elapsed > result.max_time) ? elapsed : result.max_time;
    result.avg_time += elapsed;
  }
  
  result.avg_time /= BENCHMARK_ITERATIONS;
  free(buffer);
  return result;
}

benchmark_result_t run_benchmark_2(void (*benchmark_fn)(void *sendbuffer, void *recvbuffer, size_t size), size_t buffer_size) {
  benchmark_result_t result = {
    .min_time = 1e9,
    .max_time = 0,
    .avg_time = 0,
    .msg_size = buffer_size,
    .iterations = BENCHMARK_ITERATIONS
  };

  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  int* sendbuffer = (int *) malloc(buffer_size * sizeof(int));
  if (sendbuffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int* recvbuffer = (int *) malloc(buffer_size * size * sizeof(int));
  if (recvbuffer == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
  
  // Measurement phase
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    double start_time = MPI_Wtime();
    benchmark_fn(sendbuffer, recvbuffer, buffer_size);
    double end_time = MPI_Wtime();
    
    double elapsed = end_time - start_time;
    result.min_time = (elapsed < result.min_time) ? elapsed : result.min_time;
    result.max_time = (elapsed > result.max_time) ? elapsed : result.max_time;
    result.avg_time += elapsed;
  }
  
  result.avg_time /= BENCHMARK_ITERATIONS;
  free(sendbuffer);
  free(recvbuffer);
  return result;
}

void benchmark_send_recv(void *buffer, size_t size) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) {
    MPI_Send(buffer, size, MPI_INT, 1, 0, MPI_COMM_WORLD);
  } else if (rank == 1) {
    MPI_Recv(buffer, size, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
}

void benchmark_send_recv_merkle(void *buffer, size_t size) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  if (rank == 0) {
    MPI_Send_Merkle(buffer, size, MPI_INT, 1, 0, MPI_COMM_WORLD);
  } else if (rank == 1) {
    MPI_Recv_Merkle(buffer, size, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
}

void benchmark_bcast(void *buffer, size_t size) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
  MPI_Bcast(buffer, size, MPI_INT, 0, MPI_COMM_WORLD);
}

void benchmark_bcast_merkle(void *buffer, size_t size) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  MPI_Bcast_Merkle(buffer, size, MPI_INT, 0, MPI_COMM_WORLD);
}

void benchmark_allgather(void *sendbuffer, void *recvbuffer, size_t size) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Allgather(sendbuffer, size, MPI_INT, recvbuffer, size, MPI_INT, MPI_COMM_WORLD);
}

void benchmark_allgather_merkle(void *sendbuffer, void *recvbuffer, size_t size) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Allgather_Merkle(sendbuffer, size, MPI_INT, recvbuffer, size, MPI_INT, MPI_COMM_WORLD);
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);


  void (*benchmark_fn)(void *buffer, size_t size) = NULL;
  void (*benchmark_fn_merkle)(void *buffer, size_t size) = NULL;

  void (*benchmark_fn_2)(void *sendbuffer, void *recvbuffer, size_t size) = NULL;
  void (*benchmark_fn_2_merkle)(void *sendbuffer, void *recvbuffer, size_t size) = NULL;

  char *benchmark_fn_name;
  char *benchmark_fn_merkle_name;

  if (argc < 2) {
    if (rank == 0) {
      fprintf(stderr, "Usage: mpirun -np NUM_PROCS %s [s=send, b=bcast, a=allgather]\n", argv[0]);
    }
    MPI_Abort(MPI_COMM_WORLD, 0);
  }

  if (argv[1][0] == 's') {
    benchmark_fn = benchmark_send_recv;
    benchmark_fn_merkle = benchmark_send_recv_merkle;
    benchmark_fn_name = "Send-Recv";
    benchmark_fn_merkle_name = "Send-Recv (Merkle)";
  } else if (argv[1][0] == 'b') {
    benchmark_fn = benchmark_bcast;
    benchmark_fn_merkle = benchmark_bcast_merkle;
    benchmark_fn_name = "Bcast";
    benchmark_fn_merkle_name = "Bcast (Merkle)";
  } else if (argv[1][0] == 'a') {
    benchmark_fn_2 = benchmark_allgather;
    benchmark_fn_2_merkle = benchmark_allgather_merkle;
    benchmark_fn_name = "Allgather";
    benchmark_fn_merkle_name = "Allgather (Merkle)";
  } else{
    fprintf(stderr, "Invalid operation: %s\n", argv[1]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  
  if (rank == 0) {
    printf("MPI Benchmark Results\n");
    printf("=====================\n");
    printf("Processes: %d\n", size);
    printf("Iterations: %d\n\n", BENCHMARK_ITERATIONS);
  }

  // infinite loop for debugging
  // int x = 0;
  // while (x == 0) {
  //   continue;
  // }
  
  for (int item_count = 2; item_count < 4096; item_count <<= 2) {
    _merkle_set_item_count(item_count);
    MPI_Barrier(MPI_COMM_WORLD);

    for (size_t buffer_size = MIN_BUFFER_SIZE; buffer_size <= MAX_BUFFER_SIZE; buffer_size *= 2) {
      if (rank == 0) {
        printf("%s Buffer size: %zu bytes, item count: %d\n", benchmark_fn_name, buffer_size * sizeof(int), item_count);
      }

      if (benchmark_fn != NULL && benchmark_fn_merkle) {
        benchmark_result_t base_result = run_benchmark(benchmark_fn, buffer_size);
        if (rank == 0) {
          print_results(benchmark_fn_name, &base_result);
        }
        benchmark_result_t merkle_result = run_benchmark(benchmark_fn_merkle, buffer_size);
        if (rank == 0) {
          print_results(benchmark_fn_merkle_name, &merkle_result);
        }
      } else if (benchmark_fn_2 != NULL && benchmark_fn_2_merkle) {
        benchmark_result_t base_result = run_benchmark_2(benchmark_fn_2, buffer_size);
        if (rank == 0) {
          print_results(benchmark_fn_name, &base_result);
        }
        benchmark_result_t merkle_result = run_benchmark_2(benchmark_fn_2_merkle, buffer_size);
        if (rank == 0) {
          print_results(benchmark_fn_merkle_name, &merkle_result);
        }
      }
      if (rank == 0) {
        printf("---------------------------------------------\n");
      }
      MPI_Barrier(MPI_COMM_WORLD);
    }
  }
  
  MPI_Finalize();
  return 0;
}
