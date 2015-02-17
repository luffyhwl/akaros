/* See COPYRIGHT for copyright information. */

#ifndef ROS_INC_ASSERT_H
#define ROS_INC_ASSERT_H

void ( _warn)(const char* NTS, int, const char* NTS, ...);
void ( _panic)(const char* NTS, int, const char* NTS, ...)
	__attribute__((noreturn));

#define warn(...) _warn(__FILE__, __LINE__, __VA_ARGS__)
#define warn_once(...) run_once_racy(warn(__VA_ARGS__))
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)
#define exhausted(...) _panic(__FILE__, __LINE__, __VA_ARGS__)


#define check(x)		\
	do { if (!(x)) warn("warning failed: %s", #x); } while (0)

#define assert(x)		\
	do { if (!(x)) panic("assertion failed: %s", #x); } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):

#ifdef CONFIG_DEVELOPMENT_ASSERTIONS
#define dassert(x) assert(x)
#else
#define dassert(x) ((void) (x))  // 'Use' value, stop compile warnings
#endif /* DEVELOPMENT_ASSERTIONS */

#endif /* !ROS_INC_ASSERT_H */
