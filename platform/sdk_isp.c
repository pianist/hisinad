#include "sdk.h"
#include <aux/logger.h>
#include "mpi_sys.h"
#include "mpi_isp.h"
#include "mpi_vb.h"
#include "mpi_ae.h"
#include "mpi_af.h"
#include "mpi_vi.h"
#include "mpi_awb.h"
#include "mpi_sys.h"
#include "mpi_vpss.h"
#include <pthread.h>
#include <unistd.h>

HI_S32 HI_MPI_SYS_GetChipId(HI_U32 *pu32ChipId);

static pthread_t th_isp_runner = 0;

void* isp_thread_proc(void *p)
{
    HI_S32 s32Ret = HI_MPI_ISP_Run();

    if (HI_SUCCESS != s32Ret)
    {
        log_crit("HI_MPI_ISP_Run failed with %#x!", s32Ret);
    }
    log_info("Shutdown isp_run thread\n");

    return 0;
}

int sdk_isp_init(const struct SensorConfig* sc)
{
    int s32Ret;

    ALG_LIB_S alg_lib;
    strcpy(alg_lib.acLibName, "hisi_ae_lib");
    s32Ret = HI_MPI_AE_Register(&alg_lib);
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_AE_Register/hisi_ae_lib failed with %#x!", s32Ret);
        return -1;
    }

    strcpy(alg_lib.acLibName, "hisi_af_lib");
    s32Ret = HI_MPI_AF_Register(&alg_lib);
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_AF_Register/hisi_af_lib failed with %#x!", s32Ret);
        return -1;
    }

    strcpy(alg_lib.acLibName, "hisi_awb_lib");
    s32Ret = HI_MPI_AWB_Register(&alg_lib);
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_AWB_Register/hisi_awb_lib failed with %#x!", s32Ret);
        return -1;
    }

    HI_U32 chipId;
    s32Ret = HI_MPI_SYS_GetChipId(&chipId);
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_SYS_GetChipId failed with %#x!", s32Ret);
        return -1;
    }
    log_info("HI_MPI_SYS_GetChipId: %#X", chipId);

    s32Ret = HI_MPI_ISP_Init();
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_ISP_Init failed with %#x!", s32Ret);
        return -1;
    }

    ISP_IMAGE_ATTR_S img_attr;
    img_attr.u16Width = sc->isp.isp_w;
    img_attr.u16Height = sc->isp.isp_h;
    img_attr.u16FrameRate = sc->isp.isp_frame_rate;
    img_attr.enBayer = sc->isp.isp_bayer;
    s32Ret = HI_MPI_ISP_SetImageAttr(&img_attr);
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_ISP_SetImageAttr failed with %#x!", s32Ret);
        return -1;
    }

    ISP_INPUT_TIMING_S stInputTiming;
    stInputTiming.u16HorWndStart = sc->isp.isp_x;
    stInputTiming.u16HorWndLength = sc->isp.isp_w;
    stInputTiming.u16VerWndStart = sc->isp.isp_y;
    stInputTiming.u16VerWndLength = sc->isp.isp_h;
    stInputTiming.enWndMode = ISP_WIND_ALL;
    s32Ret = HI_MPI_ISP_SetInputTiming(&stInputTiming);
    if (HI_SUCCESS != s32Ret) {
        log_crit("HI_MPI_ISP_SetInputTiming failed with %#x!", s32Ret);
        return -1;
    }

    pthread_create(&th_isp_runner, 0, isp_thread_proc, NULL);

    sleep(1);

    return 0;
}

void sdk_isp_done()
{
    HI_MPI_ISP_Exit();

    log_info("Join ISP thread if running...");
    if (th_isp_runner)
    {
        pthread_join(th_isp_runner, 0);
        th_isp_runner = 0;
    }

    log_info("ISP done: OK");
}

