#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
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

static int json_int(const json_doc_t *d, int token, int *out) {
    if (!out || token < 0 || token >= d->count || d->tokens[token].type != JSMN_PRIMITIVE) return -1;
    int start = d->tokens[token].start;
    int end = d->tokens[token].end;
    int len = end - start;
    if (len <= 0 || len >= 31) return -1;
    char num[32];
    memcpy(num, d->text + start, (size_t)len);
    num[len] = 0;
    char *p = NULL;
    long v = strtol(num, &p, 10);
    if (p == num || *p != 0) return -1;
    *out = (int)v;
    return 0;
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

s32 jellyfin_validate_token(jellyfin_client_t *c) {
    char id[128];
    if(!safe_id(c->access_token)) return -1;
    if(request(c,"GET","/Users/Me",NULL)<0) return -1;
    if(json_string(&doc,json_member(&doc,0,"Id"),id,sizeof(id))<0 || !safe_id(id)) return -1;
    snprintf(c->user_id,sizeof(c->user_id),"%s",id);
    return 0;
}

static int list_items(jellyfin_client_t *c) {
    int array=json_member(&doc,0,"Items");c->count=0;
    if(array<0 || doc.tokens[array].type!=JSMN_ARRAY) return -1;
    for(int t=array+1;t<doc.count && doc.tokens[t].start<doc.tokens[array].end;t++) {
        if(doc.tokens[t].parent!=array || doc.tokens[t].type!=JSMN_OBJECT) continue;
        if(c->count==JC_PAGE_SIZE) break;
        jellyfin_item_t *i=&c->items[c->count];memset(i,0,sizeof(*i));
        i->parent_index_number = -1;
        i->index_number = -1;
        i->production_year = -1;
        if(json_string(&doc,json_member(&doc,t,"Id"),i->id,sizeof(i->id))<0 || !safe_id(i->id) ||
           json_string(&doc,json_member(&doc,t,"Name"),i->name,sizeof(i->name))<0) return -1;
        (void)json_string(&doc,json_member(&doc,t,"Type"),i->type,sizeof(i->type));
        (void)json_string(&doc,json_member(&doc,t,"SeriesName"),i->series_name,sizeof(i->series_name));
        (void)json_int(&doc, json_member(&doc,t,"ParentIndexNumber"), &i->parent_index_number);
        (void)json_int(&doc, json_member(&doc,t,"IndexNumber"), &i->index_number);
        (void)json_int(&doc, json_member(&doc,t,"ProductionYear"), &i->production_year);
        i->folder=json_equal(&doc,json_member(&doc,t,"IsFolder"),"true") ||
                  !strcmp(i->type,"CollectionFolder") || !strcmp(i->type,"Series") || !strcmp(i->type,"Season");
        for(char *p=i->name;*p;p++) if((unsigned char)*p<32) *p=' ';
        for(char *p=i->series_name;*p;p++) if((unsigned char)*p<32) *p=' ';
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

s32 jellyfin_get_stream_options(jellyfin_client_t *c,const char *item,jellyfin_stream_options_t *options) {
    if(!options || !safe_id(item) || !safe_id(c->user_id)) return -1;
    memset(options, 0, sizeof(*options));
    char path[512];
    int n = snprintf(path, sizeof(path), "/Users/%s/Items/%s?Fields=MediaSources", c->user_id, item);
    if (n < 0 || n >= (int)sizeof(path)) return -1;
    if (request(c, "GET", path, NULL) < 0) return -1;

    int media_sources = json_member(&doc, 0, "MediaSources");
    if (media_sources < 0 || doc.tokens[media_sources].type != JSMN_ARRAY) return -1;

    int source_obj = -1;
    for (int t = media_sources + 1; t < doc.count && doc.tokens[t].start < doc.tokens[media_sources].end; t++) {
        if (doc.tokens[t].parent == media_sources && doc.tokens[t].type == JSMN_OBJECT) {
            source_obj = t;
            break;
        }
    }
    if (source_obj < 0) return -1;

    int streams = json_member(&doc, source_obj, "MediaStreams");
    if (streams < 0 || doc.tokens[streams].type != JSMN_ARRAY) return -1;

    for (int t = streams + 1; t < doc.count && doc.tokens[t].start < doc.tokens[streams].end; t++) {
        if (doc.tokens[t].parent != streams || doc.tokens[t].type != JSMN_OBJECT) continue;
        int type_tok = json_member(&doc, t, "Type");
        char type[16] = {0};
        if (json_string(&doc, type_tok, type, sizeof(type)) < 0) continue;

        jellyfin_stream_t *dst = NULL;
        if (!strcmp(type, "Audio")) {
            if (options->audio_count >= JC_MAX_STREAMS) continue;
            dst = &options->audio[options->audio_count++];
        } else if (!strcmp(type, "Subtitle")) {
            if (options->subtitle_count >= JC_MAX_STREAMS) continue;
            dst = &options->subtitle[options->subtitle_count++];
        } else {
            continue;
        }

        memset(dst, 0, sizeof(*dst));
        dst->index = -1;
        dst->is_default = 0;
        (void)json_int(&doc, json_member(&doc, t, "Index"), &dst->index);
        if (dst->index < 0) {
            if (!json_equal(&doc, json_member(&doc, t, "IsExternal"), "true")) {
                (void)json_int(&doc, json_member(&doc, t, "DeliveryIndex"), &dst->index);
            }
        }
        (void)json_string(&doc, json_member(&doc, t, "Language"), dst->language, sizeof(dst->language));
        (void)json_string(&doc, json_member(&doc, t, "DisplayTitle"), dst->title, sizeof(dst->title));
        dst->is_default = json_equal(&doc, json_member(&doc, t, "IsDefault"), "true") ? 1 : 0;

        for (char *p = dst->title; *p; ++p) if ((unsigned char)*p < 32) *p = ' ';
        for (char *p = dst->language; *p; ++p) if ((unsigned char)*p < 32) *p = ' ';
    }
    return 0;
}

void jellyfin_stream_profile_defaults(jellyfin_stream_profile_t *profile) {
    if (!profile) return;
    profile->audio_stream_index = -1;
    profile->subtitle_stream_index = -1;
    profile->video_codec = "mpeg2video";
    profile->audio_codec = "mp2";
    profile->max_width = 320;
    profile->max_height = 240;
    profile->max_framerate = 25;
    profile->video_bitrate = 1000000;
    profile->audio_bitrate = 128000;
    profile->audio_channels = 2;
    profile->audio_sample_rate = 48000;
}

int jellyfin_get_stream_url(jellyfin_client_t *c,const char *item,const jellyfin_stream_profile_t *profile,char *url,size_t size) {
    jellyfin_stream_profile_t p;
    if (!profile) {
        jellyfin_stream_profile_defaults(&p);
        profile = &p;
    }
    if(!safe_id(item) || !safe_id(c->access_token)) return -1;

    int n=snprintf(url,size,"http://%s:%d/Videos/%s/stream.ts?Static=false&VideoCodec=%s&AudioCodec=%s"
        "&VideoBitRate=%d&AudioBitRate=%d&MaxWidth=%d&MaxHeight=%d&MaxFramerate=%d"
        "&AudioChannels=%d&AudioSampleRate=%d&EnableAutoStreamCopy=false"
        "&AllowVideoStreamCopy=false&AllowAudioStreamCopy=false&api_key=%s",
        c->config->server_address,c->config->server_port,item,
        profile->video_codec, profile->audio_codec,
        profile->video_bitrate, profile->audio_bitrate,
        profile->max_width, profile->max_height, profile->max_framerate,
        profile->audio_channels, profile->audio_sample_rate,
        c->access_token);

    if (n < 0 || (size_t)n >= size) return -1;

    if (profile->audio_stream_index >= 0) {
        char tail[64];
        n = snprintf(tail, sizeof(tail), "&AudioStreamIndex=%d", profile->audio_stream_index);
        if (n < 0 || strlen(url) + (size_t)n >= size) return -1;
        strcat(url, tail);
    }
    if (profile->subtitle_stream_index >= 0) {
        char tail[64];
        n = snprintf(tail, sizeof(tail), "&SubtitleStreamIndex=%d", profile->subtitle_stream_index);
        if (n < 0 || strlen(url) + (size_t)n >= size) return -1;
        strcat(url, tail);
    }
    return 0;
}

int jellyfin_export_playlist(jellyfin_client_t *c,const char *item,const jellyfin_stream_profile_t *profile,const char *path) {
    char url[1400];if(jellyfin_get_stream_url(c,item,profile,url,sizeof(url))<0) return -1;
    FILE *f=fopen(path,"w");if(!f) return -1;
    int ok=fprintf(f,"#EXTM3U\n#EXTINF:-1,JellyCube test stream\n%s\n",url)>0;
    if(fclose(f)!=0) ok=0;
    memset(url,0,sizeof(url));return ok?0:-1;
}
