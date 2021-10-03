#include <stdint.h>
#include <stdlib.h>

#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

class Thrasher {
    public:
        Thrasher(int argc, char* argv[]);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        Thrasher(const Thrasher& t) = delete;
        Thrasher& operator=(const Thrasher& t) = delete;
        Thrasher(Thrasher&& t) = delete;
        Thrasher& operator=(Thrasher&& t) = delete;
        ~Thrasher();

        void run();

    private:
        void parse_and_validate_args(int argc, char* argv[]);
        void worker(size_t thread_idx);

        size_t next_idx_sequential(size_t curr_block_idx);
        size_t next_idx_random(std::mt19937& gen,
                std::uniform_int_distribution<>& dist);

        char* idx_to_ptr(size_t block_idx);

        // TODO parameterize this (just memset()s for now)
        void block_update_fn(char* block);

        typedef enum {
            ITERATION_MODE_FILE,
            ITERATION_MODE_RANDOM,
            ITERATION_MODE_SEQUENTIAL,
            ITERATION_MODE_INVALID,
        } iteration_mode_t;

        std::string input_filepath;
        size_t arena_n_bytes;
        size_t block_n_bytes;
        ssize_t n_iterations;
        size_t n_threads;
        ssize_t n_locks;
        iteration_mode_t iteration_mode;

        int arena_fd = -1;
        uint64_t* arena_base;
        size_t n_blocks;
        size_t block_n_bytes_log2;
        std::vector<std::thread> threads;
        std::mutex* locks = nullptr;
};
