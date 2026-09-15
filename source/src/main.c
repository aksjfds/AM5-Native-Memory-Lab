#include "lab.h"
static void help(void){printf("AM5 Native Memory Lab " VERSION "\n\nUsage: AM5MemoryLab.exe [options]\n  (no options)          Full real benchmark and active diagnostics\n  --quick               3 repeats, 180 ms per sample\n  --self-test           Validate kernels and internal calculations only\n  --no-open             Do not automatically open report.html\n  --out FOLDER          Parent output folder (must already exist or have an existing parent)\n  --config FILE         Profile annotations, not BIOS controls\n  --memory-mib N        Per-array memory: 64..2048 MiB (default >=256 and >=4x LLC)\n  --threads N           Physical worker count: 1..16, capped by detected cores\n  --repeats N           3..15 repeats per load\n  --sample-ms N         100..3000 ms per sample\n  --smoke               Developer small-working-set functional test; NOT a DRAM score\n  --help / --version\n\nNo drivers, no administrator privileges, no network, no BIOS writes.\n");}
static int number(const char* s,int lo,int hi){char* end=0;unsigned long x=strtoul(s,&end,10);if(!*s||!end||*end||x<(unsigned long)lo||x>(unsigned long)hi)die("Numeric command-line option out of range.");return (int)x;}
static int program_main(int argc,char** argv){
    platform_init();Options o={0};o.repeats=5;o.seconds=.5;bool mem_set=false;char base[2048];exe_dir(base,sizeof(base));fmt(o.config,sizeof(o.config),"%s/profile.ini",base);
    for(int i=1;i<argc;i++){
        const char* a=argv[i];if(!strcmp(a,"--help")){help();return 0;}if(!strcmp(a,"--version")){printf(ENGINE_ID "\n");return 0;}
        if(!strcmp(a,"--quick")){o.quick=true;o.repeats=3;o.seconds=.18;}
        else if(!strcmp(a,"--smoke")){o.smoke=true;o.repeats=2;o.seconds=.025;o.memory=16*MIB;mem_set=true;}
        else if(!strcmp(a,"--no-open"))o.no_open=true;
        else if(!strcmp(a,"--self-test"))o.selftest=true;
        else if(!strcmp(a,"--out")||!strcmp(a,"--config")||!strcmp(a,"--memory-mib")||!strcmp(a,"--threads")||!strcmp(a,"--repeats")||!strcmp(a,"--sample-ms")){
            if(++i>=argc)die("An option is missing its value.");const char* v=argv[i];
            if(!strcmp(a,"--out")){if(strlen(v)>=sizeof(o.out)-100)die("Output folder path too long.");fmt(o.out,sizeof(o.out),"%s",v);}
            else if(!strcmp(a,"--config")){if(strlen(v)>=sizeof(o.config))die("Config path too long.");fmt(o.config,sizeof(o.config),"%s",v);}
            else if(!strcmp(a,"--memory-mib")){o.memory=(size_t)number(v,64,2048)*MIB;mem_set=true;}
            else if(!strcmp(a,"--threads"))o.threads=number(v,1,16);
            else if(!strcmp(a,"--repeats"))o.repeats=number(v,3,15);
            else o.seconds=number(v,100,3000)/1000.0;
        }else die("Unknown option. Use --help.");
    }
    if(o.selftest){run_selftests(true);return 0;}
    Report *r=calloc(1,sizeof(*r));if(!r)die("Out of memory.");r->opt=o;machine_info(&r->hw);if(!r->hw.avx2)die("This build requires AVX2 and enabled OS YMM register state support.");
    if(!r->opt.threads)r->opt.threads=(int)(r->hw.ncores<8?r->hw.ncores:8);if(r->opt.threads>(int)r->hw.ncores)r->opt.threads=(int)r->hw.ncores;
    if(!mem_set){r->opt.memory=256*MIB;if(r->hw.llc_sum*4>r->opt.memory)r->opt.memory=(r->hw.llc_sum*4+MIB-1)/MIB*MIB;}
    if(r->opt.memory>2048*MIB)die("Detected LLC is too large for this AM5-focused default. Specify --memory-mib; undersized runs will be flagged.");
    r->sufficient_memory=r->opt.memory>=4*r->hw.llc_sum&&r->opt.memory>=256*MIB;
    if(r->hw.available&&((uint64_t)r->opt.memory*3+64*MIB)>r->hw.available*3/4)die("Insufficient free RAM for a low-paging run. Close memory-intensive apps; do not force a larger allocation.");
    r->samples=calloc(MAX_RESULTS,sizeof(Sample));if(!r->samples)die("Cannot allocate result storage.");profile_load(&r->profile,r->opt.config);
    char parent[2048],tag[128];if(*r->opt.out)fmt(parent,sizeof(parent),"%s",r->opt.out);else fmt(parent,sizeof(parent),"%s/results",base);
    if(!make_dir(parent))die("Cannot create results folder. Extract the ZIP to a writable folder, not inside the ZIP viewer or Program Files.");stamp(tag,sizeof(tag),true);fmt(r->outdir,sizeof(r->outdir),"%s/%s",parent,tag);if(!make_dir(r->outdir))die("Cannot create session output folder.");
    utc_iso(r->started,sizeof(r->started));r->whea=-2;fmt(r->whea_status,sizeof(r->whea_status),"not queried until benchmark completion");
    printf("AM5 Native Memory Lab " VERSION "\nCPU: %s\nEngine: real AVX2 memory access + dependent pointer chasing\nThreads: %d | Per-array memory: %.0f MiB | Repeats: %d | Sample: %.0f ms\nOutput: %s\n\nNo BIOS/voltage changes will be made. Press Ctrl+C to stop safely.\n",r->hw.name,r->opt.threads,(double)r->opt.memory/MIB,r->opt.repeats,r->opt.seconds*1000,r->outdir);fflush(0);
    printf("Checking execution kernels...\n");r->selftests=(unsigned)run_selftests(false);printf("%u kernel/calculation self-tests passed.\n",r->selftests);r->cpu_before=idle_cpu_usage();report_write(r);
    suite_run(r);r->whea=whea_query(r->started,r->outdir,r->whea_status,sizeof(r->whea_status));report_write(r);
    char html[2400];fmt(html,sizeof(html),"%s/report.html",r->outdir);printf("\n%s. %.0f seconds.\nReport: %s\nRaw: results.json and raw.csv in the same folder.\n",r->complete?"Measurement completed":"Measurement stopped / errors detected",r->elapsed,html);fflush(0);
    if(!r->opt.no_open)open_report(html);int code=r->complete?0:2;free(r->samples);free(r);return code;
}
#ifdef _WIN32
void mainCRTStartup(void){int argc=0;wchar_t** wa=CommandLineToArgvW(GetCommandLineW(),&argc);if(!wa||argc<1)ExitProcess(1);
    char** av=calloc((size_t)argc+1,sizeof(char*));if(!av)ExitProcess(1);
    for(int i=0;i<argc;i++){int n=WideCharToMultiByte(65001,0,wa[i],-1,0,0,0,0);if(n<1)ExitProcess(1);av[i]=malloc((size_t)n);if(!av[i])ExitProcess(1);WideCharToMultiByte(65001,0,wa[i],-1,av[i],n,0,0);}
    LocalFree(wa);int rc=program_main(argc,av);for(int i=0;i<argc;i++)free(av[i]);free(av);ExitProcess((unsigned)rc);
}
#else
int main(int argc,char** argv){return program_main(argc,argv);}
#endif
