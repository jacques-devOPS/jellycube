#include <assert.h>
#include <stdio.h>
#include "storage_order.h"
int main(void) {
    assert(storage_launch_index(NULL) == -1);
    assert(storage_launch_index("") == -1);
    assert(storage_launch_index("jellycube.dol") == -1);
    assert(storage_launch_index("sd:/apps/jellycube.dol") == -1);
    assert(storage_launch_index("sd2:/apps/mplayer-ce/jellycube.dol") == 2);
    assert(storage_launch_index("SD2:/jellycube.dol") == 2);
    assert(storage_launch_index("carda:/jellycube.dol") == 0);
    assert(storage_launch_index("sda:/jellycube.dol") == 0);
    assert(storage_launch_index("cardb:/jellycube.dol") == 1);
    assert(storage_launch_index("sdb:/jellycube.dol") == 1);
    assert(storage_launch_index("sd2-other:/jellycube.dol") == -1);
    puts("storage device selection: 11 cases PASS");
    return 0;
}
