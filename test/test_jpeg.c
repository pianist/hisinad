#include <platform/sdk.h>
#include <errno.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <aux/logger.h>
#include <cfg/sensor_config.h>

int stop_flag = 0;

void action_on_signal(int signum)
{
    log_info("STOP");
    stop_flag = 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        log_error("./test_ao sensor_config.ini");
        return -1;

    }

    signal(SIGINT, action_on_signal);

    struct SensorConfig sc;
    memset(&sc, 0, sizeof(struct SensorConfig));

    int ret = cfg_sensor_read(argv[1], &sc);

    if (ret < 0)
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
    if (!ret)
    {
        log_info("Now sleep for 100 s...");
        sleep(10);
        sleep(10);
        sleep(10);

    // TODO: vi, vpss, venc...

    }
    else
    {
        log_error("sdk_isp_init() failed: %d", ret);
    }

    sdk_isp_done();
    sdk_sensor_done();
    sdk_done();

    return 0;
}




