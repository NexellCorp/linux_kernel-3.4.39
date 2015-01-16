/******************************************************************************
 *
 * (C) COPYRIGHT 2008-2014 EASTWHO CO., LTD ALL RIGHTS RESERVED
 *
 * File name    : mio.sys.c
 * Date         : 2014.07.02
 * Author       : SD.LEE (mcdu1214@eastwho.com)
 * Abstraction  :
 * Revision     : V1.0 (2014.07.02 SD.LEE)
 *
 * Description  : API
 *
 ******************************************************************************/
#define __MIO_BLOCK_SYSFS_GLOBAL__
#include "mio.sys.h"
#include "mio.block.h"
#include "media/exchange.h"
#include "mio.definition.h"
#include "mio.smart.h"

#include "media/nfc/phy/nfc.phy.lowapi.h"

/******************************************************************************
 *
 ******************************************************************************/
DEFINE_MUTEX(miosys_ioctl_mutex);

/******************************************************************************
 * 
 ******************************************************************************/
static ssize_t miosys_read(struct file * file, char * buf, size_t count, loff_t * ppos);
static ssize_t miosys_write(struct file * file, const char * buf, size_t count, loff_t * ppos);
static long miosys_ioctl(struct file *file, u_int cmd, unsigned long arg);
static long miosys_ioctl_get_user_argument(unsigned int cmd, unsigned long arg, void * arg_buf, unsigned int arg_buf_size);
static long miosys_ioctl_read(unsigned int cmd, unsigned long arg);
static long miosys_ioctl_write(unsigned int cmd, unsigned long arg);
static long miosys_ioctl_erase(unsigned int cmd, unsigned long arg);
static long miosys_ioctl_control(unsigned int cmd, unsigned long arg);
static int miosys_print_wearleveldata(void);
static int miosys_print_smart(void);
static int miosys_print_readretrytable(void);

struct file_operations miosys_fops =
{
    .owner = THIS_MODULE,
    .read = miosys_read,
    .write = miosys_write,
    .unlocked_ioctl = miosys_ioctl,
};

struct miscdevice miosys_miscdev =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "miosys",
    .fops = &miosys_fops,
};

/******************************************************************************
 * 
 ******************************************************************************/
int miosys_init(void)
{
    miosys_dev.ioctl_mutex = &miosys_ioctl_mutex;
    miosys_dev.miscdev = &miosys_miscdev;

    return 0;
}

/******************************************************************************
 * 
 ******************************************************************************/
static ssize_t miosys_read(struct file * file, char * buf, size_t count, loff_t * ppos)
{
    char kbuf[16];
  //DBG_MIOSYS(KERN_INFO "miosys_read(file:0x%08x buf:0x%08x count:%d ppos:0x%08x)", (unsigned int)file, (unsigned int)buf, count, (unsigned int)ppos);

    if (count < 256) { return -EINVAL; }
    if (*ppos != 0) { return 0; }
    memset((void *)buf, 0, count);
    if (copy_to_user(buf, kbuf, 16)) { return -EINVAL; }
    *ppos = count;

    return count;
}

char * strtok(register char *s, register const char *delim);

static ssize_t miosys_write(struct file * file, const char * buf, size_t count, loff_t * ppos)
{
    char * cmd_buf = (char *)vmalloc(count+1);
    memset(cmd_buf, 0, count+1);
    if (copy_from_user(cmd_buf, buf, count)) { return -EINVAL; }

  //DBG_MIOSYS(KERN_INFO "miosys_write(file:0x%08x buf:0x%08x count:%d ppos:0x%08x)", (unsigned int)file, (unsigned int)buf, count, (unsigned int)ppos);
               
    // Command Parse
    {
        enum
        {
            MIOSYS_NONE = 0x00000000,

            MIOSYS_REQUEST_SMART = 0x10000000,
            MIOSYS_REQUEST_WEARLEVEL = 0x20000000,
            MIOSYS_REQUEST_READRETRYTABLE = 0x30000000,

            MIOSYS_MAX = 0xFFFFFFFF

        } state = MIOSYS_NONE;

        char delim[] = " \n";
        char * token = strtok(cmd_buf, delim);
        int breaker = 0;

        while ((token != NULL) && !breaker)
        {
            switch (state)
            {
                case MIOSYS_NONE:
                {
                    if (!memcmp((const void *)token, (const void *)"smart", strlen("smart")))
                    {
                        state = MIOSYS_REQUEST_SMART;
                    }
                    else if (!memcmp((const void *)token, (const void *)"wearlevel", strlen("wearlevel")))
                    {
                        state = MIOSYS_REQUEST_WEARLEVEL;
                    }
                    else if (!memcmp((const void *)token, (const void *)"readretrytable", strlen("readretrytable")))
                    {
                        state = MIOSYS_REQUEST_READRETRYTABLE;
                    }
                    else
                    {
                        breaker = 1;
                    }

                } break;

                case MIOSYS_REQUEST_SMART: { breaker = 1; } break;
                case MIOSYS_REQUEST_WEARLEVEL: { breaker = 1; } break;

                default: { breaker = 1; } break;

            }

            token = strtok(NULL, delim);
        }

        // execute
        switch (state)
        {
            case MIOSYS_REQUEST_SMART:          { miosys_print_smart(); } break;
            case MIOSYS_REQUEST_WEARLEVEL:      { miosys_print_wearleveldata(); } break;
            case MIOSYS_REQUEST_READRETRYTABLE: { miosys_print_readretrytable(); } break;

            default: {} break;
        }
    }

    if (cmd_buf)
    {
        vfree(cmd_buf);
    }

    *ppos = count;
    return count;
}


