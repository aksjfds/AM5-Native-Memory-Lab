#include "lab.h"
const char* kernel_id(Kernel k){static const char* a[]={"read","write_nt","copy_nt","write_cached","copy_cached","mixed_75r25w","mixed_50r50w","mixed_25r75w","rw_alternating_64B","rw_grouped_64KiB"};return a[(int)k];}
uint64_t write_value(void){return 0xa53cc35af00d9696ULL;}
uint64_t pattern_word(size_t i,uint64_t seed){uint64_t x=(uint64_t)i+seed;x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}
void fill_pattern(void* p,size_t n,uint64_t seed){uint64_t *a=p;for(size_t i=0;i<n/8;i++)a[i]=pattern_word(i,seed);}
uint64_t verify_pattern(const void* p,size_t n,uint64_t seed){const volatile uint64_t *a=p;uint64_t bad=0;for(size_t i=0;i<n/8;i++)if(a[i]!=pattern_word(i,seed))bad++;return bad;}
uint64_t verify_constant(const void* p,size_t n,uint64_t value){const volatile uint64_t *a=p;uint64_t bad=0;for(size_t i=0;i<n/8;i++)if(a[i]!=value)bad++;return bad;}
uint64_t kernel_bytes(Kernel k,size_t n){return (k==K_COPY_NT||k==K_COPY_CACHED||k==K_ALT50||k==K_GROUP50)?(uint64_t)n*2:n;}
#define READ64(pos) do {a0=_mm256_xor_si256(a0,_mm256_load_si256((const __m256i*)(a+(pos))));a1=_mm256_xor_si256(a1,_mm256_load_si256((const __m256i*)(a+(pos)+32)));}while(0)
#define WRITE64(pos) do {_mm256_store_si256((__m256i*)(b+(pos)),val);_mm256_store_si256((__m256i*)(b+(pos)+32),val);}while(0)
AVX2 uint64_t stream_kernel(Kernel k,uint8_t* a,uint8_t* b,size_t n){
    __m256i a0=_mm256_setzero_si256(),a1=a0,a2=a0,a3=a0,val=_mm256_set1_epi64x((long long)write_value());
    switch(k){
    case K_READ:
        for(size_t i=0;i<n;i+=128){a0=_mm256_xor_si256(a0,_mm256_load_si256((const __m256i*)(a+i)));a1=_mm256_xor_si256(a1,_mm256_load_si256((const __m256i*)(a+i+32)));a2=_mm256_xor_si256(a2,_mm256_load_si256((const __m256i*)(a+i+64)));a3=_mm256_xor_si256(a3,_mm256_load_si256((const __m256i*)(a+i+96)));}break;
    case K_WRITE_NT:
        for(size_t i=0;i<n;i+=128){_mm256_stream_si256((__m256i*)(b+i),val);_mm256_stream_si256((__m256i*)(b+i+32),val);_mm256_stream_si256((__m256i*)(b+i+64),val);_mm256_stream_si256((__m256i*)(b+i+96),val);}_mm_sfence();break;
    case K_WRITE_CACHED:
        for(size_t i=0;i<n;i+=128){_mm256_store_si256((__m256i*)(b+i),val);_mm256_store_si256((__m256i*)(b+i+32),val);_mm256_store_si256((__m256i*)(b+i+64),val);_mm256_store_si256((__m256i*)(b+i+96),val);}_mm_sfence();break;
    case K_COPY_NT:
        for(size_t i=0;i<n;i+=128){_mm256_stream_si256((__m256i*)(b+i),_mm256_load_si256((const __m256i*)(a+i)));_mm256_stream_si256((__m256i*)(b+i+32),_mm256_load_si256((const __m256i*)(a+i+32)));_mm256_stream_si256((__m256i*)(b+i+64),_mm256_load_si256((const __m256i*)(a+i+64)));_mm256_stream_si256((__m256i*)(b+i+96),_mm256_load_si256((const __m256i*)(a+i+96)));}_mm_sfence();break;
    case K_COPY_CACHED:
        for(size_t i=0;i<n;i+=128){_mm256_store_si256((__m256i*)(b+i),_mm256_load_si256((const __m256i*)(a+i)));_mm256_store_si256((__m256i*)(b+i+32),_mm256_load_si256((const __m256i*)(a+i+32)));_mm256_store_si256((__m256i*)(b+i+64),_mm256_load_si256((const __m256i*)(a+i+64)));_mm256_store_si256((__m256i*)(b+i+96),_mm256_load_si256((const __m256i*)(a+i+96)));}_mm_sfence();break;
    case K_MIX75:for(size_t i=0;i<n;i+=256){READ64(i);READ64(i+64);READ64(i+128);WRITE64(i+192);}_mm_sfence();break;
    case K_MIX50:for(size_t i=0;i<n;i+=256){READ64(i);READ64(i+64);WRITE64(i+128);WRITE64(i+192);}_mm_sfence();break;
    case K_MIX25:for(size_t i=0;i<n;i+=256){READ64(i);WRITE64(i+64);WRITE64(i+128);WRITE64(i+192);}_mm_sfence();break;
    case K_ALT50:for(size_t i=0;i<n;i+=64){READ64(i);WRITE64(i);}_mm_sfence();break;
    case K_GROUP50:
        for(size_t i=0;i<n;i+=65536){size_t end=i+65536<n?i+65536:n;for(size_t j=i;j<end;j+=64){READ64(j);}for(size_t j=i;j<end;j+=64){WRITE64(j);}}_mm_sfence();break;
    }
    a0=_mm256_xor_si256(_mm256_xor_si256(a0,a1),_mm256_xor_si256(a2,a3));uint64_t r[4];_mm256_storeu_si256((__m256i*)r,a0);_mm256_zeroupper();return r[0]^r[1]^r[2]^r[3];
}
#undef READ64
#undef WRITE64
static void shuffle(uint32_t* p,size_t n,uint64_t* s){for(size_t i=n;i>1;i--){size_t j=(size_t)(rng_next(s)%i);uint32_t t=p[i-1];p[i-1]=p[j];p[j]=t;}}
void chain_build(Chain* c,void* p,size_t bytes,unsigned chains,int mode,uint64_t seed){
    memset(c,0,sizeof(*c));c->base=p;c->bytes=bytes;c->count=bytes/64;c->chains=chains;
    if(!c->count||c->count>0xffffffffULL||c->count%chains||(chains!=1&&chains!=2&&chains!=4&&chains!=8))die("Invalid pointer-chain size.");
    uint32_t *order=malloc(c->count*sizeof(uint32_t));if(!order)die("Cannot allocate pointer permutation.");
    memset(p,0,bytes);for(size_t i=0;i<c->count;i++)order[i]=(uint32_t)i;
    if(mode==0)shuffle(order,c->count,&seed);
    else if(mode==1){
        size_t pages=c->count/64;if(c->count%64)die("Page-local chain must use a multiple of 4096 bytes.");
        uint32_t* po=malloc(pages*sizeof(uint32_t));if(!po)die("Cannot allocate page permutation.");for(size_t i=0;i<pages;i++)po[i]=(uint32_t)i;shuffle(po,pages,&seed);
        for(size_t i=0;i<pages;i++){uint32_t off[64];for(unsigned j=0;j<64;j++)off[j]=j;shuffle(off,64,&seed);for(unsigned j=0;j<64;j++)order[i*64+j]=po[i]*64+off[j];}free(po);
    }
    size_t len=c->count/chains;
    for(unsigned k=0;k<chains;k++){size_t base=(size_t)k*len;c->heads[k]=&c->base[order[base]];for(size_t j=0;j<len;j++)c->base[order[base+j]].next=&c->base[order[base+(j+1)%len]];}
    free(order);
}
bool chain_check(Chain* c){uint8_t *seen=calloc(c->count,1);if(!seen)return false;size_t len=c->count/c->chains;bool ok=true;
    for(unsigned k=0;k<c->chains&&ok;k++){Node* p=c->heads[k];for(size_t j=0;j<len;j++){uintptr_t x=(uintptr_t)p,b=(uintptr_t)c->base;if(x<b||x-b>=c->bytes||(x-b)%64){ok=false;break;}size_t idx=(x-b)/64;if(seen[idx]){ok=false;break;}seen[idx]=1;p=p->next;}if(p!=c->heads[k])ok=false;}
    if(ok)for(size_t i=0;i<c->count;i++)if(!seen[i]){ok=false;break;}free(seen);return ok;
}
#define REP8(x) x;x;x;x;x;x;x;x
NOINLINE void chase_batch(Node** h,unsigned n,size_t rounds){
    Node *p0=h[0],*p1=h[1],*p2=h[2],*p3=h[3],*p4=h[4],*p5=h[5],*p6=h[6],*p7=h[7];
    switch(n){
    case 1:for(size_t i=0;i<rounds;i+=8){REP8(p0=p0->next);}break;
    case 2:for(size_t i=0;i<rounds;i+=8){REP8(p0=p0->next;p1=p1->next);}break;
    case 4:for(size_t i=0;i<rounds;i+=8){REP8(p0=p0->next;p1=p1->next;p2=p2->next;p3=p3->next);}break;
    case 8:for(size_t i=0;i<rounds;i+=8){REP8(p0=p0->next;p1=p1->next;p2=p2->next;p3=p3->next;p4=p4->next;p5=p5->next;p6=p6->next;p7=p7->next);}break;
    }
    h[0]=p0;h[1]=p1;h[2]=p2;h[3]=p3;h[4]=p4;h[5]=p5;h[6]=p6;h[7]=p7;
}
#undef REP8
