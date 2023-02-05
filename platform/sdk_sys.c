#include "sdk.h"

#include <stdint.h>
#include <string.h>

#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "mpi_sys.h"
#include "mpi_vb.h"
#include "hi_comm_ao.h"
#include "mpi_ao.h"

#define SYS_ALIGN_WIDTH      64

const char* __sdk_last_call = 0;

static int platform_v1_mpi_sys_init(VB_CONF_S* pstVbConf)
{
    HI_S32 s32Ret = HI_FAILURE;

    MPP_SYS_CONF_S stSysConf;

    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
    __sdk_last_call = "HI_MPI_VB_SetConf()";
    s32Ret = HI_MPI_VB_SetConf(pstVbConf);
    if (HI_SUCCESS != s32Ret) return s32Ret;

    __sdk_last_call = "HI_MPI_VB_Init()";
    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret) return s32Ret;

    __sdk_last_call = "HI_MPI_SYS_SetConf()";
    stSysConf.u32AlignWidth = SYS_ALIGN_WIDTH;

    s32Ret = HI_MPI_SYS_SetConf(&stSysConf);
    if (HI_SUCCESS != s32Ret) return s32Ret;

    __sdk_last_call = "HI_MPI_SYS_Init()";
    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret) return s32Ret;
}

static HI_S32 SYS_GetPicSize(VIDEO_NORM_E enNorm, PIC_SIZE_E enPicSize, SIZE_S *pstSize)
{
    switch (enPicSize)
    {
        case PIC_QCIF:
            pstSize->u32Width = 176;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?144:120;
            break;
        case PIC_CIF:
            pstSize->u32Width = 352;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?288:240;
            break;
        case PIC_D1:
            pstSize->u32Width = 720;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case PIC_960H:
            pstSize->u32Width = 960;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case PIC_2CIF:
            pstSize->u32Width = 360;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case PIC_QVGA:    /* 320 * 240 */
            pstSize->u32Width = 320;
            pstSize->u32Height = 240;
            break;
        case PIC_VGA:     /* 640 * 480 */
            pstSize->u32Width = 640;
            pstSize->u32Height = 480;
            break;
        case PIC_XGA:     /* 1024 * 768 */
            pstSize->u32Width = 1024;
            pstSize->u32Height = 768;
            break;
        case PIC_SXGA:    /* 1400 * 1050 */
            pstSize->u32Width = 1400;
            pstSize->u32Height = 1050;
            break;
        case PIC_UXGA:    /* 1600 * 1200 */
            pstSize->u32Width = 1600;
            pstSize->u32Height = 1200;
            break;
        case PIC_QXGA:    /* 2048 * 1536 */
            pstSize->u32Width = 2048;
            pstSize->u32Height = 1536;
            break;
        case PIC_WVGA:    /* 854 * 480 */
            pstSize->u32Width = 854;
            pstSize->u32Height = 480;
            break;
        case PIC_WSXGA:   /* 1680 * 1050 */
            pstSize->u32Width = 1680;
            pstSize->u32Height = 1050;
            break;
        case PIC_WUXGA:   /* 1920 * 1200 */
            pstSize->u32Width = 1920;
            pstSize->u32Height = 1200;
            break;
        case PIC_WQXGA:   /* 2560 * 1600 */
            pstSize->u32Width = 2560;
            pstSize->u32Height = 1600;
            break;
        case PIC_HD720:   /* 1280 * 720 */
            pstSize->u32Width = 1280;
            pstSize->u32Height = 720;
            break;
        case PIC_HD1080:  /* 1920 * 1080 */
            pstSize->u32Width = 1920;
            pstSize->u32Height = 1080;
            break;
        default:
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}


#define VB_HEADER_STRIDE    16

#define VB_PIC_HEADER_SIZE(Width, Height, Type, size)\
    do{\
        if (PIXEL_FORMAT_YUV_SEMIPLANAR_422 == Type || PIXEL_FORMAT_RGB_BAYER == Type )\
        {\
            size = VB_HEADER_STRIDE * (Height) * 2;\
        }\
        else if(PIXEL_FORMAT_YUV_SEMIPLANAR_420 == Type)\
        {\
            size = (VB_HEADER_STRIDE * (Height) * 3) >> 1;\
        }\
    }while(0)

static HI_U32 CalcPicVbBlkSize(VIDEO_NORM_E enNorm, PIC_SIZE_E enPicSize, PIXEL_FORMAT_E enPixFmt, HI_U32 u32AlignWidth)
{
    HI_S32 s32Ret = HI_FAILURE;
    SIZE_S stSize;

    s32Ret = SYS_GetPicSize(enNorm, enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        return 0;
    }

    HI_U32 u32VbSize = (CEILING_2_POWER(stSize.u32Width, u32AlignWidth) * CEILING_2_POWER(stSize.u32Height, u32AlignWidth) * ((PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixFmt) ? 2 : 1.5));

    HI_U32 u32HeaderSize;
    VB_PIC_HEADER_SIZE(stSize.u32width, stSize.u32Height, enPixFmt, u32HeaderSize);
    u32VbSize += u32HeaderSize;
    return u32VbSize;
}

int sdk_init(const struct SensorConfig* sc)
{
    VB_CONF_S stVbConf;
    memset(&stVbConf, 0, sizeof(VB_CONF_S));

    if (sc)
    {
        stVbConf.u32MaxPoolCnt = 128;

        //unsigned i;
        //for (i = 0; i < vi_cnt && i < VB_MAX_COMM_POOLS; ++i)
        //{
        //    // TODO: move VIDEO_ENCODING_MODE_NTSC, PIXEL_FORMAT_YUV_PLANAR_422 to config
        //    __sdk_last_call = "CalcPicVbBlkSize";
        //    uint32_t u32BlkSize = CalcPicVbBlkSize(VIDEO_ENCODING_MODE_NTSC, vi_pic_szs[i], PIXEL_FORMAT_YUV_SEMIPLANAR_422, SYS_ALIGN_WIDTH);
        //    stVbConf.astCommPool[i].u32BlkSize = u32BlkSize;
        //    stVbConf.astCommPool[i].u32BlkCnt = 6;
        //    printf("u32BlkSize %u\n", u32BlkSize);
        //}
    }

    int s32Ret = platform_v1_mpi_sys_init(&stVbConf);

    return s32Ret;
}

void sdk_done()
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
}

/* ugly sdk hack: start */
#define MAP_FAILED ((void *)-1)

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *__addr, size_t __len);

void *mmap(void *start, size_t len, int prot, int flags, int fd, uint32_t off) {
    return mmap64(start, len, prot, flags, fd, off);
}
/* ugly sdk hack: end */

