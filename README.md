# Thrasher

Thrasher is a synthetic workload for stressing a computer's memory subsystem.
Thrasher supports several runtime options, including number of threads,
method of iterating through the memory pool (sequential/random), and amount of
work done per block access.

## Prerequisites
- `make`
- A C++ compiler

## Running
`bin/thrasher [OPTIONS]`

#### Example
High MPKI: `bin/thrasher -a 16G -b 4096 -m sequential -n 1B -t 8 -l 0`  
High memory writes: `bin/thrasher -a 16G -b 64 -m sequential -n 1B -t 8 -l 0`  

Note: larger block sizes will defeat the effects of next-line prefetchers.
Strided prefetchers may still see some benefit.

### Options
- `-a`: size of the memory arena
- `-b`: size of each individual memory block
- `-l`: number of lock buckets to use ('0' means no locking, possibly racy). NOTE: NYI
- `-m <sequential/random>`: whether to skip around randomly through the memory
arena, or iterate over it sequentially
- `-n`: total number of iterations to perform (across all threads)
- `-t`: number of concurrent threads
