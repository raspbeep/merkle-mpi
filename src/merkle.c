/**
 * @file      merkle.c
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

/**
 * @mainpage Validation of OpenMPI transferred data using Merkle Trees
 *
 * @section intro_sec Introduction
 *
 * This library provides a set of functions for validating data integrity in distributed computing using Merkle trees.
 * The project includes functions for sending, receiving, broadcasting, and gathering data with Merkle tree validation.
 *
 * @section install_sec Installation
 *
 * @subsection step1 Step 1: Clone the repository
 * @code{.sh}
 * git clone --recurse-submodules https://github.com/raspbeep/VUT-SCO.git
 * 
 * OR
 * 
 * git clone https://github.com/raspbeep/merkle-mpi.git
 * and then download the submodules (xxHash) using:
 * cd VUT-SCO
 * git submodule update --init --recursive
 * 
 * @endcode
 *
 * @subsection step2 Step 2: Build the project
 * @code{.sh}
 * make all
 * @endcode
 *
 * @section usage_sec Usage
 *
 * @subsection usage1 Running the tests
 * @code{.sh}
 * make run
 * @endcode
 *
 * @subsection usage2 Generating documentation
 * @code{.sh}
 * make docs
 * @endcode
 *
 * @section files_sec Files
 *
 * - src/mpi_interface.h: Header file containing the MPI Merkle tree function declarations.
 * - src/mpi_interface.c: Source file containing the MPI Merkle tree function implementations.
 * - src/test/test.c: Example user code demonstrating the usage of MPI Merkle tree functions.
 * - Makefile: Makefile for building the project and generating documentation.
 *
 * @section author_sec Author
 *
 * Pavel Kratochvil,
 * xkrato61@vutbr.cz,
 * Brno University of Technology,
 * Faculty of Information Technology,
 */

#include "merkle.h"
#include <omp.h>

#define HASH_FN XXH3_64bits_withSeed

void _merkle_set_item_count(int item_count) {
  ITEM_COUNT = item_count;
}

void _merkle_set_item_size(int item_size) {
  ITEM_SIZE = item_size;
}

void _pretty_print_tree(struct merkle_node *node, int depth) {
  if (node == NULL) {
    return;
  }

  for (int i = 0; i < depth; i++) {
    printf("\t\t");
  }

  if (node->left != NULL && node->right != NULL) {
    printf("├─ Node (%d): %llu\n", node->index, node->hash);
    _pretty_print_tree(node->right, depth + 1);
    _pretty_print_tree(node->left, depth + 1);
  } else {
    printf("└─ Leaf (%d): %p (size: %d) (", node->index, node->data, node->data_size);
    for (int i = 0; i < node->data_size / ITEM_SIZE; i++) {
      printf(" %d", ((int *)node->data)[i]);
    }
    printf(" ) (hash: %llu)\n", node->hash);
  }
}

void tree_calculate_hash(struct merkle_node *node) { // NOLINT (recursion)
  if (node == NULL) {
    return;
  }

  #pragma omp task
  {
    #ifdef DEBUG
    printf("Thread %d of %d is processing node %d\n", omp_get_thread_num(), omp_get_num_threads(), node->index);
    #endif
    tree_calculate_hash(node->left);
    tree_calculate_hash(node->right);

    if (node->left == NULL && node->right == NULL) {
      node->hash = HASH_FN(node->data, node->data_size, node->index);
    } else {
      XXH64_hash_t left_right_hash[2] = {node->left->hash, node->right->hash};
      node->hash = HASH_FN(left_right_hash, 2 * sizeof(XXH64_hash_t), node->index);
    }
  }
}

struct merkle_tree* create_merkle_tree(const void *data, const int data_size, const int merkle_segment_size) {
  struct merkle_tree *tree = malloc(sizeof(struct merkle_tree));
  if (tree == NULL) {
    printf("Error: Unable to allocate memory for merkle_tree\n");
    return NULL;
  }

