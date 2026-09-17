#include "lab.h"
#include "report_template.h"
static void write_text(const char* name,const Text* t){FILE* f=file_open(name,"wb");if(!f)die("Cannot create report; check output folder permissions.");if(fwrite(t->data,1,t->len,f)!=t->len){fclose(f);die("Incomplete report write.");}fclose(f);}
void report_write(const Report* r){
    Text j;text_init(&j);text_add(&j,"{\n\"schema\":\"AM5Native/4\",\"focus\":\"copy_bottleneck\",\"engine\":\"" ENGINE_ID "\",\"platform\":");
#ifdef _WIN32
    json_str(&j,"Windows x86-64 / native Win32");
#else
    json_str(&j,"Linux x86-64 / developer validation (not user's hardware)");
#endif
    text_add(&j,",\"started_utc\":");json_str(&j,r->started);
    text_fmt(&j,",\"timer_frequency_hz\":%.0f,\"complete\":%s,\"sufficient_memory\":%s,\"selftests_passed\":%u,\"cpu_before_percent\":%.6f,\"elapsed_seconds\":%.6f,",(double)timer_frequency(),r->complete?"true":"false",r->sufficient_memory?"true":"false",r->selftests,r->cpu_before,r->elapsed);
    text_fmt(&j,"\"options\":{\"repeats\":%d,\"sample_seconds\":%.6f,\"memory_bytes\":%.0f,\"threads\":%d,\"smoke\":%s},",r->opt.repeats,r->opt.seconds,(double)r->opt.memory,r->opt.threads,r->opt.smoke?"true":"false");
    text_add(&j,"\"machine\":{\"cpu\":");json_str(&j,r->hw.name);text_add(&j,",\"topology_status\":");json_str(&j,r->hw.topology_status);
    text_fmt(&j,",\"physical_cores_detected\":%u,\"logical_detected\":%u,\"l1\":%.0f,\"l2\":%.0f,\"l3\":%.0f,\"llc_sum\":%.0f,\"total_memory_bytes\":%.0f,\"available_memory_bytes\":%.0f,\"avx2\":%s,\"hypervisor\":%s,\"cores_used\":[",r->hw.ncores,r->hw.nlogical,(double)r->hw.l1,(double)r->hw.l2,(double)r->hw.l3,(double)r->hw.llc_sum,(double)r->hw.total,(double)r->hw.available,r->hw.avx2?"true":"false",r->hw.hypervisor?"true":"false");
    for(int i=0;i<r->opt.threads;i++){if(i)text_add(&j,",");text_fmt(&j,"{\"group\":%u,\"cpu\":%u}",r->hw.cores[i].group,r->hw.cores[i].cpu);}text_add(&j,"]},");
    text_fmt(&j,"\"integrity\":{\"errors\":%.0f,\"checked_bytes\":%.0f},\"whea\":{\"count\":%d,\"status\":",(double)r->integrity_errors,(double)r->integrity_checked_bytes,r->whea);json_str(&j,r->whea_status);text_add(&j,"},");
    text_fmt(&j,"\"parameter_source\":\"%s\",\"profile_loaded\":%s,\"profile\":{",r->profile.loaded?"profile.ini placeholder":"none",r->profile.loaded?"true":"false");for(size_t i=0;i<r->profile.n;i++){if(i)text_add(&j,",");json_str(&j,r->profile.kv[i].key);text_add(&j,":");json_str(&j,r->profile.kv[i].value);}text_add(&j,"},\n\"diagnostics\":[\n");
    for(size_t i=0;i<r->ndiagnostics;i++){const Diagnostic* d=&r->diagnostics[i];if(i)text_add(&j,",\n");text_add(&j,"{\"id\":");json_str(&j,d->id);text_add(&j,",\"title\":");json_str(&j,d->title);text_add(&j,",\"parameter_group\":");json_str(&j,d->parameter_group);text_add(&j,",\"status\":");json_str(&j,d->status);text_add(&j,",\"confidence\":");json_str(&j,d->confidence);text_add(&j,",\"evidence\":");json_str(&j,d->evidence);text_add(&j,",\"limitation\":");json_str(&j,d->limitation);text_fmt(&j,",\"score\":%.9g,\"effect_pct\":%.9g,\"noise_pct\":%.9g,\"fit\":%.9g,\"estimate_ns\":%.9g}",d->score,d->effect_pct,d->noise_pct,d->fit,d->estimate_ns);}text_add(&j,"\n],\n\"samples\":[\n");
    for(size_t i=0;i<r->nsamples;i++){const Sample* x=&r->samples[i];if(i)text_add(&j,",\n");text_add(&j,"{\"suite\":");json_str(&j,x->suite);text_add(&j,",\"test\":");json_str(&j,x->test);text_add(&j,",\"unit\":");json_str(&j,x->unit);
        text_fmt(&j,",\"trial\":%d,\"threads\":%d,\"chains\":%d,\"working_bytes\":%.0f,\"pattern_bytes\":%.0f,\"operations\":%.0f,\"events\":%.0f,\"logical_bytes\":%.0f,\"elapsed\":%.12g,\"value\":%.12g,\"p50\":%.12g,\"p95\":%.12g,\"p99\":%.12g,\"p999\":%.12g,\"max\":%.12g,\"background_gbps\":%.12g,\"pin_ok\":%s,\"errors\":%.0f}",x->trial,x->threads,x->chains,(double)x->working_bytes,(double)x->pattern_bytes,(double)x->operations,(double)x->events,(double)x->logical_bytes,x->elapsed,x->value,x->p50,x->p95,x->p99,x->p999,x->max_value,x->background_gbps,x->pin_ok?"true":"false",(double)x->errors);
    }text_add(&j,"\n]}\n");char path[2400];fmt(path,sizeof(path),"%s/results.json",r->outdir);write_text(path,&j);
    Text h;text_init(&h);const char* marker=strstr(REPORT_TEMPLATE,"__DATA__");if(!marker)die("Missing report template marker.");size_t prefix=(size_t)(marker-REPORT_TEMPLATE);char* temp=malloc(prefix+1);if(!temp)die("Out of memory.");memcpy(temp,REPORT_TEMPLATE,prefix);temp[prefix]=0;text_add(&h,temp);free(temp);text_add(&h,j.data);text_add(&h,marker+8);fmt(path,sizeof(path),"%s/report.html",r->outdir);write_text(path,&h);text_free(&h);text_free(&j);
}
