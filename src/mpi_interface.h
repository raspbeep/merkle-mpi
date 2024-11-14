/**
 * @file      mpi_interface.h
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

#pragma once

#include "merkle.h"

/**
 * @brief Tag for Merkle tree data validation messages.
 * 
 * Tag used for Merkle tree data validation messages in MPI communication.
 */
#define MPI_MERKLE_TAG 123

/**
 * @brief Macro for checking the return value of MPI calls.
 * 
 * The macro checks the return value of the MPI call and returns the status code if it is not MPI_SUCCESS.
 * 
 * @param call MPI call to be checked.
 * @return MPI status code.
 */
#define MPI_CALL(call) \
    {int ret = (call); \
    if (ret != MPI_SUCCESS) return ret; }\

/**
 * @brief Retrieves the size of the datatype and the total size in bytes.
 *
 * Internal function to retrieve the size of the datatype and the total size in bytes.
 *
 * @param datatype MPI datatype.
 * @param datatype_size Pointer to the datatype size.
 * @param size_bytes Pointer to the total size in bytes.
 * @param count Number of elements.
 * @return MPI status code.
 */
int __mpi_datatype_info(MPI_Datatype datatype, int *datatype_size, int *size_bytes, int count);

/**
 * @brief Sets the internal parameters for merkle tree construction
 *
 * ITEM_COUNT defines the count of given datatypes in a Merkle data segment 
 * (the amount of data one leaf node covers).
 *
 * @param item_count The count of given datatypes in a Merkle data segment.
 */
void _merkle_set_item_count(int item_count);

/**
 * @brief Sets the internal parameters for merkle tree debug printing
 *
 * ITEM_SIZE is only relevant for printing debug information in the 
 * _pretty_print_tree function.
 *
 * @param item_size The size of the datatype, used for debug printing.
 */
void _merkle_set_item_size(int item_size);

/**
 * @brief MPI_Send wrapper that sends data and verifies its integrity using a Merkle tree.
 * 
 * @param buf Pointer to the data buffer.
 * @param count Number of elements in the buffer.
 * @param datatype MPI datatype of the elements.
 * @param dest Destination rank.
 * @param tag Message tag.
 * @param comm MPI communicator.
 * @return MPI status code.
 */
int MPI_Send_Merkle(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

/**
 * @brief MPI_Recv wrapper that receives data and verifies its integrity using a Merkle tree.
 *
 * @param buf Pointer to the data buffer.
 * @param count Number of elements in the buffer.
 * @param datatype MPI datatype of the elements.
 * @param source Source rank.
 * @param tag Message tag.
 * @param comm MPI communicator.
 * @return MPI status code.
 */
int MPI_Recv_Merkle(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);

/**
 * @brief MPI_Bcast wrapper that transfers data and verifies its integrity using a Merkle tree.
 *
 * @param buffer Pointer to the data buffer.
 * @param count Number of elements in the buffer.
 * @param datatype MPI datatype of the elements.
 * @param root Root rank for the broadcast.
 * @param comm MPI communicator.
 * @return MPI status code.
 */
int MPI_Bcast_Merkle(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);

/**
 * @brief MPI_Allgather wrapper that transfers data and verifies its integrity using a Merkle tree.
 *
 * @param sendbuf Pointer to the data buffer.
 * @param sendcount Number of elements in the buffer.
 * @param sendtype MPI datatype of the elements.
 * @param recvbuf Pointer to the receive buffer.
 * @param recvcount Number of elements in the receive buffer.
 * @param recvtype MPI datatype of the elements.
 * @param comm MPI communicator.
 * @return MPI status code.
 */
int MPI_Allgather_Merkle(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm);

/**
 * @brief Correction protocol for validating the data integrity across all nodes.
 * 
 * The function takes the hashes of the critical paths from the root rank.
 * It then compares the hashes with the local critical paths and initiates
 * a correction protocol if a mismatch is detected.
 *
 * @param tree Local merkle tree of each rank.
 * @param datatype_size Size of the datatype.
 * @param hashes Array of hash values of critical paths from the root rank.
 * @param datatype MPI_Datatype of the elements.
 * @param root Root rank to communicate with for the correction
 * @param comm MPI communicator.
 * @return MPI status code. 
 *
 */
int __correction_all(struct merkle_tree *tree, int datatype_size, XXH64_hash_t *hashes, MPI_Datatype datatype, int root, MPI_Comm comm);


int validate_sender_merkle(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

int validate_receiver_merkle(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm);