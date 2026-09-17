#include "lab.h"

#define BAD_VALUE (-1.0e300)

typedef enum {
    FIELD_VALUE,
    FIELD_BACKGROUND_GBPS,
    FIELD_P50,
    FIELD_P99,
    FIELD_P999,
    FIELD_MAX
} SampleField;

typedef struct {
    size_t n;
    double read_gbps,write_gbps,copy_gbps,payload_gbps;
    double read_pressure,write_pressure,delta,noise;
} SideStats;

static bool finite_value(double x){return isfinite(x)&&x>BAD_VALUE/2;}
static double dmax(double a,double b){return a>b?a:b;}
static double dmin(double a,double b){return a<b?a:b;}
static double clamp100(double x){return x<0?0:x>100?100:x;}
static int dcmp(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static double median(double* a,size_t n){if(!n)return BAD_VALUE;qsort(a,n,sizeof(double),dcmp);return n&1?a[n/2]:(a[n/2-1]+a[n/2])/2.0;}
static double robust_mad(const double* a,size_t n){
    if(!n)return BAD_VALUE;
    double tmp[64],dev[64];if(n>64)n=64;
    for(size_t i=0;i<n;i++)tmp[i]=a[i];
    double m=median(tmp,n);
    for(size_t i=0;i<n;i++)dev[i]=fabs(a[i]-m);
    return 1.4826*median(dev,n);
}
static double sample_field(const Sample* s,SampleField field){
    switch(field){
    case FIELD_VALUE:return s->value;
    case FIELD_BACKGROUND_GBPS:return s->background_gbps;
    case FIELD_P50:return s->p50;
    case FIELD_P99:return s->p99;
    case FIELD_P999:return s->p999;
    case FIELD_MAX:return s->max_value;
    }
    return BAD_VALUE;
}
static const Sample* sample_find(const Report* r,const char* suite,const char* test,int trial,int threads,size_t pattern){
    for(size_t i=0;i<r->nsamples;i++){
        const Sample* s=&r->samples[i];
        if(strcmp(s->suite,suite))continue;
        if(test&&strcmp(s->test,test))continue;
        if(trial>=0&&s->trial!=trial)continue;
        if(threads>=0&&s->threads!=threads)continue;
        if(pattern&&s->pattern_bytes!=pattern)continue;
        return s;
    }
    return NULL;
}
static double group_median(const Report* r,const char* suite,const char* test,int threads,size_t pattern,SampleField field){
    double values[64];size_t n=0;
    for(size_t i=0;i<r->nsamples&&n<64;i++){
        const Sample* s=&r->samples[i];
        if(strcmp(s->suite,suite))continue;
        if(test&&strcmp(s->test,test))continue;
        if(threads>=0&&s->threads!=threads)continue;
        if(pattern&&s->pattern_bytes!=pattern)continue;
        double x=sample_field(s,field);if(finite_value(x))values[n++]=x;
    }
    return median(values,n);
}
static void linreg(const double* x,const double* y,size_t n,double* slope,double* r2){
    *slope=BAD_VALUE;*r2=BAD_VALUE;if(n<3)return;
    double sx=0,sy=0;for(size_t i=0;i<n;i++){sx+=x[i];sy+=y[i];}
    double mx=sx/n,my=sy/n,sxx=0,sxy=0,syy=0;
    for(size_t i=0;i<n;i++){double dx=x[i]-mx,dy=y[i]-my;sxx+=dx*dx;sxy+=dx*dy;syy+=dy*dy;}
    if(sxx<=0||syy<=0)return;
    *slope=sxy/sxx;double rr=sxy*sxy/(sxx*syy);*r2=rr<0?0:rr>1?1:rr;
}
static Diagnostic* add_diag(Report* r,const char* id,const char* title,const char* params){
    if(r->ndiagnostics>=MAX_DIAGNOSTICS)return NULL;
    Diagnostic* d=&r->diagnostics[r->ndiagnostics++];memset(d,0,sizeof(*d));
    fmt(d->id,sizeof(d->id),"%s",id);fmt(d->title,sizeof(d->title),"%s",title);fmt(d->parameter_group,sizeof(d->parameter_group),"%s",params);
    fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");return d;
}
static void diag_unavailable(Diagnostic* d,const char* evidence,const char* limitation){
    d->score=d->effect_pct=d->noise_pct=d->fit=d->estimate_ns=0;
    fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");
    fmt(d->evidence,sizeof(d->evidence),"%s",evidence);fmt(d->limitation,sizeof(d->limitation),"%s",limitation);
}
static void profile_values(const Report* r,char* out,size_t cap,const char* const* keys,size_t n){
    if(!cap)return;*out=0;
    if(!r->profile.loaded){fmt(out,cap,"参数读取模块尚未接入；当前没有参数快照");return;}
    size_t used=0;
    for(size_t i=0;i<n&&used<cap;i++){
        const char* v=profile_get(&r->profile,keys[i]);if(!v||!*v)continue;
        int w=fmt(out+used,cap-used,"%s%s=%s",used?", ":"",keys[i],v);
        if(w<0||(size_t)w>=cap-used)break;used+=(size_t)w;
    }
    if(!used)fmt(out,cap,"参数快照中没有该组参数");
}

static void copy_turnaround_diag(Report* r){
    Diagnostic* d=add_diag(r,"copy_turnaround","Copy短板：读写方向切换","tRDWR / tWRRD / tWTRS / tWTRL / IMC turnaround");if(!d)return;
    const size_t groups[10]={64,128,256,512,1024,2048,4096,8192,16384,65536};
    double effects[32],slopes[32],fits[32];size_t ne=0,ns=0;
    for(int trial=1;trial<=r->opt.repeats&&trial<=31;trial++){
        const Sample* fine=sample_find(r,"copy_turnaround",NULL,trial,-1,groups[0]);
        const Sample* coarse=sample_find(r,"copy_turnaround",NULL,trial,-1,groups[9]);
        if(fine&&coarse&&fine->value>0&&coarse->value>0)effects[ne++]=100.0*(coarse->value/fine->value-1.0);
        double x[10],y[10];size_t n=0;
        for(int k=0;k<10;k++){
            const Sample* s=sample_find(r,"copy_turnaround",NULL,trial,-1,groups[k]);
            if(!s||!s->logical_bytes||!s->events)continue;
            x[n]=(double)s->events/(double)s->logical_bytes;
            y[n]=s->elapsed*1e9/(double)s->logical_bytes;n++;
        }
        double slope,r2;linreg(x,y,n,&slope,&r2);
        if(n==10&&finite_value(slope)&&finite_value(r2)){slopes[ns]=slope;fits[ns]=r2;ns++;}
    }
    if(ne<3||ns<3){
        diag_unavailable(d,"Turnaround sweep 缺少至少 3 轮完整配对/回归样本。","样本不足时不允许把噪声或缺测误判为参数短板。");return;
    }
    double ebuf[32],sbuf[32],fbuf[32];memcpy(ebuf,effects,ne*sizeof(double));memcpy(sbuf,slopes,ns*sizeof(double));memcpy(fbuf,fits,ns*sizeof(double));
    double effect=median(ebuf,ne),noise=robust_mad(effects,ne),slope=median(sbuf,ns),fit=median(fbuf,ns);
    double v64=group_median(r,"copy_turnaround",NULL,-1,64,FIELD_VALUE),v64k=group_median(r,"copy_turnaround",NULL,-1,65536,FIELD_VALUE);
    if(!finite_value(v64)||!finite_value(v64k)||!finite_value(effect)||!finite_value(noise)||!finite_value(slope)||!finite_value(fit)){
        diag_unavailable(d,"Turnaround sweep 统计量不完整。","无法形成完整统计量时不进入参数排名。");return;
    }
    d->effect_pct=effect;d->noise_pct=noise;d->fit=fit;d->estimate_ns=slope;
    double signal=effect/dmax(noise,.25);
    d->score=(effect>0&&fit>.55&&slope>0)?clamp100(effect*7.0*fit*dmin(signal/3.0,1.25)):0;
    double high_threshold=dmax(1.0,3.0*noise),medium_threshold=dmax(.75,2.0*noise);
    bool high=effect>high_threshold&&fit>=.85&&slope>0;
    bool medium=effect>medium_threshold&&fit>=.65&&slope>0;
    if(high)fmt(d->status,sizeof(d->status),d->score>=45?"priority":"watch");
    else if(medium)fmt(d->status,sizeof(d->status),"watch");
    else fmt(d->status,sizeof(d->status),"no_evidence");
    if(high&&effect>5*dmax(noise,.01))fmt(d->confidence,sizeof(d->confidence),"high");
    else if(medium)fmt(d->confidence,sizeof(d->confidence),"medium");
    else fmt(d->confidence,sizeof(d->confidence),"medium");
    const char* keys[]={"tRDWR","tWRRD","tWTRS","tWTRL"};char pv[192];profile_values(r,pv,sizeof(pv),keys,4);
    fmt(d->evidence,sizeof(d->evidence),"固定50/50读写量，只改变切换间距：64B %.2f GB/s，64KiB %.2f GB/s；%zu轮配对效应 %.2f%%，MAD %.2f%%；%zu轮完整回归 %.3f ns/transition，R² %.3f。当前参数快照：%s。",v64,v64k,ne,effect,noise,ns,slope,fit,pv);
    fmt(d->limitation,sizeof(d->limitation),"该测试能确认Copy相关读写转向路径是否昂贵，但CPU乱序、store buffer与IMC请求重排意味着它不能仅凭一次固定配置唯一拆出tRDWR、tWRRD、tWTRS或tWTRL中的某一个。");
}

static bool collect_side_stats(const Report* r,SideStats* out){
    memset(out,0,sizeof(*out));double read_p[32],write_p[32],delta[32];size_t n=0;int nt=r->opt.threads;
    for(int trial=1;trial<=r->opt.repeats&&trial<=31;trial++){
        const Sample* rd=sample_find(r,"copy_baseline","read",trial,nt,0);
        const Sample* wr=sample_find(r,"copy_baseline","write_nt",trial,nt,0);
        const Sample* cp=sample_find(r,"copy_baseline","copy_nt",trial,nt,0);
        if(!rd||!wr||!cp||rd->value<=0||wr->value<=0||cp->value<=0)continue;
        double payload=cp->value/2.0,ur=payload/rd->value,uw=payload/wr->value;
        if(!finite_value(ur)||!finite_value(uw))continue;
        read_p[n]=ur;write_p[n]=uw;delta[n]=uw-ur;n++;
    }
    if(n<3)return false;
    double a[32],b[32],c[32];memcpy(a,read_p,n*sizeof(double));memcpy(b,write_p,n*sizeof(double));memcpy(c,delta,n*sizeof(double));
    out->n=n;out->read_pressure=median(a,n);out->write_pressure=median(b,n);out->delta=median(c,n);out->noise=robust_mad(delta,n);
    out->read_gbps=group_median(r,"copy_baseline","read",nt,0,FIELD_VALUE);
    out->write_gbps=group_median(r,"copy_baseline","write_nt",nt,0,FIELD_VALUE);
    out->copy_gbps=group_median(r,"copy_baseline","copy_nt",nt,0,FIELD_VALUE);
    out->payload_gbps=finite_value(out->copy_gbps)?out->copy_gbps/2.0:BAD_VALUE;
    return finite_value(out->read_gbps)&&finite_value(out->write_gbps)&&finite_value(out->copy_gbps)&&finite_value(out->read_pressure)&&finite_value(out->write_pressure)&&finite_value(out->delta)&&finite_value(out->noise);
}
static void fill_side_diag(Report* r,Diagnostic* d,const SideStats* s,bool write_side){
    double direction=write_side?s->delta:-s->delta,target=write_side?s->write_pressure:s->read_pressure;
    d->effect_pct=direction>0?100*direction:0;d->noise_pct=100*s->noise;
    const char* keys_write[]={"tWRWRSCL","tWRWRSC","tWRWRSD","tWRWRDD","tCWL","tRCDWR","tWR"};
    const char* keys_read[]={"tRDRDSCL","tRDRDSC","tRDRDSD","tRDRDDD","tRCDRD","tCL"};
    char pv[256];profile_values(r,pv,sizeof(pv),write_side?keys_write:keys_read,write_side?7:6);
    bool reference_inconsistent=s->read_pressure>1.20||s->write_pressure>1.20;
    if(reference_inconsistent){
        d->score=0;fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");
    }else{
        double signal=direction/dmax(s->noise,.01),score_factor=dmin(signal/3.0,1.0);
        d->score=direction>0?clamp100(direction*400.0*dmin(target,1.0)*dmax(score_factor,0)):0;
        double high=dmax(.12,3.0*s->noise),medium=dmax(.07,2.0*s->noise);
        if(direction>high&&target>=.65){fmt(d->status,sizeof(d->status),"priority");fmt(d->confidence,sizeof(d->confidence),"medium");}
        else if(direction>medium&&target>=.55){fmt(d->status,sizeof(d->status),"watch");fmt(d->confidence,sizeof(d->confidence),"medium");}
        else {fmt(d->status,sizeof(d->status),"no_evidence");fmt(d->confidence,sizeof(d->confidence),"medium");d->score=0;}
    }
    fmt(d->evidence,sizeof(d->evidence),"%zu轮配对：%d线程独立Read %.2f GB/s，Write-NT %.2f GB/s，Copy %.2f GB/s（有效复制 %.2f GB/s）。Copy每个方向占独立Read %.1f%%、独立Write %.1f%%；%s侧相对压力差 %.1f 个百分点，配对MAD %.1f 个百分点。参数快照：%s。",s->n,r->opt.threads,s->read_gbps,s->write_gbps,s->copy_gbps,s->payload_gbps,s->read_pressure*100,s->write_pressure*100,write_side?"写":"读",d->effect_pct,d->noise_pct,pv);
    if(reference_inconsistent)fmt(d->limitation,sizeof(d->limitation),"至少一侧 Copy/独立参考比超过120%%，说明独立Read/Write不能在本轮充当可靠边界；为避免假阳性，本项降级为unresolved。后续需重复测试或使用更接近Copy的数据流参考。");
    else fmt(d->limitation,sizeof(d->limitation),"独立Read/Write不是并发Copy的严格物理上限。本项使用逐轮配对和MAD抑制偶然波动，只定位哪一侧更接近自身参考边界；具体单时序因果仍需Bank/命令级证据或A/B。");
}
static void copy_side_diags(Report* r){
    Diagnostic* write=add_diag(r,"copy_write_path","Copy短板：写入数据路径","tWRWRSCL / tWRWRSC-SD-DD / tCWL / write-side IMC");
    Diagnostic* read=add_diag(r,"copy_read_path","Copy短板：读取数据路径","tRDRDSCL / tRDRDSC-SD-DD / read-side IMC");
    if(!write||!read)return;SideStats s;
    if(!collect_side_stats(r,&s)){
        diag_unavailable(write,"Read/Write/Copy 配对样本不足或统计量无效。","至少需要3轮同trial的Read、Write和Copy样本，缺测时不做侧向瓶颈排名。");
        diag_unavailable(read,"Read/Write/Copy 配对样本不足或统计量无效。","至少需要3轮同trial的Read、Write和Copy样本，缺测时不做侧向瓶颈排名。");return;
    }
    fill_side_diag(r,write,&s,true);fill_side_diag(r,read,&s,false);
}

static void copy_scaling_diag(Report* r){
    Diagnostic* d=add_diag(r,"copy_scaling","Copy线程扩展/全局数据路径","MCLK / UCLK / FCLK / IMC / channel bandwidth (context)");if(!d)return;
    double one=group_median(r,"copy_baseline","copy_nt",1,0,FIELD_VALUE),best=BAD_VALUE;int best_threads=0;
    for(int t=1;t<=r->opt.threads;t++){double x=group_median(r,"copy_baseline","copy_nt",t,0,FIELD_VALUE);if(finite_value(x)&&(!finite_value(best)||x>best)){best=x;best_threads=t;}}
    if(!finite_value(one)||!finite_value(best)||one<=0||best<=0){diag_unavailable(d,"Copy线程扩展样本不完整。","缺少1线程或至少一个有效Copy样本时不解释全局数据路径。");return;}
    double scale=best/one;d->effect_pct=100*(scale-1);d->score=0;fmt(d->status,sizeof(d->status),"observed");fmt(d->confidence,sizeof(d->confidence),"high");
    fmt(d->evidence,sizeof(d->evidence),"1线程Copy %.2f GB/s；最高已测 %d线程 %.2f GB/s；扩展倍率 %.3f×。",one,best_threads,best,scale);
    fmt(d->limitation,sizeof(d->limitation),"线程扩展用于识别是否很早触及全局数据路径上限。饱和本身不是时序设置错误，因此不参与具体时序优先级排名。");
}
static void copy_loaded_diag(Report* r){
    Diagnostic* d=add_diag(r,"copy_queue_pressure","Copy负载下IMC排队/转向压力","IMC scheduling / turnaround / UCLK-FCLK path (combined)");if(!d)return;
    int bg=r->opt.threads>1?r->opt.threads-1:0;if(bg<=0){diag_unavailable(d,"只有1个测试线程，无法同时运行延迟探针和Copy背景流量。","该上下文测试至少需要2个物理核心。");return;}
    double idle=group_median(r,"loaded_latency","idle",0,0,FIELD_VALUE),copy=group_median(r,"loaded_latency","copy_load",bg,0,FIELD_VALUE),read=group_median(r,"loaded_latency","read_load",bg,0,FIELD_VALUE),copy_bg=group_median(r,"loaded_latency","copy_load",bg,0,FIELD_BACKGROUND_GBPS),read_bg=group_median(r,"loaded_latency","read_load",bg,0,FIELD_BACKGROUND_GBPS);
    if(!finite_value(idle)||!finite_value(copy)||!finite_value(read)||!finite_value(copy_bg)||!finite_value(read_bg)||idle<=0||copy<=0||read<=0){diag_unavailable(d,"Copy/Read loaded-latency 样本不完整。","缺测时不使用占位数值解释控制器排队。");return;}
    double extra=100*(copy/read-1);d->effect_pct=extra;d->score=0;fmt(d->status,sizeof(d->status),"observed");fmt(d->confidence,sizeof(d->confidence),"high");
    fmt(d->evidence,sizeof(d->evidence),"空载随机延迟 %.2f ns；%d线程纯读 %.2f ns @ %.2f GB/s；同线程Copy背景 %.2f ns @ %.2f logical GB/s；Copy相对纯读延迟差 %.1f%%。",idle,bg,read,read_bg,copy,copy_bg,extra);
    fmt(d->limitation,sizeof(d->limitation),"Copy背景同时改变读写比例、流量组成与排队，不能单独识别具体时序；它只用于交叉验证turnaround/控制器压力是否真实存在。");
}
static void copy_tail_diag(Report* r){
    Diagnostic* d=add_diag(r,"copy_tail","Copy负载下短窗口尾延迟","tRFC/tREFI/Refresh Mode or IMC queueing (unresolved)");if(!d)return;
    int bg=r->opt.threads>1?r->opt.threads-1:0;if(bg<=0){diag_unavailable(d,"只有1个测试线程，无法生成Copy负载下短窗口尾延迟。","该探针至少需要2个物理核心。");return;}
    double i50=group_median(r,"copy_stall_probe","idle_short_window",-1,0,FIELD_P50),i99=group_median(r,"copy_stall_probe","idle_short_window",-1,0,FIELD_P99),c50=group_median(r,"copy_stall_probe","copy_load_short_window",bg,0,FIELD_P50),c99=group_median(r,"copy_stall_probe","copy_load_short_window",bg,0,FIELD_P99),c999=group_median(r,"copy_stall_probe","copy_load_short_window",bg,0,FIELD_P999);
    if(!finite_value(i50)||!finite_value(i99)||!finite_value(c50)||!finite_value(c99)||!finite_value(c999)||i50<=0||i99<=0||c50<=0||c99<=0){diag_unavailable(d,"短窗口尾延迟样本不完整。","缺测时不生成refresh/尾部解释。");return;}
    double amp=100*((c99/c50)/(i99/i50)-1);d->effect_pct=amp;d->score=0;fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");
    fmt(d->evidence,sizeof(d->evidence),"空载短窗口 P50/P99 %.2f/%.2f ns；Copy负载 P50/P99/P99.9 %.2f/%.2f/%.2f ns；归一化尾部放大 %.1f%%。",i50,i99,c50,c99,c999,amp);
    fmt(d->limitation,sizeof(d->limitation),"没有Refresh事件计数器和周期相关性，不能把尾部放大自动归因给tRFC/tREFI；Copy排队、OS中断和其他stall也会进入尾部，因此本项不参与参数排名。");
}
static void copy_parallel_proxy(Report* r){
    Diagnostic* d=add_diag(r,"copy_bank_parallel_proxy","Bank/ACT并行候选路径","tRRDS / tRRDL / tFAW / SCL (需要Bank映射)");if(!d)return;
    double one=group_median(r,"copy_path_proxy","random_latency",-1,0,FIELD_VALUE),eight=group_median(r,"copy_path_proxy","parallel_8_chains",-1,0,FIELD_VALUE);
    if(!finite_value(one)||!finite_value(eight)||one<=0||eight<=0){diag_unavailable(d,"独立链并行代理样本不完整。","没有有效单链和8链样本时不解释Bank/ACT并行路径。");return;}
    double efficiency=100.0*(one/eight)/8.0;d->effect_pct=100-efficiency;d->score=0;fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");
    fmt(d->evidence,sizeof(d->evidence),"单依赖链 %.2f ns/access；8独立链 %.2f ns/access；相对理想8×并发效率 %.1f%%。",one,eight,efficiency);
    fmt(d->limitation,sizeof(d->limitation),"没有物理地址到Bank/BankGroup映射时，独立链不能证明ACT落在哪些Bank，因此不能把并发效率直接变成tRRDS/tRRDL/tFAW的Copy损失估计。");
}
static void copy_row_proxy(Report* r){
    Diagnostic* d=add_diag(r,"copy_row_proxy","Row conflict候选路径","tRP / tRCDRD / tRC / tRAS (需要Bank/Row映射)");if(!d)return;
    double random=group_median(r,"copy_path_proxy","random_latency",-1,0,FIELD_VALUE),local=group_median(r,"copy_path_proxy","page_local_latency",-1,0,FIELD_VALUE);
    if(!finite_value(random)||!finite_value(local)||random<=0||local<=0){diag_unavailable(d,"地址局部性代理样本不完整。","没有有效随机和页内样本时不解释Row conflict候选路径。");return;}
    d->effect_pct=100*(random/local-1);d->score=0;fmt(d->status,sizeof(d->status),"unresolved");fmt(d->confidence,sizeof(d->confidence),"low");
    fmt(d->evidence,sizeof(d->evidence),"全范围随机 %.2f ns；页内随机 %.2f ns；观测差 %.2f ns。",random,local,random-local);
    fmt(d->limitation,sizeof(d->limitation),"虚拟页局部性同时改变TLB、预取和可能的DRAM行命中；没有Bank/Row映射不能构造same-row与same-bank-different-row正交对照，因此不参与Copy参数排名。");
}
static void copy_cached_diag(Report* r){
    Diagnostic* d=add_diag(r,"copy_store_policy","Copy写策略敏感度","CPU cache/store policy (not BIOS timing ranking)");if(!d)return;
    int nt=r->opt.threads;double nt_copy=group_median(r,"copy_baseline","copy_nt",nt,0,FIELD_VALUE),cached=group_median(r,"copy_baseline","copy_cached",nt,0,FIELD_VALUE);
    if(!finite_value(nt_copy)||!finite_value(cached)||nt_copy<=0||cached<=0){diag_unavailable(d,"Copy-NT/Cached Copy 对照样本不完整。","缺测时不解释store policy敏感度。");return;}
    double effect=100*(nt_copy/cached-1);d->effect_pct=effect;d->score=0;fmt(d->status,sizeof(d->status),"observed");fmt(d->confidence,sizeof(d->confidence),"high");
    fmt(d->evidence,sizeof(d->evidence),"%d线程Copy-NT %.2f GB/s；Cached Copy %.2f GB/s；NT相对差异 %.1f%%。",nt,nt_copy,cached,effect);
    fmt(d->limitation,sizeof(d->limitation),"Cached/NT差异主要反映cache/RFO/store策略，不作为内存时序短板排名依据。");
}
static int status_weight(const char* status){if(!strcmp(status,"priority"))return 4;if(!strcmp(status,"watch"))return 3;if(!strcmp(status,"no_evidence"))return 2;if(!strcmp(status,"observed"))return 1;return 0;}
static void sort_diags(Report* r){
    for(size_t i=0;i<r->ndiagnostics;i++)for(size_t j=i+1;j<r->ndiagnostics;j++){
        Diagnostic* a=&r->diagnostics[i],*b=&r->diagnostics[j];int wa=status_weight(a->status),wb=status_weight(b->status);
        if(wb>wa||(wb==wa&&b->score>a->score)){Diagnostic t=*a;*a=*b;*b=t;}
    }
}
void diagnostics_build(Report* r){
    r->ndiagnostics=0;
    if(!r->complete||r->opt.smoke||!r->sufficient_memory||r->integrity_errors)return;
    copy_turnaround_diag(r);copy_side_diags(r);copy_loaded_diag(r);copy_scaling_diag(r);copy_cached_diag(r);copy_parallel_proxy(r);copy_row_proxy(r);copy_tail_diag(r);sort_diags(r);
}
