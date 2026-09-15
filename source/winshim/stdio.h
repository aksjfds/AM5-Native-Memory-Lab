#ifndef LAB_STDIO_H
#define LAB_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct _iobuf FILE;
__declspec(dllimport) int printf(const char*,...);
__declspec(dllimport) int fprintf(FILE*,const char*,...);
__declspec(dllimport) int fflush(FILE*);
__declspec(dllimport) int fclose(FILE*);
__declspec(dllimport) size_t fread(void*,size_t,size_t,FILE*);
__declspec(dllimport) size_t fwrite(const void*,size_t,size_t,FILE*);
__declspec(dllimport) FILE *fopen(const char*,const char*);
__declspec(dllimport) FILE *_wfopen(const wchar_t*,const wchar_t*);
__declspec(dllimport) int _vscprintf(const char*,va_list);
__declspec(dllimport) int _vsnprintf(char*,size_t,const char*,va_list);
__declspec(dllimport) int remove(const char*);
#endif
