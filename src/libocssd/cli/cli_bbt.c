/**
 * bbt - CLI example for bad-block-table
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "libnvm_full.h"
#include "nvm_cli.h"

/**
 * load csv file and fill all bbt struct.
 * @param file
 * @param geo
 * @param bbt
 * @return
 */
static bool bbt_load_csv(const char *file, const struct nvm_geo *geo, struct nvm_bbt *bbt[])
{
    char line[1024];
    FILE *fd = fopen(file, "r");
    if (fd == NULL)
        return false;

    fgets(line, sizeof(line), fd);
    printf("%s\n", line);
    fgets(line, sizeof(line), fd);
    printf("%s\n", line);
    fgets(line, sizeof(line), fd);
    printf("%s\n", line);

    for (uint32_t i = 0; i < geo->nblocks; i++) {
        int index;
        fscanf(fd, "%4d,", &index);
        for (uint32_t blk_id = 0; blk_id < geo->nchannels * geo->nluns; blk_id++) {
            for (uint32_t pl = 0; pl < geo->nplanes; ++pl) {
                fscanf(fd, "%d,", (int *) &bbt[blk_id]->blks[i * geo->nplanes + pl]);
            }
        }
        fscanf(fd, "\n");
    }
    fclose(fd);
    return true;
}

static int load(struct nvm_cli *cli)
{
    nvm_cli_info_pr("nvm_bbt_all");

    bbt_save_csv("bbt_backup.csv", cli->args.dev);
    bbt_save_man("bbt_backup.txt", cli->args.dev);
    return 0;
}

static int restore(struct nvm_cli *cli)
{
    struct nvm_bbt *bbt[128] = {NULL};
    struct nvm_ret ret = {0, 0};
    const struct nvm_geo *geo = cli->args.geo;

    nvm_cli_info_pr("nvm_bbt_restore");

    int num = 0;
    for (uint64_t i = 0; i < geo->nchannels; ++i) {
        for (uint64_t j = 0; j < geo->nluns; ++j) {
            bbt[num] = calloc(1, sizeof(struct nvm_bbt) + sizeof(uint8_t) * geo->nblocks * geo->nplanes);
            struct nvm_addr addr = {.ppa=0};
            addr.g.ch = i;
            addr.g.lun = j;
            bbt[num]->addr = addr;
            bbt[num]->nblks = geo->nblocks * geo->nplanes;
            num++;
        }
    }

    if (false == bbt_load_csv("bbt_backup.csv", geo, bbt))
        return -1;

    for (int i = 0; i < num; ++i) {
        nvm_bbt_set(cli->args.dev, bbt[i], &ret);
        free((void *) bbt[i]);
    }
    return 0;
}

static int get(struct nvm_cli *cli)
{
    const struct nvm_bbt *bbt;
    struct nvm_ret ret = {0, 0};

    nvm_cli_info_pr("nvm_bbt_get");

    bbt = nvm_bbt_get(cli->args.dev, cli->args.addrs[0], &ret);
    if (!bbt) {
        nvm_cli_perror("nvm_bbt_get");
        nvm_ret_pr(&ret);
        return -1;
    }

    nvm_bbt_pr(bbt);

    return 0;
}

static int _set(struct nvm_cli_cmd_args *args, enum nvm_bbt_state state)
{
    struct nvm_bbt *bbt;
    struct nvm_ret ret = {0, 0};
    int nupdates;

    nvm_cli_info_pr("nvm_bbt_set\n");

    // Get bbt state
    bbt = nvm_bbt_alloc_cp(nvm_bbt_get(args->dev, args->addrs[0], &ret));
    if (!bbt) {
        nvm_cli_perror("nvm_bbt_get");
        nvm_ret_pr(&ret);
        return -1;
    }

    nvm_cli_info_pr("current_state");
    nvm_bbt_pr(bbt);

    for (uint64_t i = 0; i < bbt->nblks; ++i) {
        bbt->blks[i] = state;
    }

    nvm_cli_info_pr("new_state");
    nvm_bbt_pr(bbt);

    nupdates = nvm_bbt_set(args->dev, bbt, &ret);    // Persist bbt on device
    if (nupdates < 0) {
        nvm_cli_perror("nvm_bbt_set");
        nvm_ret_pr(&ret);
    } else {
        nvm_cli_info_pr("SUCCESS: {nupdates: %d}", nupdates);
    }

    nvm_bbt_free(bbt);

    return 0;
}

