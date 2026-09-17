#include "lab.h"

typedef struct Shared Shared;
typedef struct {
    Shared* shared;
    Core core;
    uint8_t *a,*b;
    size_t n,pattern_bytes;
    Kernel kernel;
    uint64_t bytes,passes,events,sink,errors,expected;
    double finished;
    bool pinned;
    Thread thread;
} Worker;
struct Shared {atomic_int ready;atomic_bool go;double start,end;};

typedef struct {Kernel k;int t;} Job;

static uint64_t xor_words(const void* p,size_t n){const uint64_t *a=p;uint64_t v=0;for(size_t i=0;i<n/8;i++)v^=a[i];return v;}
static uint64_t worker_kernel(Worker* w){return w->kernel==K_TURN_SWEEP?turnaround_kernel(w->a,w->b,w->n,w->pattern_bytes):stream_kernel(w->kernel,w->a,w->b,w->n);}
static THREAD_RETURN worker_run(void* param){
    Worker* w=param;w->pinned=pin_core(w->core);w->sink=worker_kernel(w);
    atomic_fetch_add(&w->shared->ready,1);
    while(!atomic_load_explicit(&w->shared->go,memory_order_acquire))yield_cpu();
    do {
        if(atomic_load(&cancelled))break;
        uint64_t v=worker_kernel(w);w->sink+=v+1;w->passes++;
        if((w->kernel==K_READ||w->kernel==K_TURN_SWEEP)&&v!=w->expected)w->errors++;
    }while(now_sec()<w->shared->end);
    w->finished=now_sec();w->bytes=kernel_bytes(w->kernel,w->n)*w->passes;
    if(w->kernel==K_TURN_SWEEP)w->events=turnaround_events(w->n,w->pattern_bytes)*w->passes+(w->passes?w->passes-1:0);
    atomic_fetch_xor(&global_sink,w->sink);THREAD_END;
}
static Worker* workers_launch(Report* r,Shared* s,uint8_t* a,uint8_t* b,size_t bytes,Kernel k,int threads,int first,size_t pattern_bytes){
    if(threads<=0||first<0||(unsigned)(first+threads)>r->hw.ncores)die("Internal worker/core range is invalid.");
    size_t blocks=bytes/4096;if(blocks<(size_t)threads)die("Too many threads for the working set.");
    Worker* w=calloc((size_t)threads,sizeof(*w));if(!w)die("Cannot allocate worker metadata.");
    atomic_init(&s->ready,0);atomic_init(&s->go,false);
    for(int i=0;i<threads;i++){
        size_t begin=blocks*(size_t)i/(size_t)threads*4096,end=blocks*(size_t)(i+1)/(size_t)threads*4096;
        w[i].shared=s;w[i].core=r->hw.cores[first+i];w[i].a=a+begin;w[i].b=b+begin;w[i].n=end-begin;w[i].kernel=k;w[i].pattern_bytes=pattern_bytes;
        if(w[i].n<4096){free(w);die("Too many threads for the working set.");}
        if(k==K_READ||k==K_TURN_SWEEP)w[i].expected=xor_words(w[i].a,w[i].n);
        if(!thread_start(&w[i].thread,worker_run,&w[i])){
            s->start=now_sec();s->end=s->start;atomic_store(&cancelled,true);atomic_store_explicit(&s->go,true,memory_order_release);
            for(int j=0;j<i;j++)thread_join(w[j].thread);free(w);die("Thread creation failed.");
        }
    }
    while(atomic_load(&s->ready)<threads)yield_cpu();return w;
}
static void gate_start(Shared* s,double seconds){s->start=now_sec();s->end=s->start+seconds;atomic_store_explicit(&s->go,true,memory_order_release);}
static Sample bandwidth_ex(Report* r,uint8_t* a,uint8_t* b,size_t n,Kernel k,int threads,const char* suite,const char* test,int trial,size_t pattern_bytes){
    Sample x={0};fmt(x.suite,sizeof(x.suite),"%s",suite);fmt(x.test,sizeof(x.test),"%s",test?test:kernel_id(k));fmt(x.unit,sizeof(x.unit),"GB/s");
    x.trial=trial;x.threads=threads;x.working_bytes=n;x.pattern_bytes=pattern_bytes;x.pin_ok=true;
    Shared s;Worker* w=workers_launch(r,&s,a,b,n,k,threads,0,pattern_bytes);gate_start(&s,r->opt.seconds);double end=s.start;
    for(int i=0;i<threads;i++){
        thread_join(w[i].thread);if(w[i].finished>end)end=w[i].finished;
        x.logical_bytes+=w[i].bytes;x.operations+=w[i].passes;x.events+=w[i].events;x.pin_ok=x.pin_ok&&w[i].pinned;x.errors+=w[i].errors;
    }
    x.elapsed=end-s.start;x.value=x.elapsed>0?(double)x.logical_bytes/x.elapsed/1e9:0;free(w);
    if(k==K_WRITE_NT||k==K_WRITE_CACHED||k==K_TURN_SWEEP){x.errors+=verify_constant(b,n,write_value());r->integrity_checked_bytes+=n;}
    if(k==K_COPY_NT||k==K_COPY_CACHED){if(memcmp(a,b,n)!=0)x.errors++;r->integrity_checked_bytes+=n;}
    r->integrity_errors+=x.errors;return x;
}
static Sample bandwidth(Report* r,uint8_t* a,uint8_t* b,size_t n,Kernel k,int threads,const char* suite,int trial){return bandwidth_ex(r,a,b,n,k,threads,suite,NULL,trial,0);}

