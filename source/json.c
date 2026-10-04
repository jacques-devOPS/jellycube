#include <string.h>
#include <stdio.h>
#define JSMN_PARENT_LINKS
#include "jsmn.h"
#include "json.h"
int json_parse(json_doc_t *d, const char *s) {
    jsmn_parser p; jsmn_init(&p); d->text=s;
    d->count=jsmn_parse(&p,s,strlen(s),d->tokens,JSON_TOKENS);
    return d->count > 0 ? 0 : -1;
}
int json_equal(const json_doc_t *d, int t, const char *v) {
    return t>=0 && t<d->count && (size_t)(d->tokens[t].end-d->tokens[t].start)==strlen(v)
        && !memcmp(d->text+d->tokens[t].start,v,strlen(v));
}
int json_member(const json_doc_t *d, int obj, const char *key) {
    if(obj<0 || obj>=d->count || d->tokens[obj].type!=JSMN_OBJECT) return -1;
    for(int i=obj+1;i<d->count && d->tokens[i].start<d->tokens[obj].end;i++)
        if(d->tokens[i].parent==obj && d->tokens[i].type==JSMN_STRING && json_equal(d,i,key)) return i+1;
    return -1;
}
static int hex4(const char *s) {
    int v=0;
    for(int i=0;i<4;i++) { int c=s[i],n;
        if(c>='0' && c<='9') n=c-'0'; else if(c>='a' && c<='f') n=c-'a'+10;
        else if(c>='A' && c<='F') n=c-'A'+10; else return -1;
        v=v*16+n;
    } return v;
}
int json_string(const json_doc_t *d,int t,char *out,size_t size) {
    size_t n=0; if(!size) return -1; out[0]=0;
    if(t<0 || t>=d->count || d->tokens[t].type!=JSMN_STRING) return -1;
    const char *s=d->text+d->tokens[t].start,*end=d->text+d->tokens[t].end;
    while(s<end) {
        unsigned c=(unsigned char)*s++;
        if(c=='\\') {
            if(s==end) return -1;
            c=(unsigned char)*s++;
            if(c=='u') {
                if(end-s<4) return -1;
                int h=hex4(s); if(h<0) return -1; s+=4; c=(unsigned)h;
                if(c>=0xd800 && c<=0xdbff) {
                    if(end-s<6 || s[0]!='\\' || s[1]!='u') return -1;
                    h=hex4(s+2); if(h<0xdc00 || h>0xdfff) return -1;
                    c=0x10000+((c-0xd800)<<10)+(h-0xdc00); s+=6;
                } else if(c>=0xdc00 && c<=0xdfff) return -1;
                unsigned char b[4]; size_t k;
                if(c<0x80) { b[0]=c;k=1; }
                else if(c<0x800) { b[0]=0xc0|(c>>6);b[1]=0x80|(c&63);k=2; }
                else if(c<0x10000) { b[0]=0xe0|(c>>12);b[1]=0x80|((c>>6)&63);b[2]=0x80|(c&63);k=3; }
                else { b[0]=0xf0|(c>>18);b[1]=0x80|((c>>12)&63);b[2]=0x80|((c>>6)&63);b[3]=0x80|(c&63);k=4; }
                if(!c || n+k>=size) return -1;
                memcpy(out+n,b,k); n+=k; continue;
            }
            switch(c) { case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;
                case 'b':c='\b';break;case 'f':c='\f';break;case '"':case '\\':case '/':break;default:return -1; }
        }
        if(!c || n+1>=size) return -1;
        out[n++]=(char)c;
    }
    out[n]=0; return 0;
}
int json_escape(const char *in,char *out,size_t size) {
    size_t n=0; if(!size) return -1;
    for(const unsigned char *p=(const unsigned char *)in;*p;p++) {
        char b[7];size_t k;
        if(*p=='"' || *p=='\\') {b[0]='\\';b[1]=*p;k=2;}
        else if(*p<32) {snprintf(b,sizeof(b),"\\u%04x",*p);k=6;}
        else {b[0]=*p;k=1;}
        if(n+k>=size) return -1;
        memcpy(out+n,b,k);n+=k;
    } out[n]=0;return 0;
}
