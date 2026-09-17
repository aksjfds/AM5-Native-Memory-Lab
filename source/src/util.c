#include "lab.h"

static void reserve(Text* t,size_t extra){
    if(extra>64*MIB||t->len>64*MIB-extra)die("Report size limit exceeded.");
    size_t need=t->len+extra+1;
    if(need<=t->cap)return;
    size_t cap=t->cap?t->cap:1024;
    while(cap<need)cap*=2;
    char *p=realloc(t->data,cap);
    if(!p)die("Out of memory building report.");
    t->data=p;t->cap=cap;
}
void text_init(Text* t){memset(t,0,sizeof(*t));reserve(t,1);t->data[0]=0;}
void text_free(Text* t){free(t->data);memset(t,0,sizeof(*t));}
void text_add(Text* t,const char* s){size_t n=strlen(s);reserve(t,n);memcpy(t->data+t->len,s,n+1);t->len+=n;}

static int vlength(const char* f,va_list a){
#ifdef _WIN32
    return _vscprintf(f,a);
#else
    return vsnprintf(0,0,f,a);
#endif
}
static int vformat(char* b,size_t cap,const char* f,va_list a){
#ifdef _WIN32
    int n=_vsnprintf(b,cap,f,a);if(cap)b[cap-1]=0;return n;
#else
    return vsnprintf(b,cap,f,a);
#endif
}
int fmt(char* b,size_t cap,const char* f,...){va_list a;va_start(a,f);int n=vformat(b,cap,f,a);va_end(a);return n;}
void text_fmt(Text* t,const char* f,...){
    va_list a,b;va_start(a,f);va_copy(b,a);int n=vlength(f,b);va_end(b);
    if(n<0)die("Formatting failure.");
    reserve(t,(size_t)n);vformat(t->data+t->len,t->cap-t->len,f,a);va_end(a);t->len+=(size_t)n;
}
void json_str(Text* t,const char* s){
    text_add(t,"\"");
    for(const unsigned char* p=(const unsigned char*)s;*p;p++){
        unsigned c=*p;
        if(c=='"'||c=='\\')text_fmt(t,"\\%c",c);
        else if(c<32||c=='<'||c=='>'||c=='&')text_fmt(t,"\\u%04x",c);
        else {char x[2]={(char)c,0};text_add(t,x);}
    }
    text_add(t,"\"");
}
void die(const char* s){printf("\nERROR: %s\nNo BIOS or voltage settings were changed.\n",s);fflush(0);exit(1);}
uint64_t rng_next(uint64_t* state){uint64_t z=(*state+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}

static char* trim(char* s){
    while(*s==' '||*s=='\t'||*s=='\r')s++;
    size_t n=strlen(s);
    while(n&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'))s[--n]=0;
    return s;
}
void profile_load(Profile* p,const char* path){
    memset(p,0,sizeof(*p));fmt(p->path,sizeof(p->path),"%s",path);
    FILE* f=file_open(path,"rb");if(!f)return;
    char *buf=calloc(65538,1);if(!buf){fclose(f);return;}
    size_t n=fread(buf,1,65537,f);bool io_error=ferror(f)!=0;fclose(f);
    if(io_error||n>65536){free(buf);return;}
    buf[n]=0;char *line=buf;
    if(n>=3&&(unsigned char)buf[0]==0xef&&(unsigned char)buf[1]==0xbb&&(unsigned char)buf[2]==0xbf)line+=3;
    while(*line&&p->n<MAX_PROFILE){
        char* end=strchr(line,'\n');if(end)*end=0;char *s=trim(line);
        if(*s&&*s!='#'&&*s!=';'&&*s!='['){
            char* eq=strchr(s,'=');
            if(eq){
                *eq=0;char*k=trim(s),*v=trim(eq+1);
                if(strlen(k)<sizeof(p->kv[0].key)&&strlen(v)<sizeof(p->kv[0].value)){
                    bool found=false;
                    for(size_t i=0;i<p->n;i++)if(!strcmp(p->kv[i].key,k)){fmt(p->kv[i].value,sizeof(p->kv[i].value),"%s",v);found=true;break;}
                    if(!found){fmt(p->kv[p->n].key,sizeof(p->kv[p->n].key),"%s",k);fmt(p->kv[p->n].value,sizeof(p->kv[p->n].value),"%s",v);p->n++;}
                }
            }
        }
        if(!end)break;line=end+1;
    }
    p->loaded=true;free(buf);
}
const char* profile_get(const Profile* p,const char* key){
    for(size_t i=0;i<p->n;i++)if(!strcmp(p->kv[i].key,key))return p->kv[i].value;
    return NULL;
}
