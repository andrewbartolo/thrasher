/*
 * Basic utility for creating a random-permutation file to be used as input
 * for Thrasher.
 */
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "util.h"

#define RAND_SEED 2021



typedef struct {
    std::string output_filename;
    int64_t size_bytes;
} args_t;

void
parse_and_validate_args(int argc, char* argv[], args_t& args)
{
    int c;
    optind = 0; // global: clear previous getopt() state, if any
    opterr = 0; // global: don't explicitly warn on unrecognized args

    // sentinels
    args.output_filename = "";
    args.size_bytes = -1;

    while ((c = getopt(argc, argv, "o:s:")) != -1) {
        try {
            switch (c) {
                case 'o':
                    args.output_filename = optarg;
                    break;
                case 's':
                    args.size_bytes = shorthand_to_integer(optarg, 1024);
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


    if (args.output_filename == "")
        die("must supply output filename: <-o OUTPUT_FILENAME>");
    if (args.size_bytes == -1)
        die("must supply output file size: <-s SIZE_STR>");
    if (args.size_bytes % 8 != 0)
        die("output file size must be a multiple of 8 bytes: <-s SIZE_STR>");
}


void
gen_file(args_t& args)
{
    size_t n_elems = args.size_bytes / sizeof(uint64_t);

    std::vector<uint64_t> vec;
    // do all the allocation once, up front
    // NOTE: size is still 0 here! (only capacity has changed)
    vec.reserve(n_elems);

    // first pass: fill the vector with sequential elements
    printf("filling vector...\n");
    for (size_t i = 0; i < n_elems; ++i)
        vec.emplace_back(i);

    // second pass: Sattolo's algorithm
    printf("permuting w/Sattolo's algorithm...\n");
    std::mt19937 gen(RAND_SEED);
    std::uniform_int_distribution<uint64_t>
            dist(0, std::numeric_limits<uint64_t>::max());

    // NOTE: this has modulo bias, but it's more efficient than constructing
    // a new distribution for every PRNG call
    for (size_t i = 0; i < n_elems - 1; ++i) {
        // gen rand int. NOTE: biased, see above
        size_t j = (i + 1) + (dist(gen) % (n_elems - (i + 1)));
        // swap
        vec[j] ^= vec[i];
        vec[i] ^= vec[j];
        vec[j] ^= vec[i];
    }

    // now, dump it to the file
    printf("writing to file...\n");
    std::ofstream ofs(args.output_filename, std::ofstream::binary);
    ofs.write((char*) vec.data(), args.size_bytes);
    ofs.close();
}


int
main(int argc, char* argv[])
{
    args_t args;
    parse_and_validate_args(argc, argv, args);

    gen_file(args);

    return 0;
}
