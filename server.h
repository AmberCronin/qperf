#pragma once

#include <quicly.h>
#include <stdbool.h>

int run_server(const char* port, bool gso, const char *logfile, const char *cc, int iw, quicly_ss_type_t *ss, int mw, const char *cert, const char *key);

