#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <random>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <thread>
#include <string>

#include "util.h"
#include "Thrasher.h"


Thrasher::Thrasher(int argc, char* argv[])
{
    parse_and_validate_args(argc, argv);

    // allocate the memory arena
    if (iteration_mode == ITERATION_MODE_FILE) {
        arena_fd = open(input_filepath.c_str(), O_RDWR);
        if (arena_fd < 0) die("could not open input file");

        struct stat st;
       if (stat(input_filepath.c_str(), &st) != 0)
           die("could not stat input file");
       arena_n_bytes = (size_t) st.st_size;

       arena_base = (uint64_t*) mmap(NULL, arena_n_bytes,
               PROT_READ | PROT_WRITE, MAP_SHARED, arena_fd, 0);
    }
    else {
        arena_base = (uint64_t*) mmap(NULL, arena_n_bytes,
                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    if (arena_base == MAP_FAILED)
        die("could not mmap() file");

    // std::mutexes aren't movable, so use placement new to construct in place
    if (n_locks != 0) {
        locks = new std::mutex[n_locks];
        for (size_t i = 0; i < n_locks; ++i) {
            assert((new (&locks[i]) std::mutex()) != nullptr);
        }
    }
}


Thrasher::~Thrasher()
{
    assert(munmap(arena_base, arena_n_bytes) == 0);

    if (arena_fd != -1) assert(close(arena_fd) == 0);

    if (n_locks != 0) {
        for (size_t i = 0; i < n_locks; ++i) locks[i].~mutex();
        delete locks;
    }
}

void
Thrasher::parse_and_validate_args(int argc, char* argv[])
{
    int c;
    optind = 0; // global: clear previous getopt() state, if any
    opterr = 0; // global: don't explicitly warn on unrecognized args

    // sentinels
    input_filepath = "";
    arena_n_bytes = 0;
    block_n_bytes = 0;
    n_iterations = 0;
    n_threads = 0;
    n_locks = -1;
    iteration_mode = ITERATION_MODE_INVALID;

    while ((c = getopt(argc, argv, "a:b:i:l:m:n:t:")) != -1) {
        try {
            switch (c) {
                case 'a':
                    arena_n_bytes = shorthand_to_integer(optarg, 1024);
                    break;
                case 'b':
                    block_n_bytes = shorthand_to_integer(optarg, 1024);
                    break;
                case 'i':
                    input_filepath = optarg;
                    break;
                case 'l':
                    n_locks = std::stoll(optarg);
                    break;
                case 'm': {
                    std::string mode_str = optarg;
                    std::transform(mode_str.begin(), mode_str.end(),
                            mode_str.begin(), ::tolower);
                    if (mode_str == "file")
                        iteration_mode = ITERATION_MODE_FILE;
                    else if (mode_str == "random")
                        iteration_mode = ITERATION_MODE_RANDOM;
                    else if (mode_str == "sequential")
                        iteration_mode = ITERATION_MODE_SEQUENTIAL;
                    break;
                }
                case 'n':
                    n_iterations = shorthand_to_integer(optarg, 1000);
                    break;
                case 't':
                    n_threads = std::stoll(optarg);
                    break;
                case '?':
                    die("unrecognized argument");
                    break;
            }
        }
        catch (...) {
            die("exception occurred while parsing arguments");
        }
    }


    if (iteration_mode == ITERATION_MODE_INVALID)
        die("must specify iteration mode: <-m file|sequential|random>");
    if (iteration_mode == ITERATION_MODE_FILE and input_filepath == "")
        die("must specify input filepath: <-i INPUT_FILEPATH>");
    if (iteration_mode != ITERATION_MODE_FILE and arena_n_bytes == 0)
        die("must specify non-zero arena n. bytes: <-a N_BYTES>");
    if (iteration_mode != ITERATION_MODE_FILE and block_n_bytes == 0)
        die("must specify non-zero block n. bytes: <-b N_BYTES>");
    if (n_iterations == 0)
        die("must specify non-zero n. iterations (-1 for infinite): "
                "<-n N_ITERATIONS>");

    if (iteration_mode != ITERATION_MODE_FILE and
            arena_n_bytes % block_n_bytes != 0)
        die("arena size must be a perfect multiple of block size");

    if (iteration_mode != ITERATION_MODE_FILE and
            __builtin_popcount(block_n_bytes) != 1)
        die("block size must be a power of two");

    if (n_threads == 0)
        die("must specify a non-zero num. threads: <-t N_THREADS>");

    size_t n_hw_threads = std::thread::hardware_concurrency();
    if (n_threads > n_hw_threads) warn("running with more threads than are "
            "available on the system (" + std::to_string(n_hw_threads) + ")");

    // NOTE: NYI
    if (n_locks == -1)
        die("must specify num. locks: <-l NUM_LOCKS>");

    // derived members
    if (iteration_mode != ITERATION_MODE_FILE) {
        n_blocks = arena_n_bytes / block_n_bytes;
        block_n_bytes_log2 = __builtin_ctz(block_n_bytes);
    }
}

void
Thrasher::run()
{

    printf("Kicking off threads...\n");


    for (size_t i = 0; i < n_threads; ++i) {
        threads.emplace_back(std::move(std::thread(&Thrasher::worker, this,
                i)));
    }

    for (auto& t : threads) t.join();
    printf("Joined worker threads.\n");

    printf("done.\n");
}

/*
 * Worker function, to be run in a std::thread.
 */
void
Thrasher::worker(size_t thread_idx)
{
    // just use integer division, possibly doing up to (n_threads-1)-fewer
    // iters.
    size_t n_thread_iterations = n_iterations / n_threads;
    size_t avg_n_blocks_per_thread = n_blocks / n_threads;

    // for sequential and random modes: the starting block index
    // (attempt to have threads work on different parts of the arena)
    size_t block_idx = avg_n_blocks_per_thread * thread_idx;


    // trifurcate the logic based upon iteration mode (i.e., pull the
    // conditional out of the innermost loop)
    if (iteration_mode == ITERATION_MODE_SEQUENTIAL) {
        // infinite sentinel: tight loop forever
        if (n_iterations == -1) {
            while (true) {
                char* block = idx_to_ptr(block_idx);
                block_update_fn(block);
                // update block_idx
                block_idx = next_idx_sequential(block_idx);
            }
        }
        else {
            // actually count iterations in the loop
            for (size_t i = 0; i < n_thread_iterations; ++i) {
                // apply the block update function to the current block_idx
                char* block = idx_to_ptr(block_idx);
                block_update_fn(block);
                // update block_idx
                block_idx = next_idx_sequential(block_idx);
            }
        }
    }
    else if (iteration_mode == ITERATION_MODE_RANDOM) {
        // seed gen with thread_idx to ensure different across threads
        std::mt19937 gen(thread_idx);
        // NOTE: std::uniform_int_distribution is on [a, b], so we actually
        // want dist(0, n_blocks - 1)
        std::uniform_int_distribution<> dist(0, n_blocks - 1);

        if (n_iterations == -1) {
            while (true) {
                char* block = idx_to_ptr(block_idx);
                block_update_fn(block);
                block_idx = next_idx_random(gen, dist);
            }
        }
        else {
            for (size_t i = 0; i < n_thread_iterations; ++i) {
                char* block = idx_to_ptr(block_idx);
                block_update_fn(block);
                block_idx = next_idx_random(gen, dist);
            }
        }

    }
    else if (iteration_mode == ITERATION_MODE_FILE) {
        // each thread starts at offset equal to its index...
        volatile uint64_t* ptr = &arena_base[thread_idx];

        if (n_iterations == -1) {
            // ...then follows the Sattolo cycle
            while (true) {
                ptr = arena_base + *ptr;
            }
        }
        else {
            for (size_t i = 0; i < n_thread_iterations; ++i) {
                ptr = arena_base + *ptr;
            }
        }
    }

    printf("Worker %zu done.\n", thread_idx);
}

/*
 * Called each time a thread wants to update a block.
 * NOTE: locking NYI
 */
inline void
Thrasher::block_update_fn(char* block)
{
    // read and write
    ++(*block);
}

/*
 * Used only privately to Thrasher class, so can be declared inline in the .cpp
 * file.
 */
inline size_t
Thrasher::next_idx_sequential(size_t curr_block_idx)
{
    return (curr_block_idx + 1) % n_blocks;
}

/*
 * Takes in thread_idx to support per-thread PRNGs.
 */
inline size_t
Thrasher::next_idx_random(std::mt19937& gen, std::uniform_int_distribution<>&
        dist)
{
    return dist(gen);
}

/*
 * Block size is guaranteed to be a power of two, so we can use left-shift to
 * multiply.
 */
inline char*
Thrasher::idx_to_ptr(size_t block_idx)
{
    return ((char*) arena_base) + (block_idx << block_n_bytes_log2);
}
