# MPI Merkle Tree Validation Library

A high-performance library for validating data integrity in MPI communications using Merkle trees with parallel hash calculation support via OpenMP.

## Features

- Merkle tree-based data validation for MPI communications
- Parallel hash computation using OpenMP
- Support for common MPI operations:
  - Point-to-point communication (Send/Receive)
  - Collective operations (Broadcast, Allgather)
- Automatic corruption detection and recovery
- XXHash-based high-performance hashing

### Prerequisites

- OpenMPI 
- OpenMP-compatible C compiler

### Building from Source

```bash
git clone --recurse-submodules https://github.com/raspbeep/merkle-mpi.git

# Or clone and update submodules separately
git clone https://github.com/raspbeep/merkle-mpi.git
cd merkle-mpi
git submodule update --init --recursive

# Build the project
make all
```

## Usage

### Basic Example

```c
#include "mpi_interface.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank, data[10];
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == 0) {
        // Send data with Merkle tree validation
        MPI_Send_Merkle(data, 10, MPI_INT, 1, 0, MPI_COMM_WORLD);
    } else {
        // Receive data with automatic validation
        MPI_Status status;
        MPI_Recv_Merkle(data, 10, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    }
    
    MPI_Finalize();
    return 0;
}
```

## API Reference

### Core Functions

The library provides Merkle tree-enhanced versions of standard MPI operations:

- `MPI_Send_Merkle`: Send data with Merkle tree validation
- `MPI_Recv_Merkle`: Receive and validate data using Merkle trees
- `MPI_Bcast_Merkle`: Broadcast data with integrity checking
- `MPI_Allgather_Merkle`: Gather data from all processes with validation

### Configuration Functions

```c
// Set the number of items per Merkle leaf node
_merkle_set_item_count(int item_count);

// Set the size of individual items for debug printing
_merkle_set_item_size(int item_size);
```

## Testing

```bash
sh benchmark.sh
```

## Documentation

Generate documentation using:

```bash
make docs
```

## License

This project is licensed under the MIT License.

## Author

Pavel Kratochvil  
Faculty of Information Technology  
Brno University of Technology  
xkrato61@fit.vutbr.cz
