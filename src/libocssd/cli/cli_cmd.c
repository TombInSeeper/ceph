#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <nvm_cli.h>
#include <nvm_cmd.h>
#include "nvm_dev.h"
#include "libnvm_full.h"

#define DEFAULT_PM_OP_LENTH      1024
#define DEFAULT_PM_OP_OFFSET      0

//one page write
 static ssize_t ppa_wr_sync(struct nvm_dev *dev, bool do_write,
                           struct nvm_addr *blks, size_t nblks,
                           uint8_t *data, uint8_t *meta, uint16_t flags, size_t sec_len, struct nvm_ret *ret)
{
    const struct nvm_geo *geo = nvm_dev_get_geo(dev);
    int err = 0;

    for(uint32_t i = 0; i < nblks; i++)
    {
        struct nvm_addr new_nvm_addr;
        for(uint32_t j = 0; j < sec_len; j++)
        {
            new_nvm_addr = blks[i];
            uint32_t sec = (new_nvm_addr.g.sec + j ) % (geo->nsectors);
            uint32_t add_to_next_plane = (new_nvm_addr.g.sec + j ) /(geo->nsectors);

            uint32_t plane = (new_nvm_addr.g.pl + add_to_next_plane) %(geo->nplanes);
            uint32_t add_to_next_page = (new_nvm_addr.g.pl + add_to_next_plane ) /(geo->nplanes);

            uint32_t page = (new_nvm_addr.g.pg + add_to_next_page) % (geo->npages);
            uint32_t add_to_next_block = (new_nvm_addr.g.pg + add_to_next_page) / (geo->npages);

            uint32_t block = new_nvm_addr.g.blk + add_to_next_block;   // do not concer the block is over large
            if(block >= geo->nblocks)
            {
                printf("[WARN]: the sector length [%ld] you specify is overlage than a block\n", sec_len);
                printf("[WARN]: the address [%lx] will only write a few [%d] sector\n",new_nvm_addr.ppa, i);
                break;
            }

            new_nvm_addr.g.sec = sec;
            new_nvm_addr.g.pl = plane;
            new_nvm_addr.g.pg  = page;
            new_nvm_addr.g.blk = block;
            
            printf("new_nvm_addr.g.blk = %d\n", block);
            printf("new_nvm_addr.g.pg = %d\n", page);
            printf("new_nvm_addr.g.pl = %d\n", plane);
            printf("new_nvm_addr.g.sec = %d\n", sec);

             if (do_write == true) {
                err = nvm_addr_write(dev, &new_nvm_addr, 1, &data[j * geo->sector_nbytes], &meta[j * geo->sector_nbytes], flags, ret);
                if (err) {
                    nvm_cli_perror("nvm_cmd_write");
                    nvm_ret_pr(ret);
                }
            } else {
                //nvm_addr_read(dev, &addr, 1, &buf_off[j * geo->sector_nbytes], NULL, NVM_FLAG_PMODE_DUAL, NULL);
            }
        }
    }

    return 0;
}

static inline int NVM_MIN(int x, int y)
{
    return x < y ? x : y;
}

static int cmd_idfy(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;
    struct nvm_spec_idfy *idfy = NULL;

    nvm_cli_info_pr("nvm_cmd_idfy");

    idfy = nvm_cmd_idfy(dev, NULL);
    if (!idfy)
        return -1;

    nvm_spec_idfy_pr(idfy, nvm_dev_get_quirks(dev));

    nvm_buf_free(idfy);

    return 0;
}

static int cmd_format(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;

    nvm_cli_info_pr("nvm_cmd_format");

    int rlt = nvm_ext_format(dev);
    if (rlt != 0)
        return -1;

    return 0;
}

/**
 * get program erase num
 * @param cli
 * @return
 */
static int cmd_get_pe(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;

    nvm_cli_info_pr("nvm_cmd: get program erase num");
    uint16_t *buf = aligned_alloc(4096, 4096);

    int rlt = nvm_ext_get_pe(dev, buf, 4096);
    if (rlt != 0) {
        free(buf);
        return -1;
    }

    for (uint32_t i = 0; i < dev->geo.nblocks; ++i) {
        printf("[%4d], %4d\n", i, buf[i]);
    }
    free(buf);
    return 0;
}


