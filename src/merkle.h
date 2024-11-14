/**
 * @file      merkle.h
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
/**
 * @file main.c
 * @brief Implementation of a Merkle tree with parallel hash calculation using OpenMP.
 */

#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <assert.h>
#include "mpi.h"
#include "xxhash.h"

extern int ITEM_COUNT, ITEM_SIZE;

/**
 * @struct merkle_node
 * @brief Represents a node in the Merkle tree.
 */
struct merkle_node {
    void *data; /**< Pointer to the data segment */
    int data_size; /**< Size of the data segment */
    int index; /**< Index of the node */
    struct merkle_node *left; /**< Pointer to the left child node */
    struct merkle_node *right; /**< Pointer to the right child node */
    XXH64_hash_t hash; /**< Hash value of the node */
};

/**
 * @struct merkle_tree
 * @brief Represents the Merkle tree structure.
 */
struct merkle_tree {
    struct merkle_node *root; /**< Pointer to the root node */
    int n_nodes; /**< Total number of nodes in the tree */
    int n_leaves; /**< Number of leaf nodes in the tree */
    int n_critical_paths; /**< Number of critical paths in the tree */
    XXH64_hash_t *critical_paths; /**< Array of hash values for critical paths */
};

/**
 * @brief Prints the Merkle tree in a readable format.
 * @param node Pointer to the root node of the tree.
 * @param depth Current depth of the node in the tree.
 */
void _pretty_print_tree(struct merkle_node *node, int depth);

/**
 * @brief Recursively calculates the hash for each node in the Merkle tree.
 * @param node Pointer to the tree root node.
 */
void tree_calculate_hash(struct merkle_node *node); // NOLINT (recursion)

/**
 * @brief Retrieves the critical paths of the Merkle tree.
 * @param tree Pointer to the Merkle tree structure.
 */
void tree_retrieve_critical_paths(struct merkle_tree *tree);

/**
 * @brief Creates a Merkle tree from the given data.
 * @param data Pointer to the data buffer.
 * @param data_size Size of the data buffer.
 * @param merkle_segment_size Size of each data segment.
 * @return Pointer to the created Merkle tree.
 */
struct merkle_tree* create_merkle_tree(const void *data, int data_size, int merkle_segment_size);

/**
 * @brief Compares the hash values of two Merkle trees.
 * @param my_hashes Array of hash values from the first tree.
 * @param other_hashes Array of hash values from the second tree.
 * @param n_hashes Number of hash values to compare.
 * @return Index of the faulty node if hashes do not match, 0 otherwise.
 */
int compare_hashes(const XXH64_hash_t *my_hashes, const XXH64_hash_t *other_hashes, int n_hashes);

/**
 * @brief Frees the memory allocated for the Merkle tree.
 * @param tree Pointer to the Merkle tree.
 */
void free_merkle_tree(struct merkle_tree *tree);