char * strtok(register char *s, register const char *delim)
{
    register char *spanp;
    register int c, sc;
    char *tok;
    static char *last;

    if (s == NULL && (s = last) == NULL)
        return (NULL);

    /* Skip (span) leading delimiters (s += strspn(s, delim), sort of). */
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;)
    {
        if (c == sc)
            goto cont;
    }

    if (c == 0)
    {
        /* no non-delimiter characters */
        last = NULL;
        return (NULL);
    }
    tok = s - 1;

    /* Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
     * Note that delim must have one NUL; we stop if we see that, too. */
    for (;;)
    {
        c = *s++;
        spanp = (char *)delim;

        do
        {
            if ((sc = *spanp++) == c)
            {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;

                last = s;

                return (tok);
            }

        } while (sc != 0);
    }
}

int miosys_print_smart(void)
{
    MIO_SMART_CE_DATA *pnand_accumulate = 0;
    MIO_SMART_CE_DATA *pnand_current = 0;
    unsigned int channel = 0, way = 0;
    unsigned int sum_erasecount = 0, sum_usableblocks = 0;
    unsigned int min_erasecount = 0xFFFFFFFF, max_erasecount = 0;
    unsigned int accumulated_sum_readretrycount = 0;
    unsigned int average_erasecount[2] = {123,456};

    if (!miosmart_is_init())
    {
        __PRINT(KERN_ERR "miosys_print_smart: miosmart is not inited!");
        return -1;
    }

    miosmart_update_eccstatus();

    miosmart_get_erasecount(&min_erasecount, &max_erasecount, &sum_erasecount, average_erasecount);
    sum_usableblocks = miosmart_get_total_usableblocks();

    for (way = 0; way < *Exchange.ftl.Way; way++)
    {
        for (channel = 0; channel < *Exchange.ftl.Channel; channel++)
        {
            pnand_accumulate = &(MioSmartInfo.nand_accumulate[way][channel]);

            if (pnand_accumulate)
            {
                accumulated_sum_readretrycount += pnand_accumulate->readretry_count;
            }
        }
    }

    __PRINT(KERN_INFO "SMART Information");

    __PRINT(KERN_INFO "\n Life Cycle I/O");
    __PRINT(KERN_INFO " - Read (%lld MB, %lld Sectors) and Retried (%u Times)", MioSmartInfo.io_accumulate.read_bytes >> 20, MioSmartInfo.io_accumulate.read_sectors, accumulated_sum_readretrycount);
    __PRINT(KERN_INFO " - Write (%lld MB, %lld Sectors)", MioSmartInfo.io_accumulate.write_bytes >> 20, MioSmartInfo.io_accumulate.write_sectors);

    __PRINT(KERN_INFO "\n Power Cycle I/O");
    __PRINT(KERN_INFO " - Read (%lld MB, %lld Sectors) and Retried (%u Times)", MioSmartInfo.io_current.read_bytes >> 20, MioSmartInfo.io_current.read_sectors, *Exchange.ftl.ReadRetryCount);
    __PRINT(KERN_INFO " - Write (%lld MB, %lld Sectors)", MioSmartInfo.io_current.write_bytes >> 20, MioSmartInfo.io_current.write_sectors);

    __PRINT(KERN_INFO "\n Erase Status");
    __PRINT(KERN_INFO " - Total Erase Count:    %u", sum_erasecount);
    __PRINT(KERN_INFO " - Max Erase Count:      %u", max_erasecount);
    __PRINT(KERN_INFO " - Min Erase Count:      %u", min_erasecount);
    __PRINT(KERN_INFO " - Average Erase Count:  %u.%02u", average_erasecount[0], average_erasecount[1]);

    __PRINT(KERN_INFO "\n Block Status");
    __PRINT(KERN_INFO " - Total Usable Blocks: %u", sum_usableblocks);

    for (way = 0; way < *Exchange.ftl.Way; way++)
    {
        for (channel = 0; channel < *Exchange.ftl.Channel; channel++)
        {
            unsigned int max_channel = 0;
            unsigned int max_way = 0;
            NAND nand;

            unsigned int total_block = 0;
            unsigned int total_usable_block = 0;
            unsigned int total_bad_block = 0;
            unsigned int retired_block = 0;
            unsigned int free_block = 0;
            unsigned int used_block = 0;

            Exchange.nfc.fnGetFeatures(&max_channel, &max_way, (void *)&nand);
            total_block = nand._f.mainblocks_per_lun;

            pnand_accumulate = &(MioSmartInfo.nand_accumulate[way][channel]);
            pnand_current = &(MioSmartInfo.nand_current[way][channel]);

            if (!pnand_accumulate || !pnand_current)
            {
                return -1;
            }

            total_usable_block = total_block - Exchange.ftl.fnGetBlocksCount(channel, way, BLOCK_TYPE_DATA_HOT_BAD, BLOCK_TYPE_DATA_COLD_BAD, BLOCK_TYPE_FBAD, BLOCK_TYPE_IBAD, BLOCK_TYPE_RBAD, 0xFF);
            total_bad_block    = Exchange.ftl.fnGetBlocksCount(channel, way, BLOCK_TYPE_DATA_HOT_BAD, BLOCK_TYPE_DATA_COLD_BAD, BLOCK_TYPE_FBAD, BLOCK_TYPE_IBAD, BLOCK_TYPE_RBAD, 0xFF);
            retired_block      = Exchange.ftl.fnGetBlocksCount(channel, way, BLOCK_TYPE_DATA_HOT_BAD, BLOCK_TYPE_DATA_COLD_BAD, BLOCK_TYPE_RBAD, 0xFF);
            free_block         = Exchange.ftl.fnGetBlocksCount(channel, way, BLOCK_TYPE_FREE, 0xFF);
            used_block         = total_usable_block - free_block;

            __PRINT(KERN_INFO "\n NAND CHANNEL:%d WAY:%d Information", channel, way);

            /******************************************************************
             * ECC Information
             ******************************************************************/

            __PRINT(KERN_INFO "\n  ECC Status                    Power Cycle          Life Cycle");
            __PRINT(KERN_INFO "  - Corrected Sectors:           %10u %19u", pnand_current->ecc_sector.corrected, pnand_accumulate->ecc_sector.corrected);
            __PRINT(KERN_INFO "  - Level Detected Sectors:      %10u %19u", pnand_current->ecc_sector.leveldetected, pnand_accumulate->ecc_sector.leveldetected);
            __PRINT(KERN_INFO "  - Uncorrectable Sectors:       %10u %19u", pnand_current->ecc_sector.uncorrectable, pnand_accumulate->ecc_sector.uncorrectable);

            __PRINT(KERN_INFO "\n  Fail Status                   Power Cycle          Life Cycle");
            __PRINT(KERN_INFO "  - Write Fail Count:            %10u %19u", pnand_current->writefail_count, pnand_accumulate->writefail_count);
            __PRINT(KERN_INFO "  - Erase Fail Count:            %10u %19u", pnand_current->erasefail_count, pnand_accumulate->erasefail_count);

            __PRINT(KERN_INFO "\n  Retry Status                  Power Cycle          Life Cycle");
            __PRINT(KERN_INFO "  - Read Retry Count:            %10u %19u", pnand_current->readretry_count, pnand_accumulate->readretry_count);

            __PRINT(KERN_INFO "\n  Block Status");
            __PRINT(KERN_INFO "  - Total Block:        %d", total_block);
            __PRINT(KERN_INFO "  - Total Usable Block: %d", total_usable_block);
            __PRINT(KERN_INFO "  - Total Bad Block:    %d", total_bad_block);
            __PRINT(KERN_INFO "  - Retired Block:      %d", retired_block);
            __PRINT(KERN_INFO "  - Free Block:         %d", free_block);
            __PRINT(KERN_INFO "  - Used Block:         %d", used_block);

        }
    }
    __PRINT(KERN_INFO " \n");

    return 0;
}