static int cmd_get_blk_pe(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;
    struct nvm_addr addr = cli->args.addrs[0];

    nvm_cli_info_pr("nvm_cmd: get single blk pe count");

    uint32_t count;
    int rlt = nvm_ext_get_blk_pe(dev, addr, &count);
    if (rlt != 0)
        return -1;

    nvm_addr_pr_man(dev, addr);
    printf("result= %d\n", count);

    return 0;
}

/**
 * get error bit count
 * @param cli
 * @return
 */
static int cmd_get_ebc(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;
    struct nvm_addr addr = cli->args.addrs[0];

    nvm_cli_info_pr("nvm_cmd: get error bit count");

    uint32_t count;
    int rlt = nvm_ext_error_bit_count(dev, addr, &count);
    if (rlt != 0)
        return -1;

    nvm_addr_pr_man(dev, addr);
    printf("result= %d\n", count);

    return 0;
}

/**
 * get error bit count
 * @param cli
 * @return
 */
static int cmd_reset(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;

    nvm_cli_info_pr("nvm_cmd: reset device");

    int rlt = nvm_ext_reset(dev);
    if (rlt != 0)
        return -1;

    return 0;
}

static int cmd_gbbt(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;
    struct nvm_spec_bbt *bbt = NULL;
    struct nvm_addr addr = cli->args.addrs[0];

    nvm_cli_info_pr("nvm_cmd_gbbt");

    bbt = nvm_cmd_gbbt(dev, addr, NULL);
    if (!bbt)
        return -1;

    nvm_spec_bbt_pr(bbt);

    nvm_buf_free(bbt);

    return 0;
}

static int cmd_sbbt(struct nvm_cli *cli)
{
    struct nvm_dev *dev = cli->args.dev;
    struct nvm_addr *addrs = cli->args.addrs;
    size_t naddrs = cli->args.naddrs;
    uint64_t flags = cli->args.hex_vals[0];

    nvm_cli_info_pr("nvm_cmd_sbbt");
    nvm_cli_info_pr("flags: 0x%04X", flags);
    nvm_addr_prn(addrs, naddrs, dev);

    if (nvm_cmd_sbbt(dev, addrs, naddrs, flags, NULL))
        nvm_cli_perror("nvm_cmd_sbbt");

    return 0;
}

static int cmd_erase(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    int pmode = cli->evars.pmode;
    struct nvm_ret ret = {0, 0};
    ssize_t err = 0;

    nvm_cli_info_pr("nvm_cmd_erase: {pmode: %s}", nvm_pmode_str(pmode));
    if ((cli->opts.mask & NVM_CLI_OPT_SLC_MODE) &&
        cli->opts.slc_mode) {    // Fill with content from file
        nvm_cli_info_pr("nvm_cmd_erase to slc mode", nvm_pmode_str(pmode));
        pmode = pmode | NVM_FLASH_SLC_ENABLE;
    } else {
        //do nothing
    }
    
    nvm_addr_prn(args->addrs, args->naddrs, args->dev);

    struct hal_timer t;
    hal_timer_start(&t);

    if (cli->opts.mask & NVM_CLI_OPT_SLC_MODE)
    {
        int flags = 0;
        flags = pmode|NVM_FLASH_SLC_ENABLE;
        err = nvm_cmd_erase(args->dev, args->addrs, args->naddrs, flags, &ret);
    }
    else
    {
        err = nvm_cmd_erase(args->dev, args->addrs, args->naddrs, pmode, &ret);
    }
    
    if (err || ret.status != 0) {
        nvm_cli_perror("nvm_cmd_erase");
        nvm_ret_pr(&ret);
    }
    hal_timer_stop(&t);
    hal_timer_pr(&t, "nvm_cmd_erase");

    return err ? 1 : 0;
}

