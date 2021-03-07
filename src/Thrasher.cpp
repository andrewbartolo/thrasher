#include <assert.h>
#include <getopt.h>
#include <random>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <algorithm>
#include <thread>
#include <string>

#include "util.h"
#include "Thrasher.h"


Thrasher::Thrasher(int argc, char* argv[])
{
    parse_and_validate_args(argc, argv);

    // allocate the memory arena
    arena_base = (char*) mmap(NULL, arena_n_bytes, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(arena_base != MAP_FAILED);

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
    arena_n_bytes = 0;
    block_n_bytes = 0;
    n_iterations = 0;
    n_threads = 0;
    n_locks = -1;
    iteration_mode = ITERATION_MODE_INVALID;

    while ((c = getopt(argc, argv, "a:b:l:m:n:t:")) != -1) {
        try {
            switch (c) {
                case 'a':
                    arena_n_bytes = shorthand_to_integer(optarg, 1024);
                    break;
                case 'b':
                    block_n_bytes = shorthand_to_integer(optarg, 1024);
                    break;
                case 'l':
                    n_locks = std::stoll(optarg);
                    break;
                case 'm': {
                    std::string mode_str = optarg;
                    std::transform(mode_str.begin(), mode_str.end(),
                            mode_str.begin(), ::tolower);
                    if (mode_str == "sequential")
                            iteration_mode = ITERATION_MODE_SEQUENTIAL;
                    else if (mode_str == "random")
                            iteration_mode = ITERATION_MODE_RANDOM;
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


    if (arena_n_bytes == 0)
            die("must specify non-zero arena n. bytes: <-a N_BYTES>");
    if (block_n_bytes == 0)
            die("must specify non-zero block n. bytes: <-b N_BYTES>");
    if (iteration_mode == ITERATION_MODE_INVALID)
            die("must specify iteration mode: <-m sequential|random>");
    if (n_iterations == 0)
            die("must specify non-zero n. iterations: <-n N_ITERATIONS>");

    if (arena_n_bytes % block_n_bytes != 0)
            die("arena size must be a perfect multiple of block size");

    if (__builtin_popcount(block_n_bytes) != 1)
            die("block size must be a power of two");

    if (n_threads == 0)
            die("must specify a non-zero num. threads: <-t N_THREADS>");

    size_t n_hw_threads = std::thread::hardware_concurrency();
    if (n_threads > n_hw_threads) warn("running with more threads than are "
            "available on the system (" + std::to_string(n_hw_threads) + ")");

    if (n_locks == -1)
            die("must specify num. locks: <-l NUM_LOCKS>");

    // derived members
    n_blocks = arena_n_bytes / block_n_bytes;
    block_n_bytes_log2 = __builtin_ctz(block_n_bytes);
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

    // the starting block index
    // (attempt to have threads work on different parts of the arena)
    size_t block_idx = avg_n_blocks_per_thread * thread_idx;


    // bifurcate the logic based upon iteration mode (i.e., pull the conditional
    // out of the innermost loop)
    if (iteration_mode == ITERATION_MODE_SEQUENTIAL) {
        for (size_t i = 0; i < n_thread_iterations; ++i) {
            // apply the block update function to the current block_idx
            char* block = idx_to_ptr(block_idx);
            block_update_fn(block);

            // update block_idx
            block_idx = next_idx_sequential(block_idx);
        }
    }
    else if (iteration_mode == ITERATION_MODE_RANDOM) {
        // seed gen with thread_idx to ensure different across threads
        std::mt19937 gen(thread_idx);
        // NOTE: std::uniform_int_distribution is on [a, b], so we actually
        // want dist(0, n_blocks - 1)
        std::uniform_int_distribution<> dist(0, n_blocks - 1);

        for (size_t i = 0; i < n_thread_iterations; ++i) {
            char* block = idx_to_ptr(block_idx);
            block_update_fn(block);

            block_idx = next_idx_random(gen, dist);
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
    return arena_base + (block_idx << block_n_bytes_log2);
}
