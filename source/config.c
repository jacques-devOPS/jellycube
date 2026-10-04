#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
static char *trim(char *s) {
    while(isspace((unsigned char)*s)) s++;
    char *e=s+strlen(s);while(e>s && isspace((unsigned char)e[-1])) *--e=0;
    return s;
}
void config_set_defaults(jellyfin_config_t *c) {
    memset(c,0,sizeof(*c));c->server_port=8096;
    strcpy(c->device_id,"jellycube_test_01");strcpy(c->client_name,"JellyCube");strcpy(c->client_version,"0.2-test");
}
int config_load(const char *path,jellyfin_config_t *c) {
    char line[1024];int result=-1;config_set_defaults(c);
    FILE *f=fopen(path,"r");if(!f) return -1;
    while(fgets(line,sizeof(line),f)) {
        if(!strchr(line,'\n') && !feof(f)) goto done;
        char *k=trim(line);if(!*k || *k=='#' || *k==';') continue;
        char *v=strchr(k,'=');if(!v) goto done;*v++=0;k=trim(k);v=trim(v);
        char *dest=NULL;size_t cap=0;
#define FIELD(key,field) if(!strcmp(k,key)) {dest=c->field;cap=sizeof(c->field);}
        FIELD("server",server_address)
        else FIELD("username",username)
        else FIELD("password",password)
        else FIELD("device_id",device_id)
        else FIELD("client_name",client_name)
        else if(!strcmp(k,"port")) {
            char *end;long p=strtol(v,&end,10);if(!*v || *end || p<1 || p>65535) goto done;
            c->server_port=(int)p;continue;
        } else goto done;
#undef FIELD
        if(strlen(v)>=cap) goto done;
        memcpy(dest,v,strlen(v)+1);
    }
    if(ferror(f) || !*c->server_address || !*c->username) goto done;
    /* Reject URL schemes, base paths and authorization-header injection. */
    if(strpbrk(c->server_address,"/\r\n \t") || strpbrk(c->device_id,"\"\\\r\n") ||
       strpbrk(c->client_name,"\"\\\r\n")) goto done;
    unsigned a,b,d,e;char extra;
    if(sscanf(c->server_address,"%u.%u.%u.%u%c",&a,&b,&d,&e,&extra)!=4 ||
       a>255 || b>255 || d>255 || e>255 || a==0 || a>=224) goto done;
    result=0;
done:fclose(f);return result;
}
