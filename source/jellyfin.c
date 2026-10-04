#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "jellyfin.h"
#include "http.h"
#include "json.h"
static char response[JSON_BUFFER_SIZE];
static json_doc_t doc;
static int safe_id(const char *s) {
    if(!*s) return 0;
    for(;*s;s++) if(!isalnum((unsigned char)*s) && *s!='-') return 0;
    return 1;
}
static int auth_header(jellyfin_client_t *c,char *out,size_t size) {
    if(strpbrk(c->access_token,"\"\\\r\n")) return -1;
    int n;
    if(c->access_token[0]) {
        n=snprintf(out,size,"X-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"GameCube\", "
            "DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"\r\n",c->config->client_name,
            c->config->device_id,c->config->client_version,c->access_token);
    } else {
        n=snprintf(out,size,"X-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"GameCube\", "
            "DeviceId=\"%s\", Version=\"%s\"\r\n",c->config->client_name,
            c->config->device_id,c->config->client_version);
    }
    return n>=0 && (size_t)n<size?0:-1;
}
static int request(jellyfin_client_t *c,const char *method,const char *path,const char *body) {
    http_connection_t conn={.socket=-1};char auth[1024];int result=-1;
    if(auth_header(c,auth,sizeof(auth))<0) {printf("Jellyfin: authorization header too long.\n");return -1;}
    printf("HTTP: %s %s:%d ...\n",method,c->config->server_address,c->config->server_port);
    if(http_connect(&conn,c->config->server_address,c->config->server_port)<0) goto fail;
    if(http_send_request(&conn,method,path,body,auth)<0) goto fail;
    if(http_receive_response(&conn,response,sizeof(response))<0) goto fail;
    result=json_parse(&doc,response);
    if(result<0) printf("JSON: response parse failed (%d).\n",doc.count);
    goto done;
fail:
    if(conn.stage==HTTP_STAGE_STATUS) printf("Network: server returned HTTP %d.\n",conn.status);
    else printf("Network: %s failed (error %d).\n",http_stage_name(conn.stage),(int)conn.error);
done:
    http_close(&conn);return result;
}
s32 jellyfin_authenticate(jellyfin_client_t *c) {
    char user[1537],pw[1537],body[3120];
    memset(c->access_token,0,sizeof(c->access_token));memset(c->user_id,0,sizeof(c->user_id));
    if(json_escape(c->config->username,user,sizeof(user))<0 || json_escape(c->config->password,pw,sizeof(pw))<0) return -1;
    int n=snprintf(body,sizeof(body),"{\"Username\":\"%s\",\"Pw\":\"%s\"}",user,pw);
    if(n<0 || n>=(int)sizeof(body)) return -1;
    int r=request(c,"POST","/Users/AuthenticateByName",body);
    memset(pw,0,sizeof(pw));memset(body,0,sizeof(body));
    if(r<0) return -1;
    int u=json_member(&doc,0,"User");
    int tok=json_member(&doc,0,"AccessToken");
    if(json_string(&doc,tok,c->access_token,sizeof(c->access_token))<0 ||
       json_string(&doc,json_member(&doc,u,"Id"),c->user_id,sizeof(c->user_id))<0 ||
       !safe_id(c->user_id) || !safe_id(c->access_token)) {
        printf("Jellyfin: authentication response omitted token or user ID.\n");
        memset(c->access_token,0,sizeof(c->access_token));return -1;
    }
    return 0;
}
static int list_items(jellyfin_client_t *c) {
    int array=json_member(&doc,0,"Items");c->count=0;
    if(array<0 || doc.tokens[array].type!=JSMN_ARRAY) return -1;
    for(int t=array+1;t<doc.count && doc.tokens[t].start<doc.tokens[array].end;t++) {
        if(doc.tokens[t].parent!=array || doc.tokens[t].type!=JSMN_OBJECT) continue;
        if(c->count==JC_PAGE_SIZE) break;
        jellyfin_item_t *i=&c->items[c->count];memset(i,0,sizeof(*i));
        if(json_string(&doc,json_member(&doc,t,"Id"),i->id,sizeof(i->id))<0 || !safe_id(i->id) ||
           json_string(&doc,json_member(&doc,t,"Name"),i->name,sizeof(i->name))<0) return -1;
        (void)json_string(&doc,json_member(&doc,t,"Type"),i->type,sizeof(i->type));
        i->folder=json_equal(&doc,json_member(&doc,t,"IsFolder"),"true") ||
                  !strcmp(i->type,"CollectionFolder") || !strcmp(i->type,"Series") || !strcmp(i->type,"Season");
        for(char *p=i->name;*p;p++) if((unsigned char)*p<32) *p=' ';
        c->count++;
    } return 0;
}
s32 jellyfin_get_libraries(jellyfin_client_t *c) {
    char path[256];snprintf(path,sizeof(path),"/Users/%s/Views",c->user_id);
    return request(c,"GET",path,NULL)<0?-1:list_items(c);
}
s32 jellyfin_get_items(jellyfin_client_t *c,const char *parent,int start) {
    if(!parent || !safe_id(parent) || start<0) return -1;
    char path[512];int n=snprintf(path,sizeof(path),
        "/Users/%s/Items?ParentId=%s&StartIndex=%d&Limit=%d&SortBy=SortName&SortOrder=Ascending&EnableImages=false&EnableUserData=false",
        c->user_id,parent,start,JC_PAGE_SIZE);
    if(n<0 || n>=(int)sizeof(path)) return -1;
    return request(c,"GET",path,NULL)<0?-1:list_items(c);
}
int jellyfin_get_stream_url(jellyfin_client_t *c,const char *item,char *url,size_t size) {
    if(!safe_id(item) || !safe_id(c->access_token)) return -1;
    /* Progressive MPEG-TS, not HLS. This conservative profile requires hardware validation. */
    int n=snprintf(url,size,"http://%s:%d/Videos/%s/stream.ts?Static=false&VideoCodec=mpeg2video&AudioCodec=mp2"
        "&VideoBitRate=1000000&AudioBitRate=128000&MaxWidth=320&MaxHeight=240&MaxFramerate=25"
        "&AudioChannels=2&AudioSampleRate=48000&EnableAutoStreamCopy=false"
        "&AllowVideoStreamCopy=false&AllowAudioStreamCopy=false&api_key=%s",
        c->config->server_address,c->config->server_port,item,c->access_token);
    return n>=0 && (size_t)n<size?0:-1;
}
int jellyfin_export_playlist(jellyfin_client_t *c,const char *item,const char *path) {
    char url[1024];if(jellyfin_get_stream_url(c,item,url,sizeof(url))<0) return -1;
    FILE *f=fopen(path,"w");if(!f) return -1;
    int ok=fprintf(f,"#EXTM3U\n#EXTINF:-1,JellyCube test stream\n%s\n",url)>0;
    if(fclose(f)!=0) ok=0;
    memset(url,0,sizeof(url));return ok?0:-1;
}