int miosys_print_wearleveldata(void)
{
    unsigned char channel = 0;
    unsigned char way = 0;
    unsigned int *buff = 0;
    unsigned int buff_size = 0;
    NAND nand;
    int blockindex = 0;
    unsigned int entrydata = 0;
    unsigned int attribute = 0, sub_attribute = 0;
    unsigned int erasecount = 0, partition = 0;
    unsigned int badblock_count = 0;
    unsigned int min_erasecount = 0xFFFFFFFF, max_erasecount = 0;
    unsigned int average_erasecount[2] = {0,0};
    unsigned char isvalid_erasecount = 0;
    unsigned int validnum_erasecount = 0, sum_erasecount = 0;


    if (!miosmart_is_init())
    {
        __PRINT(KERN_INFO "miosys_print_smart: miosmart is not inited!");
        return -1;
    }

    // create buffer
    Exchange.ftl.fnGetNandInfo(&nand);
    buff_size = nand._f.luns_per_ce * nand._f.mainblocks_per_lun * sizeof(unsigned int);

  //DBG_MIOSYS(KERN_INFO "buffer size:%d", buff_size);

    buff = (unsigned int *)vmalloc(buff_size);
    if (!buff)
    {
        __PRINT(KERN_INFO "mio.sys: wearlevel: memory alloc: fail");
        return -1;
    }

    for (channel=0; channel < *Exchange.ftl.Channel; channel++)
    {
        for (way=0; way < *Exchange.ftl.Way; way++)
        {
            __PRINT(KERN_INFO "NAND CH%02d-WAY%02d", channel, way);

            // Get Wearlevel data
            Exchange.ftl.fnGetWearLevelData(channel, way, buff, buff_size);

            // print wearlevel data
            for (blockindex = 0; blockindex < nand._f.mainblocks_per_lun; blockindex++)
            {
                isvalid_erasecount = 0;
                entrydata = *(buff + blockindex);
                attribute = (entrydata & 0xF0000000) >> 28;
                partition = (entrydata & 0x0F000000) >> 24;
                sub_attribute = (entrydata & 0x00F00000) >> 20;
                erasecount = (entrydata & 0x000FFFFF);

                switch (attribute)
                {
                    case BLOCK_TYPE_UPDATE_SEQUENT:
                    case BLOCK_TYPE_UPDATE_RANDOM:
                    case BLOCK_TYPE_DATA_HOT:
                    case BLOCK_TYPE_DATA_HOT_BAD:
                    case BLOCK_TYPE_DATA_COLD:
                    case BLOCK_TYPE_DATA_COLD_BAD:
                    {
                        if (!partition) { __PRINT(KERN_INFO " %08x BLOCK:%5d EraseCount:%5d DATA", entrydata, blockindex, erasecount); isvalid_erasecount = 1; }
                        else            { __PRINT(KERN_INFO " %08x BLOCK:%5d EraseCount:%5d DATA (ADMIN)", entrydata, blockindex, erasecount); }
                    } break;

                    case BLOCK_TYPE_MAPLOG: { __PRINT(KERN_INFO " %08x BLOCK:%5d EraseCount:%5d SYSTEM (M)", entrydata, blockindex, erasecount); isvalid_erasecount = 1; } break;
                    case BLOCK_TYPE_FREE:   { __PRINT(KERN_INFO " %08x BLOCK:%5d EraseCount:%5d FREE", entrydata, blockindex, erasecount); isvalid_erasecount = 1; } break;
                    case 0xA:
                    {
                        switch (sub_attribute)
                        {
                            case 0x7:             { __PRINT(KERN_INFO " %08x BLOCK:%5d PROHIBIT", entrydata, blockindex); } break;
                            case 0x8:             { __PRINT(KERN_INFO " %08x BLOCK:%5d CONFIG", entrydata, blockindex); } break;
                            case 0x9:             { __PRINT(KERN_INFO " %08x BLOCK:%5d SYSTEM (RETRY)", entrydata, blockindex); } break;
                            case BLOCK_TYPE_IBAD: { __PRINT(KERN_INFO " %08x BLOCK:%5d BAD (Initial)", entrydata, blockindex); badblock_count++; } break;
                            case BLOCK_TYPE_FBAD: { __PRINT(KERN_INFO " %08x BLOCK:%5d BAD (Factory)", entrydata, blockindex); badblock_count++; } break;
                            case BLOCK_TYPE_RBAD: { __PRINT(KERN_INFO " %08x BLOCK:%5d BAD (Runtime)", entrydata, blockindex); badblock_count++; } break;
                            case BLOCK_TYPE_ROOT: { __PRINT(KERN_INFO " %08x BLOCK:%5d SYSTEM (ROOT)", entrydata, blockindex); } break;
                            case BLOCK_TYPE_ENED: { __PRINT(KERN_INFO " %08x BLOCK:%5d SYSTEM (ENED)", entrydata, blockindex); } break;
                            case BLOCK_TYPE_FIRM: { __PRINT(KERN_INFO " %08x BLOCK:%5d SYSTEM (FW)", entrydata, blockindex); } break;
                            default:              { __PRINT(KERN_INFO " %08x BLOCK %5d SYSTEM (0x%x)", entrydata, blockindex, sub_attribute); } break;
                        }
                    } break;
                    default:  { __PRINT(KERN_INFO " %08x BLOCK %5d, unknown (0x%02x)", entrydata, blockindex, attribute); } break;
                }

                // erasecount: max/min/average
                if (isvalid_erasecount)
                {
                    if (erasecount < min_erasecount) { min_erasecount = erasecount; }
                    if (erasecount > max_erasecount) { max_erasecount = erasecount; }

                    sum_erasecount += erasecount;
                    validnum_erasecount += 1;
                }
            }

            if (validnum_erasecount)
            {
                average_erasecount[0] = sum_erasecount / validnum_erasecount;
                average_erasecount[1] = ((sum_erasecount % validnum_erasecount) * 100) / validnum_erasecount;
            }

            __PRINT(KERN_INFO "bad blocks %d", badblock_count);
            __PRINT(KERN_INFO "max erasecount %5d", max_erasecount);
            __PRINT(KERN_INFO "min erasecount %5d", min_erasecount);
            __PRINT(KERN_INFO "average erasecount %d.%02d", average_erasecount[0], average_erasecount[1]);
        }
    }
    __PRINT(KERN_INFO " \n");

    // destory buffer
    vfree(buff);

    return 0;
}