static int cmd_erase_sb(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    const int pmode = cli->evars.pmode;
    struct nvm_ret ret = {0, 0};
    ssize_t err = 0;

    nvm_cli_info_pr("nvm_cmd_erase: {pmode: %s}", nvm_pmode_str(pmode));
    nvm_addr_prn(args->addrs, args->naddrs, args->dev);

    struct hal_timer t;
    hal_timer_start(&t);

    if (cli->opts.mask & NVM_CLI_OPT_SLC_MODE)
    {
        int flags = 0;
        flags = pmode|NVM_FLASH_SLC_ENABLE;
        err = nvm_addr_erase_sb(args->dev, args->addrs, args->naddrs, flags, &ret);
    }
    else
    {
        err = nvm_addr_erase_sb(args->dev, args->addrs, args->naddrs, pmode, &ret);
    }
    
    if (err || ret.status != 0) {
        nvm_cli_perror("nvm_cmd_erase_sb");
        nvm_ret_pr(&ret);
    }
    hal_timer_stop(&t);
    hal_timer_pr(&t, "nvm_cmd_erase_sb");

    return err ? 1 : 0;
}

static int cmd_write(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    const int pmode = cli->evars.pmode;
    struct nvm_ret ret = {0, 0};
    ssize_t err = 0;
    int sector_cnt = 1;

    if ((cli->opts.mask & NVM_CLI_OPT_VAL_LENGTH) &&
        cli->opts.length) {    // Fill with content from file
        sector_cnt = cli->opts.length;
        printf("%s set sector cnt =%d\n", __func__, sector_cnt);
        
    } else {
        //do nothing
    }

    //if(sector_cnt > args->geo->npages * args->geo->nsectors * args->geo->nluns * args->geo->nchannels * args->geo->nplanes)
    //{
    //      printf("%s the length  = {%d} you specify is over large\n", __func__, sector_cnt);
    //      return -1;
    //}

    printf("%s naddrs=%d sector_nbytes=%ld meta_nbytes=%ld\n", __func__, args->naddrs, args->geo->sector_nbytes,
           args->geo->meta_nbytes);
    int buf_nbytes = args->naddrs * args->geo->sector_nbytes * sector_cnt ;
    char *buf = NULL;
    int meta_tbytes = args->naddrs * args->geo->meta_nbytes * sector_cnt;
    char *meta = NULL;

    buf = nvm_buf_alloc(args->geo, buf_nbytes);    // data buffer
    if (!buf) {
        nvm_cli_perror("nvm_buf_alloc");
        return -1;
    }
    if ((cli->opts.mask & NVM_CLI_OPT_FILE_INPUT) &&
        cli->opts.file_input) {    // Fill with content from file
        if (nvm_buf_from_file(buf, buf_nbytes, cli->opts.file_input)) {
            nvm_cli_perror("nvm_buf_from_file");
            nvm_buf_free(buf);
            return -1;
        }
    } else {
        nvm_buf_fill(buf, buf_nbytes);    // Fill with synthetic payload
    }

    meta = nvm_buf_alloc(args->geo, meta_tbytes);
    if (!meta) {
        nvm_cli_perror("nvm_buf_alloc");
        nvm_buf_free(buf);
        return -1;
    }
    memset(meta, 0, meta_tbytes);

    nvm_cli_info_pr("nvm_cmd_write: {pmode: %s}", nvm_pmode_str(pmode));
    nvm_addr_prn(args->addrs, args->naddrs, args->dev);

    struct hal_timer t;
    hal_timer_start(&t);

    err = ppa_wr_sync(args->dev, true,  args->addrs, args->naddrs, (uint8_t*)buf,  (uint8_t*)meta, pmode, sector_cnt, &ret);
    if (err) {
        nvm_cli_perror("nvm_cmd_write");
        nvm_ret_pr(&ret);
    }
    hal_timer_stop(&t);
    hal_timer_bw_pr(&t, "nvm_cmd_write", buf_nbytes);

    nvm_buf_free(buf);
    nvm_buf_free(meta);

    return err;
}

