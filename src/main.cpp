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

MYMODCFGNAME(net.hexedstudio.mobiladorid, Mobilador ID, 1.0.0, HEXED Studio, MobiladorID)
NEEDGAME(com.rockstargames.gtasa)

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

bool patchString(uintptr_t base, uintptr_t length, const char* source, const char* replacement) {
    if (!base || !length || !source || !replacement) return false;

    const size_t sourceLen = std::strlen(source);
    const size_t replacementLen = std::strlen(replacement);
    if (replacementLen > sourceLen) {
        log("[MobiladorID] replacement is longer than source; refusing patch");
        return false;
    }

    // Search the mapped image instead of relying on a fixed RVA.
    for (uintptr_t off = 0; off + sourceLen + 1 <= length; ++off) {
        const uintptr_t addr = base + off;
        if (std::memcmp(reinterpret_cast<const void*>(addr), source, sourceLen) != 0)
            continue;
        if (*reinterpret_cast<const uint8_t*>(addr + sourceLen) != 0)
            continue;

        // Keep the original allocation and terminate the replacement with NULs.
        std::initializer_list<uint8_t> bytes = {
            replacementLen > 0 ? static_cast<uint8_t>(replacement[0]) : 0,
            replacementLen > 1 ? static_cast<uint8_t>(replacement[1]) : 0,
            replacementLen > 2 ? static_cast<uint8_t>(replacement[2]) : 0,
            replacementLen > 3 ? static_cast<uint8_t>(replacement[3]) : 0,
            replacementLen > 4 ? static_cast<uint8_t>(replacement[4]) : 0,
            replacementLen > 5 ? static_cast<uint8_t>(replacement[5]) : 0,
            replacementLen > 6 ? static_cast<uint8_t>(replacement[6]) : 0,
            replacementLen > 7 ? static_cast<uint8_t>(replacement[7]) : 0,
            0
        };

        if (!aml->PatchMemory(addr, bytes)) {
            log("[MobiladorID] PatchMemory failed at +0x%lX", static_cast<unsigned long>(off));
            return false;
        }

        log("[MobiladorID] %s -> %s in image +0x%lX",
            source, replacement, static_cast<unsigned long>(off));
        return true;
    }

    return false;
}

bool tryPatch(const char* reported) {
    for (const char* lib : kLibs) {
        const uintptr_t base = aml->GetLib(lib);
        const uintptr_t length = aml->GetLibLength(lib);
        if (!base || !length) continue;

        if (patchString(base, length, kSource, reported)) {
            log("[MobiladorID] patched %s", lib);
            return true;
        }

        // If the target already contains the desired identifier, treat it as success.
        const size_t len = std::strlen(reported);
        for (uintptr_t off = 0; off + len + 1 <= length; ++off) {
            if (std::memcmp(reinterpret_cast<const void*>(base + off), reported, len) == 0 &&
                *reinterpret_cast<const uint8_t*>(base + off + len) == 0) {
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
