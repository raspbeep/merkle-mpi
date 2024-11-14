
ml OpenMPI/4.1.4-GCC-11.3.0
make clean
make all

rm -rf results
mkdir results

# send receive 2 ranks
mpirun -np 2 build/test_merkle s > results/send_receive.txt

# allgather 16, 32, 64, 128 ranks
mpirun -np 16 build/test_merkle a > results/allgather_16.txt
mpirun -np 32 build/test_merkle a > results/allgather_32.txt
mpirun -np 64 build/test_merkle a > results/allgather_64.txt
mpirun -np 128 build/test_merkle a > results/allgather_128.txt

# bcast 16, 32, 64, 128 ranks
mpirun -np 16 build/test_merkle b > results/bcast_16.txt
mpirun -np 32 build/test_merkle b > results/bcast_32.txt
mpirun -np 64 build/test_merkle b > results/bcast_64.txt
mpirun -np 128 build/test_merkle b > results/bcast_128.txt