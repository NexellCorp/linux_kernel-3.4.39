#ifndef _LINUX_NXP_ION_H_
#define _LINUX_NXP_ION_H_

#include <linux/ion.h>

enum {
    /* fix for compile warning : -Wswtich */
    ION_HEAP_TYPE_NXP_CONTIG = ION_HEAP_TYPE_CUSTOM + 1,
    // ION_HEAP_TYPE_NXP_CONTIG = ION_HEAP_TYPE_CUSTOM,
};

#define ION_HEAP_NXP_CONTIG_MASK    (1 << ION_HEAP_TYPE_NXP_CONTIG)

/* for private ioctl */
/* cmd */
#define NXP_ION_GET_PHY_ADDR        1
/* arg */
struct nxp_ion_physical {
    int ion_buffer_fd;  /* input */
    unsigned long phys; /* output */
};

#endif