static int cmd_read(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    const int pmode = cli->evars.pmode;
    struct nvm_ret ret = {0, 0};
    int err = 0;

    size_t buf_nbytes = args->naddrs * args->geo->sector_nbytes;
    char *buf = NULL;
    size_t meta_tbytes = args->naddrs * args->geo->meta_nbytes;
    char *meta = NULL;

    buf = nvm_buf_alloc(args->geo, buf_nbytes);    // data buffer
    if (!buf) {
        nvm_cli_perror("nvm_buf_alloc");
        return -1;
    }

    meta = nvm_buf_alloc(args->geo, meta_tbytes);
    if (!meta) {
        nvm_cli_perror("nvm_buf_alloc");
        nvm_buf_free(buf);
        return -1;
    }
    memset(meta, 0, meta_tbytes);

    nvm_cli_info_pr("nvm_cmd_read: {pmode: %s}", nvm_pmode_str(pmode));
    nvm_addr_prn(args->addrs, args->naddrs, args->dev);

    if (meta && cli->evars.meta_pr) {
        nvm_cli_info_pr("before_read: {meta_tbytes: %d}", meta_tbytes);
        nvm_buf_pr(meta, meta_tbytes);
    }

    struct hal_timer t;
    hal_timer_start(&t);
    err = nvm_cmd_read(args->dev, args->addrs, args->naddrs, buf, meta, pmode, &ret);
    if (err) {
        nvm_cli_perror("nvm_cmd_read");
        nvm_ret_pr(&ret);
    }
    hal_timer_stop(&t);
    hal_timer_bw_pr(&t, "nvm_cmd_read", buf_nbytes);

    if (meta && cli->evars.meta_pr) {
        nvm_cli_info_pr("after_read: {meta_tbytes: %d}", meta_tbytes);
        for (size_t i = 0; i < meta_tbytes; ++i) {
            if (i)
                printf(", ");
            printf("0x%0x", meta[i]);
        }
    }

    if ((cli->opts.mask & NVM_CLI_OPT_FILE_OUTPUT) &&
        cli->opts.file_output) {    // Write buffer to file system
        if (nvm_buf_to_file(buf, buf_nbytes, cli->opts.file_output))
            nvm_cli_perror("nvm_buf_to_file");
    }

    nvm_buf_free(buf);
    nvm_buf_free(meta);

    return err;
}

static int cmd_parity_init(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    struct nvm_ret ret = {0, 0};
    int err = 0;

    err = nvm_addr_parity_init(args->dev, args->addrs, args->naddrs, &ret);
    if (err < 0) {
        nvm_cli_perror("cmd_parity_init");
        nvm_ret_pr(&ret);
    }

    return err;
}

static int cmd_parity_out(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    struct nvm_ret ret = {0, 0};
    int err = 0;

    err = nvm_addr_parity_out(args->dev, args->addrs, args->naddrs, &ret);
    if (err < 0) {
        nvm_cli_perror("cmd_parity_out");
        nvm_ret_pr(&ret);
    }

    return err;
}

static int inject_error(struct nvm_cli *cli)
{
    struct nvm_cli_cmd_args *args = &cli->args;
    int err = 0;

    err = nvm_ext_inject_error(args->dev, args->addrs[0], cli->opts.error);
    if (err < 0) {
        nvm_cli_perror("cmd_parity_out");
    }
    return err;
}

static int cmd_version(struct nvm_cli *cli)
{
    (void) cli;
    nvm_cli_info_pr("nvm_cmd_version");
    nvm_ver_pr();
    printf("Build Time: {%s %s}\n", __DATE__, __TIME__);
    printf("Git Commit: {%s}\n", GIT_SHA1);

    return 0;
}

/**
 * Command-line interface (CLI) boiler-plate
 */


/**
 * save buf to csv file.
 * @param file : file name
 * @param buf
 * @param length: the buf length to save
 * @param offset: offset of the buf to save
 */
static bool buf_save_csv(const char *file, const char* buf, long length,  long offset)
{
    FILE *fd = fopen(file, "w+");
    if (fd == NULL)
        return false;
    const char *write_buf;
    write_buf = &buf[offset]; 

    for(uint32_t i = 0; i < length; ++i)
    {
        //each 8 byte per line
        if((i != 0) && (i%8 == 0))
        {
            fprintf(fd, "\n");
        }
        fprintf(fd, "%d,", write_buf[i]);
    }

    fprintf(fd, "\n");
    fclose(fd);
    return true;
}

