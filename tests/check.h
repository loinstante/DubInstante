// Test assertion that stays active in Release, unlike assert() under NDEBUG.
// The enclosing function must return int: CHECK returns 1 on failure.

#ifndef TESTS_CHECK_H
#define TESTS_CHECK_H

#include <cstdio>

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#endif // TESTS_CHECK_H
