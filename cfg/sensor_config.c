#include <cfg/sensor_config.h>
#include <stdio.h>
#include <cfg/common.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#define SECTION_sensor      1
#define SECTION_mode        2
#define SECTION_mipi        3
#define SECTION_lvds        4
#define SECTION_vb_conf     5
#define SECTION_isp_image   10
#define SECTION_vi_dev      20
#define SECTION_vi_chn      21
#define SECTION_vpss_group  30
#define SECTION_vpss_crop   31
#define SECTION_vpss_chn    32
#define SECTION_venc_comm   99
#define SECTION_venc_0      100
#define SECTION_venc_1      101
#define SECTION_bind        1000


static int __cfg_sensor_current_section = 0;
static char __cfg_sensor_error_value[256];

const char* cfg_sensor_read_error_value()
{
    return __cfg_sensor_error_value;
}

#define START_SECTION_BLOCK if (0) {}
#define ADD_SECTION(section) if (strcmp(s, #section) == 0) { __cfg_sensor_current_section = SECTION_##section; return CFG_PROC_OK; }

static int __cfg_sensor_read_section_cb(const char* s)
{
    START_SECTION_BLOCK
    ADD_SECTION(sensor)
    ADD_SECTION(mode)
    ADD_SECTION(mipi)
    ADD_SECTION(lvds)
    ADD_SECTION(vb_conf)
    ADD_SECTION(isp_image)
    ADD_SECTION(vi_dev)
    ADD_SECTION(vi_chn)
    ADD_SECTION(vpss_group)
    ADD_SECTION(vpss_crop)
    ADD_SECTION(vpss_chn)
    ADD_SECTION(venc_comm)
    ADD_SECTION(venc_0)
    ADD_SECTION(venc_1)
    ADD_SECTION(bind)

    snprintf(__cfg_sensor_error_value, 256, "%s", s);
    return CFG_PROC_WRONG_SECTION;
}

struct SensorConfig* __current_sc = 0;
#define KEYVAL_PARAM_COPY_STR(key, dest, MAX) if (strcasecmp(key, k) == 0) { snprintf(dest, MAX, "%s", v); return CFG_PROC_OK; }
#define KEYVAL_PARAM_IGN(key) if (strcasecmp(key, k) == 0) { return CFG_PROC_OK; }

#define KEYVAL_PARAM_UL_dec(key, dest) if (strcasecmp(key, k) == 0) {   \
        char* endptr;                                                   \
        dest = strtoul(v, &endptr, 10);                                 \
        if (!*endptr) return CFG_PROC_OK;                               \
        snprintf(__cfg_sensor_error_value, 256, "%s", v);               \
        return CFG_PROC_VALUE_BAD;                                      \
    }

#define KEYVAL_PARAM_ENUM(key, dest, pvs) if (strcasecmp(key, k) == 0) {        \
        unsigned n = 0;                                                         \
        while (pvs[n]) {                                                        \
            if (strcmp(pvs[n], v) == 0) {                                       \
                dest = n; return CFG_PROC_OK;                                   \
            }                                                                   \
            n++;                                                                \
        }                                                                       \
        char* endptr;                                                           \
        unsigned x = strtoul(v, &endptr, 10);                                   \
        if (!*endptr && (n > x)) {                                             \
            dest = x; return CFG_PROC_OK;                                       \
        }                                                                       \
        snprintf(__cfg_sensor_error_value, 256, "%s", v);                       \
        return CFG_PROC_VALUE_BAD;                                              \
    }

static int __cfg_sensor_read_keyval_cb_sensor(const char* k, const char* v)
{
    KEYVAL_PARAM_COPY_STR("sensor_type", __current_sc->sensor_type, 128);
    KEYVAL_PARAM_COPY_STR("dllfile", __current_sc->dll_file, 256);
    KEYVAL_PARAM_IGN("mode");

    snprintf(__cfg_sensor_error_value, 256, "%s", k);
    return CFG_PROC_KEY_BAD;
}

static int __cfg_sensor_read_keyval_cb_mode(const char* k, const char* v)
{
    KEYVAL_PARAM_IGN("input_mode");
    KEYVAL_PARAM_IGN("dev_attr");

    snprintf(__cfg_sensor_error_value, 256, "%s", k);
    return CFG_PROC_KEY_BAD;
}

const char *cfg_sensor_possible_values_bayer[] = { "BAYER_RGGB", "BAYER_GRBG", "BAYER_GBRG", "BAYER_BGGR", 0 };

static int __cfg_sensor_read_keyval_cb_isp_image(const char* k, const char* v)
{
    KEYVAL_PARAM_UL_dec("isp_x", __current_sc->isp.isp_x);
    KEYVAL_PARAM_UL_dec("isp_y", __current_sc->isp.isp_y);
    KEYVAL_PARAM_UL_dec("isp_w", __current_sc->isp.isp_w);
    KEYVAL_PARAM_UL_dec("isp_h", __current_sc->isp.isp_h);
    KEYVAL_PARAM_UL_dec("isp_framerate", __current_sc->isp.isp_frame_rate);
    KEYVAL_PARAM_ENUM("isp_bayer", __current_sc->isp.isp_bayer, cfg_sensor_possible_values_bayer);

    snprintf(__cfg_sensor_error_value, 256, "%s", k);
    return CFG_PROC_KEY_BAD;
}

#define KEYVAL_CASE(section) case SECTION_##section: { return __cfg_sensor_read_keyval_cb_##section(k, v); }

static int __cfg_sensor_read_keyval_cb(const char* k, const char* v)
{
    switch (__cfg_sensor_current_section)
    {
        KEYVAL_CASE(sensor)
        KEYVAL_CASE(mode)
        KEYVAL_CASE(isp_image)
    }
//    printf("%s=%s\n", k, v);
    return CFG_PROC_OK;
}



int cfg_sensor_read(const char* fname, struct SensorConfig* sc)
{
    __cfg_sensor_current_section = 0;
    __current_sc = sc;

    return cfg_proc_read(fname, __cfg_sensor_read_section_cb, __cfg_sensor_read_keyval_cb);
}


