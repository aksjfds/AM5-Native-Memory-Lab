#include <stddef.h>
__declspec(dllimport) void *memcpy(void*,const void*,size_t);
__declspec(dllimport) void *memmove(void*,const void*,size_t);
__declspec(dllimport) void *memset(void*,int,size_t);
__declspec(dllimport) int memcmp(const void*,const void*,size_t);
__declspec(dllimport) size_t strlen(const char*);
__declspec(dllimport) int strcmp(const char*,const char*);
__declspec(dllimport) int strncmp(const char*,const char*,size_t);
__declspec(dllimport) char *strchr(const char*,int);
__declspec(dllimport) char *strrchr(const char*,int);
__declspec(dllimport) char *strstr(const char*,const char*);
