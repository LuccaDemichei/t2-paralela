# MPI Parallelized Average Filter

This is an MPI-parallelized version of the average filter kernel for PGM images using a master-slave pattern with dynamic work distribution.

## Architecture

### Master-Slave Pattern

- **Master (rank 0)**:

  - Loads the input image
  - Broadcasts image data to all processes
  - Distributes work dynamically to slaves
  - Receives filtered lines from slaves
  - Assembles the final filtered image
  - Sends termination signals when complete

- **Slaves (rank > 0)**:
  - Request work from master
  - Process assigned image lines
  - Send results back to master
  - Continue until receiving termination signal

### Dynamic Work Distribution

Slaves request work from the master rather than having work statically assigned. This provides better load balancing, especially when processing times vary.

## Communication Protocol

1. Slave sends `WORK_REQUEST` to master
2. Master responds with either:
   - `WORK_ASSIGNMENT`: Contains line number to process
   - `TERMINATE`: Signals slave to exit
3. Slave processes the line and sends `RESULT` (line number + filtered data)
4. Slave requests more work (loop back to step 1)

## Compilation

```bash
make
```

Or manually:

```bash
mpicc -Wall -O2 -o AverageFilterKernel AverageFilterKernel.c
```

## Usage

```bash
mpirun -np <num_processes> ./AverageFilterKernel <input_file> <filter_size>
```

### Parameters:

- `num_processes`: Number of MPI processes (1 master + N slaves)
- `input_file`: Input PGM image file
- `filter_size`: Size of the filter kernel (odd number, e.g., 3, 5, 7)

### Examples:

```bash
# Run with 4 processes (1 master + 3 slaves)
mpirun -np 4 ./AverageFilterKernel SaltAndPepperNoise.pgm 3

# Run with 8 processes
mpirun -np 8 ./AverageFilterKernel Big.pgm 5

# Quick test with make
make run
```

## Output

The filtered image is saved as `filtered.pgm` in the current directory.

## Performance Considerations

- More slaves generally improve performance for large images
- Filter size affects the amount of computation per line
- Dynamic work distribution helps with load balancing
- Communication overhead should be considered for small images

## Requirements

- MPI implementation (OpenMPI, MPICH, etc.)
- C compiler with MPI support (mpicc)
- PGM format images for input
