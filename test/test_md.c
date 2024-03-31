#include <platform/sdk.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <signal.h>
#include <unistd.h>
#define __NO_MMAN__
#include <aux/logger.h>
#include <cfg/sensor_config.h>
#include "mpi_vda.h"
#include "mpi_sys.h"
#include <stdint.h>


void *mmap(void *start, size_t len, int prot, int flags, int fd, uint32_t off);

int stop_flag = 0;

void action_on_signal(int signum)
{
    log_info("STOP");
    stop_flag = 1;
}

void print_error(int ret)
{
    if (-1 == ret)
    {
        log_error("I/O error, %d: %s", errno, strerror(errno));
    }
    else
    {
        log_error("Error %s(%d) at line %u, pos %u", cfg_proc_err_msg(ret), ret, cfg_proc_err_line_num(), cfg_proc_err_line_pos());
        if (ret == CFG_PROC_WRONG_SECTION)
        {
            log_error("Wrong section name [%s]", cfg_sensor_read_error_value());
        }
        else if (ret == CFG_PROC_KEY_BAD)
        {
            log_error("Wrong key \"%s\"", cfg_sensor_read_error_key());
        }
        else if (ret == CFG_PROC_VALUE_BAD)
        {
            log_error("Wrong value \"%s\" for %s", cfg_sensor_read_error_value(), cfg_sensor_read_error_key());
        }
    }
}

typedef struct vda_chn_info_s
{
    int fd;
    unsigned phy_addr;
    void* ptr;
    unsigned sz;
} vda_chn_info_t;

vda_chn_info_t vda_chns[VDA_CHN_NUM_MAX];
static int vda_mem_fd = -1;

int xhi_MPI_VDA_GetFd(unsigned VdaChn)
{
    if (VDA_CHN_NUM_MAX <= VdaChn) return 0xA0098002;

    int fd = open("/dev/vda", 2, 0);
    if (fd < 0)
    {
        log_error("Can't open /dev/vda: %s (%d)", strerror(errno), errno);
        return fd;
    }

    unsigned param = VdaChn;
    int res = ioctl(fd, 0x40044d0d, &param);
    if (res) return res;

    vda_chns[VdaChn].fd = fd;

    return fd;
}

int xhi_MPI_VDA_CreateChn(VDA_CHN VdaChn, const VDA_CHN_ATTR_S *pstAttr)
{
    vda_mem_fd = open("/dev/mem", 4162);
    if (vda_mem_fd < 0)
    {
        log_error("Can't open /dev/mem: %s (%d)", strerror(errno), errno);
        return vda_mem_fd;
    }

    int vda_chn_fd = xhi_MPI_VDA_GetFd(VdaChn);

    log_info("ptr is 0x%x, fd=%d", pstAttr, vda_chn_fd);
    int ret = ioctl(vda_chn_fd, 0x40a84d00, pstAttr);
    if (ret)
    {
        log_error("Can't ioctl 0x40a84d00: 0x%x", ret);
        return ret;
    }

    unsigned param[2];
    ret = ioctl(vda_chn_fd, 0Xc0084d0e, param);
    if (ret)
    {
        log_error("Can't ioctl 0Xc0084d0e: 0x%x", ret);
        return ret;
    }

    log_info("GOT mem at 0x%x, sz=%u", param[0], param[1]);
    vda_chns[VdaChn].phy_addr = param[0];
    vda_chns[VdaChn].sz = param[1];

    unsigned alligned = param[0] & 0xFFFFF000;
    unsigned offset = param[0] - (param[0] & 0xFFFFF000);
    unsigned sz_mmap = ((param[1] + offset - 1) & 0xFFFFF000) + 0x1000;

    log_info("mmap now from 0x%x %u bytes", alligned, sz_mmap);
    void* ptr = mmap(0, sz_mmap, 3, 1, vda_mem_fd, alligned);
    if (ptr == (void*)(-1))
    {
        log_error("Can't mmap: (%d) %s", errno, strerror(errno));
        ioctl(vda_chn_fd, 0X4d01);
        return 0xa009800c;
    }

    vda_chns[VdaChn].ptr = ptr + offset;
    log_info("vda chnl ptr is 0x%x", vda_chns[VdaChn].ptr);

    return 0;
}

HI_S32 xhi_MPI_VDA_StartRecvPic(VDA_CHN VdaChn)
{
    int vda_chn_fd = xhi_MPI_VDA_GetFd(VdaChn);

    if (vda_chn_fd < 0) return vda_chn_fd;

    return ioctl(vda_chn_fd, 0x4d06);
}

