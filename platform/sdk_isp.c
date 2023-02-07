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