int miosys_print_readretrytable(void)
{
    if (Exchange.nfc.fnReadRetry_PrintTable)
    {
        Exchange.nfc.fnReadRetry_PrintTable();
    }
    return 0;
}

/******************************************************************************
 * 
 ******************************************************************************/
long miosys_ioctl(struct file *file, u_int cmd, unsigned long arg)
{
    long ret = 0;

#if 0
    __PRINT("miosys_ioctl:CMD:%08x", cmd);
    switch (cmd)
    {
        case  MIOSYS_MEDIA_READ   : { __PRINT("(MIOSYS_MEDIA_READ   )"); } break;
        case  MIOSYS_MEDIA_WRITE  : { __PRINT("(MIOSYS_MEDIA_WRITE  )"); } break;
        case  MIOSYS_NAND_READ    : { __PRINT("(MIOSYS_NAND_READ    )"); } break;
        case  MIOSYS_NAND_WRITE   : { __PRINT("(MIOSYS_NAND_WRITE   )"); } break;
        case  MIOSYS_NAND_ERASE   : { __PRINT("(MIOSYS_NAND_ERASE   )"); } break;
        case  MIOSYS_NANDRAW_READ : { __PRINT("(MIOSYS_NANDRAW_READ )"); } break;
        case  MIOSYS_NANDRAW_WRITE: { __PRINT("(MIOSYS_NANDRAW_WRITE)"); } break;
        case  MIOSYS_NANDRAW_ERASE: { __PRINT("(MIOSYS_NANDRAW_ERASE)"); } break;
        default: {} break;
    }
    __PRINT(" arg:%08lx\n", arg);
#endif

    mutex_lock(miosys_dev.ioctl_mutex);
    {
        // prerequisite - standby command
        while (1)
        {
            if (Exchange.ftl.fnMain)
            {
                Exchange.ftl.fnMain();
            }

            if (Exchange.ftl.fnIsReady())
            {
                if (Exchange.ftl.fnPrePutCommand(IO_CMD_STANDBY, 0, 0, 0) >= 0)
                {
                    Exchange.ftl.fnPutCommand(IO_CMD_STANDBY, 0, 0, 0);
                    break;
                }
            }
        }

        while (Exchange.ftl.fnIsBusy())
        {
            if (Exchange.ftl.fnMain)
            {
                Exchange.ftl.fnMain();
            }
        }

        switch (cmd)
        {
        case MIOSYS_CONTROL:
        {
            ret = miosys_ioctl_control(cmd, arg);
        } break;

      //case MIOSYS_MEDIA_READ:
        case MIOSYS_NAND_READ:
        case MIOSYS_NANDRAW_READ:
        {
            ret = miosys_ioctl_read(cmd, arg);
        } break;

      //case MIOSYS_MEDIA_WRITE:
        case MIOSYS_NAND_WRITE:
        case MIOSYS_NANDRAW_WRITE:
        {
            ret = miosys_ioctl_write(cmd, arg);
        } break;

        case MIOSYS_NAND_ERASE:
        case MIOSYS_NANDRAW_ERASE:
        {
            ret = miosys_ioctl_erase(cmd, arg);
        } break;

        default: { ret = -EINVAL; } break;
        }
    }
    mutex_unlock(miosys_dev.ioctl_mutex);

    return ret;
}

