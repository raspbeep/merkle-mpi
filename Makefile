# build directory
BUILD_DIR = build

# documentation directory
DOC_DIR = docs

# source files
SRC_FILES = src/merkle.c src/mpi_interface.c external/xxhash/xxhash.c src/test/test.c

# compiler and flags
CC = mpicc
CFLAGS = -Iexternal/xxhash -Isrc -O3 -fopenmp -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-unused-result -Wno-unused-label -Wno-unused-local-typedefs -Wno-unused-const-variable -Wno-unused-macros

# target executable
TARGET = $(BUILD_DIR)/test_merkle

# Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build the target executable
$(TARGET): $(BUILD_DIR) $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_FILES) $(LDFLAGS)

# Define the all target
all: $(TARGET)

# Clean the build directory
clean:
	rm -rf $(DOC_DIR)
	rm -rf $(BUILD_DIR)

.PHONY: clean all doc

docs:
	doxygen Doxyfile

run: $(TARGET)
	mpirun -np 2 $(TARGET)