int sdk_vi_init(const struct SensorConfig* sc)
{
    VI_DEV_ATTR_EX_S vi_dev_attr;
    memset(&vi_dev_attr, 0, sizeof(VI_DEV_ATTR_EX_S));
    vi_dev_attr.enInputMode = sc->videv.input_mod;
    vi_dev_attr.au32CompMask[0] = sc->videv.mask_0;
    vi_dev_attr.au32CompMask[1] = sc->videv.mask_1;
    vi_dev_attr.enScanMode = sc->videv.scan_mode;
    vi_dev_attr.s32AdChnId[0] = -1;
    vi_dev_attr.s32AdChnId[1] = -1;
    vi_dev_attr.s32AdChnId[2] = -1;
    vi_dev_attr.s32AdChnId[3] = -1;
    vi_dev_attr.enDataSeq = sc->videv.data_seq;

    vi_dev_attr.stSynCfg.enVsync = sc->videv.vsync;
    vi_dev_attr.stSynCfg.enVsyncNeg = sc->videv.vsync_neg;
    vi_dev_attr.stSynCfg.enHsync = sc->videv.hsync;
    vi_dev_attr.stSynCfg.enHsyncNeg = sc->videv.hsync_neg;
    vi_dev_attr.stSynCfg.enVsyncValid = sc->videv.vsync_valid;
    vi_dev_attr.stSynCfg.enVsyncValidNeg = sc->videv.vsync_valid_neg;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHfb = sc->videv.timing_blank_hsync_hfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncAct = sc->videv.timing_blank_hsync_act;
    vi_dev_attr.stSynCfg.stTimingBlank.u32HsyncHbb = sc->videv.timing_blank_hsync_hbb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVfb = sc->videv.timing_blank_vsync_vfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVact = sc->videv.timing_blank_vsync_vact;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbb = sc->videv.timing_blank_vsync_vbb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbfb = sc->videv.timing_blank_vsync_vbfb;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbact = sc->videv.timing_blank_vsync_vbact;
    vi_dev_attr.stSynCfg.stTimingBlank.u32VsyncVbbb = sc->videv.timing_blank_vsync_vbbb;
    vi_dev_attr.enDataPath = sc->videv.data_path;
    vi_dev_attr.enInputDataType = sc->videv.input_data_type;
    vi_dev_attr.bDataRev = sc->videv.data_rev;

    int s32Ret = HI_MPI_VI_SetDevAttrEx(0, &vi_dev_attr);
    if (HI_SUCCESS != s32Ret)
    {
        log_error("HI_MPI_VI_SetDevAttrEx failed with %#x!", s32Ret);
        return -1;
    }

    s32Ret = HI_MPI_VI_EnableDev(0);
    if (HI_SUCCESS != s32Ret) {
        log_error("HI_MPI_VI_EnableDev failed with %#x!", s32Ret);
        return -1;
    }

    VI_CHN_ATTR_S chn_attr;
    memset(&chn_attr, 0, sizeof(VI_CHN_ATTR_S));
    chn_attr.stCapRect.s32X = sc->vichn.cap_rect_x;
    chn_attr.stCapRect.s32Y = sc->vichn.cap_rect_y;
    chn_attr.stCapRect.u32Width = sc->vichn.cap_rect_width;
    chn_attr.stCapRect.u32Height = sc->vichn.cap_rect_height;
    chn_attr.stDestSize.u32Width = sc->vichn.dest_size_width;
    chn_attr.stDestSize.u32Height = sc->vichn.dest_size_height;
    chn_attr.enCapSel = sc->vichn.cap_sel;
    chn_attr.enPixFormat = sc->vichn.pix_format;
    chn_attr.bMirror = HI_FALSE;
    chn_attr.bFlip = HI_FALSE;
    chn_attr.s32SrcFrameRate = 30;
    chn_attr.bChromaResample = HI_FALSE;
    chn_attr.s32FrameRate = 30;
    s32Ret = HI_MPI_VI_SetChnAttr(0, &chn_attr);
    if (HI_SUCCESS != s32Ret) {
        log_error("HI_MPI_VI_SetChnAttr failed with %#x!", s32Ret);
        return -1;
    }

    s32Ret = HI_MPI_VI_EnableChn(0);
    if (HI_SUCCESS != s32Ret) {
        log_error("HI_MPI_VI_EnableChn failed with %#x!", s32Ret);
        return -1;
    }

    return 0;
}

int sdk_vpss_init(const struct SensorConfig* sc)
{
    VPSS_GRP_ATTR_S vpss_grp_attr;
    memset(&vpss_grp_attr, 0 ,sizeof(VPSS_GRP_ATTR_S));
    vpss_grp_attr.u32MaxW = sc->vichn.dest_size_width;
    vpss_grp_attr.u32MaxH = sc->vichn.dest_size_height;
    vpss_grp_attr.enPixFmt = sc->vichn.pix_format;
    vpss_grp_attr.bIeEn = HI_FALSE;
    vpss_grp_attr.bDrEn = HI_FALSE;
    vpss_grp_attr.bDbEn = HI_FALSE;
    vpss_grp_attr.bHistEn = HI_FALSE;
    vpss_grp_attr.enDieMode = VPSS_DIE_MODE_NODIE;
    vpss_grp_attr.bNrEn = HI_TRUE;

    int s32Ret = HI_MPI_VPSS_CreateGrp(0, &vpss_grp_attr);
    if (HI_SUCCESS != s32Ret)
    {
        log_error("HI_MPI_VPSS_CreateGrp failed with %#x!", s32Ret);
        return -1;
    }

}

void sdk_vi_done()
{
    
}