static long miosys_ioctl_get_user_argument(unsigned int cmd, unsigned long arg, void * arg_buf, unsigned int arg_buf_size)
{
    long ret = 0;
    void __user *argp = (void __user *)arg;
    int arg_size = 0;
  //struct miosys_media_io    *arg_media   = (struct miosys_media_io *)arg_buf;
    struct miosys_nand_io     *arg_nand    = (struct miosys_nand_io *)arg_buf;
    struct miosys_nand_raw_io *arg_nandraw = (struct miosys_nand_raw_io *)arg_buf;

    if ((arg_buf_size < sizeof(struct miosys_media_io)) ||
        (arg_buf_size < sizeof(struct miosys_nand_io)) ||
        (arg_buf_size < sizeof(struct miosys_nand_raw_io)))
    {
        return -EFAULT;
    }

    arg_size = _IOC_SIZE(cmd);
    if (!arg_size)
    {
        return -EFAULT;
    }

    if (copy_from_user(arg_buf, argp, arg_size))
    {
        return -EFAULT;
    }

    // check arguments
    switch (cmd)
    {
#if 0
    case MIOSYS_MEDIA_READ:
    case MIOSYS_MEDIA_WRITE:
    {
        if (!arg_media->buf)    { return -EINVAL; }
        if (!arg_media->blkcnt) { return -EINVAL; }
        if (arg_media->blknr + arg_media->blkcnt > *Exchange.ftl.Capacity) { return -ENXIO; }
    } break;
#endif

    case MIOSYS_NAND_READ:
    case MIOSYS_NAND_WRITE:
    {
        if (!arg_nand->buf) { return -EINVAL; }
        if (!arg_nand->len) { return -EINVAL; }
        if (((arg_nand->ofs >> 9) + (arg_nand->len >> 9)) > *Exchange.ftl.Capacity) { return -ENXIO; }
    } break;

    case MIOSYS_NAND_ERASE:
    {
        if (!arg_nand->len) { return -EINVAL; }
        if (((arg_nand->ofs >> 9) + (arg_nand->len >> 9)) > *Exchange.ftl.Capacity) { return -ENXIO; }
    } break;

    case MIOSYS_NANDRAW_READ:
    case MIOSYS_NANDRAW_WRITE:
    {
        if (!arg_nandraw->buf) { return -EINVAL; }
        if (!arg_nandraw->len) { return -EINVAL; }
        if (((arg_nandraw->ofs >> 9) + (arg_nandraw->len >> 9)) > *Exchange.ftl.Capacity) { return -ENXIO; }
        if (!arg_nandraw->pages_per_block || !arg_nandraw->bytes_per_page || !arg_nandraw->blocks_per_lun) { return -EINVAL; }
    } break;

    case MIOSYS_NANDRAW_ERASE:
    {
        if (!arg_nandraw->len) { return -EINVAL; }
        if (((arg_nandraw->ofs >> 9) + (arg_nandraw->len >> 9)) > *Exchange.ftl.Capacity) { return -ENXIO; }
        if (!arg_nandraw->pages_per_block || !arg_nandraw->bytes_per_page || !arg_nandraw->blocks_per_lun) { return -EINVAL; }
    } break;

    default: { return -ENOEXEC; } break;
    }

    return ret;
}

