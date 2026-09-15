#ifndef WIN_MIN_H
#define WIN_MIN_H
#include <stdint.h>
#include <stddef.h>
#define WINAPI __stdcall
#define IMP __declspec(dllimport)
typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef uintptr_t ULONG_PTR;
typedef struct {long long QuadPart;} LARGE_INTEGER;
typedef struct {WORD wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds;} SYSTEMTIME;
typedef struct {DWORD dwLength,dwMemoryLoad;uint64_t ullTotalPhys,ullAvailPhys,ullTotalPageFile,ullAvailPageFile,ullTotalVirtual,ullAvailVirtual,ullAvailExtendedVirtual;} MEMORYSTATUSEX;
typedef struct {ULONG_PTR Mask;WORD Group;WORD Reserved[3];} GROUP_AFFINITY;
typedef struct {DWORD dwLowDateTime,dwHighDateTime;} FILETIME;
IMP void WINAPI ExitProcess(unsigned int);
IMP BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER*);
IMP BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER*);
IMP void* WINAPI VirtualAlloc(void*,size_t,DWORD,DWORD);
IMP BOOL WINAPI VirtualFree(void*,size_t,DWORD);
IMP HANDLE WINAPI CreateThread(void*,size_t,DWORD(WINAPI*)(void*),void*,DWORD,DWORD*);
IMP DWORD WINAPI WaitForSingleObject(HANDLE,DWORD);
IMP BOOL WINAPI CloseHandle(HANDLE);
IMP void WINAPI Sleep(DWORD);
IMP BOOL WINAPI SwitchToThread(void);
IMP HANDLE WINAPI GetCurrentThread(void);
IMP HANDLE WINAPI GetCurrentProcess(void);
IMP DWORD WINAPI GetCurrentProcessId(void);
IMP BOOL WINAPI SetThreadGroupAffinity(HANDLE,const GROUP_AFFINITY*,GROUP_AFFINITY*);
IMP BOOL WINAPI GetLogicalProcessorInformationEx(int,void*,DWORD*);
IMP BOOL WINAPI GlobalMemoryStatusEx(MEMORYSTATUSEX*);
IMP BOOL WINAPI SetConsoleOutputCP(unsigned int);
IMP BOOL WINAPI SetConsoleCtrlHandler(BOOL(WINAPI*)(DWORD),BOOL);
IMP DWORD WINAPI GetModuleFileNameW(HANDLE,wchar_t*,DWORD);
IMP int WINAPI WideCharToMultiByte(unsigned int,DWORD,const wchar_t*,int,char*,int,const char*,BOOL*);
IMP int WINAPI MultiByteToWideChar(unsigned int,DWORD,const char*,int,wchar_t*,int);
IMP wchar_t* WINAPI GetCommandLineW(void);
IMP wchar_t** WINAPI CommandLineToArgvW(const wchar_t*,int*);
IMP HANDLE WINAPI LocalFree(HANDLE);
IMP BOOL WINAPI CreateDirectoryW(const wchar_t*,void*);
IMP DWORD WINAPI GetLastError(void);
IMP void WINAPI GetSystemTime(SYSTEMTIME*);
IMP void WINAPI GetLocalTime(SYSTEMTIME*);
IMP BOOL WINAPI GetSystemTimes(FILETIME*,FILETIME*,FILETIME*);
IMP HANDLE WINAPI ShellExecuteW(HANDLE,const wchar_t*,const wchar_t*,const wchar_t*,const wchar_t*,int);
IMP HANDLE WINAPI EvtQuery(HANDLE,const wchar_t*,const wchar_t*,DWORD);
IMP BOOL WINAPI EvtNext(HANDLE,DWORD,HANDLE*,DWORD,DWORD,DWORD*);
IMP BOOL WINAPI EvtRender(HANDLE,HANDLE,DWORD,DWORD,void*,DWORD*,DWORD*);
IMP BOOL WINAPI EvtClose(HANDLE);
_Static_assert(sizeof(DWORD)==4,"DWORD ABI");
_Static_assert(sizeof(GROUP_AFFINITY)==16,"GROUP_AFFINITY ABI");
_Static_assert(sizeof(MEMORYSTATUSEX)==64,"MEMORYSTATUSEX ABI");
_Static_assert(sizeof(SYSTEMTIME)==16,"SYSTEMTIME ABI");
#endif
