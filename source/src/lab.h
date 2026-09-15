#ifndef LAB_H
#define LAB_H
#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>
#include <immintrin.h>
#define NOINLINE __attribute__((noinline))
#define AVX2 __attribute__((target("avx2"),noinline))
#define MAX_CORES 128
#define MAX_RESULTS 6000
#define MAX_PROFILE 96
#define MAX_DIAGNOSTICS 12
#define MIB ((size_t)1048576)
#define VERSION "2.1.0"
#define ENGINE_ID "AM5-Native-2.1.0-AVX2"
typedef struct {char *data;size_t len,cap;} Text;
void text_init(Text*);
void text_free(Text*);
void text_add(Text*,const char*);
void text_fmt(Text*,const char*,...);
void json_str(Text*,const char*);
int fmt(char*,size_t,const char*,...);
void die(const char*);

typedef struct {unsigned group;unsigned cpu;} Core;
typedef struct {char name[128];Core cores[MAX_CORES];unsigned ncores,nlogical;size_t l1,l2,l3,llc_sum;uint64_t available,total;bool avx2,hypervisor;char topology_status[128];} Machine;
void platform_init(void);
void machine_info(Machine*);
double now_sec(void);
uint64_t timer_frequency(void);
void* pages_alloc(size_t);
void pages_free(void*,size_t);
bool pin_core(Core);
void yield_cpu(void);
void sleep_ms(unsigned);
FILE* file_open(const char*,const char*);
bool make_dir(const char*);
void exe_dir(char*,size_t);
void stamp(char*,size_t,bool);
void utc_iso(char*,size_t);
void open_report(const char*);
int whea_query(const char*,const char*,char*,size_t);
double idle_cpu_usage(void);
extern atomic_bool cancelled;
extern atomic_uint_fast64_t global_sink;
#ifdef _WIN32
#include "win_min.h"
typedef HANDLE Thread;
#define THREAD_RETURN DWORD WINAPI
#define THREAD_END return 0
#else
#include <pthread.h>
typedef pthread_t Thread;
#define THREAD_RETURN void*
#define THREAD_END return NULL
#endif
bool thread_start(Thread*,THREAD_RETURN(*)(void*),void*);
void thread_join(Thread);

typedef enum {K_READ,K_WRITE_NT,K_COPY_NT,K_WRITE_CACHED,K_COPY_CACHED,K_MIX75,K_MIX50,K_MIX25,K_ALT50,K_GROUP50,K_TURN_SWEEP} Kernel;
const char* kernel_id(Kernel);
uint64_t stream_kernel(Kernel,uint8_t*,uint8_t*,size_t);
uint64_t turnaround_kernel(uint8_t*,uint8_t*,size_t,size_t);
uint64_t turnaround_events(size_t,size_t);
uint64_t pattern_word(size_t,uint64_t);
void fill_pattern(void*,size_t,uint64_t);
uint64_t verify_pattern(const void*,size_t,uint64_t);
uint64_t verify_constant(const void*,size_t,uint64_t);
uint64_t kernel_bytes(Kernel,size_t);
uint64_t write_value(void);

typedef struct Node {struct Node* next;uint8_t pad[64-sizeof(void*)];} Node;
typedef struct {Node* base;size_t bytes,count;unsigned chains;Node* heads[8];} Chain;
uint64_t rng_next(uint64_t*);
void chain_build(Chain*,void*,size_t,unsigned,int,uint64_t);
NOINLINE void chase_batch(Node**,unsigned,size_t);
bool chain_check(Chain*);

typedef struct {char key[64],value[192];} Pair;
typedef struct {Pair kv[MAX_PROFILE];size_t n;bool loaded;char path[1024];} Profile;
void profile_load(Profile*,const char*);
const char* profile_get(const Profile*,const char*);

typedef struct {
    char suite[40],test[64],unit[16];
    int trial,threads,chains;
    size_t working_bytes,pattern_bytes;
    uint64_t operations,logical_bytes,events;
    double elapsed,value,p50,p95,p99,p999,max_value,background_gbps;
    bool pin_ok;
    uint64_t errors;
} Sample;
typedef struct {int repeats;double seconds;size_t memory;int threads;bool quick,smoke,no_open,selftest;char out[1024],config[1024];} Options;
typedef struct {
    char id[40],title[96],parameter_group[192],status[32],confidence[16],evidence[512],limitation[384];
    double score,effect_pct,noise_pct,fit,estimate_ns;
} Diagnostic;
typedef struct {
    Options opt;Machine hw;Profile profile;Sample* samples;size_t nsamples;
    Diagnostic diagnostics[MAX_DIAGNOSTICS];size_t ndiagnostics;
    char outdir[2048],started[64];
    uint64_t integrity_errors,integrity_checked_bytes;
    unsigned selftests;bool complete,sufficient_memory;
    int whea;char whea_status[160];double cpu_before;double elapsed;
} Report;
void record_sample(Report*,const Sample*);
void report_write(const Report*);
int run_selftests(bool);
void suite_run(Report*);
void diagnostics_build(Report*);
#endif