static long miosys_ioctl_read(unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    size_t len = 0;
    unsigned char * kbuf = 0;
    unsigned char *buf = 0;
    unsigned int arg_buf_size = 40;
    char __user arg_buf[40] = {0,};
    unsigned int buf_size = 0;
  //struct miosys_media_io    *arg_media   = (struct miosys_media_io *)arg_buf;
    struct miosys_nand_io     *arg_nand    = (struct miosys_nand_io *)arg_buf;
    struct miosys_nand_raw_io *arg_nandraw = (struct miosys_nand_raw_io *)arg_buf;

    ret = miosys_ioctl_get_user_argument(cmd, arg, (void *)arg_buf, arg_buf_size);
    if (ret < 0)
    {
        __PRINT("miosys_ioctl_read: miosys_ioctl_get_user_argument() return %ld!\n", ret);
        return ret;
    }

    switch (cmd)
    {
  //case MIOSYS_MEDIA_READ:   { buf = (unsigned char *)arg_media->buf;   buf_size = arg_media->blkcnt << 9; } break;
    case MIOSYS_NAND_READ:    { buf = (unsigned char *)arg_nand->buf;    buf_size = arg_nand->len;          } break;
    case MIOSYS_NANDRAW_READ: { buf = (unsigned char *)arg_nandraw->buf; buf_size = arg_nandraw->len;       } break;
    }

    kbuf = (unsigned char *)vmalloc(buf_size);
    if (!kbuf)  { return -ENOMEM; }

    switch (cmd)
    {
  //case MIOSYS_MEDIA_READ:
  //{
  //    int sectors = 0;
  //
  //    sectors = Exchange.ftl.fnRead(arg_media->blknr, arg_media->blkcnt, (void *)kbuf);
  //    len = sectors << 9;
  //    ret = len;
  //} break;

    case MIOSYS_NAND_READ:
    {
        if (!NFC_PHY_LOWAPI_is_init())
        {
            ret = -ENODEV;
            __PRINT("miosys_ioctl_read: error! NFC_PHY_LOWAPI is not initialized!\n");
            break;
        }

        len = arg_nand->len;
        if (NFC_PHY_LOWAPI_nand_read(arg_nand->ofs, &len, (u_char *)kbuf) < 0)
        {
            ret = -EIO;
        }

    } break;

    case MIOSYS_NANDRAW_READ:
    {
        MIO_NAND_RAW_INFO info;

        info.channel = arg_nandraw->channel;
        info.phyway = arg_nandraw->phyway;
        info.pages_per_block = arg_nandraw->pages_per_block;
        info.bytes_per_page = arg_nandraw->bytes_per_page;
        info.blocks_per_lun = arg_nandraw->blocks_per_lun;

        len = arg_nandraw->len;
        if (NFC_PHY_LOWAPI_nand_raw_read(&info, arg_nandraw->ofs, &len, (u_char *)kbuf) < 0)
        {
            ret = -EIO;
        }

    } break;

    default: { len = 0; } break;

    }

    if (copy_to_user(buf, kbuf, len))
    {
        vfree(kbuf);
        return -EFAULT;
    }

#if 0 // print read data 
if (len <= 32 * 1024)
{
    int data_idx=0;
    unsigned int *data = (unsigned int *)buf;

    DBG_MIOSYS("buf(%08x) len:%d", (unsigned int)buf, len);
    for (data_idx=0; data_idx < (len/sizeof(unsigned int)); data_idx++)
    {
        if (!(data_idx % 16))
        {
            DBG_MIOSYS("\n");
            DBG_MIOSYS("[%04x] ", data_idx);
        }

        DBG_MIOSYS("%08x ", data[data_idx]);
    }
    DBG_MIOSYS("\n");
}
#endif

    if (kbuf)
    {
        vfree(kbuf);
    }

    return ret;
}

