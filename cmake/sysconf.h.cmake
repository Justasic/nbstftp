#pragma once

#cmakedefine HAVE_SYS_TYPES_H 1
#cmakedefine HAVE_STDINT_H 1
#cmakedefine HAVE_FCNTL_H 1
#cmakedefine HAVE_STDDEF_H 1
#cmakedefine HAVE_BACKTRACE 1
#cmakedefine HAVE_SETJMP_H 1
#cmakedefine HAVE_SYS_EPOLL_H 1
#cmakedefine HAVE_GETTIMEOFDAY 1
#cmakedefine HAVE_SETGRENT 1
#cmakedefine HAVE_STRCASECMP 1
#cmakedefine HAVE_STRICMP 1
#cmakedefine HAVE_STRINGS_H 1
#cmakedefine HAVE_STRLCAT 1
#cmakedefine HAVE_STRLCPY 1
#cmakedefine HAVE_SYS_SELECT_H 1
#cmakedefine HAVE_UMASK 1
#cmakedefine HAVE_EVENTFD 1
#cmakedefine HAVE_DLSYM 1
#cmakedefine HAVE_DLFCN_H 1
#cmakedefine HAVE_EXECINFO_H 1
#cmakedefine HAVE_STRNDUPA 1
#cmakedefine HAVE_STRNLEN 1
#cmakedefine HAVE_KQUEUE 1

#define VERSION_MAJOR        @PROJECT_MAJOR_VERSION@
#define VERSION_MINOR        @PROJECT_MINOR_VERSION@
#define VERSION_PATCH        @PROJECT_PATCH_LEVEL@
#define VERSION              "@VERSION_SIMPLE@"
#define VERSION_GIT          "@VERSION_GIT@"
#define VERSION_FULL         "@VERSION_FULL@"
#define GIT_SHA_SUM          "@VERSION_STR@"
#define COMPILED_NAME        "@PROJECT_NAME@"
#define COMPILED_ARCH        "@CMAKE_ARCH@"
#define C_COMPILER_NAME      "@CMAKE_C_COMPILER_ID@"
#define CXX_COMPILER_NAME    "@CMAKE_CXX_COMPILER_ID@"

#ifndef NDEBUG
# define CFLAGS "@CMAKE_C_FLAGS@"
# define CXXFLAGS "@CXXFLAGS@"
# define LDFLAGS "@LDFLAGS@"
#else
# define CFLAGS ""
# define CXXFLAGS ""
# define LDFLAGS ""
#endif

#cmakedefine HAVE_UINT8_T 1
#cmakedefine HAVE_U_INT8_T 1
#cmakedefine HAVE_INT16_T 1
#cmakedefine HAVE_UINT16_T 1
#cmakedefine HAVE_U_INT16_T 1
#cmakedefine HAVE_INT32_T 1
#cmakedefine HAVE_UINT32_T 1
#cmakedefine HAVE_U_INT32_T 1

//#ifdef HAVE_SETJMP_H
//# include <setjmp.h>
//#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef HAVE_UINT8_T
typedef uint8_t uint8;
#else
# ifdef HAVE_U_INT8_T
typedef u_int8_t uint8;
# else
#  ifdef _WIN32
typedef unsigned __int8 uint8;
#  else
typedef unsigned short uint8;
#  endif
# endif
#endif

#ifdef HAVE_INT16_T
typedef int16_t int16;
#else
# ifdef _WIN32
typedef signed __int16 int16;
# else
typedef int int16;
# endif
#endif

#ifdef HAVE_UINT16_T
typedef uint16_t uint16;
#else
# ifdef HAVE_U_INT16_T
typedef u_int16_t uint16;
# else
#  ifdef _WIN32
typedef unsigned __int16 uint16;
#  else
typedef unsigned int uint16;
#  endif
# endif
#endif

#ifdef HAVE_INT32_T
typedef int32_t int32;
#else
# ifdef _WIN32
typedef signed __int32 int32;
# else
typedef long int32;
# endif
#endif

#ifdef HAVE_UINT32_T
typedef uint32_t uint32;
#else
# ifdef HAVE_U_INT32_T
typedef u_int32_t uint32;
# else
#  ifdef _WIN32
typedef unsigned __int32 uint32;
#  else
typedef unsigned long uint32;
#  endif
# endif
#endif
