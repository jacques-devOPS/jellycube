#ifndef JC_TOKEN_CACHE_H
#define JC_TOKEN_CACHE_H
#include <stddef.h>
/*
 * Persist the Jellyfin access token next to config.ini so playlists
 * exported for MPlayer CE stay valid across JellyCube sessions.
 * Each re-authentication revokes the previous token server-side,
 * which invalidated previously exported playlists.
 */
int token_cache_path(const char *config_path, char *out, size_t size);
int token_cache_load(const char *path, char *token, size_t tsize,
                     char *user_id, size_t usize);
int token_cache_save(const char *path, const char *token, const char *user_id);
void token_cache_clear(const char *path);
#endif
