#include "mod/amlmod.h"
#include "mod/logger.h"
#include "mod/config.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <initializer_list>
#include <cstdarg>
#include <vector>
#include <unistd.h>

MYMODCFGNAME(net.hexedstudio.mobiladorid, Mobilador ID, 1.0.0, HEXED Studio, MobiladorID)
NEEDGAME(com.nezukosamp.game)

namespace {

constexpr const char* kLibs[] = {
    "libNezukoSA-MP.so",
    "libsamp.so"
};
constexpr const char* kSource = "0.3.7-R4";
constexpr const char* kDefaultReported = "0.3.7";

std::atomic<bool> g_stop{false};
std::thread g_worker;

void log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logger->Info("%s", buf);
}

struct Range { uintptr_t begin; uintptr_t end; bool writable; };

// Only scan pages that are actually mapped and readable. The span returned by
// GetLibLength() can include unmapped gaps, and reading those would SIGSEGV
// inside the game process.
std::vector<Range> readableRanges(const char* lib) {
    std::vector<Range> out;
    FILE* f = std::fopen("/proc/self/maps", "r");
    if (!f) return out;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        if (!std::strstr(line, lib)) continue;
        unsigned long b = 0, e = 0;
        char perms[8] = {0};
        if (std::sscanf(line, "%lx-%lx %7s", &b, &e, perms) != 3) continue;
        if (perms[0] != 'r') continue;
        out.push_back({static_cast<uintptr_t>(b), static_cast<uintptr_t>(e), perms[1] == 'w'});
    }
    std::fclose(f);
    return out;
}

// Searches one readable range for `needle\0`. Returns the address or 0.
uintptr_t findLiteral(const Range& r, const char* needle) {
    const size_t n = std::strlen(needle);
    if (r.end <= r.begin || r.end - r.begin < n + 1) return 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(r.begin);
    const size_t span = r.end - r.begin;
    for (size_t off = 0; off + n + 1 <= span; ++off) {
        if (p[off] == static_cast<uint8_t>(needle[0]) &&
            std::memcmp(p + off, needle, n) == 0 && p[off + n] == 0)
            return r.begin + off;
    }
    return 0;
}

bool patchString(const std::vector<Range>& ranges, const char* source, const char* replacement) {
    const size_t sourceLen = std::strlen(source);
    const size_t replacementLen = std::strlen(replacement);
    if (replacementLen > sourceLen) {
        log("[MobiladorID] replacement is longer than source; refusing patch");
        return false;
    }

    for (const Range& r : ranges) {
        const uintptr_t addr = findLiteral(r, source);
        if (!addr) continue;

        // Copy the replacement and pad with NULs over the whole original literal.
        uint8_t bytes[64] = {0};
        if (sourceLen + 1 > sizeof(bytes)) return false;
        std::memcpy(bytes, replacement, replacementLen);

        aml->Unprot(addr, sourceLen + 1);
        aml->Write(addr, reinterpret_cast<uintptr_t>(bytes), sourceLen + 1);

        log("[MobiladorID] %s -> %s at %p", source, replacement, reinterpret_cast<void*>(addr));
        return true;
    }
    return false;
}

bool tryPatch(const char* reported) {
    for (const char* lib : kLibs) {
        if (!aml->GetLib(lib)) continue;              // not loaded yet
        const std::vector<Range> ranges = readableRanges(lib);
        if (ranges.empty()) continue;

        if (patchString(ranges, kSource, reported)) {
            log("[MobiladorID] patched %s", lib);
            return true;
        }
        for (const Range& r : ranges) {
            if (findLiteral(r, reported)) {
                log("[MobiladorID] %s already contains %s", lib, reported);
                return true;
            }
        }
    }
    return false;
}

void worker() {
    const bool enabled = cfg->GetBool("Enabled", true, "Settings");
    if (!enabled) {
        log("[MobiladorID] disabled in config");
        return;
    }

    const char* reported = cfg->GetString("ReportedVersion", kDefaultReported, "Settings");
    if (!reported || !*reported) reported = kDefaultReported;

    // Wait for the target networking library to appear.
    for (int i = 0; i < 200 && !g_stop; ++i) {
        if (tryPatch(reported)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    log("[MobiladorID] target version literal was not found; no networking/input hooks installed");
}

} // namespace

ON_MOD_LOAD() {
    g_stop = false;
    g_worker = std::thread(worker);
}

ON_MOD_UNLOAD() {
    g_stop = true;
    if (g_worker.joinable()) g_worker.join();
}
