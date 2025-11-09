#include <string.h>
#include <stdint.h>
#include <utils_getopt.h>
#include <iperf.h>
#include <lwip/ip_addr.h>

#define NL "\r\n"

static void iperf_cmd(int argc, char **argv)
{
    int opt;
    getopt_env_t opt_env;
    int o_c = 0, o_s = 0, o_u = 0, o_a = 0;
    int o_p = IPERF_DEFAULT_PORT, o_l = 0, o_i = IPERF_DEFAULT_INTERVAL, o_t = IPERF_DEFAULT_TIME, o_b = IPERF_DEFAULT_NO_BW_LIMIT, o_S = 0, o_n = 0;
    int o_d = 0;
    int o_P = IPERF_TRAFFIC_TASK_PRIORITY;
    uint32_t dst_addr = 0;

    iperf_cfg_t cfg;

    utils_getopt_init(&opt_env, 0);
    while ((opt = utils_getopt(&opt_env, argc, argv, ":c:sup:l:i:t:b:S:n:P:ad")) != -1) {
        #define ARG_READ(v) v = atoi(opt_env.optarg)
        switch (opt) {
        case 'c':
            ++o_c;
            dst_addr = ipaddr_addr(opt_env.optarg);
            break;
        case 's': ++o_s; break;
        case 'u': ++o_u; break;
        case 'p': ARG_READ(o_p); break;
        case 'l': ARG_READ(o_l); break;
        case 'i': ARG_READ(o_i); break;
        case 't': ARG_READ(o_t); break;
        case 'b': ARG_READ(o_b); break;
        case 'S': ARG_READ(o_S); break;
        case 'n': ARG_READ(o_n); break;
        case 'P': ARG_READ(o_P); break;
        case 'd': ++o_d; break;
        case 'a': ++o_a; break;
        }
        #undef ARG_READ
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.type = IPERF_IP_TYPE_IPV4;

    if (o_a) {
        iperf_stop();
        return;
    }
    if (!((o_c && !o_s) || (!o_c && o_s))) {
        printf("client/server required" NL);
        return;
    }
    if (o_c) {
        cfg.destination_ip4 = dst_addr;
        cfg.flag |= IPERF_FLAG_CLIENT;
    } else {
        cfg.flag |= IPERF_FLAG_SERVER;
    }
    if (o_u) {
        cfg.flag |= IPERF_FLAG_UDP;
    } else {
        cfg.flag |= IPERF_FLAG_TCP;
    }

    if (o_c && !o_u && o_d) {
        cfg.flag |= IPERF_FLAG_DUAL;
    }

    cfg.len_buf = o_l;
    cfg.sport = o_p;
    cfg.dport = o_p;
    cfg.interval = o_i;
    cfg.time = o_t;
    if (cfg.time < cfg.interval) {
        cfg.time = cfg.interval;
    }
    cfg.bw_lim = o_b;
    cfg.tos = o_S;
    cfg.num_bytes = o_n * 1000 * 1000;
    if (cfg.bw_lim <= 0) {
        cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
    }
    cfg.traffic_task_priority = o_P;

    iperf_start(&cfg);
}

#include <shell.h>
#define ML(s) s NL
#define IPERF_USAGE \
    ML("iperf") \
    ML(" -c server_addr: run in client mode") \
    ML(" -s: run in server mode") \
    ML(" -u: UDP") \
    ML(" -p port: specify port") \
    ML(" -l length: set read/write buffer size") \
    ML(" -i interval: seconds between bandwidth reports") \
    ML(" -t time: time in seconds to run") \
    ML(" -b bandwith: bandwidth to send in Mbps") \
    ML(" -S tos: TOS") \
    ML(" -n MB: number of MB to send/recv") \
    ML(" -P priority: traffic task priority") \
    ML(" -d: dual mode") \
    ML(" -a: abort running iperf") \

#if 0
const static struct cli_command iperf_cmds[] STATIC_CLI_CMD_ATTRIBUTE = {
    {"iperf", IPERF_USAGE, iperf_cmd},
};
#endif
CSH_CMD_EXPORT_ALIAS(iperf_cmd, iperf, iperf command);