  // for number of bytes indivisible by segment size
  tree->n_leaves = (data_size + merkle_segment_size - 1) / merkle_segment_size;
  tree->n_nodes = (tree->n_leaves << 1) - 1;

  tree->root = (struct merkle_node *) malloc(tree->n_nodes * sizeof(struct merkle_node));
  if (tree->root == NULL) {
    printf("Error: Unable to allocate memory for merkle_nodes\n");
    return NULL;
  }

  for (int i = 0; i < tree->n_nodes; i++) {
    int left_index = (i << 1) + 1;
    int right_index = (i << 1) + 2;

    tree->root[i].index = i;
    tree->root[i].data = NULL;
    tree->root[i].data_size = 0;
    tree->root[i].left = left_index < tree->n_nodes ? &tree->root[left_index] : NULL;
    tree->root[i].right = right_index < tree->n_nodes ? &tree->root[right_index] : NULL;

    // calculate hash for leaf nodes
    if (tree->root[i].left == NULL && tree->root[i].right == NULL) {
      // find the index of leaf node
      int leaf_index = i - (tree->n_nodes - tree->n_leaves);
      // the last leaf node may have a smaller size
      int leaf_size = (leaf_index == tree->n_leaves - 1 && data_size % merkle_segment_size != 0) ? data_size % merkle_segment_size : merkle_segment_size;
      tree->root[i].data_size = leaf_size;
      tree->root[i].data = (void *)((char *)data + leaf_index * merkle_segment_size);
    } else {
      tree->root[i].hash = 0;
    }
  }

  tree_calculate_hash(tree->root);

  tree->n_critical_paths = ((tree->n_nodes - 1) >> 1) + 1;
  tree->critical_paths = (XXH64_hash_t *)malloc(tree->n_critical_paths * sizeof(XXH64_hash_t));
  if (tree->critical_paths == NULL) {
    printf("Error: Unable to allocate memory for critical paths\n");
    return NULL;
  }

  return tree;
}

void tree_retrieve_critical_paths(struct merkle_tree *tree) {
  if (tree == NULL) {
    return;
  }
  tree->critical_paths[0] = tree->root[0].hash;
  #pragma omp parallel for default(none) shared(tree)
  for (int i = 1; i < tree->n_critical_paths; i++) {
    int idx = 1 + 2 * (i - 1);
    tree->critical_paths[i] = tree->root[idx].hash;
  }
}

int compare_hashes(const XXH64_hash_t *my_hashes, const XXH64_hash_t *other_hashes, int n_hashes) {
  assert(my_hashes != NULL);
  assert(other_hashes != NULL);

  // -1 is no fault
  if (my_hashes[0] == other_hashes[0]) return -1;
#if DEBUG
  printf("Error: Root hashes do not match\n");
#endif
  int max_faulty_index = 0;
  for (int i = 0; i < n_hashes; i++) {
    if (my_hashes[i] != other_hashes[i]) {
#ifdef DEBUG
      printf("Hash %d does not match\n", i);
#endif
      max_faulty_index = i;
    }
  }

  // find the index of the faulty node
  int node_index = max_faulty_index * 2 - 1;

  // check if it is a leaf node
  int total_nodes = (n_hashes << 1) - 1;

  // second part can be removed, the first part is sufficient
  if (node_index * 2 + 1 < total_nodes /* && node_index * 2 + 2 < total_nodes */) {
    // find the rightmost leaf node in the subtree rooted at the faulty node
    do {
      node_index = node_index * 2 + 2;
    } while (node_index * 2 + 1 < total_nodes);

    return node_index;
  } else {
#ifdef DEBUG
    printf("Faulty node is not a leaf node\n");
#endif
    return node_index;
  }
}

void free_merkle_tree(struct merkle_tree *tree) {
  free(tree->root);
  free(tree->critical_paths);
  free(tree);
}
