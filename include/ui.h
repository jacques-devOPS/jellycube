#ifndef JC_UI_H
#define JC_UI_H

#include "jellyfin.h"

/* Phase 1 scaffold: UI backend abstraction.
   Return 0 when UI path handled input/export flow.
   Return -1 to keep current text-mode flow. */
int ui_browser_run(jellyfin_client_t *client);

#endif
