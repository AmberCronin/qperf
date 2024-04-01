#pragma once

#include <quicly.h>
#include <stdbool.h>
#include <stdint.h>

int run_client(const char *port, bool gso, const char *logfile, const char *cc, int iw, quicly_ss_type_t *ss, int mw, const char *cmdg, const char *host, int runtime_s, uint64_t max_bytes, bool ttfb_only);
void quit_client();

void on_first_byte();