static int doublecmp(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static double quantile_sorted(const double* a,size_t n,double p){if(!n)return 0;double idx=(n-1)*p;size_t lo=(size_t)idx,hi=lo+1<n?lo+1:lo;return a[lo]+(a[hi]-a[lo])*(idx-lo);}
static void finish_windows(Sample* x,double* windows,size_t stored){
    qsort(windows,stored,sizeof(double),doublecmp);x->p50=quantile_sorted(windows,stored,.5);x->p95=quantile_sorted(windows,stored,.95);x->p99=quantile_sorted(windows,stored,.99);x->p999=quantile_sorted(windows,stored,.999);x->max_value=stored?windows[stored-1]:0;
}
static Sample latency_measure(Report* r,Chain* c,const char* suite,const char* test,int trial,double deadline){
    Sample x={0};fmt(x.suite,sizeof(x.suite),"%s",suite);fmt(x.test,sizeof(x.test),"%s",test);fmt(x.unit,sizeof(x.unit),"ns/access");
    x.trial=trial;x.threads=1;x.chains=(int)c->chains;x.working_bytes=c->bytes;x.pin_ok=pin_core(r->hw.cores[0]);
    Node* h[8]={0};memcpy(h,c->heads,sizeof(h));double *windows=malloc(8192*sizeof(double));if(!windows)die("Cannot allocate latency samples.");
    size_t stored=0,seen=0;uint64_t seed=0x9817a83d1ULL;size_t rounds=8192/c->chains;
    if(deadline<=0){chase_batch(h,c->chains,8192);deadline=now_sec()+r->opt.seconds;}
    while(!atomic_load(&cancelled)&&now_sec()<deadline){
        double a=now_sec();chase_batch(h,c->chains,rounds);double b=now_sec(),dt=b-a;if(dt<=0)continue;
        x.operations+=(uint64_t)rounds*c->chains;x.elapsed+=dt;double ns=dt*1e9/((double)rounds*c->chains);seen++;
        if(stored<8192)windows[stored++]=ns;else {size_t j=(size_t)(rng_next(&seed)%seen);if(j<8192)windows[j]=ns;}
    }
    x.value=x.operations?x.elapsed*1e9/(double)x.operations:0;finish_windows(&x,windows,stored);free(windows);
    uint64_t sink=0;for(unsigned i=0;i<c->chains;i++)sink^=(uintptr_t)h[i];atomic_fetch_xor(&global_sink,sink);return x;
}
static Sample stall_probe_measure(Report* r,Chain* c,const char* suite,const char* test,int trial,double deadline){
    Sample x={0};fmt(x.suite,sizeof(x.suite),"%s",suite);fmt(x.test,sizeof(x.test),"%s",test);fmt(x.unit,sizeof(x.unit),"ns/access");
    x.trial=trial;x.threads=1;x.chains=1;x.working_bytes=c->bytes;x.pattern_bytes=64;x.pin_ok=pin_core(r->hw.cores[0]);
    Node* h[8]={c->heads[0]};chase_batch(h,1,8192);const size_t cap=32768,batch=64;double *windows=malloc(cap*sizeof(double));if(!windows)die("Cannot allocate stall probe samples.");
    size_t stored=0,seen=0;uint64_t seed=0x5e1187b93ULL;if(deadline<=0)deadline=now_sec()+r->opt.seconds;
    while(!atomic_load(&cancelled)&&now_sec()<deadline){
        double a=now_sec();chase_batch(h,1,batch);double b=now_sec(),dt=b-a;if(dt<=0)continue;double ns=dt*1e9/(double)batch;
        x.operations+=batch;x.elapsed+=dt;seen++;if(stored<cap)windows[stored++]=ns;else {size_t j=(size_t)(rng_next(&seed)%seen);if(j<cap)windows[j]=ns;}
    }
    x.value=x.operations?x.elapsed*1e9/(double)x.operations:0;finish_windows(&x,windows,stored);free(windows);atomic_fetch_xor(&global_sink,(uintptr_t)h[0]);return x;
}
static Sample loaded(Report* r,Chain* c,uint8_t* a,uint8_t* b,size_t bytes,Kernel k,int n,int trial){
    if(n==0){Sample x=latency_measure(r,c,"loaded_latency","idle",trial,0);x.threads=0;return x;}
    pin_core(r->hw.cores[0]);Node* warm[8]={0};memcpy(warm,c->heads,sizeof(warm));chase_batch(warm,1,8192);
    Shared s;Worker* w=workers_launch(r,&s,a,b,bytes,k,n,1,0);gate_start(&s,r->opt.seconds);
    const char* name=k==K_READ?"read_load":k==K_COPY_NT?"copy_load":"mixed_load";Sample x=latency_measure(r,c,"loaded_latency",name,trial,s.end);x.threads=n;uint64_t traffic=0;double end=s.start;
    for(int i=0;i<n;i++){thread_join(w[i].thread);traffic+=w[i].bytes;if(w[i].finished>end)end=w[i].finished;x.pin_ok=x.pin_ok&&w[i].pinned;x.errors+=w[i].errors;}
    if(k==K_COPY_NT){if(memcmp(a,b,bytes)!=0)x.errors++;r->integrity_checked_bytes+=bytes;}
    x.background_gbps=end>s.start?(double)traffic/(end-s.start)/1e9:0;r->integrity_errors+=x.errors;free(w);return x;
}
static Sample loaded_stall_copy(Report* r,Chain* c,uint8_t* a,uint8_t* b,size_t bytes,int n,int trial){
    if(n<=0)return stall_probe_measure(r,c,"copy_stall_probe","idle_short_window",trial,0);
    pin_core(r->hw.cores[0]);Node* warm[8]={c->heads[0]};chase_batch(warm,1,8192);
    Shared s;Worker* w=workers_launch(r,&s,a,b,bytes,K_COPY_NT,n,1,0);gate_start(&s,r->opt.seconds);
    Sample x=stall_probe_measure(r,c,"copy_stall_probe","copy_load_short_window",trial,s.end);x.threads=n;uint64_t traffic=0;double end=s.start;
    for(int i=0;i<n;i++){thread_join(w[i].thread);traffic+=w[i].bytes;if(w[i].finished>end)end=w[i].finished;x.pin_ok=x.pin_ok&&w[i].pinned;x.errors+=w[i].errors;}
    if(memcmp(a,b,bytes)!=0)x.errors++;r->integrity_checked_bytes+=bytes;
    x.background_gbps=end>s.start?(double)traffic/(end-s.start)/1e9:0;r->integrity_errors+=x.errors;free(w);return x;
}
void record_sample(Report* r,const Sample* x){
    if(r->nsamples>=MAX_RESULTS)die("Result limit exceeded.");r->samples[r->nsamples++]=*x;
    char path[2400];fmt(path,sizeof(path),"%s/raw.csv",r->outdir);FILE* f=file_open(path,r->nsamples==1?"wb":"ab");if(!f)die("Cannot write raw.csv; extract the program to a writable folder.");
    if(r->nsamples==1)fprintf(f,"suite,test,trial,threads,chains,working_bytes,pattern_bytes,operations,events,logical_bytes,elapsed_s,value,unit,batch_p50_ns,batch_p95_ns,batch_p99_ns,batch_p999_ns,batch_max_ns,background_GBps,affinity_ok,errors\n");
    fprintf(f,"%s,%s,%d,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.9f,%.9f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.0f\n",x->suite,x->test,x->trial,x->threads,x->chains,(double)x->working_bytes,(double)x->pattern_bytes,(double)x->operations,(double)x->events,(double)x->logical_bytes,x->elapsed,x->value,x->unit,x->p50,x->p95,x->p99,x->p999,x->max_value,x->background_gbps,x->pin_ok,(double)x->errors);
    bool failed=ferror(f)!=0;if(fclose(f)!=0)failed=true;if(failed)die("Failed while writing raw.csv.");
    printf("  %-18s %-24s t=%d run=%d %9.3f %-9s%s\n",x->suite,x->test,x->threads,x->trial,x->value,x->unit,x->errors?" DATA MISMATCH":"");fflush(0);
}

static void permute_int(int* a,int n,uint64_t* seed){for(int i=n;i>1;i--){int j=(int)(rng_next(seed)%(unsigned)i),tmp=a[i-1];a[i-1]=a[j];a[j]=tmp;}}
static bool job_exists(const Job* jobs,int n,Kernel k,int t){for(int i=0;i<n;i++)if(jobs[i].k==k&&jobs[i].t==t)return true;return false;}
static void add_job(Job* jobs,int* n,size_t cap,Kernel k,int t){
    if(t<=0||job_exists(jobs,*n,k,t))return;
    if((size_t)*n>=cap)die("Internal job list overflow.");jobs[(*n)++]=(Job){k,t};
}

void suite_run(Report* r){
    const size_t n=r->opt.memory;int nt=r->opt.threads;uint64_t seed=0x947b29378ULL;double started=now_sec();pin_core(r->hw.cores[0]);
    printf("\n[1/7] Allocate and pre-fault arrays; full-buffer integrity checks\n");fflush(0);
    uint8_t* a=pages_alloc(n),*b=pages_alloc(n);
    for(unsigned pass=0;pass<2&&!atomic_load(&cancelled);pass++){uint64_t sd=pass?0xf02acbb16ULL:0x34867127ULL;fill_pattern(a,n,sd);r->integrity_errors+=verify_pattern(a,n,sd);r->integrity_checked_bytes+=n;}
    fill_pattern(a,n,0x12345987ULL);memset(b,0,n);
    if(r->integrity_errors){printf("Data errors detected before benchmarks. Copy diagnosis will be suppressed.\n");atomic_store(&cancelled,true);}

    printf("\n[2/7] Copy baseline, isolated read/write references, and Copy thread scaling\n");fflush(0);
    Job jobs[32];int nj=0;int counts[5]={1,2,4,8,nt};
    for(int i=0;i<5;i++)if(counts[i]<=nt)add_job(jobs,&nj,sizeof(jobs)/sizeof(jobs[0]),K_COPY_NT,counts[i]);
    add_job(jobs,&nj,sizeof(jobs)/sizeof(jobs[0]),K_READ,1);add_job(jobs,&nj,sizeof(jobs)/sizeof(jobs[0]),K_READ,nt);
    add_job(jobs,&nj,sizeof(jobs)/sizeof(jobs[0]),K_WRITE_NT,1);add_job(jobs,&nj,sizeof(jobs)/sizeof(jobs[0]),K_WRITE_NT,nt);add_job(jobs,&nj,sizeof(jobs)/sizeof(jobs[0]),K_COPY_CACHED,nt);
    for(int rep=1;rep<=r->opt.repeats&&!atomic_load(&cancelled);rep++){
        int order[32];for(int i=0;i<nj;i++)order[i]=i;permute_int(order,nj,&seed);
        for(int j=0;j<nj&&!atomic_load(&cancelled);j++){Job x=jobs[order[j]];Sample s=bandwidth(r,a,b,n,x.k,x.t,"copy_baseline",rep);record_sample(r,&s);if(s.errors)atomic_store(&cancelled,true);}
    }
    report_write(r);

    printf("\n[3/7] Copy-relevant read/write turnaround sweep (fixed 50/50 bytes)\n");fflush(0);
    const size_t groups[10]={64,128,256,512,1024,2048,4096,8192,16384,65536};
    for(int rep=1;rep<=r->opt.repeats&&!atomic_load(&cancelled);rep++){
        int order[10]={0,1,2,3,4,5,6,7,8,9};permute_int(order,10,&seed);
        for(int j=0;j<10&&!atomic_load(&cancelled);j++){size_t g=groups[order[j]];char name[64];fmt(name,sizeof(name),"rw_group_%lluB",(unsigned long long)g);Sample s=bandwidth_ex(r,a,b,n,K_TURN_SWEEP,nt,"copy_turnaround",name,rep,g);record_sample(r,&s);if(s.errors)atomic_store(&cancelled,true);}
    }
    report_write(r);

    printf("\n[4/7] Address/parallelism proxies used only when they can support Copy diagnosis\n");fflush(0);
    uint8_t* cp=pages_alloc(n);int modes[5]={0,1,0,0,0};unsigned chains[5]={1,1,2,4,8};const char* names[5]={"random_latency","page_local_latency","parallel_2_chains","parallel_4_chains","parallel_8_chains"};
    for(int idx=0;idx<5&&!atomic_load(&cancelled);idx++){Chain c;chain_build(&c,cp,n,chains[idx],modes[idx],0x719ff02);for(int rep=1;rep<=r->opt.repeats&&!atomic_load(&cancelled);rep++){Sample s=latency_measure(r,&c,"copy_path_proxy",names[idx],rep,0);record_sample(r,&s);}}
    report_write(r);

    printf("\n[5/7] Idle short-window stall distribution (refresh/tail reference only)\n");fflush(0);
    if(!atomic_load(&cancelled)){Chain c;chain_build(&c,cp,n,1,0,0x3190ab27);for(int rep=1;rep<=r->opt.repeats&&!atomic_load(&cancelled);rep++){Sample s=stall_probe_measure(r,&c,"copy_stall_probe","idle_short_window",rep,0);record_sample(r,&s);}}
    report_write(r);

    printf("\n[6/7] Random latency under read, mixed, and Copy background traffic\n");fflush(0);
    if(!atomic_load(&cancelled)){
        Chain c;chain_build(&c,cp,n,1,0,0x719ff02);int bg=nt>1?nt-1:0;Job loaded_jobs[12];int nl=0;loaded_jobs[nl++]=(Job){K_READ,0};
        if(bg>0){
            add_job(loaded_jobs,&nl,sizeof(loaded_jobs)/sizeof(loaded_jobs[0]),K_READ,bg);add_job(loaded_jobs,&nl,sizeof(loaded_jobs)/sizeof(loaded_jobs[0]),K_MIX50,bg);
            int copy_counts[4]={1,2,4,bg};for(int i=0;i<4;i++)if(copy_counts[i]<=bg)add_job(loaded_jobs,&nl,sizeof(loaded_jobs)/sizeof(loaded_jobs[0]),K_COPY_NT,copy_counts[i]);
        }
        for(int rep=1;rep<=r->opt.repeats&&!atomic_load(&cancelled);rep++){
            int order[12];for(int i=0;i<nl;i++)order[i]=i;permute_int(order,nl,&seed);
            for(int j=0;j<nl&&!atomic_load(&cancelled);j++){Job job=loaded_jobs[order[j]];Sample s=loaded(r,&c,a,b,n,job.k,job.t,rep);record_sample(r,&s);if(s.errors)atomic_store(&cancelled,true);}
        }
    }
    report_write(r);

    printf("\n[7/7] Short-window tail probe while Copy traffic is active\n");fflush(0);
    if(!atomic_load(&cancelled)){
        Chain c;chain_build(&c,cp,n,1,0,0x4d12a991);int bg=nt>1?nt-1:0;
        if(bg>0)for(int rep=1;rep<=r->opt.repeats&&!atomic_load(&cancelled);rep++){Sample s=loaded_stall_copy(r,&c,a,b,n,bg,rep);record_sample(r,&s);if(s.errors)atomic_store(&cancelled,true);}
    }
    if(!atomic_load(&cancelled)){r->integrity_errors+=verify_pattern(a,n,0x12345987ULL);r->integrity_checked_bytes+=n;}
    pages_free(cp,n);pages_free(a,n);pages_free(b,n);
    r->elapsed=now_sec()-started;r->complete=!atomic_load(&cancelled)&&!r->integrity_errors;diagnostics_build(r);report_write(r);
}