struct vda_get_data_param_t
{
    VDA_DATA_S vda_data;
    HI_BOOL blocking;
    uint32_t reserved[5];
};

HI_S32 xhi_MPI_VDA_GetData(VDA_CHN VdaChn, VDA_DATA_S *pstVdaData, HI_BOOL bBlock)
{
    int fd = xhi_MPI_VDA_GetFd(VdaChn);
    if (fd < 0) return fd;

    if (!pstVdaData) return 0xA0098006;

    struct vda_get_data_param_t param;
    memset(&param, 0, sizeof(struct vda_get_data_param_t));
    param.blocking = bBlock;

    int ret = ioctl(fd, 0xc0504d08, &param);
    if (ret) return ret;

//    log_info("Got VDA DATA: enWorkMode = %u, enMbSize = %u, razmer %ux%u", 
//        param.vda_data.enWorkMode, param.vda_data.enMbSize,
//        param.vda_data.u32MbWidth, param.vda_data.u32MbHeight);

    *pstVdaData = param.vda_data;
    if (param.vda_data.enWorkMode == VDA_WORK_MODE_MD)
    {
        if (param.vda_data.unData.stMdData.bMbSadValid)
        {
            pstVdaData->unData.stMdData.stMbSadData.pAddr = vda_chns[VdaChn].ptr + (unsigned)pstVdaData->unData.stMdData.stMbSadData.pAddr - vda_chns[VdaChn].phy_addr;
//            log_info("unData.stMdData.stMbSadData.pAddr is 0x%x (global ptr 0x%x)", pstVdaData->unData.stMdData.stMbSadData.pAddr, vda_chns[VdaChn].ptr);
        }
        if (param.vda_data.unData.stMdData.bObjValid)
        {
            pstVdaData->unData.stMdData.stObjData.pstAddr = (VDA_OBJ_S *)(vda_chns[VdaChn].ptr + (unsigned)pstVdaData->unData.stMdData.stObjData.pstAddr - vda_chns[VdaChn].phy_addr);
//            log_info("OBJ! Data at 0x%x", pstVdaData->unData.stMdData.stObjData.pstAddr);
        }
    }

    return 0;
}

HI_S32 xhi_MPI_VDA_ReleaseData(VDA_CHN VdaChn, const VDA_DATA_S* pstVdaData)
{
    int fd = xhi_MPI_VDA_GetFd(VdaChn);
    if (fd < 0) return fd;

    if (!pstVdaData) return 0xA0098006;

    return ioctl(fd, 0x40484D09, pstVdaData);
}

void print_md_data(const VDA_DATA_S *pstVdaData)
{
    if (pstVdaData->unData.stMdData.bObjValid && pstVdaData->unData.stMdData.stObjData.u32ObjNum > 0)
    {
        log_info("ObjNum=%d, IndexOfMaxObj=%d, SizeOfMaxObj=%d, SizeOfTotalObj=%d", \
                   pstVdaData->unData.stMdData.stObjData.u32ObjNum, \
             pstVdaData->unData.stMdData.stObjData.u32IndexOfMaxObj, \
             pstVdaData->unData.stMdData.stObjData.u32SizeOfMaxObj,\
             pstVdaData->unData.stMdData.stObjData.u32SizeOfTotalObj);
        for (int i=0; i<pstVdaData->unData.stMdData.stObjData.u32ObjNum; i++)
        {
            VDA_OBJ_S *pstVdaObj = pstVdaData->unData.stMdData.stObjData.pstAddr + i;
            log_info("[%d]\t left=%d, top=%d, right=%d, bottom=%d", i, \
              pstVdaObj->u16Left, pstVdaObj->u16Top, \
              pstVdaObj->u16Right, pstVdaObj->u16Bottom);
        }
    }
/*
    if (!pstVdaData->unData.stMdData.bPelsNumValid)
    {
        log_info("bMbObjValid = FALSE");
    }
    else
    {
        log_info("AlarmPixelCount=%d\n", pstVdaData->unData.stMdData.u32AlarmPixCnt);
    }
*/
}