/**
 * load csv file and fill buf.
 * @param file : file name
 * @param buf
 * @param length: the buf length to load from file
 * @param offset: offset of the buf to load file
 * @param get_length: real length get from the csv file
 * @return 
 */
static bool buf_load_csv(const char *file, char *buf, long length, long offset, long *get_length) 
{
    char *read_buf = &buf[offset];
    FILE *fd = fopen(file, "r");
    char error_info[1024];
    *get_length = 0;
    if (fd == NULL)
    {
        sprintf(error_info, "open File = [%s] failed\n", file);
        nvm_cli_perror(error_info);
        return false;
    }
       
    uint32_t i = 0;
    while (!feof(fd))
    {
        int temp = 0;
        if(fscanf(fd, "%d,", &temp))
        {
            read_buf[i++] = (char)temp;
            //set the get_length from the csv file
             *get_length = i;
            if(i+1 > length)
            {
                break;
            }
        }
        if(fscanf(fd, "\n"))
        {
            //do nothing
        }
    }
    fclose(fd);
    return true;
}


static int pm_write(struct nvm_cli *cli)
{
	nvm_cli_info_pr("pm write");

    struct nvm_ret ret = {0, 0};
    int length = DEFAULT_PM_OP_LENTH;
    int offset = DEFAULT_PM_OP_OFFSET;
    long file_write_length = 0;

    char *buf;
    if ((cli->opts.mask & NVM_CLI_OPT_VAL_LENGTH) &&
        cli->opts.length) {    // Fill with content from file
        length = cli->opts.length;
        
    } else {
        length = DEFAULT_PM_OP_LENTH;
    }

    //buf = (char *)calloc(length, sizeof(char));
    buf = nvm_buf_alloca(4, length);    // data buffer
    if (!buf) {
        nvm_cli_perror("nvm_buf_alloca");
        return -1;
    }

    if ((cli->opts.mask & NVM_CLI_OPT_VAL_OFFSET) &&
        cli->opts.offset) {    // Fill with content from file
        offset = cli->opts.offset;
        
    } else {
        offset = DEFAULT_PM_OP_OFFSET;
    }

     if(buf_load_csv(cli->args.pm_write_file, buf, length, 0, &file_write_length) == false)
    {
        nvm_cli_perror("open csv failed");
        nvm_buf_free(buf);
        return -1;
    }

    if(file_write_length == 0)
    {
        nvm_cli_info_pr("there is no data in file [%s] to write to pm", cli->args.pm_write_file);
    }
    else
    {
        nvm_cli_info_pr("the lenth write to pm is %ld", file_write_length);
        nvm_write_pm(cli->args.dev, buf, 0, length, offset, &ret);
    }
   

    nvm_buf_free(buf);

    return 0;
}

static void buf_info_pr(char *buf, size_t nbytes)
{
	const int width = 32;

	// TODO: YAML
	printf("# NVM_BUF_PR - BEGIN **\n");
	for (size_t i = 0; i < nbytes; i++) {
		if (!(i % width))
			printf("\ni[%zu,%zu]: ", i, i + (width - 1));
		printf(" %d", buf[i]);
	}
	printf("# NVM_BUF_PR - END\n");
}