static int set_f(struct nvm_cli *cli)
{
    return _set(&cli->args, NVM_BBT_FREE);
}

static int set_b(struct nvm_cli *cli)
{
    return _set(&cli->args, NVM_BBT_BAD);
}

static int set_g(struct nvm_cli *cli)
{
    return _set(&cli->args, NVM_BBT_GBAD);
}

static int set_d(struct nvm_cli *cli)
{
    return _set(&cli->args, NVM_BBT_DMRK);
}

static int set_h(struct nvm_cli *cli)
{
    return _set(&cli->args, NVM_BBT_HMRK);
}

static int _mark(struct nvm_cli_cmd_args *args, enum nvm_bbt_state state)
{
    ssize_t err = 0;
    struct nvm_ret ret = {0, 0};

    nvm_cli_info_pr("nvm_bbt_mark");
    nvm_addr_prn(args->addrs, args->naddrs, args->dev);

    err = nvm_bbt_mark(args->dev, args->addrs, args->naddrs, state, &ret);
    if (err) {
        nvm_cli_perror("nvm_bbt_mark");
        nvm_ret_pr(&ret);
    }

    return err;
}

static int mark_f(struct nvm_cli *cli)
{
    return _mark(&cli->args, NVM_BBT_FREE);
}

static int mark_b(struct nvm_cli *cli)
{
    return _mark(&cli->args, NVM_BBT_BAD);
}

static int mark_g(struct nvm_cli *cli)
{
    return _mark(&cli->args, NVM_BBT_GBAD);
}

static int mark_h(struct nvm_cli *cli)
{
    return _mark(&cli->args, NVM_BBT_HMRK);
}

static int mark_d(struct nvm_cli *cli)
{
    return _mark(&cli->args, NVM_BBT_DMRK);
}

//
// Remaining code is CLI boiler-plate
//
static struct nvm_cli_cmd cmds[] = {
        //{"get",     get,     NVM_CLI_ARG_ADDR_LUN,  NVM_CLI_OPT_DEFAULT},
        //{"set_f",   set_f,   NVM_CLI_ARG_ADDR_LUN,  NVM_CLI_OPT_DEFAULT},
        //{"set_b",   set_b,   NVM_CLI_ARG_ADDR_LUN,  NVM_CLI_OPT_DEFAULT},
        //{"set_g",   set_g,   NVM_CLI_ARG_ADDR_LUN,  NVM_CLI_OPT_DEFAULT},
        //{"set_d",   set_d,   NVM_CLI_ARG_ADDR_LUN,  NVM_CLI_OPT_DEFAULT},
        //{"set_h",   set_h,   NVM_CLI_ARG_ADDR_LUN,  NVM_CLI_OPT_DEFAULT},
        //{"mark_f",  mark_f,  NVM_CLI_ARG_ADDR_LIST, NVM_CLI_OPT_DEFAULT},
        //{"mark_b",  mark_b,  NVM_CLI_ARG_ADDR_LIST, NVM_CLI_OPT_DEFAULT},
        //{"mark_g",  mark_g,  NVM_CLI_ARG_ADDR_LIST, NVM_CLI_OPT_DEFAULT},
        //{"mark_d",  mark_d,  NVM_CLI_ARG_ADDR_LIST, NVM_CLI_OPT_DEFAULT},
        //{"mark_h",  mark_h,  NVM_CLI_ARG_ADDR_LIST, NVM_CLI_OPT_DEFAULT},
        {"load",    load,    NVM_CLI_ARG_DEV_PATH,  NVM_CLI_OPT_DEFAULT},
        {"restore", restore, NVM_CLI_ARG_DEV_PATH,  NVM_CLI_OPT_DEFAULT},
};

static struct nvm_cli cli = {
        .title = "NVM bad-block-table (nvm_bbt_*)",
        .descr_short = "Retrieve (get) and modify (set/mark) the bad block table",
        .cmds = cmds,
        .ncmds = sizeof(cmds) / sizeof(cmds[0]),
};

/* Initialize and run */
int main(int argc, char **argv)
{
    int res = 0;

    if (nvm_cli_init(&cli, argc, argv) < 0) {
        nvm_cli_perror("FAILED");
        return 1;
    }

    res = nvm_cli_run(&cli);

    nvm_cli_destroy(&cli);

    return res;
}
