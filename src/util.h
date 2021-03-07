/*
 * Miscellaneous utility functions.
 * For tracer-specific utilities, see tracer_util.cpp/.h.
 */
#include <stdint.h>

#include <string>


#define MAX(x, y) (x >= y ? x : y)
#define MIN(x, y) (x <= y ? x : y)

void warn(const std::string& msg);
void die(const std::string& msg);
// for C-style gen_uid(): buf must be of size at least uid_len + 1
void gen_uid(char* buf, size_t uid_len);
std::string gen_uid(size_t len);
// both of these mute their input strings internally, so don't pass by ref.
int string_to_boolean(std::string s);
int64_t shorthand_to_integer(std::string s, const size_t b);