void test_RUN()
{
    unsigned VdaChn = 0;

    fd_set read_fds;
    struct timeval TimeoutVal;

    int fd = xhi_MPI_VDA_GetFd(VdaChn);
    if (fd < 0) return;

    int maxfd = fd;

    while (!stop_flag)
    {
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        int s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            log_error("select() error");
            return;
        }
        else if (s32Ret == 0)
        {
            log_info("TIMEOUT");
            break;
        }

        if (!FD_ISSET(fd, &read_fds))
        {
            log_error("WHAT???");
            return;
        }

        VDA_DATA_S stVdaData;

        s32Ret = xhi_MPI_VDA_GetData(VdaChn, &stVdaData, HI_TRUE);
        if (!s32Ret)
        {
            print_md_data(&stVdaData);
        }
        else
        {
            log_error("xhi_MPI_VDA_GetData returned error 0x%x", s32Ret);
        }
        s32Ret = xhi_MPI_VDA_ReleaseData(VdaChn,&stVdaData);
    }
}

void test_md()
{
    unsigned VdaChn = 0;

    VDA_CHN_ATTR_S stVdaChnAttr;
    MPP_CHN_S stSrcChn, stDestChn;

    stVdaChnAttr.enWorkMode = VDA_WORK_MODE_MD;
    stVdaChnAttr.u32Width   = 256;
    stVdaChnAttr.u32Height  = 144;

    stVdaChnAttr.unAttr.stMdAttr.enVdaAlg      = VDA_ALG_REF;
    stVdaChnAttr.unAttr.stMdAttr.enMbSize      = VDA_MB_16PIXEL;
    stVdaChnAttr.unAttr.stMdAttr.enMbSadBits   = VDA_MB_SAD_8BIT;
    stVdaChnAttr.unAttr.stMdAttr.enRefMode     = VDA_REF_MODE_DYNAMIC;
    stVdaChnAttr.unAttr.stMdAttr.u32MdBufNum   = 8;
    stVdaChnAttr.unAttr.stMdAttr.u32VdaIntvl   = 10;
    stVdaChnAttr.unAttr.stMdAttr.u32BgUpSrcWgt = 128;
    stVdaChnAttr.unAttr.stMdAttr.u32SadTh      = 4000;
    stVdaChnAttr.unAttr.stMdAttr.u32ObjNumMax  = 16;

    int fd = xhi_MPI_VDA_GetFd(VdaChn);
    if(fd < 0)
    {
        log_error("Can't open VDA chn: 0x%x", fd);
        return;
    }

    int s32Ret = xhi_MPI_VDA_CreateChn(VdaChn, &stVdaChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        log_error("Can't create VDA chn: 0x%x", s32Ret);
        return;
    }

    log_info("going to bind");
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = 1;
    stSrcChn.s32DevId = 0;

    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = VdaChn;
    stDestChn.s32DevId = 0;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if(s32Ret != HI_SUCCESS)
    {
        log_error("Can't bind VDA to VI: 0x%x", s32Ret);
        return;
    }

    /* step 3: vda chn start recv picture */
    log_info("going to start");
    s32Ret = xhi_MPI_VDA_StartRecvPic(VdaChn);
    if(s32Ret != HI_SUCCESS)
    {
        log_error("Can't HI_MPI_VDA_StartRecvPic: 0x%x", s32Ret);
        return;
    }


    test_RUN();

return;
// GYGYGY

    s32Ret = HI_MPI_VDA_DestroyChn(VdaChn);
    if(s32Ret != HI_SUCCESS)
    {
        log_error("Can't destroy VDA chn: 0x%x", s32Ret);
    }
}


int main(int argc, char** argv)
{
    if (argc < 2)
    {
        log_error("./test_jpeg sensor_config.ini");
        return -1;

    }

    signal(SIGINT, action_on_signal);

    struct SensorConfig sc;
    memset(&sc, 0, sizeof(struct SensorConfig));

    int ret = cfg_sensor_read(argv[1], &sc);

    if (ret < 0)
    {
        print_error(ret);
        return ret;
    }

    ret = sdk_init(&sc);
    if (ret < 0)
    {
        log_error("sdk_init() failed at %s: 0x%X", __sdk_last_call, ret);
        return -1;
    }

    ret = sdk_sensor_init(&sc);
    if (ret < 0)
    {
        log_error("sdk_sensor_init() failed: %d", ret);
        sdk_done();
        return ret;
    }

    ret = sdk_isp_init(&sc);
    if (ret < 0)
    {
        log_error("sdk_isp_init() failed: %d", ret);
        sdk_sensor_done();
        sdk_done();
        return ret;
    }

    ret = sdk_vi_init(&sc);
    
    if (!ret)
    {
        sdk_vpss_init(&sc);
log_info("PRE md");
        test_md();
log_info("POST md");
        
        sleep(1);
    }

    sdk_isp_done();
    sdk_sensor_done();
    sdk_done();

    return 0;
}



