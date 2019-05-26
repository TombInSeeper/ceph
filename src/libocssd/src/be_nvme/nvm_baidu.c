//
// Created by jiash on 04/09/18.
//

#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <signal.h>

#include "nvm_dev.h"
#include "libnvm_full.h"
#include "nvm_debug.h"
#include "lightnvm.h"
#include "nvm_be.h"
#include "nvm_cmb.h"
#include "nvm_async.h"
#include "nvm_aer.h"
#include "nvme_ioctl.h"
#include "nvme.h"

enum nvm_spec_baidu_opcodes {
    NVM_OPC_FORMAT = 0x80,  //Format NVM
    NVM_OPC_EBC = 0xC8,  //Read Page EBC
    NVM_OPC_WFSA = 0xC9,  //Write File System Area
    NVM_OPC_RFSA = 0xCA,  //Read File System Area
    NVM_OPC_INJECT = 0xCC,  //Error Injection
    NVM_OPC_DEV_RST = 0xD0,  //Device Reset
    NVM_OPC_WPM = 0xD1,  //Write Persist Memory
    NVM_OPC_RPM = 0xD2,  //Read Persist Memory
    NVM_OPC_PECNT = 0xD3,  //Get PE Count Table

    NVM_OPC_RAID_INIT = 0xA0,  //RAID Parity Init
    NVM_OPC_RAID_OUT = 0xA4,  //RAID Parity Out
    NVM_OPC_ERASE_SB = 0xA8,  //Super Block Erase
    NVM_OPC_WRITE_PPA = 0xAC,  //PPA Write
    NVM_OPC_READ_PPA = 0xAD,  //PPA Read
    NVM_OPC_GET_BLK_PE = 0xE1,  //get single block erase count
    NVM_OPC_AER_SET = 0xF3,
    NVM_OPC_AER_CLEAR = 0xF4,
};

#if 0
//ony use for cdw11
enum nvm_lba_format_for_aio{
    NVM_LBA_OPCODE_START     = 0x1 << 8,
    NVM_LBA_FAKE_WRITE       = 0x1 << 10,
    NVM_LBA_SUSPEND_ENABLE   = 0x1 << 11,
    NVM_LBA_SCRAMBLE_DISABLE = 0x1 << 12,
    NVM_LBA_LIMIT_RETRY      = 0x1 << 13,
    NVM_LBA_FUA              = 0x1 << 14,
    NVM_LBA_SLC_MODE         = 0x1 << 15,
};
#endif

void nvm_baidu_close(struct nvm_dev *dev);