static long miosys_ioctl_write(unsigned int cmd, unsigned long arg)
{
    size_t len = 0;
    long ret = 0;
    unsigned char * kbuf = 0;
    unsigned char *buf = 0;
    unsigned int arg_buf_size = 40;
    char __user arg_buf[40] = {0,};
    unsigned int buf_size = 0;
  //struct miosys_media_io    *arg_media   = (struct miosys_media_io *)arg_buf;
    struct miosys_nand_io     *arg_nand    = (struct miosys_nand_io *)arg_buf;
    struct miosys_nand_raw_io *arg_nandraw = (struct miosys_nand_raw_io *)arg_buf;

    ret = miosys_ioctl_get_user_argument(cmd, arg, (void *)arg_buf, arg_buf_size);
    if (ret < 0)
    {
        return ret;
    }

    switch (cmd)
    {
  //case MIOSYS_MEDIA_WRITE:   { buf = (unsigned char *)arg_media->buf;   buf_size = arg_media->blkcnt << 9; } break;
    case MIOSYS_NAND_WRITE:    { buf = (unsigned char *)arg_nand->buf;    buf_size = arg_nand->len;          } break;
    case MIOSYS_NANDRAW_WRITE: { buf = (unsigned char *)arg_nandraw->buf; buf_size = arg_nandraw->len;       } break;
    }

    kbuf = (unsigned char *)vmalloc(buf_size);
    if (!kbuf)  { return -ENOMEM; }

    if (copy_from_user(kbuf, buf, buf_size))
    {
        vfree(kbuf);
        return -EFAULT;
    }

    switch (cmd)
    {
  //case MIOSYS_MEDIA_WRITE:
  //{
  //    int sectors = 0;
  //
  //    sectors = Exchange.ftl.fnWrite(arg_media->blknr, arg_media->blkcnt, (const void *)kbuf);
  //    len = sectors << 9;
  //    ret = len;
  //} break;

    case MIOSYS_NAND_WRITE:
    {
        if (!NFC_PHY_LOWAPI_is_init())
        {
            ret = -ENODEV;
            __PRINT("miosys_ioctl_write: error! NFC_PHY_LOWAPI is not initialized!\n");
            break;
        }

        len = arg_nand->len;
        if (NFC_PHY_LOWAPI_nand_write(arg_nand->ofs, &len, (u_char *)kbuf) < 0)
        {
            ret = -EIO;
        }

    } break;

    case MIOSYS_NANDRAW_WRITE:
    {
        MIO_NAND_RAW_INFO info;

        info.channel = arg_nandraw->channel;
        info.phyway = arg_nandraw->phyway;
        info.pages_per_block = arg_nandraw->pages_per_block;
        info.bytes_per_page = arg_nandraw->bytes_per_page;
        info.blocks_per_lun = arg_nandraw->blocks_per_lun;

        len = arg_nandraw->len;
        if (NFC_PHY_LOWAPI_nand_raw_write(&info, arg_nandraw->ofs, &len, (u_char *)kbuf) < 0)
        {
            ret = -EIO;
        }

    } break;

    default: { len = 0; } break;
    }

#if 0 // print write data 
if (len <= 32 * 1024)
{
    int data_idx=0;
    unsigned int *data = (unsigned int *)kbuf;

    DBG_MIOSYS("buf(%08x) len:%d", (unsigned int)kbuf, buf_size);
    for (data_idx=0; data_idx < (buf_size/sizeof(unsigned int)); data_idx++)
    {
        if (!(data_idx % 16))
        {
            DBG_MIOSYS("\n");
            DBG_MIOSYS("[%04x] ", data_idx);
        }

        DBG_MIOSYS("%08x ", data[data_idx]);
    }
    DBG_MIOSYS("\n");
}
#endif

    if (kbuf)
    {
        vfree(kbuf);
    }

    return ret;
}

