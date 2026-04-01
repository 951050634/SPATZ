// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include <printf.h>

#include <cstdlib>
#include <cstring>
#include <vector>

#include "ipc.hh"
#include "sim.hh"

int main(int argc, char **argv, char **env) {
    bool enable_trace = false;
    std::vector<char *> sim_argv;
    sim_argv.reserve(argc + 1);
    sim_argv.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-t") == 0 || std::strcmp(argv[i], "--trace") == 0) {
            enable_trace = true;
            continue;
        }
        sim_argv.push_back(argv[i]);
    }
    sim_argv.push_back(nullptr);

    int sim_argc = static_cast<int>(sim_argv.size()) - 1;
    if (enable_trace) {
        setenv("SNITCH_TRACE", "1", 1);
    }

    // Write binary path to logs/binary for the `make annotate` target
    FILE *fd;
    fd = fopen("logs/.rtlbinary", "w");
    if (fd != NULL && sim_argc >= 2) {
        fprintf(fd, "%s\n", sim_argv[1]);
        fclose(fd);
    } else {
        fprintf(stderr,
                "Warning: Failed to write binary name to logs/.rtlbinary\n");
    }

    // Initialize IPC bridge if specified
    IpcIface ipc_iface(sim_argc, sim_argv.data());

    auto sim = std::make_unique<sim::Sim>(sim_argc, sim_argv.data());
    return sim->run();
}