static inline int ioctl_baidu_io(struct nvm_dev *dev, struct nvme_passthru_cmd *cmd, struct nvm_ret *ret)
{
    cmd->nsid = (uint32_t )dev->nsid;

    const int err = ioctl(dev->fd, NVME_IOCTL_IO_CMD, cmd);
    LOG_INFO("opcode=0x%x err=0x%x result=0x%x", cmd->opcode, err, cmd->result);
    /**
     * err=0x290 : read erased page
     * err=0x281 : read UECC page
     */

    if (ret) {
        ret->result = cmd->result;
        ret->status = (uint64_t)err;
    }

    if (err == -1)
        return err;		// Propagate errno from IOCTL error

    if (cmd->result) {	// Construct errno on cmd error
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int ioctl_baidu_admin(struct nvm_dev *dev, struct nvme_admin_cmd *cmd, struct nvm_ret *ret)
{
    cmd->nsid = (uint32_t )dev->nsid;
    const int err = ioctl(dev->fd, NVME_IOCTL_ADMIN_CMD, cmd);
    LOG_INFO("opcode=0x%x err=0x%x result=0x%x", cmd->opcode, err, cmd->result);

    if (ret) {
        ret->result = cmd->result;
        ret->status = (uint64_t)err;
    }

    if (err < 0) {
        errno = EIO;
        return err;        // Propagate errno from IOCTL error
    }

    return 0;
}
static inline void set_ppa_list(struct nvme_passthru_cmd *cmd, uint64_t ppa_list)
{
    cmd->cdw10 = (uint32_t )ppa_list;
    cmd->cdw11 = (uint32_t )(ppa_list>>32);
}

int nvm_ext_error_bit_count(struct nvm_dev *dev, struct nvm_addr address, uint32_t *ebc)
{
    struct nvme_passthru_cmd cmd = {0};
    uint64_t ppa = nvm_addr_gen2dev(dev, address);

    cmd.opcode = NVM_OPC_EBC;
    set_ppa_list(&cmd, ppa);
    cmd.cdw12 = 0;

    LOG_RAW("%s ", __func__);nvm_addr_pr_man(dev, address);
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    *ebc = (uint32_t) cmd.result;
    return 0;
}

int nvm_ext_format(struct nvm_dev *dev)
{
    struct nvme_passthru_cmd cmd = {0};

    cmd.opcode = NVM_OPC_FORMAT;
    cmd.cdw10 = 1 << 12;

    LOG_INFO("");
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return 0;
}

int nvm_ext_inject_error(struct nvm_dev *dev, struct nvm_addr address, enum nvm_error_inject error_type)
{
    struct nvme_passthru_cmd cmd = {0};
    uint64_t ppa = nvm_addr_gen2dev(dev, address);

    cmd.opcode = NVM_OPC_INJECT;
    set_ppa_list(&cmd, ppa);
    cmd.cdw12 = error_type;

    LOG_RAW("%s ", __func__);nvm_addr_pr_man(dev, address);
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return 0;
}

/**
 * Inject read error
 *
 * @note
 * This is error inject cmd use to trigger read data error
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_inject_read_failed(struct nvm_dev* dev, struct nvm_addr addrs[],
                                    int naddrs, struct nvm_ret* ret)
{
	(void)(ret);
    int rlt = 0;
    for (int i = 0; i < naddrs; ++i) {
        rlt = nvm_ext_inject_error(dev, addrs[i], INJECT_Read_UNCORRECTABLE);
        if(rlt != 0)
            return rlt;
    }
    return rlt;
}
/**
 * Inject program error
 *
 * @note
 * This is error inject cmd use to trigger program data data to nand failed
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_inject_write_failed(struct nvm_dev* dev, struct nvm_addr addrs[],
                                     int naddrs, struct nvm_ret* ret)
{
	(void)(ret);
    int rlt = 0;
    for (int i = 0; i < naddrs; ++i) {
        rlt = nvm_ext_inject_error(dev, addrs[i], INJECT_Program);
        if(rlt != 0)
            return rlt;
    }
    return rlt;
}

/**
 * Inject erase error
 *
 * @note
 * This is error inject cmd use to trigger erase super block failed
 *
 *
 * @param dev Device handle obtained with `nvm_dev_open`
 * @param addrs List of memory address
 * @param naddrs Length of array of memory addresses, must be 1 in this cmd
 * @param ret Pointer to structure in which to store lower-level status and
 *            result.
 * @returns 0 on success. On error: returns -1, sets `errno` accordingly, and
 *          fills `ret` with lower-level result and status codes
 */
ssize_t nvm_addr_inject_erase_failed(struct nvm_dev* dev, struct nvm_addr addrs[],
                                     int naddrs, struct nvm_ret* ret)
{
	(void)(ret);
    int rlt = 0;
    for (int i = 0; i < naddrs; ++i) {
        rlt = nvm_ext_inject_error(dev, addrs[i], INJECT_ERASE);
        if(rlt != 0)
            return rlt;
    }
    return rlt;
}

int nvm_ext_reset(struct nvm_dev *dev)
{
    struct nvme_passthru_cmd cmd = {0};

    cmd.opcode = NVM_OPC_DEV_RST;

    LOG_INFO("");
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return 0;
}

int nvm_ext_get_pe(struct nvm_dev *dev, uint16_t *buf_pe, uint32_t len)
{
    struct nvme_passthru_cmd cmd = {0};

    memset(buf_pe, 0x00, len); //bugfix :linux system core dump
    cmd.opcode = NVM_OPC_PECNT;
    cmd.addr = (uint64_t) buf_pe;
    cmd.data_len = len;
    cmd.cdw12 = (len/512 << 16) | 0x03;

    LOG_INFO("");
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return 0;
}

int nvm_ext_get_per_lun_blks_pe(struct nvm_dev *dev, struct nvm_addr addr, uint16_t *buf_pe, uint32_t len)
{
    struct nvme_passthru_cmd cmd = {0};
    if(len != 4096)
    {
        LOG_ERROR("the lenth must be 4096");
        return -1;
    }

    memset(buf_pe, 0x00, len); //bugfix :linux system core dump
    cmd.opcode = NVM_OPC_GET_BLK_PE;
    cmd.addr = (uint64_t)buf_pe;
    cmd.data_len = len;
    cmd.cdw12 = (len/512 << 16) | 0x03;

    addr.g.blk = 0;
    addr.g.pg = 0;
    addr.g.pl = 0;
    addr.g.sec = 0;

    set_ppa_list(&cmd, nvm_addr_gen2dev(dev, addr));
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if(err)
    {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return  0;

}

int nvm_ext_get_blk_pe(struct nvm_dev *dev, struct nvm_addr addr, uint32_t *count)
{
    struct nvme_passthru_cmd cmd = {0};
    struct nvm_addr addr_back;
    memcpy(&addr_back, &addr, sizeof(struct nvm_addr));

    const struct nvm_geo *geo = nvm_dev_get_geo(dev);

    uint32_t buf_lenth = 4096;
    if(buf_lenth < (geo->nblocks *2 + 4))
    {
        LOG_WARN("illegal disk");
    }
    uint16_t *buf_pe  = aligned_alloc(4096,  buf_lenth);
    memset(buf_pe, 0x00, buf_lenth); //bugfix :linux system core dump

    cmd.opcode = NVM_OPC_GET_BLK_PE;
    cmd.addr = (uint64_t)buf_pe;
    cmd.data_len = buf_lenth;
    cmd.cdw12 = (buf_lenth/512 << 16) | 0x03;

    addr.g.blk = 0;
    addr.g.pg = 0;
    addr.g.pl = 0;
    addr.g.sec = 0;

    set_ppa_list(&cmd, nvm_addr_gen2dev(dev, addr));
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if(err)
    {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    int index = 2 + addr_back.g.blk;
    *count =  buf_pe[index];
    free(buf_pe);

    return  0;
}

int nvm_ext_aer_clean(struct nvm_dev *dev)
{
    struct nvme_passthru_cmd cmd = {0};

    cmd.opcode = NVM_OPC_AER_CLEAR;

    LOG_INFO("");
    int err = ioctl_baidu_admin(dev, &cmd, NULL);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return 0;
}

int nvm_ext_aer_set(struct nvm_dev *dev, struct nvm_ret *ret)
{
    struct nvme_passthru_cmd cmd = {0};

    cmd.opcode = NVM_OPC_AER_SET;

    LOG_INFO("");
    cmd.timeout_ms = 120000;
    int err = ioctl_baidu_admin(dev, &cmd, ret);
    if (err) {
        LOG_ERROR("FAILED: ioctl = %d", err);
        return -1;
    }

    return 0;
}

ssize_t nvm_addr_parity_init(struct nvm_dev* dev, struct nvm_addr addrs[], int naddrs, struct nvm_ret* ret)
{
    struct nvme_passthru_cmd cmd = {0};

    LOG_INFO("");
    for (int i = 0; i < naddrs; ++i) {
        uint64_t ppa = nvm_addr_gen2dev(dev, addrs[i]);
        cmd.opcode = NVM_OPC_RAID_INIT;
        cmd.cdw10 = (uint32_t )ppa;
        cmd.cdw11 = (uint32_t )(ppa>>32);

        int err = ioctl_baidu_io(dev, &cmd, NULL);
        if (err) {
            ret->result = cmd.result;
            ret->status = (uint64_t)err;
            LOG_ERROR("FAILED: ioctl = %d", err);
            return -1;
        }
    }

    ret->result = cmd.result;
    ret->status = 0;

    return 0;
}

ssize_t nvm_addr_parity_out(struct nvm_dev* dev, struct nvm_addr addrs[], int naddrs, struct nvm_ret* ret)
{
    struct nvme_passthru_cmd cmd = {0};

    LOG_INFO("");
    for (int i = 0; i < naddrs; ++i) {
        uint64_t ppa = nvm_addr_gen2dev(dev, addrs[i]);
        cmd.opcode = NVM_OPC_RAID_OUT;
        cmd.cdw10 = (uint32_t )ppa;
        cmd.cdw11 = (uint32_t )(ppa>>32);

        int err = ioctl_baidu_io(dev, &cmd, NULL);
        if (err) {
            ret->result = cmd.result;
            ret->status = (uint64_t)err;
            LOG_ERROR("FAILED: ioctl = %d", err);
            return -1;
        }
    }

    ret->result = cmd.result;
    ret->status = 0;

    return 0;
}

ssize_t nvm_addr_async_parity_init(struct nvm_dev* dev, struct cmd_ctx* ctx)
{
    (void)dev;
    (void)ctx;
    return 0;
}
ssize_t nvm_addr_async_parity_out(struct nvm_dev* dev, struct cmd_ctx* ctx)
{
    (void)dev;
    (void)ctx;
    return 0;
}
int nvm_baidu_write_file(struct nvm_dev *dev, uint32_t fileID, const uint8_t *buf, uint32_t len)
{
    (void)dev; (void)fileID; (void)buf; (void)len;
    return -1;
}

int nvm_baidu_read_file(struct nvm_dev *dev, uint32_t fileID, const uint8_t *buf, uint32_t len)
{
    (void)dev; (void)fileID; (void)buf; (void)len;
    return -1;
}

bool nvme_get_namespace(struct nvm_dev *dev)
{
    struct nvme_passthru_cmd cmd = { 0 };
    cmd.opcode = nvme_admin_identify;
    cmd.nsid = dev->nsid;
    cmd.addr = (uint64_t)(uintptr_t) &dev->ns;
    cmd.data_len = 0x1000;
    if(ioctl(dev->fd, NVME_IOCTL_ADMIN_CMD, &cmd)) {
        LOG_ERROR("ioctl failed");
        return false;
    }
    return true;
}

static struct nvm_dev *handle_dev;

static void stop_handler(int signum)
{
    struct nvm_dev *dev = handle_dev;

    nvm_baidu_close(dev);
    LOG_DEBUG("singal %d exit", signum);
    exit(-1);
}

struct nvm_dev *nvm_baidu_open(const char *dev_path, int flags)
{
    (void)flags;
    struct nvm_dev *dev = NULL;
    char nvme_name[NVM_DEV_PATH_LEN] = {0};
    int err;

    if (strlen(dev_path) > NVM_DEV_PATH_LEN) {
        LOG_ERROR("FAILED: Device path too long\n");
        errno = EINVAL;
        return NULL;
    }

    dev = calloc(1, sizeof(*dev));
    if (!dev) {
        LOG_ERROR("FAILED: allocating 'struct nvm_dev'\n");
        return NULL;	// Propagate errno from malloc
    }
    handle_dev = dev;

    strncpy(dev->path, dev_path, NVM_DEV_PATH_LEN);
    strncpy(dev->name, dev_path+5, NVM_DEV_NAME_LEN);

    // HACK: use naming conventions to determine nsid, fallback to hardcode
    if (nvm_be_split_dpath(dev_path, nvme_name, &dev->nsid))
        dev->nsid = 1;

    LOG_INFO("path=%s", dev->path);
    dev->fd = open(dev->path, O_RDWR | O_DIRECT);
    if (dev->fd < 0) {
        LOG_ERROR("FAILED: open(dev->path(%s)), dev->fd(%d)\n", dev->path, dev->fd);
        free(dev);
        return NULL;	// Propagate errno from open
    }

    nvme_get_namespace(dev);

    err = nvm_be_populate(dev, &nvm_be_baidu);
    if (err) {
        LOG_ERROR("FAILED: nvm_be_populate");
        goto error;
    }

    err = nvm_be_populate_derived(dev);
    if (err) {
        LOG_ERROR("FAILED: nvm_be_populate_derived");
        goto error;
    }

    char pm_file[4096];
    sprintf(pm_file, "/sys/block/%sn%d/device/device/resource0", nvme_name, dev->nsid);
    dev->pm = nvm_pm_open(pm_file, DEFAULT_PM_SIZE, DEFAULT_PM_OFFSET);
    if(dev->pm == NULL) {
        LOG_ERROR("open pm(%s) failed", pm_file);
        goto error;
    }

    dev->aio = nvm_async_init(dev->fd, 0);
    if(dev->aio == NULL) {
        LOG_ERROR("async_init failed");
        goto error;
    }

    nvm_aer_open(dev);

    signal(SIGINT, stop_handler);
    return dev;

error:
    close(dev->fd);
    free(dev);
    return NULL;
}

void nvm_baidu_close(struct nvm_dev *dev)
{
    LOG_INFO("");
    nvm_aer_close(dev);
    nvm_async_term(dev->aio);
    nvm_pm_close(dev->pm);
    close(dev->fd);
    LOG_INFO("exit");
}

struct nvm_spec_idfy *nvm_baidu_idfy(struct nvm_dev *dev, struct nvm_ret *ret)
{
    struct nvme_passthru_cmd cmd = {0};
    struct nvm_spec_idfy *idfy = NULL;

    LOG_INFO("");
    idfy = nvm_buf_alloca(0x1000, sizeof(*idfy));
    if (!idfy) {
        errno = ENOMEM;
        return NULL;
    }
    memset(idfy, 0, sizeof(*idfy));

    cmd.opcode = NVM_OPC_IDFY;
    cmd.addr = (uint64_t)idfy;
    cmd.data_len = sizeof(*idfy);

    int err = ioctl_baidu_admin(dev, &cmd, ret);
    if (err) {
        LOG_ERROR("FAILED: ioctl_vam");
        nvm_buf_free(idfy);
        return NULL; // NOTE: Propagate errno
    }

    return idfy;
}

struct nvm_spec_bbt *nvm_baidu_gbbt(struct nvm_dev *dev, struct nvm_addr addr, struct nvm_ret *ret)
{
    struct nvme_passthru_cmd cmd = {0};
    struct nvm_spec_bbt *spec_bbt;
    size_t spec_bbt_sz;

    size_t nblks = dev->geo.nblocks * dev->geo.nplanes;
    spec_bbt_sz = sizeof(*spec_bbt) + sizeof(*(spec_bbt->blk)) * nblks;
    spec_bbt = nvm_buf_alloc(&dev->geo, spec_bbt_sz);
    if (!spec_bbt) {
        LOG_ERROR("FAILED: malloc k_bbt failed");
        errno = ENOMEM;
        return NULL;
    }

    cmd.opcode = NVM_S12_OPC_GET_BBT;
    cmd.addr = (uint64_t)spec_bbt;
    cmd.data_len = (uint32_t)spec_bbt_sz;
    addr.g.blk = 0;
    addr.g.pg = 0;
    addr.g.pl = 0;
    addr.g.sec = 0;
    set_ppa_list(&cmd, nvm_addr_gen2dev(dev, addr));
    cmd.cdw12 = 0;//nppas = 0
#if 0
    printf("%s\n", __func__);
    uint32_t *buf= (uint32_t*)&cmd.vadmin;
    for (uint32_t i = 0; i < sizeof(cmd.vadmin)/sizeof(uint32_t); ++i) {
        printf("[%02d] 0x%08x\n", i, buf[i]);
    }
#endif
    int err = ioctl_baidu_admin(dev, &cmd, ret);
    if (err || (spec_bbt->tblks != nblks)) {
        LOG_ERROR("FAILED: be execution failed");
        errno = EIO;
        nvm_buf_free(spec_bbt);
        return NULL;
    }
    if (!(spec_bbt->tblid[0] == 'B' && spec_bbt->tblid[1] == 'B' &&
          spec_bbt->tblid[2] == 'L' && spec_bbt->tblid[3] == 'T')) {
        LOG_ERROR("FAILED: invalid format of returned bbt");
        errno = EIO;
        nvm_buf_free(spec_bbt);
        return NULL;
    }

    return spec_bbt;
}

int nvm_baidu_sbbt(struct nvm_dev *dev, struct nvm_addr *addrs, int naddrs, uint16_t flags, struct nvm_ret *ret)
{
    struct nvme_passthru_cmd cmd = {0};
    uint64_t dev_addrs[naddrs];

    switch(flags) {
        case NVM_BBT_FREE:
        case NVM_BBT_BAD:
        case NVM_BBT_DMRK:
        case NVM_BBT_GBAD:
        case NVM_BBT_HMRK:
            break;
        default:
        LOG_ERROR("FAILED: invalid mark");
            errno = EINVAL;
            return -1;
    }

    if (naddrs > NVM_NADDR_MAX) {
        LOG_ERROR("FAILED: naddrs > NVM_NADDR_MAX");
        errno = EINVAL;
        return -1;
    }

    for (int i = 0; i < naddrs; ++i) { // Setup PPAs: Convert format
        if (nvm_addr_check(addrs[i], dev)) {
            LOG_ERROR("FAILED: invalid addrs[i]");
            errno = EINVAL;
            return -1;
        }
        dev_addrs[i] = nvm_addr_gen2dev(dev, addrs[i]);
    }

    cmd.opcode = NVM_S12_OPC_SET_BBT; // Construct command
    cmd.cdw12 = naddrs - 1; // Unnatural numbers: counting from zero
    cmd.cdw12 |= (flags | NVM_FLAG_PMODE_SNGL | NVM_FLAG_SCRBL) <<16; //control
    set_ppa_list(&cmd, naddrs == 1 ? dev_addrs[0] : (uint64_t)dev_addrs);
#if 0
    printf("%s\n", __func__);
    uint32_t *buf= (uint32_t*)&cmd.vadmin;
    for (uint32_t i = 0; i < sizeof(cmd.vadmin)/sizeof(uint32_t); ++i) {
        printf("[%02d] 0x%08x\n", i, buf[i]);
    }
#endif
    int err = ioctl_baidu_admin(dev, &cmd, ret);
    if (err) {
        LOG_ERROR("FAILED: be execution failed");
        errno = EIO;
        return -1;
    }

    return 0;
}

/**
 * Helper function for vector IO: erase/write/read
 */
static inline int cmd_baidu_ewr(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs,
                                void *data, void *meta, uint16_t flags, uint16_t opcode, struct nvm_ret *ret)
{
    struct nvme_passthru_cmd cmd = {0};
    uint64_t dev_addrs[naddrs];
    int err = 0;

    LOG_INFO("");
    if (naddrs > NVM_NADDR_MAX) {
        errno = EINVAL;
        return -1;
    }

    cmd.opcode = (uint8_t)opcode;

    // Setup PPAs: Convert address format from generic to device specific
    for (int i = 0; i < naddrs; ++i) {
        dev_addrs[i] = nvm_addr_gen2dev(dev, addrs[i]);
//        LOG_RAW("op=0x%x ", opcode);
//        nvm_addr_pr_man(dev, addrs[i]);
    }

    // Unnatural numbers: counting from zero
    cmd.cdw12 = naddrs-1;
    cmd.cdw12 |= (flags | NVM_FLAG_PMODE_SNGL | NVM_FLAG_SCRBL) <<16;
    cmd.cdw10 = (uint32_t )dev_addrs[0];

#if 0
    //the slc write/read specify at cdw11, but incompatible with erase api, so the user can specify the flags when want to 
    //read or write at slc mode
    if(flags & NVM_FLASH_SLC_ENABLE)
    {
        cmd.cdw11 = (uint32_t )(dev_addrs[0]>>32) | NVM_LBA_SLC_MODE;
    }
    else
    {
        cmd.cdw11 = (uint32_t )(dev_addrs[0]>>32);
    }
#endif
	cmd.cdw11 = (uint32_t )(dev_addrs[0]>>32);
    cmd.addr = (uint64_t)data;
    cmd.data_len = data ? dev->geo.sector_nbytes * naddrs : 0;
    cmd.metadata = (uint64_t)meta;
    cmd.metadata_len = meta ? dev->geo.meta_nbytes * naddrs : 0;
#if 0
	uint32_t *buf= (uint32_t*)&cmd;
	for (uint32_t i = 0; i < sizeof(cmd)/sizeof(uint32_t); ++i) {
		printf("[%02d] 0x%08x\n", i, buf[i]);
	}
    fflush(stdout);
#endif
    err = ioctl_baidu_io(dev, &cmd, ret);
    return err;
}

int nvm_baidu_erase(struct nvm_dev *dev, struct nvm_addr addrs[], int naddrs, uint16_t flags, struct nvm_ret *ret)
{
    int rlt[naddrs];
//#ifdef _OPENMP
//#pragma omp parallel for
//#endif
    for (int i = 0; i < naddrs; ++i) {
        rlt[i] = cmd_baidu_ewr(dev, &addrs[i], 1, NULL, NULL, flags, NVM_OPC_ERASE, ret);
    }

    for (int j = 0; j < naddrs; ++j) {
        if(rlt[j] != 0)
            return rlt[j];
    }
    return 0;
}

ssize_t nvm_addr_erase_sb(struct nvm_dev* dev, struct nvm_addr addrs[], int naddrs, uint16_t flags, struct nvm_ret* ret)
{
    LOG_INFO("naddrs=%d flags=%x", naddrs, flags);
    for (int i = 0; i < naddrs; ++i) {
        struct nvm_addr tmp = {.ppa=0};
        tmp.g.blk = addrs[i].g.blk;
        int rlt = cmd_baidu_ewr(dev, &tmp, 1, NULL, NULL, flags, NVM_OPC_ERASE_SB, ret);
        if(rlt < 0) {
            LOG_ERROR("rlt=%d", rlt);
            return rlt;
        }
    }
    return 0;
}

int nvm_ioctl_write_PPA(struct nvm_dev *dev, struct nvm_addr *addrs, int naddrs, void *data, void *meta, uint16_t flags,
                        struct nvm_ret *ret)
{
    (void)meta;
    return cmd_baidu_ewr(dev, addrs, naddrs, data, NULL, flags, NVM_S12_OPC_WRITE, ret);
}

int nvm_ioctl_read_PPA(struct nvm_dev *dev, struct nvm_addr *addrs, int naddrs, void *data, void *meta, uint16_t flags,
                       struct nvm_ret *ret)
{
    (void)meta;
    int rlt = cmd_baidu_ewr(dev, addrs, naddrs, data, NULL, flags, NVM_S12_OPC_READ, ret);
    return rlt;
}

struct nvm_be nvm_be_baidu = {
        .id = NVM_BE_IOCTL,

        .open = nvm_baidu_open,
        .close = nvm_baidu_close,

        .idfy = nvm_baidu_idfy,
        .rprt = NULL,
        .sbbt = nvm_baidu_sbbt,
        .gbbt = nvm_baidu_gbbt,

        .erase = nvm_baidu_erase,
        .write = nvm_ioctl_write_PPA,
        .read = nvm_ioctl_read_PPA,
        .copy = NULL,
};
