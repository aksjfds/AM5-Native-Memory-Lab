#ifndef LAB_STDLIB_H
#define LAB_STDLIB_H
#include <stddef.h>
__declspec(dllimport) void *malloc(size_t);
__declspec(dllimport) void *calloc(size_t,size_t);
__declspec(dllimport) void *realloc(void*,size_t);
__declspec(dllimport) void free(void*);
__declspec(dllimport) void qsort(void*,size_t,size_t,int(*)(const void*,const void*));
__declspec(dllimport) unsigned long strtoul(const char*,char**,int);
__declspec(dllimport) double strtod(const char*,char**);
__declspec(dllimport) int atoi(const char*);
__declspec(dllimport) void exit(int);
#endif
