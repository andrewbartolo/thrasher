#include "util.h"

#include <string.h>

#include <cassert>
#include <random>


/*
 * Helper function for issuing a non-fatal warning to stderr.
 */
void
warn(const std::string& msg = "")
{
    std::string w = "WARNING";
    if (msg != "") w += ": " + msg;

    fprintf(stderr, "%s\n", w.c_str());
}

/*
 * Helper function for killing the current process with an optional message
 * printed to stderr.
 */
void
die(const std::string& msg = "")
{
    std::string e = "ERROR";
    if (msg != "") e += ": " + msg;

    fprintf(stderr, "%s\n", e.c_str());
    exit(1);
}

/*
 * C- and C++-style helper functions for generating UID strings.
 * For C-style gen_uid(): buf must be of size at least uid_len + 1.
 * gen_uid() is thread-safe.
 */
void
gen_uid(char* buf, size_t uid_len)
{
    static const char UID_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, strlen(UID_CHARS) - 1);

    for (size_t i = 0; i < uid_len; ++i) {
        buf[i] = UID_CHARS[dis(gen)];
    }
    buf[uid_len] = 0x0;
}

std::string
gen_uid(size_t uid_len)
{
    char buf[uid_len + 1];
    gen_uid(buf, uid_len);

    return std::string(buf);
}

/*
 * Parse a human-supplied string into a boolean value.
 * Returns 0 if false, 1 if true, and -1 if couldn't parse.
 */
int
string_to_boolean(std::string s)
{
    // convert to all-lowercase for comparison
    for (size_t i = 0; i < s.size(); ++i) s[i] = std::tolower(s[i]);

    // big jump table
    if (s == "e")           return 1;
    if (s == "enabled")     return 1;
    if (s == "on")          return 1;
    if (s == "t")           return 1;
    if (s == "true")        return 1;
    if (s == "y")           return 1;
    if (s == "yes")         return 1;
    if (s == "1")           return 1;

    if (s == "d")           return 0;
    if (s == "disabled")    return 0;
    if (s == "off")         return 0;
    if (s == "f")           return 0;
    if (s == "false")       return 0;
    if (s == "n")           return 0;
    if (s == "no")          return 0;
    if (s == "0")           return 0;

    return -1;
}

/*
 * Parses shorthand strings, e.g., "20B" for  20 billion, to the corresponding
 * int64. TODO: check for overflow! 50+ year durations, etc. may overflow!
 */
int64_t
shorthand_to_integer(std::string s, const size_t b)
{
    assert(b == 1000 or b == 1024);


    char lastChar = toupper(s[s.size()-1]);
    int64_t multiplier = 1;

    if      (lastChar == 'K')                    multiplier = b;
    else if (lastChar == 'M')                    multiplier = b*b;
    else if (lastChar == 'B' or lastChar == 'G') multiplier = b*b*b;
    else if (lastChar == 'T')                    multiplier = b*b*b*b;
    else if (lastChar == 'Q')                    multiplier = b*b*b*b*b;

    if (multiplier != 1) s.pop_back();  // trim the last character

    // parse the string
    int64_t mant = std::stoll(s);

    return mant * multiplier;
}
