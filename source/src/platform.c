#include "lab.h"
#ifdef _WIN32
int _fltused = 0;
#else
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#endif
atomic_bool cancelled=false;
atomic_uint_fast64_t global_sink=0;
static uint64_t qfreq=1000000000ULL;
static void cpuid(unsigned leaf,unsigned sub,unsigned v[4]){
    __asm__ volatile("cpuid":"=a"(v[0]),"=b"(v[1]),"=c"(v[2]),"=d"(v[3]):"a"(leaf),"c"(sub));
}
static bool has_avx2(void){
    unsigned v[4];cpuid(0,0,v);if(v[0]<7)return false;
    cpuid(1,0,v);if((v[2]&(1u<<27))==0||(v[2]&(1u<<28))==0)return false;
    unsigned lo,hi;__asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0));
    if((lo&6)!=6)return false;cpuid(7,0,v);return (v[1]&(1u<<5))!=0;
}
#ifdef _WIN32
static BOOL WINAPI ctrl_handler(DWORD v){if(v<=2){atomic_store(&cancelled,true);return 1;}return 0;}
#else
static void ctrl_handler(int v){(void)v;atomic_store(&cancelled,true);}
#endif
void platform_init(void){
#ifdef _WIN32
    LARGE_INTEGER x;if(!QueryPerformanceFrequency(&x)||x.QuadPart<=0)die("QPC initialization failed.");
    qfreq=(uint64_t)x.QuadPart;SetConsoleOutputCP(65001);SetConsoleCtrlHandler(ctrl_handler,1);
#else
    signal(SIGINT,ctrl_handler);signal(SIGTERM,ctrl_handler);
#endif
}
uint64_t timer_frequency(void){return qfreq;}
double now_sec(void){
#ifdef _WIN32
    LARGE_INTEGER x;QueryPerformanceCounter(&x);return (double)x.QuadPart/(double)qfreq;
#else
    struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (double)ts.tv_sec+ts.tv_nsec*1e-9;
#endif
}
void sleep_ms(unsigned n){
#ifdef _WIN32
    Sleep(n);
#else
    struct timespec t={(time_t)(n/1000),(long)(n%1000)*1000000};nanosleep(&t,0);
#endif
}
void yield_cpu(void){
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}
void* pages_alloc(size_t bytes){
#ifdef _WIN32
    void *p=VirtualAlloc(0,bytes,0x1000|0x2000,0x04);
#else
    void *p=mmap(0,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED)p=0;
#ifdef MADV_NOHUGEPAGE
    if(p)madvise(p,bytes,MADV_NOHUGEPAGE);
#endif
#endif
    if(!p)die("Memory allocation failed. Close other memory-intensive programs or reduce --memory-mib.");
    return p;
}
void pages_free(void* p,size_t n){if(!p)return;
#ifdef _WIN32
    (void)n;VirtualFree(p,0,0x8000);
#else
    munmap(p,n);
#endif
}
bool pin_core(Core c){
#ifdef _WIN32
    GROUP_AFFINITY g={0};g.Mask=(ULONG_PTR)1<<c.cpu;g.Group=(WORD)c.group;
    return SetThreadGroupAffinity(GetCurrentThread(),&g,0)!=0;
#else
    cpu_set_t set;CPU_ZERO(&set);if(c.cpu>=CPU_SETSIZE)return false;CPU_SET(c.cpu,&set);
    return pthread_setaffinity_np(pthread_self(),sizeof(set),&set)==0;
#endif
}
bool thread_start(Thread* t,THREAD_RETURN(*f)(void*),void* a){
#ifdef _WIN32
    *t=CreateThread(0,0,f,a,0,0);return *t!=0;
#else
    return pthread_create(t,0,f,a)==0;
#endif
}
void thread_join(Thread t){
#ifdef _WIN32
    WaitForSingleObject(t,0xffffffffUL);CloseHandle(t);
#else
    pthread_join(t,0);
#endif
}
static void cache_cpuid(Machine* m){
    unsigned v[4],leaf=4;cpuid(0x80000000,0,v);if(v[0]>=0x8000001d)leaf=0x8000001d;
    for(unsigned i=0;i<16;i++){
        cpuid(leaf,i,v);unsigned type=v[0]&31,level=(v[0]>>5)&7;if(!type)break;
        size_t size=(size_t)((v[1]&4095)+1)*(((v[1]>>12)&1023)+1)*(((v[1]>>22)&1023)+1)*((size_t)v[2]+1);
        if(level==1&&type==1&&size>m->l1)m->l1=size;
        if(level==2&&size>m->l2)m->l2=size;
        if(level==3&&size>m->l3)m->l3=size;
    }
}
void machine_info(Machine* m){
    memset(m,0,sizeof(*m));unsigned v[4];cpuid(0x80000000,0,v);
    if(v[0]>=0x80000004){for(unsigned i=0;i<3;i++){cpuid(0x80000002+i,0,v);memcpy(m->name+i*16,v,16);}m->name[48]=0;}
    else fmt(m->name,sizeof(m->name),"x86-64 CPU");
    cpuid(1,0,v);m->hypervisor=(v[2]>>31)!=0;m->avx2=has_avx2();cache_cpuid(m);
#ifdef _WIN32
    DWORD len=0;GetLogicalProcessorInformationEx(0,0,&len);
    uint8_t *buf=len?malloc(len):0;
    if(buf&&GetLogicalProcessorInformationEx(0,buf,&len)){
        size_t p=0;while(p+8<=len){DWORD rel,size;memcpy(&rel,buf+p,4);memcpy(&size,buf+p+4,4);if(size<8||p+size>len)break;
            if(rel==0&&size>=48){WORD gc;memcpy(&gc,buf+p+8+22,2);bool chosen=false;
                for(unsigned g=0;g<gc&&p+8+24+(g+1)*16<=p+size;g++){
                    GROUP_AFFINITY a;memcpy(&a,buf+p+8+24+g*16,16);
                    for(unsigned b=0;b<64;b++)if((a.Mask>>b)&1){m->nlogical++;if(!chosen&&m->ncores<MAX_CORES){m->cores[m->ncores++]=(Core){a.Group,b};chosen=true;}}
                }
            }p+=size;
        }
    }free(buf);
    len=0;GetLogicalProcessorInformationEx(2,0,&len);buf=len?malloc(len):0;
    if(buf&&GetLogicalProcessorInformationEx(2,buf,&len)){size_t p=0;while(p+8<=len){DWORD rel,size;memcpy(&rel,buf+p,4);memcpy(&size,buf+p+4,4);if(size<8||p+size>len)break;
        if(rel==2&&size>=16){BYTE lev=buf[p+8];DWORD cap;memcpy(&cap,buf+p+12,4);if(lev==3)m->llc_sum+=cap;}
        p+=size;}}
    free(buf);
    if(m->ncores)fmt(m->topology_status,sizeof(m->topology_status),"GetLogicalProcessorInformationEx: one logical CPU per physical core");
    MEMORYSTATUSEX s={0};s.dwLength=sizeof(s);if(GlobalMemoryStatusEx(&s)){m->total=s.ullTotalPhys;m->available=s.ullAvailPhys;}
#else
    cpu_set_t set;CPU_ZERO(&set);if(!sched_getaffinity(0,sizeof(set),&set)){
        for(unsigned i=0;i<CPU_SETSIZE&&m->ncores<MAX_CORES;i++)if(CPU_ISSET(i,&set)){m->cores[m->ncores++]=(Core){0,i};m->nlogical++;}
    }
    fmt(m->topology_status,sizeof(m->topology_status),"Linux developer environment: allowed logical CPUs (not target hardware)");
    struct sysinfo si;if(!sysinfo(&si)){m->total=(uint64_t)si.totalram*si.mem_unit;m->available=(uint64_t)(si.freeram+si.bufferram)*si.mem_unit;}
#endif
    if(!m->ncores){m->ncores=m->nlogical=1;m->cores[0]=(Core){0,0};fmt(m->topology_status,sizeof(m->topology_status),"Topology unavailable: single-core fallback; affinity may fail");}
    if(!m->l1)m->l1=32*1024;if(!m->l2)m->l2=1024*1024;if(!m->l3)m->l3=32*MIB;
    if(!m->llc_sum)m->llc_sum=m->l3;
}
#ifdef _WIN32
static wchar_t* wide(const char* s){int n=MultiByteToWideChar(65001,0,s,-1,0,0);if(n<=0)return 0;wchar_t* p=malloc((size_t)n*sizeof(wchar_t));if(p)MultiByteToWideChar(65001,0,s,-1,p,n);return p;}
#endif
FILE* file_open(const char* name,const char* mode){
#ifdef _WIN32
    wchar_t *w=wide(name),*md=wide(mode);FILE* f=w&&md?_wfopen(w,md):0;free(w);free(md);return f;
#else
    return fopen(name,mode);
#endif
}
bool make_dir(const char* name){
#ifdef _WIN32
    wchar_t *w=wide(name);if(!w)return false;BOOL r=CreateDirectoryW(w,0);DWORD e=GetLastError();free(w);return r||e==183;
#else
    return mkdir(name,0755)==0||errno==EEXIST;
#endif
}
void exe_dir(char* out,size_t cap){
#ifdef _WIN32
    wchar_t *w=calloc(32768,sizeof(wchar_t));DWORD n=GetModuleFileNameW(0,w,32768);if(!n||n>=32768)die("Cannot determine executable folder.");
    int len=WideCharToMultiByte(65001,0,w,-1,0,0,0,0);if(len<=0||(size_t)len>cap)die("Executable path too long for this build (max 2047 UTF-8 bytes).");
    WideCharToMultiByte(65001,0,w,-1,out,(int)cap,0,0);free(w);
#else
    ssize_t n=readlink("/proc/self/exe",out,cap-1);if(n<0)die("Cannot determine executable path.");out[n]=0;
#endif
    char *a=strrchr(out,'/'),*b=strrchr(out,'\\');if(b&&(!a||b>a))a=b;if(a)*a=0;else fmt(out,cap,".");
}
void stamp(char* out,size_t cap,bool compact){
#ifdef _WIN32
    SYSTEMTIME s;GetLocalTime(&s);fmt(out,cap,compact?"%04u%02u%02u_%02u%02u%02u_%lu":"%04u-%02u-%02u %02u:%02u:%02u / PID %lu",s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute,s.wSecond,GetCurrentProcessId());
#else
    time_t t=time(0);struct tm s;localtime_r(&t,&s);fmt(out,cap,compact?"%04d%02d%02d_%02d%02d%02d_%ld":"%04d-%02d-%02d %02d:%02d:%02d / PID %ld",s.tm_year+1900,s.tm_mon+1,s.tm_mday,s.tm_hour,s.tm_min,s.tm_sec,(long)getpid());
#endif
}
void utc_iso(char* out,size_t cap){
#ifdef _WIN32
    SYSTEMTIME s;GetSystemTime(&s);fmt(out,cap,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute,s.wSecond,s.wMilliseconds);
#else
    time_t t=time(0);struct tm s;gmtime_r(&t,&s);fmt(out,cap,"%04d-%02d-%02dT%02d:%02d:%02d.000Z",s.tm_year+1900,s.tm_mon+1,s.tm_mday,s.tm_hour,s.tm_min,s.tm_sec);
#endif
}
void open_report(const char* name){
#ifdef _WIN32
    wchar_t* w=wide(name);if(w){HANDLE r=ShellExecuteW(0,L"open",w,0,0,1);if((uintptr_t)r<=32)printf("Report saved; open report.html manually.\n");free(w);}
#else
    (void)name;
#endif
}
int whea_query(const char* since,const char* folder,char* status,size_t cap){
#ifdef _WIN32
    char query[512];fmt(query,sizeof(query),"*[System[Provider[@Name='Microsoft-Windows-WHEA-Logger'] and TimeCreated[@SystemTime >= '%s']]]",since);
    wchar_t *wq=wide(query);HANDLE q=EvtQuery(0,L"System",wq,0x1);free(wq);
    if(!q){fmt(status,cap,"unavailable: EvtQuery error %lu",GetLastError());return -1;}
    int count=0;HANDLE ev[8];DWORD n=0;Text xml;text_init(&xml);text_add(&xml,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<events>\n");
    while(count<256&&EvtNext(q,8,ev,0,0,&n)){
        for(unsigned i=0;i<n;i++){count++;DWORD len=0,props=0;EvtRender(0,ev[i],1,0,0,&len,&props);
            if(len){wchar_t* wb=malloc(len);if(wb&&EvtRender(0,ev[i],1,len,wb,&len,&props)){int m=WideCharToMultiByte(65001,0,wb,-1,0,0,0,0);if(m>0){char* b=malloc(m);if(b){WideCharToMultiByte(65001,0,wb,-1,b,m,0,0);text_add(&xml,b);text_add(&xml,"\n");free(b);}}}free(wb);}EvtClose(ev[i]);}
    }
    DWORD last=GetLastError();EvtClose(q);text_add(&xml,"</events>\n");
    if(count){char path[2400];fmt(path,sizeof(path),"%s/whea-events.xml",folder);FILE* f=file_open(path,"wb");if(f){fwrite(xml.data,1,xml.len,f);fclose(f);}}
    text_free(&xml);
    if(last!=259&&count<256){fmt(status,cap,"partial/unavailable: EvtNext error %lu; observed %d events",last,count);return -1;}
    fmt(status,cap,count>=256?"256+ WHEA events observed during session; truncated":"System WHEA-Logger query completed; events are not automatically RAM-attributed");return count;
#else
    (void)since;(void)folder;fmt(status,cap,"unavailable on Linux developer environment");return -1;
#endif
}
#ifdef _WIN32
static uint64_t ft(FILETIME x){return ((uint64_t)x.dwHighDateTime<<32)|x.dwLowDateTime;}
#endif
double idle_cpu_usage(void){
#ifdef _WIN32
    FILETIME i1,k1,u1,i2,k2,u2;if(!GetSystemTimes(&i1,&k1,&u1))return -1;sleep_ms(700);if(!GetSystemTimes(&i2,&k2,&u2))return -1;
    uint64_t all=(ft(k2)-ft(k1))+(ft(u2)-ft(u1)),idle=ft(i2)-ft(i1);return all?100.0*(double)(all-idle)/(double)all:-1;
#else
    return -1;
#endif
}