static long miosys_ioctl_erase(unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    unsigned int arg_buf_size = 40;
    char __user arg_buf[40] = {0,};
    struct miosys_nand_io     *arg_nand    = (struct miosys_nand_io *)arg_buf;
    struct miosys_nand_raw_io *arg_nandraw = (struct miosys_nand_raw_io *)arg_buf;

    ret = miosys_ioctl_get_user_argument(cmd, arg, (void *)arg_buf, arg_buf_size);
    if (ret < 0)
    {
        return ret;
    }

    switch (cmd)
    {
    case MIOSYS_NAND_ERASE:
    {
        if (!NFC_PHY_LOWAPI_is_init())
        {
            ret = -ENODEV;
            __PRINT("miosys_ioctl_erase(): error! NFC_PHY_LOWAPI is not initialized!\n");
            break;
        }

        if (NFC_PHY_LOWAPI_nand_erase(arg_nand->ofs, arg_nand->len) < 0)
        {
            ret = -EIO;
        }
    } break;

    case MIOSYS_NANDRAW_ERASE:
    {
        MIO_NAND_RAW_INFO info;
    
        info.channel = arg_nandraw->channel;
        info.phyway = arg_nandraw->phyway;
        info.pages_per_block = arg_nandraw->pages_per_block;
        info.bytes_per_page = arg_nandraw->bytes_per_page;
        info.blocks_per_lun = arg_nandraw->blocks_per_lun;
    
        if (NFC_PHY_LOWAPI_nand_raw_erase(&info, arg_nandraw->ofs, arg_nandraw->len) < 0)
        {
            ret = -EIO;
        }
    } break;

    }

    return ret;
}

static long miosys_ioctl_control(unsigned int cmd, unsigned long arg)
{
    long ret = 0;

    // add here
    // ..

    return ret;
}