static int pm_read(struct nvm_cli *cli)
{
	nvm_cli_info_pr("pm read");

    struct nvm_ret ret = {0, 0};
    int length = DEFAULT_PM_OP_LENTH;
    int offset = DEFAULT_PM_OP_OFFSET;

    char *buf;
     if ((cli->opts.mask & NVM_CLI_OPT_VAL_LENGTH) &&
        cli->opts.length) {    // Fill with content from file
        length = cli->opts.length;
    } else {
        length = DEFAULT_PM_OP_LENTH;
        
    }

    buf = nvm_buf_alloca(4, length);    // data buffer
    if (!buf) {
        nvm_cli_perror("nvm_buf_alloca");
        return -1;
    }

    if ((cli->opts.mask & NVM_CLI_OPT_VAL_OFFSET) &&
        cli->opts.offset) {    // Fill with content from file
        offset = cli->opts.offset;
        
    } else {
        offset = DEFAULT_PM_OP_OFFSET;
    }

    int temp_ret = nvm_read_pm(cli->args.dev, buf, 0, length, offset, &ret);
    if(temp_ret < 0)
    {
        nvm_cli_perror("nvm_read_pm failed");
        nvm_buf_free(buf);
        return -1;
    }

    if ((cli->opts.mask & NVM_CLI_OPT_FILE_OUTPUT) &&
        cli->opts.file_output) {    // Write buffer to file system
        if(buf_save_csv(cli->opts.file_output, buf, length, 0) == false)
        {
            nvm_cli_perror("buf_save_csv");
        }
    }

    buf_info_pr(buf, length);

    nvm_buf_free(buf);

    return 0;
}

/* Define commands */
static struct nvm_cli_cmd cmds[] = {
        {"idfy",        cmd_idfy,        NVM_CLI_ARG_DEV_PATH,        NVM_CLI_OPT_DEFAULT},
        //{"gbbt",        cmd_gbbt,        NVM_CLI_ARG_ADDR_LUN,        NVM_CLI_OPT_DEFAULT},
        //{"sbbt",        cmd_sbbt,        NVM_CLI_ARG_ADDR_BLK_HEXVAL, NVM_CLI_OPT_DEFAULT},
        {"erase",       cmd_erase,       NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_SLC_MODE},
        {"erase_sb",    cmd_erase_sb,    NVM_CLI_ARG_ADDR_SBLK,       NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_SLC_MODE},
        {"write",       cmd_write,       NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_FILE_INPUT | NVM_CLI_OPT_VAL_LENGTH},
        {"read",        cmd_read,        NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_FILE_OUTPUT},

        {"format",      cmd_format,      NVM_CLI_ARG_DEV_PATH,        NVM_CLI_OPT_DEFAULT},
        {"get_pe",      cmd_get_pe,      NVM_CLI_ARG_DEV_PATH,        NVM_CLI_OPT_DEFAULT},
        {"get_blk_pe",  cmd_get_blk_pe,  NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT},
        {"get_ebc",     cmd_get_ebc,     NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT},
        {"reset",       cmd_reset,       NVM_CLI_ARG_DEV_PATH,        NVM_CLI_OPT_DEFAULT},
        {"parity_init", cmd_parity_init, NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT},
        {"parity_out", cmd_parity_out,   NVM_CLI_ARG_ADDR_LIST,       NVM_CLI_OPT_DEFAULT},
        {"inject",  inject_error,        NVM_CLI_ARG_INJECT_LIST,     NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_VAL_ERROR},

        {"pm_write",    pm_write,          NVM_CLI_ARG_PM_WRITE_LIST,     NVM_CLI_OPT_VAL_OFFSET | NVM_CLI_OPT_VAL_LENGTH | NVM_CLI_OPT_DEFAULT },
        {"pm_read",     pm_read,           NVM_CLI_ARG_DEV_PATH,          NVM_CLI_OPT_VAL_OFFSET | NVM_CLI_OPT_VAL_LENGTH | NVM_CLI_OPT_DEFAULT | NVM_CLI_OPT_FILE_OUTPUT},
        {"version",     cmd_version,     NVM_CLI_ARG_DEV_PATH,        NVM_CLI_OPT_DEFAULT},
};


/* Define the CLI */
static struct nvm_cli cli = {
        .title = "NVM command (nvm_cmd_*)",
        .descr_short = "Construct and execute NVM commands",
        .cmds = cmds,
        .ncmds = sizeof(cmds) / sizeof(cmds[0]),
};

/* Initialize and run */
int main(int argc, char **argv)
{
    int res = 0;

    if (nvm_cli_init(&cli, argc, argv) < 0) {
        nvm_cli_perror("nvm_cli_init");
        return 1;
    }

    res = nvm_cli_run(&cli);

    nvm_cli_destroy(&cli);

    return res;
}

