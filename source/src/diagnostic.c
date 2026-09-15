#include "lab.h"
#define BAD_VALUE (-1.0e300)
static bool good(double x){return x>BAD_VALUE/2&&x<1.0e300;}
static double dmax(double a,double b){return a>b?a:b;}
static int dcmp(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static double median(double* a,size_t n){if(!n)return BAD_VALUE;qsort(a,n,sizeof(double),dcmp);return n&1?a[n/2]:(a[n/2-1]+a[n/2])/2.0;}
static double robust_mad(double* a,size_t n){if(!n)return BAD_VALUE;double tmp[64],dev[64];if(n>64)n=64;for(size_t i=0;i<n;i++)tmp[i]=a[i];double m=median(tmp,n);for(size_t i=0;i<n;i++)dev[i]=fabs(a[i]-m);return 1.4826*median(dev,n);}
static const Sample* sample_find(const Report* r,const char* suite,const char* test,int trial,int threads,size_t pattern){
    for(size_t i=0;i<r->nsamples;i++){const Sample* s=&r->samples[i];if(strcmp(s->suite,suite))continue;if(test&&strcmp(s->test,test))continue;if(trial>=0&&s->trial!=trial)continue;if(threads>=0&&s->threads!=threads)continue;if(pattern&&s->pattern_bytes!=pattern)continue;return s;}return 0;
}
static double group_median(const Report* r,const char* suite,const char* test,int threads,size_t pattern,int field){
    double v[64];size_t n=0;for(size_t i=0;i<r->nsamples&&n<64;i++){const Sample* s=&r->samples[i];if(strcmp(s->suite,suite))continue;if(test&&strcmp(s->test,test))continue;if(threads>=0&&s->threads!=threads)continue;if(pattern&&s->pattern_bytes!=pattern)continue;
        double x=field==0?s->value:field==1?s->background_gbps:field==2?s->p50:field==3?s->p99:field==4?s->p999:s->max_value;if(good(x))v[n++]=x;}
    return median(v,n);
}
static void linreg(const double* x,const double* y,size_t n,double* slope,double* r2){
    *slope=BAD_VALUE;*r2=BAD_VALUE;if(n<3)return;double sx=0,sy=0;for(size_t i=0;i<n;i++){sx+=x[i];sy+=y[i];}double mx=sx/n,my=sy/n,sxx=0,sxy=0,syy=0;for(size_t i=0;i<n;i++){double dx=x[i]-mx,dy=y[i]-my;sxx+=dx*dx;sxy+=dx*dy;syy+=dy*dy;}if(sxx<=0||syy<=0)return;*slope=sxy/sxx;double rr=sxy*sxy/(sxx*syy);*r2=rr<0?0:rr>1?1:rr;
}
static double clamp100(double x){return x<0?0:x>100?100:x;}
static Diagnostic* add_diag(Report* r,const char* id,const char* title,const char* params){if(r->ndiagnostics>=MAX_DIAGNOSTICS)return 0;Diagnostic* d=&r->diagnostics[r->ndiagnostics++];memset(d,0,sizeof(*d));fmt(d->id,sizeof(d->id),"%s",id);fmt(d->title,sizeof(d->title),"%s",title);fmt(d->parameter_group,sizeof(d->parameter_group),"%s",params);fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");return d;}
static void turnaround_diag(Report* r){
    Diagnostic* d=add_diag(r,"rw_turnaround","读写方向切换路径","tWTRS / tWTRL / tRDWR / tWRRD / IMC turnaround");if(!d)return;
    const size_t g[6]={64,256,1024,4096,16384,65536};double effects[32],slopes[32],fits[32];size_t ne=0,ns=0;
    for(int trial=1;trial<=r->opt.repeats&&trial<=31;trial++){
        const Sample* a=sample_find(r,"turnaround_sweep",0,trial,-1,g[0]);const Sample* b=sample_find(r,"turnaround_sweep",0,trial,-1,g[5]);if(a&&b&&a->value>0)effects[ne++]=100.0*(b->value/a->value-1.0);
        double x[6],y[6];size_t n=0;for(int k=0;k<6;k++){const Sample* s=sample_find(r,"turnaround_sweep",0,trial,-1,g[k]);if(!s||!s->logical_bytes||!s->events)continue;x[n]=(double)s->events/(double)s->logical_bytes;y[n]=s->elapsed*1e9/(double)s->logical_bytes;n++;}
        double sl,rr;linreg(x,y,n,&sl,&rr);if(good(sl)&&good(rr)){slopes[ns]=sl;fits[ns]=rr;ns++;}
    }
    double ebuf[32],sbuf[32],fbuf[32];memcpy(ebuf,effects,ne*sizeof(double));memcpy(sbuf,slopes,ns*sizeof(double));memcpy(fbuf,fits,ns*sizeof(double));double effect=median(ebuf,ne),noise=robust_mad(effects,ne),slope=median(sbuf,ns),fit=median(fbuf,ns);
    double v64=group_median(r,"turnaround_sweep",0,-1,64,0),v64k=group_median(r,"turnaround_sweep",0,-1,65536,0);
    int mt=r->opt.threads>1?r->opt.threads-1:0;double readlat=group_median(r,"loaded_latency","read_load",mt,0,0),mixlat=group_median(r,"loaded_latency","mixed_load",mt,0,0),readbg=group_median(r,"loaded_latency","read_load",mt,0,1),mixbg=group_median(r,"loaded_latency","mixed_load",mt,0,1);double mix_effect=(readlat>0&&mixlat>0)?100*(mixlat/readlat-1):BAD_VALUE,bgr=(readbg>0&&mixbg>0)?mixbg/readbg:BAD_VALUE;
    d->effect_pct=good(effect)?effect:0;d->noise_pct=good(noise)?noise:0;d->fit=good(fit)?fit:0;d->estimate_ns=good(slope)?slope:0;
    double sweep_score=(good(effect)&&effect>0&&good(fit))?clamp100(effect*4.0*fit):0;double mixed_score=(good(mix_effect)&&mix_effect>0&&good(bgr)&&bgr<1.0)?clamp100(mix_effect*2.2):0;d->score=sweep_score>mixed_score?sweep_score:mixed_score;
    double threshold=dmax(1.0,good(noise)?3.0*noise:1.0);bool strong_sweep=good(effect)&&effect>threshold&&good(fit)&&fit>=.70&&good(slope)&&slope>0;bool strong_mixed=good(mix_effect)&&mix_effect>5.0&&good(bgr)&&bgr<.9;
    if(strong_sweep||strong_mixed)fmt(d->status,sizeof(d->status),d->score>=45?"priority":"watch");else fmt(d->status,sizeof(d->status),"no_evidence");
    if(strong_sweep&&fit>=.90&&(!good(noise)||effect>5*dmax(noise,.01)))fmt(d->confidence,sizeof(d->confidence),"high");else if(strong_sweep||strong_mixed)fmt(d->confidence,sizeof(d->confidence),"medium");
    fmt(d->evidence,sizeof(d->evidence),"50/50 NT读写方向切换间距扫描：64B %.2f GB/s，64KiB %.2f GB/s，配对效应 %.2f%%，MAD噪声 %.2f%%，斜率 %.3f ns/transition，R² %.3f；同线程混合负载延迟 %.2f ns vs 纯读 %.2f ns，后台流量比 %.2f。",v64,v64k,good(effect)?effect:0,good(noise)?noise:0,good(slope)?slope:0,good(fit)?fit:0,mixlat,readlat,good(bgr)?bgr:0);
    fmt(d->limitation,sizeof(d->limitation),"这是有效读写切换敏感度，不是单个DRAM命令周期。CPU乱序、store buffer和IMC重排仍存在；只有方向切换扫描本身呈稳定斜率时才提高参数组优先级。");
}
static void parallel_diag(Report* r){
    Diagnostic* d=add_diag(r,"parallel_activation","独立请求并行路径","tRRDS / tRRDL / tFAW / tRDRDSCL / Bank parallelism (proxy)");if(!d)return;double a=group_median(r,"memory_latency","random_latency",-1,0,0),b=group_median(r,"memory_latency","parallel_8_chains",-1,0,0);double eff=(a>0&&b>0)?100.0*(a/b)/8.0:BAD_VALUE;d->effect_pct=good(eff)?100-eff:0;d->score=good(eff)&&eff<85?clamp100((85-eff)*4):0;
    if(good(eff)&&eff<70){fmt(d->status,sizeof(d->status),"priority");fmt(d->confidence,sizeof(d->confidence),"medium");}else if(good(eff)&&eff<85){fmt(d->status,sizeof(d->status),"watch");fmt(d->confidence,sizeof(d->confidence),"medium");}else {fmt(d->status,sizeof(d->status),"no_evidence");fmt(d->confidence,sizeof(d->confidence),"medium");}
    fmt(d->evidence,sizeof(d->evidence),"单依赖链 %.2f ns/access；8条独立链 %.2f ns/access；并行完成率相对理想8×的效率 %.1f%%。",a,b,good(eff)?eff:0);
    fmt(d->limitation,sizeof(d->limitation),"当前没有物理地址→Bank/BankGroup映射，所以不能把并行效率直接归因给tRRD/tFAW/SCL；高效率只说明这一路径暂时没有明显并行缺口。");
}
static void locality_diag(Report* r){
    Diagnostic* d=add_diag(r,"address_locality","地址局部性/行冲突候选路径","tRP / tRCDRD / tRC (需要Bank/Row映射后才能定责)");if(!d)return;double a=group_median(r,"memory_latency","random_latency",-1,0,0),b=group_median(r,"memory_latency","page_local_latency",-1,0,0);d->effect_pct=(a>0&&b>0)?100*(a/b-1):0;d->score=0;fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");fmt(d->evidence,sizeof(d->evidence),"全范围随机 %.2f ns；页内随机 %.2f ns；差值 %.2f ns。",a,b,a-b);fmt(d->limitation,sizeof(d->limitation),"这个差值同时包含TLB、虚拟页局部性、预取与可能的DRAM行局部性。本版没有物理Bank/Row映射，因此明确不进入参数优先级评分。");
}
static void refresh_diag(Report* r){
    Diagnostic* d=add_diag(r,"stall_tail","短窗口尾延迟/刷新候选","tRFC / tREFI / Refresh Mode (未隔离刷新事件)");if(!d)return;double p50=group_median(r,"stall_probe","short_window_random",-1,0,2),p99=group_median(r,"stall_probe","short_window_random",-1,0,3),p999=group_median(r,"stall_probe","short_window_random",-1,0,4);d->effect_pct=p50>0?100*(p99/p50-1):0;d->score=0;fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");fmt(d->evidence,sizeof(d->evidence),"64次依赖读取短窗口：P50 %.2f ns/access，P99 %.2f，P99.9 %.2f，P99相对P50 +%.1f%%。",p50,p99,p999,d->effect_pct);fmt(d->limitation,sizeof(d->limitation),"短窗口能显示stall尾部，但没有UMC refresh计数器和周期事件相关性，不能把尾部自动标成刷新，更不能由此直接判断tRFC/tREFI应改多少。");
}
static void load_diag(Report* r){
    Diagnostic* d=add_diag(r,"loaded_latency","带宽负载下的控制器排队","UCLK / FCLK / Nitro / SCL / IMC scheduling (路径级)");if(!d)return;double idle=group_median(r,"loaded_latency","read_load",0,0,0);int mt=r->opt.threads>1?r->opt.threads-1:0;double load=group_median(r,"loaded_latency","read_load",mt,0,0),bg=group_median(r,"loaded_latency","read_load",mt,0,1);d->effect_pct=(idle>0&&load>0)?100*(load/idle-1):0;d->score=0;fmt(d->status,sizeof(d->status),"observed");fmt(d->confidence,sizeof(d->confidence),"high");fmt(d->evidence,sizeof(d->evidence),"空载依赖读取 %.2f ns；%d个后台纯读线程、%.2f GB/s 时 %.2f ns，延迟 +%.1f%%。",idle,mt,bg,load,d->effect_pct);fmt(d->limitation,sizeof(d->limitation),"满载排队是正常现象，单次配置没有外部基线时不能把它判成某项时序设置错误；只作为其他差分诊断的上下文。");
}
static void stream_diag(Report* r){
    Diagnostic* d=add_diag(r,"stream_saturation","流式读取饱和位置","MCLK / UCLK / IMC data path (不是单项时序归因)");if(!d)return;double one=group_median(r,"memory_bandwidth","read",1,0,0),peak=0;int pt=0;for(size_t i=0;i<r->nsamples;i++){const Sample* s=&r->samples[i];if(strcmp(s->suite,"memory_bandwidth")||strcmp(s->test,"read"))continue;double m=group_median(r,"memory_bandwidth","read",s->threads,0,0);if(m>peak){peak=m;pt=s->threads;}}double scale=one>0?peak/one:BAD_VALUE;d->effect_pct=good(scale)?100*(scale-1):0;d->score=0;fmt(d->status,sizeof(d->status),"observed");fmt(d->confidence,sizeof(d->confidence),"high");fmt(d->evidence,sizeof(d->evidence),"1线程Read %.2f GB/s；最高已测 %d线程 %.2f GB/s；扩展倍率 %.3f×。",one,pt,peak,good(scale)?scale:0);fmt(d->limitation,sizeof(d->limitation),"单线程已经接近或达到本测试的流式上限并不等于参数错误；没有真实总线计数器和理论带宽读回时不计算所谓效率百分比。");
}
void diagnostics_build(Report* r){r->ndiagnostics=0;if(!r->complete||r->opt.smoke||!r->sufficient_memory||r->integrity_errors)return;turnaround_diag(r);parallel_diag(r);locality_diag(r);refresh_diag(r);load_diag(r);stream_diag(r);}
