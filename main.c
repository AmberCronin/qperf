#include <getopt.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <quicly.h>
#include <quicly/defaults.h>

// #include "quicly/ss.h"
#include "server.h"
#include "client.h"


static void usage(const char *cmd)
{
    printf("Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -c target                run as client and connect to target server\n"
            "  --cc [reno,cubic]        congestion control algorithm to use (default reno)\n"
            "  --ss [rfc2001,search10,search10interp,search10delv,hybla]\n"
            "                           slow start algorithm to use (default rfc2001)\n"
            "  --cmdg [none,hybla]      client-side max data growth scaling (default none)\n"
            "  -e                       measure time for connection establishment and first byte only\n"
            "  -g                       enable UDP generic segmentation offload\n"
            "  --iw initial-window      initial window to use (default 10)\n"
            "  --mw max-window          maximum window to use in MB (default 16)\n"
            "  -l log-file              file to log tls secrets\n"
            "  -p                       port to listen on/connect to (default 18080)\n"
            "  -s                       run as server\n"
            "  -t time (s)              run for X seconds (default 10s)\n"
            "  -n byte-count[kMG]       request N bytes from the server\n"
            "  --searchexit [0,1]       enable SEARCH to exit slow-start before loss\n"
            "  -h                       print this help\n"
            "\n",
           cmd);
}

static struct option long_options[] = 
{
    {"cc", required_argument, NULL, 0},
    {"iw", required_argument, NULL, 1},
    {"ss", required_argument, NULL, 2},
    {"mw", required_argument, NULL, 3},
    {"cmdg", required_argument, NULL, 4},
    {"searchexit", required_argument, NULL, 5},
    {NULL, 0, NULL, 0}
};

extern bool search_exit;

int main(int argc, char** argv)
{
    int port = 18080;
    bool server_mode = false;
    const char *host = NULL;
    int runtime_s = 10;
    uint64_t max_bytes = 0;
    char scale = 0;
    int ch;
    bool ttfb_only = false;
    bool gso = false;
    const char *logfile = NULL;
    const char *cc = "reno";
    const char *cmdg = "none";
    int iw = 10;
    quicly_ss_type_t* ss = &quicly_default_ss;
    int mw = 16; // in megabytes

    while ((ch = getopt_long(argc, argv, "c:egl:p:st:n:h", long_options, NULL)) != -1) {
        switch (ch) {
        case 0:
            if(strcmp(optarg, "reno") != 0 && strcmp(optarg, "cubic") != 0) {
                fprintf(stderr, "invalid argument passed to --cc\n");
                exit(1);
            }
            cc = optarg;
            break;
        case 1:
            iw = (intptr_t)optarg;
            if (sscanf(optarg, "%" SCNu32, &iw) != 1) {
                fprintf(stderr, "invalid argument passed to --iw\n");
                exit(1);
            }
            break;
        case 2: {
            quicly_ss_type_t **ss_iter;
            for (ss_iter = quicly_ss_all_types; *ss_iter != NULL; ++ss_iter) {
                printf("checking %s\n",(*ss_iter)->name);
                if (strcmp((*ss_iter)->name, optarg) == 0)
                    break;
            }
            if (*ss_iter != NULL) {
                ss = (*ss_iter);
            } else {
                fprintf(stderr, "unknown slowstart algorithm: %s\n", optarg);
                exit(1);
            }
        } break;
        case 3:
            mw = (intptr_t)optarg;
            if (sscanf(optarg, "%" SCNu32, &mw) != 1) {
                fprintf(stderr, "invalid argument passed to --mw\n");
                exit(1);
            }
            break;
        case 4:
            if(strcmp(optarg, "none") != 0 && strcmp(optarg, "hybla") != 0) {
                fprintf(stderr, "invalid argument passed to --cmdg\n");
                exit(1);
            }
            cmdg = optarg;
            break;
        case 5:
            if (strcmp(optarg, "0") == 0) {
                search_exit = false;
            }
            else if (strcmp(optarg, "1") == 0) {
                search_exit = true;
            }
            else {
                fprintf(stderr, "invalid argument passed to --searchexit\n");
                exit(1);
            }
            break;
        case 'c':
            host = optarg;
            break;
        case 'e':
            ttfb_only = true;
            break;
        case 'g':
            #ifdef __linux__
                gso = true;
                printf("using UDP GSO, requires kernel >= 4.18\n");
            #else
                fprintf(stderr, "UDP GSO only supported on linux\n");
                exit(1);
            #endif
            break;
        case 'l':
            logfile = optarg;
            break;
        case 'p':
            port = (intptr_t)optarg;
            if(sscanf(optarg, "%u", &port) < 0 || port > 65535) {
                fprintf(stderr, "invalid argument passed to -p\n");
                exit(1);
            }
            break;
        case 's':
            server_mode = true;
            break;
        case 't':
            if(sscanf(optarg, "%u", &runtime_s) != 1 || runtime_s < 1) {
                fprintf(stderr, "invalid argument passed to -t\n");
                exit(1);
            }
            break;
        case 'n': {
                uint32_t _max_bytes;
                if(sscanf(optarg, "%u%c", &_max_bytes, &scale) < 1) {
                    fprintf(stderr, "invalid argument passed to -n: improper size format\n");
                    exit(1);
                }
                if (scale == 0) {
                    max_bytes = _max_bytes;
                }
                else if (scale == 'k') {
                    max_bytes = ((uint64_t)_max_bytes) * 1000;
                }
                else if (scale == 'M') {
                    max_bytes = ((uint64_t)_max_bytes) * 1000000;
                }
                else if (scale == 'G') {
                    max_bytes = ((uint64_t)_max_bytes) * 1000000000;
                }
                else {
                    fprintf(stderr, "invalid argument passed to -n: size multiplier must be one of [kMG]\n");
                    exit(1);
                }
                printf("setting data count to %lu\n", max_bytes);
            }
            break;
        default:
            usage(argv[0]);
            exit(1);
        }
    }

    if(server_mode && host != NULL) {
        printf("cannot use -c in server mode\n");
        exit(1);
    }

    if(!server_mode && host == NULL) {
        usage(argv[0]);
        exit(1);
    }

    char port_char[16];
    sprintf(port_char, "%d", port);
    return server_mode ?
                run_server(port_char, gso, logfile, cc, iw, ss, mw, "server.crt", "server.key") :
                run_client(port_char, gso, logfile, cc, iw, ss, mw, cmdg, host, runtime_s, max_bytes, ttfb_only);
}