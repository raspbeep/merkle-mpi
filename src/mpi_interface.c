/**
 * @file      mpi_interface.c
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

#include "mpi_interface.h"
#include <string.h>

int ITEM_COUNT = 10;
int ITEM_SIZE = sizeof(int);

// #define DEBUG
// #define SIMULATE_CORRUPTION

int __mpi_datatype_info(MPI_Datatype datatype, int *datatype_size, int *size_bytes, int count) {
  MPI_CALL(MPI_Type_size(datatype, datatype_size));
  *size_bytes = (*datatype_size) * count;
  return MPI_SUCCESS;
} // __mpi_datatype_info

int __correction_sender(struct merkle_tree *tree, int datatype_size, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
  int faulty_node_index;

  while(1) {
    // receive corrupt data index or confirmation of data integrity
    MPI_CALL(MPI_Recv(&faulty_node_index, 1, MPI_INT, dest, MPI_MERKLE_TAG, comm, MPI_STATUS_IGNORE));

    // if corrupt data index is -1, data is valid
    if (faulty_node_index == -1) {
#ifdef DEBUG
printf("Data is valid\n");
#endif
      break;
    } else {
#ifdef DEBUG
printf("Data is corrupt at index %d\n", faulty_node_index);
#endif
    }

    struct merkle_node *corrupt_node = &tree->root[faulty_node_index];
    MPI_CALL(MPI_Send(corrupt_node->data, corrupt_node->data_size / datatype_size, datatype, dest, MPI_MERKLE_TAG, comm));
  }

  return MPI_SUCCESS;
} // __correction_sender

int __correction_receiver(struct merkle_tree *tree, int datatype_size, XXH64_hash_t *hashes, MPI_Datatype datatype, int source, int tag, MPI_Comm comm) {
  int faulty_node_index;
  MPI_Status status;
    // repeat for a maximum of n_data_segments times, but at least once for confirmation
  while (1) {
#ifdef DEBUG
    printf("Debug tree receive\n");
    _pretty_print_tree(tree->root, 0);
#endif
    // get hashes on critical paths
    tree_retrieve_critical_paths(tree);
    // compare hashes
    faulty_node_index = compare_hashes(tree->critical_paths, hashes, tree->n_critical_paths);
    // if (faulty_node_index != -1) printf("Faulty node index: %d\n", faulty_node_index);
    // send corrupt data index or confirmation of data integrity
    MPI_CALL(MPI_Send(&faulty_node_index, 1, MPI_INT, source, MPI_MERKLE_TAG, comm));
    // break if the data is valid
    if (faulty_node_index == -1) break;

    // receive data for corrupt node from the sender
    struct merkle_node *corrupt_node = &tree->root[faulty_node_index];
    MPI_CALL(MPI_Recv(corrupt_node->data, corrupt_node->data_size / datatype_size, datatype, source, MPI_MERKLE_TAG, comm, &status));

    // recalculate hashes for the received data
    tree_calculate_hash(tree->root);
  }
  
  return MPI_SUCCESS;
} // __correction_receiver

int __correction_all(struct merkle_tree *tree, int datatype_size, XXH64_hash_t *hashes, MPI_Datatype datatype, int root, MPI_Comm comm) {
  int faulty_node_index, rank;
  MPI_Comm_rank(comm, &rank);
  MPI_Status status;
  // for simplicity, compare hashes at root rank as well
  faulty_node_index = compare_hashes(tree->critical_paths, hashes, tree->n_critical_paths);

  int *faulty_node_indexes = NULL;
  int size;
  MPI_CALL(MPI_Comm_size(comm, &size));
  if (rank == root) {  
    faulty_node_indexes = (int *) malloc(size * sizeof(int));
  }

  // gather fault indicators and ranks
  MPI_Gather(&faulty_node_index, 1, MPI_INT, faulty_node_indexes, 1, MPI_INT, root, comm);

  if (rank == root) {
    for (int i = 0; i < size; i++) {
      if (faulty_node_indexes[i] != -1) {
        // all ranks in a communicator take part in bcast, we can safely index with i
        MPI_CALL(__correction_sender(tree, datatype_size, datatype, i, MPI_MERKLE_TAG, comm));
      } else {
        #ifdef DEBUG
        printf("Data is valid at rank %d\n", i);
        #endif
      }

    }
  } else {
    if (faulty_node_index != -1) {
      MPI_CALL(__correction_receiver(tree, datatype_size, hashes, datatype, root, MPI_MERKLE_TAG, comm));
    }
  }

  if (rank == root) {
    free(faulty_node_indexes);
  }
  
  return MPI_SUCCESS;
}

int MPI_Send_Merkle(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
  int datatype_size, size_bytes;
  MPI_CALL(__mpi_datatype_info(datatype, &datatype_size, &size_bytes, count));

  // send data
  MPI_CALL(MPI_Send(buf, count, datatype, dest, tag, comm));

  // create merkle tree from data
  struct merkle_tree *tree = create_merkle_tree(buf, size_bytes, ITEM_COUNT * datatype_size);
  tree_retrieve_critical_paths(tree);

  // send critical paths' hashes
  MPI_CALL(MPI_Send(tree->critical_paths, tree->n_critical_paths, MPI_UINT64_T, dest, MPI_MERKLE_TAG, comm));

  // correction loop sender
  MPI_CALL(__correction_sender(tree, datatype_size, datatype, dest, tag, comm));

#ifdef DEBUG
  printf("Debug tree sent\n");
  _pretty_print_tree(tree->root, 0);
#endif

  free_merkle_tree(tree);
  return 0;
} // MPI_Send_Merkle

int validate_sender_merkle(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
  int datatype_size, size_bytes;
  MPI_CALL(__mpi_datatype_info(datatype, &datatype_size, &size_bytes, count));

  // create merkle tree from data
  struct merkle_tree *tree = create_merkle_tree(buf, size_bytes, ITEM_COUNT * datatype_size);
  tree_retrieve_critical_paths(tree);

  // send critical paths' hashes
  MPI_CALL(MPI_Send(tree->critical_paths, tree->n_critical_paths, MPI_UINT64_T, dest, MPI_MERKLE_TAG, comm));

  // correction loop sender
  MPI_CALL(__correction_sender(tree, datatype_size, datatype, dest, tag, comm));
  
  free_merkle_tree(tree);
  
  return MPI_SUCCESS;
}

int validate_receiver_merkle(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm) {
  int datatype_size, size_bytes;
  MPI_CALL(__mpi_datatype_info(datatype, &datatype_size, &size_bytes, count));

  // create merkle tree from data
  struct merkle_tree *tree = create_merkle_tree(buf, size_bytes, ITEM_COUNT * datatype_size);

  // allocate memory for critical paths' hashes
  XXH64_hash_t *hashes = (XXH64_hash_t *) malloc(tree->n_critical_paths * sizeof(XXH64_hash_t));
  if (hashes == NULL) {
    printf("Error: Unable to allocate memory for hashes\n");
    return -1;
  }

  // receive critical paths' hashes
  MPI_CALL(MPI_Recv(hashes, tree->n_critical_paths, MPI_UINT64_T, source, MPI_MERKLE_TAG, comm, MPI_STATUS_IGNORE));

  MPI_CALL(__correction_receiver(tree, datatype_size, hashes, datatype, source, tag, comm));

  free(hashes);
  free_merkle_tree(tree);
  return MPI_SUCCESS;
}

int MPI_Recv_Merkle(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {
  int datatype_size, size_bytes;
  MPI_CALL(__mpi_datatype_info(datatype, &datatype_size, &size_bytes, count));

  // receive data
  MPI_CALL(MPI_Recv(buf, count, datatype, source, tag, comm, status));

#ifdef SIMULATE_CORRUPTION
  ((int *)buf)[0]++;
  ((int *)buf)[7]++;
#endif

  // create merkle tree from data
  struct merkle_tree *tree = create_merkle_tree(buf, size_bytes, ITEM_COUNT * datatype_size);

  // allocate memory for critical paths' hashes
  XXH64_hash_t *hashes = (XXH64_hash_t *) malloc(tree->n_critical_paths * sizeof(XXH64_hash_t));
  if (hashes == NULL) {
    printf("Error: Unable to allocate memory for hashes\n");
    return -1;
  }

  // receive critical paths' hashes
  MPI_CALL(MPI_Recv(hashes, tree->n_critical_paths, MPI_UINT64_T, source, MPI_MERKLE_TAG, comm, status));

  MPI_CALL(__correction_receiver(tree, datatype_size, hashes, datatype, source, tag, comm));

#ifdef DEBUG
  printf("Debug tree fixed\n");
  _pretty_print_tree(tree->root, 0);
#endif

  free(hashes);
  free_merkle_tree(tree);
  return 0;
} // MPI_Recv_Merkle

int MPI_Bcast_Merkle(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
  int datatype_size, size_bytes, rank;
  MPI_CALL(__mpi_datatype_info(datatype, &datatype_size, &size_bytes, count));
  MPI_CALL(MPI_Comm_rank(comm, &rank));

  // send the data
  MPI_Bcast(buffer, count, datatype, root, comm);

#ifdef SIMULATE_CORRUPTION
  if (rank != root) {
    ((int *)buffer)[0]++;
    ((int *)buffer)[7]++;
  }
#endif
  
  // construct the merkle tree
  struct merkle_tree *tree = create_merkle_tree(buffer, size_bytes, ITEM_COUNT * datatype_size);
  tree_retrieve_critical_paths(tree);
  
  XXH64_hash_t *hashes;
  if (rank != root) {
    hashes = (XXH64_hash_t *) malloc(tree->n_critical_paths * sizeof(XXH64_hash_t));
    if (hashes == NULL) {
      printf("Error: Unable to allocate memory for hashes\n");
      return -1;
    }
  } else {
    hashes = tree->critical_paths;
  }

  // broadcast critical paths' hashes
  MPI_CALL(MPI_Bcast(hashes, tree->n_critical_paths, MPI_UINT64_T, root, comm));

  __correction_all(tree, datatype_size, hashes, datatype, root, comm);

  if (rank != root) {
    free(hashes);
  }

  free_merkle_tree(tree);
  return 0;
} // MPI_Bcast_Merkle

int MPI_Allgather_Merkle(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
  int send_datatype_size, recv_datatype_size, recv_size_bytes, send_size_bytes, rank, size;
  MPI_CALL(__mpi_datatype_info(sendtype, &send_datatype_size, &recv_size_bytes, sendcount));
  MPI_CALL(__mpi_datatype_info(recvtype, &recv_datatype_size, &send_size_bytes, recvcount));
  
  // the size in bytes of the send and receive buffers must be the same
  assert(recv_size_bytes == send_size_bytes);

  MPI_CALL(MPI_Comm_rank(comm, &rank));
  MPI_CALL(MPI_Comm_size(comm, &size));

  // allgather the data
  MPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);

#ifdef SIMULATE_CORRUPTION
  if (rank != 0) {
    ((int *)recvbuf)[0]++;
    ((int *)recvbuf)[7]++;
  }
#endif

  struct merkle_tree *tree = create_merkle_tree(recvbuf, recv_size_bytes * size, ITEM_COUNT * recv_datatype_size);
  tree_retrieve_critical_paths(tree);

  XXH64_hash_t *hashes;
  if (rank == 0) {
    hashes = tree->critical_paths;
  } else {
    hashes = (XXH64_hash_t *) malloc(tree->n_critical_paths * sizeof(XXH64_hash_t));
    if (hashes == NULL) {
      printf("Error: Unable to allocate memory for hashes\n");
      return -1;
    }
  }

  // broadcast critical paths' hashes from root to all ranks
  MPI_Bcast(hashes, tree->n_critical_paths, MPI_UINT64_T, 0, comm);

  // gathers the confirmations from all ranks and if any rank has faulty data,
  // it initiates the correction protocol
  __correction_all(tree, recv_datatype_size, hashes, recvtype, 0, comm);

  if (rank != 0) {
    free(hashes);
  }
  free_merkle_tree(tree);

  return MPI_SUCCESS;
}