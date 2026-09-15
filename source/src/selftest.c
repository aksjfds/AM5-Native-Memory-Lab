#include "lab.h"
static unsigned checks;
static bool noisy;
static void check(bool ok,const char* what){if(!ok){printf("SELFTEST FAILED: %s\n",what);die("A calculation/kernel self-test failed. Benchmark results must not be used.");}checks++;if(noisy)printf("PASS %02u  %s\n",checks,what);}
static uint64_t scalar_xor(const uint8_t* a,size_t n,int ratio){uint64_t x=0;for(size_t i=0;i<n/8;i++)if(ratio<0||((i*8)%256)<(size_t)ratio*64)x^=((const uint64_t*)a)[i];return x;}
int run_selftests(bool verbose){
    noisy=verbose;checks=0;Machine m;machine_info(&m);check(m.avx2,"AVX2 plus OS XMM/YMM state support");check(sizeof(Node)==64,"Pointer node has exactly one 64-byte cache line");
    double t=now_sec();sleep_ms(2);check(now_sec()>t,"Monotonic timer advances");check(timer_frequency()>0,"Timer frequency is valid");
    size_t n=MIB;uint8_t *a=pages_alloc(n),*b=pages_alloc(n);check(((uintptr_t)a%4096)==0,"Page allocation alignment");fill_pattern(a,n,87123);
    check(verify_pattern(a,n,87123)==0,"Address-dependent pattern round-trip");((uint64_t*)a)[19]^=1;check(verify_pattern(a,n,87123)==1,"Intentional pattern corruption is detected");((uint64_t*)a)[19]^=1;
    uint64_t reference=scalar_xor(a,n,-1);check(stream_kernel(K_READ,a,b,n)==reference,"AVX2 Read consumes every requested 64-bit word");
    stream_kernel(K_COPY_NT,a,b,n);check(memcmp(a,b,n)==0,"Non-temporal copy full-buffer verification");
    memset(b,0,n);stream_kernel(K_COPY_CACHED,a,b,n);check(memcmp(a,b,n)==0,"Cached copy full-buffer verification");
    stream_kernel(K_WRITE_NT,a,b,n);check(verify_constant(b,n,write_value())==0,"Non-temporal write full-buffer verification");
    memset(b,0,n);stream_kernel(K_WRITE_CACHED,a,b,n);check(verify_constant(b,n,write_value())==0,"Cached write full-buffer verification");
    Kernel ks[3]={K_MIX75,K_MIX50,K_MIX25};int rr[3]={3,2,1};
    for(int k=0;k<3;k++){memset(b,0xcc,n);uint64_t v=stream_kernel(ks[k],a,b,n);check(v==scalar_xor(a,n,rr[k]),"Mixed kernel read subset checksum");bool good=true;
        for(size_t i=0;i<n/8;i++){uint64_t want=((i*8)%256)>=(size_t)rr[k]*64?write_value():0xccccccccccccccccULL;if(((uint64_t*)b)[i]!=want){good=false;break;}}check(good,"Mixed kernel exact write subset and untouched addresses");}
    memset(b,0,n);check(stream_kernel(K_ALT50,a,b,n)==reference,"Alternating read/write read checksum");check(verify_constant(b,n,write_value())==0,"Alternating read/write full write coverage");
    memset(b,0,n);check(stream_kernel(K_GROUP50,a,b,n)==reference,"Grouped read/write read checksum");check(verify_constant(b,n,write_value())==0,"Grouped read/write full write coverage");
    memset(b,0,n);check(turnaround_kernel(a,b,n,64)==reference,"64B turnaround sweep consumes the full read stream");check(verify_constant(b,n,write_value())==0,"64B turnaround sweep full write coverage");
    memset(b,0,n);check(turnaround_kernel(a,b,n,65536)==reference,"64KiB turnaround sweep consumes the full read stream");check(verify_constant(b,n,write_value())==0,"64KiB turnaround sweep full write coverage");
    check(kernel_bytes(K_READ,n)==n,"Read byte accounting");check(kernel_bytes(K_WRITE_NT,n)==n,"Write byte accounting");check(kernel_bytes(K_COPY_NT,n)==n*2,"Copy read-plus-write byte accounting");check(kernel_bytes(K_ALT50,n)==kernel_bytes(K_GROUP50,n),"Grouped and alternating workloads count equal logical bytes");
    check(kernel_bytes(K_MIX75,n)==n&&kernel_bytes(K_MIX25,n)==n,"Mixed workloads count requested logical bytes, not assumed bus traffic");
    const size_t cn=256*1024;
    for(int mode=0;mode<3;mode++)for(unsigned k=1;k<=8;k*=2){Chain c;chain_build(&c,a,cn,k,mode,17492);check(chain_check(&c),"Disjoint pointer cycles cover the full working set");Node* h[8]={0};memcpy(h,c.heads,sizeof(h));chase_batch(h,k,c.count/k);bool same=true;for(unsigned j=0;j<k;j++)if(h[j]!=c.heads[j])same=false;check(same,"Pointer kernel counts loads and returns after a full cycle");}
    Chain c;chain_build(&c,a,cn,1,0,92);c.base[0].next=(Node*)(uintptr_t)1;check(!chain_check(&c),"Cycle validator rejects an intentionally invalid pointer without dereferencing it");
    Text z;text_init(&z);json_str(&z,"</script>\"\\\n&");check(strstr(z.data,"\\u003c")&&strstr(z.data,"\\u0026")&&strstr(z.data,"\\u000a"),"Embedded JSON escapes script-breaking text and controls");text_free(&z);
    text_init(&z);text_fmt(&z,"%.6f %d %.0f",2000.0/8400.0,42,(double)1073741824ULL);check(!strcmp(z.data,"0.238095 42 1073741824"),"Numeric formatting and timing math");text_free(&z);
    check(fabs(42.0*2000.0/8400.0-10.0)<1e-12,"DDR5 8400 CL42 converts to 10 ns (theory only)");
    check(fabs(1073741824.0/0.5/1e9-2.147483648)<1e-12,"Throughput uses measured bytes/seconds and decimal GB/s");
    pages_free(a,n);pages_free(b,n);if(verbose)printf("\n%u self-tests passed. No hardware stability guarantee is implied.\n",checks);return (int)checks;
}
