/**
 * @file dllmain.cpp
 * @brief 007 First Light - Ultrawide Cutscene Patch (ASI).
 *
 * Reproduces the community hex edit at runtime instead of in the file on disk.
 *
 * The Glacier engine stores the cutscene aspect ratio as a raw 32-bit float.
 * For 16:9 that float is @c 1.77778f, which is the little-endian byte sequence
 * @c 39 @c 8E @c E3 @c 3F.
 *
 * On load this ASI spins up a worker thread that scans the main executable's
 * in-memory image and overwrites every occurrence of that constant with the
 * user's target ratio. Because the patch is applied to memory on every launch,
 * the fix re-applies automatically and should survive most game updates.
 *
 * Configuration is read from @c 007FirstLightUltrawide.ini placed next to this
 * file; set a monitor resolution (@c Width / @c Height) or an explicit
 * @c AspectRatio. Activity is logged to @c 007FirstLightUltrawide.log alongside.
 *
 * @note An ASI is an ordinary DLL renamed to @c .asi and loaded by an ASI
 *       loader (e.g. Ultimate-ASI-Loader). Entry point is @ref DllMain.
 */

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

/// Build version, injected by the build system (CI passes the computed semver
/// tag via @c -DPROJECT_VERSION). Falls back to "dev" for local/ad-hoc builds.
#ifndef PATCH_VERSION
#define PATCH_VERSION "dev"
#endif

/// The game's default 16:9 aspect ratio (@c 1.77778f) as it appears in memory,
/// little-endian. This is the byte pattern the scanner searches for by default.
static constexpr uint8_t kNeedle[4] = {0x39, 0x8E, 0xE3, 0x3F};

/// Handle to this loaded module, captured in @ref DllMain. Used to resolve the
/// directory the .asi lives in so the ini and log sit beside it.
static HMODULE g_self = nullptr;
/// Absolute path to the configuration ini, built by @ref InitPaths.
static char g_iniPath[MAX_PATH] = {0};
/// Absolute path to the activity log, built by @ref InitPaths.
static char g_logPath[MAX_PATH] = {0};

/**
 * @brief Portable file open that compiles cleanly on both toolchains.
 *
 * MSVC deprecates the CRT @c fopen (warning C4996) in favour of the
 * bounds-checked @c fopen_s; MinGW/GCC has no such variant, so we fall back to
 * plain @c fopen there.
 *
 * @param path  Path to the file to open.
 * @param mode  Standard C @c fopen mode string (e.g. @c "a", @c "w").
 * @return The opened stream, or @c nullptr on failure.
 */
static FILE* OpenFile(const char* path, const char* mode) {
#ifdef _MSC_VER
    FILE* f = nullptr;
    if (fopen_s(&f, path, mode) != 0) return nullptr;
    return f;
#else
    return fopen(path, mode);
#endif
}

/**
 * @brief Parse a @c float from a C string.
 *
 * Uses @c strtod rather than @c atof: it is well-defined on malformed input
 * (yielding 0 instead of undefined behaviour) and lets the static analyzer
 * verify conversion-error handling.
 *
 * @param s  Null-terminated string to parse.
 * @return The parsed value, or @c 0.0f if @p s is not a number.
 */
static float ParseFloat(const char* s) {
    return static_cast<float>(strtod(s, nullptr));
}

/**
 * @brief Append a formatted line to the log file (@ref g_logPath).
 *
 * Opens, writes, and closes the file per call so the log stays consistent even
 * if the host process is killed mid-run. A trailing newline is added
 * automatically. Silently does nothing if the log cannot be opened.
 *
 * @param format @c printf-style format string.
 * @param ...    Arguments matching @p format.
 */
static void Log(const char* format, ...) {
    FILE* f = OpenFile(g_logPath, "a");
    if (!f) return;
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

/**
 * @brief Populate @ref g_iniPath and @ref g_logPath.
 *
 * Resolves the directory containing this module (via @ref g_self) and places
 * the ini and log files alongside it, so configuration travels with the .asi
 * regardless of the host's working directory. Falls back to the current
 * directory if the module path has no directory component.
 *
 * @pre @ref g_self has been set (done in @ref DllMain).
 */
static void InitPaths() {
    char modPath[MAX_PATH] = {0};
    GetModuleFileName(g_self, modPath, MAX_PATH);
    const std::string p(modPath);
    const size_t slash = p.find_last_of("\\/");
    const std::string dir = (slash == std::string::npos) ? std::string(".") : p.substr(0, slash);
    snprintf(g_iniPath, MAX_PATH, "%s\\007FirstLightUltrawide.ini", dir.c_str());
    snprintf(g_logPath, MAX_PATH, "%s\\007FirstLightUltrawide.log", dir.c_str());
}

/**
 * @brief Read the master on/off switch from @c [General] @c Enabled.
 *
 * Accepts a range of truthy/falsy spellings: @c 1 / @c 0, @c true / @c false,
 * @c yes / @c no, @c on / @c off (case-insensitive, leading whitespace
 * tolerated).
 *
 * @return @c true if the patch is enabled or the key is missing/unrecognised
 *         (fail-open default); @c false only on an explicit falsy value.
 */
static bool IsPatchEnabled() {
    char buf[32] = {0};
    GetPrivateProfileStringA("General", "Enabled", "1", buf, sizeof(buf), g_iniPath);
    const char* s = buf;
    while (*s == ' ' || *s == '\t') s++; // trim leading whitespace
    if (s[0] == '1') return true;
    if (s[0] == '0') return false;
    if (_stricmp(s, "true") == 0 || _stricmp(s, "yes") == 0 || _stricmp(s, "on") == 0) return true;
    if (_stricmp(s, "false") == 0 || _stricmp(s, "no") == 0 || _stricmp(s, "off") == 0)
        return false;
    return true;
}

/**
 * @brief Determine the target aspect ratio to write into the game.
 *
 * Resolved by priority:
 *   1. @c [Aspect] @c AspectRatio if it is greater than zero (explicit value).
 *   2. Otherwise @c [Aspect] @c Width / @c Height, computed as a float.
 *   3. Falling back to @c 3440x1440 (≈21:9) when keys are absent or invalid.
 *
 * The chosen value and its source are written to the log.
 *
 * @return The target aspect ratio as a 32-bit float.
 */
static float ComputeAspectRatio() {
    char buf[64] = {0};
    GetPrivateProfileStringA("Aspect", "AspectRatio", "0", buf, sizeof(buf), g_iniPath);
    if (const float explicitAr = ParseFloat(buf); explicitAr > 0.01f) {
        Log("Using explicit AspectRatio from ini: %.6f", explicitAr);
        return explicitAr;
    }
    int width = static_cast<int>(GetPrivateProfileIntA("Aspect", "Width", 3440, g_iniPath));
    int height = static_cast<int>(GetPrivateProfileIntA("Aspect", "Height", 3440, g_iniPath));
    if (width <= 0) width = 3440;
    if (height <= 0) height = 1440;
    const float ar = static_cast<float>(width) / static_cast<float>(height);
    Log("Using resolution from ini: %dx%d -> aspect %.6f", width, height, ar);
    return ar;
}

/**
 * @brief Fetch the load address and image size of the main executable.
 *
 * Reads the process's primary module (@c GetModuleHandle(NULL)) and walks its
 * PE headers to obtain @c SizeOfImage, validating the DOS and NT signatures
 * along the way.
 *
 * @param[out] base  Receives the module's base address on success.
 * @param[out] size  Receives the module's @c SizeOfImage in bytes on success.
 * @return @c true on success; @c false if the module or its PE headers are
 *         invalid (outputs are left untouched).
 */
static bool GetMainModuleRange(uint8_t** base, size_t* size) {
    HMODULE hModule = GetModuleHandleA(nullptr);
    if (!hModule) return false;
    auto* b = reinterpret_cast<uint8_t*>(hModule);
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(b);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(b + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    *base = b;
    *size = nt->OptionalHeader.SizeOfImage;
    return true;
}

/**
 * @brief Test whether a memory region is safe to read during the scan.
 *
 * A region qualifies only if it is committed, not guarded or no-access, and
 * carries one of the readable protection flags. This keeps @ref PatchAll from
 * faulting on reserved, freed, or guard pages.
 *
 * @param memInfo  Region descriptor from @c VirtualQuery.
 * @return @c true if the region's bytes can be safely dereferenced.
 */
static bool RegionReadable(const MEMORY_BASIC_INFORMATION& memInfo) {
    if (memInfo.State != MEM_COMMIT) return false;
    if (memInfo.Protect & PAGE_GUARD) return false;
    if (memInfo.Protect & PAGE_NOACCESS) return false;
    constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                               PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (memInfo.Protect & readable) != 0;
}

/**
 * @brief Scan the main module and overwrite every copy of the aspect constant.
 *
 * Walks the executable's image region by region (using @c VirtualQuery and
 * @ref RegionReadable to skip unreadable pages), and wherever the 4-byte
 * @p needle pattern is found, temporarily makes the page writable, replaces the
 * bytes with @p newAspectRatio, restores the original protection, and flushes
 * the instruction cache. Every match and any failure are logged.
 *
 * @param needle          The 4-byte little-endian float pattern to search for.
 * @param newAspectRatio  The replacement aspect ratio written over each match.
 * @return The number of occurrences patched, or @c -1 if the module range
 *         could not be read.
 */
static int PatchAll(const uint8_t needle[4], const float newAspectRatio) {
    uint8_t* base = nullptr;
    size_t size = 0;
    if (!GetMainModuleRange(&base, &size)) {
        Log("ERROR: Failed to read main module PE headers!");
        return -1;
    }

    uint8_t newBytes[4];
    memcpy(newBytes, &newAspectRatio, sizeof(newBytes));

    Log("Main module: base=%p  SizeOfImage=0x%zx", static_cast<void*>(base), size);
    Log("Search  bytes: %02X %02X %02X %02X", needle[0], needle[1], needle[2], needle[3]);
    Log("Replace bytes: %02X %02X %02X %02X (%.6f)", newBytes[0], newBytes[1], newBytes[2],
        newBytes[3], newAspectRatio);

    int count = 0;
    uint8_t* end = base + size;
    uint8_t* cursor = base;
    MEMORY_BASIC_INFORMATION memInfo;

    while (cursor < end) {
        if (VirtualQuery(cursor, &memInfo, sizeof(memInfo)) == 0) break;
        auto* regionStart = static_cast<uint8_t*>(memInfo.BaseAddress);
        uint8_t* regionEnd = regionStart + memInfo.RegionSize;
        if (regionEnd > end) regionEnd = end;

        if (RegionReadable(memInfo)) {
            uint8_t* p = (cursor > regionStart) ? cursor : regionStart;
            for (; p + 4 <= regionEnd; ++p) {
                if (p[0] == kNeedle[0] && p[1] == kNeedle[1] && p[2] == kNeedle[2] &&
                    p[3] == kNeedle[3]) {
                    DWORD oldProt = 0;
                    if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt)) {
                        memcpy(p, newBytes, 4);
                        VirtualProtect(p, 4, oldProt, &oldProt);
                        FlushInstructionCache(GetCurrentProcess(), p, 4);
                        count++;
                        Log("Patched occurrence #%d at %p", count, static_cast<void*>(p));
                    } else {
                        Log("VirtualProtect failed at %p (err %lu)", static_cast<void*>(p),
                            GetLastError());
                    }
                }
            }
        }
        cursor = regionEnd;
    }

    Log("Finished. Occurrences patched: %d", count);
    if (count == 0) {
        Log("NOTE: 0 matches. The constant may have changed in a game update, "
            "or the .asi loaded too early. See the README troubleshooting section.");
    }
    return count;
}

/**
 * @brief Worker thread that performs the patch; the body of the ASI.
 *
 * Initialises paths, resets the log, honours the @c [General] @c Enabled switch,
 * selects the search pattern (built-in 16:9 or a @c [Advanced] @c SearchAspect
 * override), computes the target ratio, and applies the patch. The scan is
 * retried on a bounded schedule until it succeeds, so the fix lands regardless
 * of how long the engine takes to map the constant. Run off the loader thread
 * so @ref DllMain can return immediately.
 *
 * @return The thread exit code: the number of occurrences patched on the last
 *         attempt, or @ref PatchAll's @c -1 sentinel (@c 0xFFFFFFFF) on failure;
 *         @c 0 when the patch is disabled via ini or no match was ever found.
 */
static DWORD WINAPI MainThread(LPVOID /*lpParam*/) {
    InitPaths();
    // Truncate the log each launch.
    if (FILE* f = OpenFile(g_logPath, "w")) {
        fputs("007 First Light Ultrawide Cutscene Patch v" PATCH_VERSION "\n", f);
        fclose(f);
    }

    if (!IsPatchEnabled()) {
        Log("Patch is disabled via ini. Exiting...");
        return 0;
    }

    // Determine what to search for. Default is the game's built-in 16:9 value
    // (1.777778 -> 39 8E E3 3F). [Advanced] SearchAspect lets you point the
    // scan at a different value without recompiling, e.g. if a future update
    // changes the stored constant and the log shows "Occurrences patched: 0".
    uint8_t needle[4];
    char sbuf[64] = {0};
    GetPrivateProfileStringA("Advanced", "SearchAspect", "0", sbuf, sizeof(sbuf), g_iniPath);
    const float searchAspect = ParseFloat(sbuf);
    if (searchAspect > 0.01f) {
        memcpy(needle, &searchAspect, 4);
        Log("Search aspect overridden via ini: %.6f", searchAspect);
    } else {
        memcpy(needle, kNeedle, 4); // built-in 16:9
    }

    const float aspect = ComputeAspectRatio();

    // The constant isn't necessarily mapped/initialised the instant our thread
    // runs - an ASI loader injects very early. Rather than betting on a single
    // fixed delay, scan repeatedly: succeed as soon as the value appears, and
    // keep trying on slow systems up to a bounded deadline.
    //
    // PatchAll returns >0 on success, -1 on a hard failure (unreadable PE
    // headers - retrying cannot help), and 0 when nothing matched yet. Only the
    // last case is worth another attempt.
    constexpr int kMaxScanAttempts = 20;
    constexpr DWORD kRetryDelayMs = 500; // ~10s worst case before giving up

    int patched = 0;
    for (int attempt = 1; attempt <= kMaxScanAttempts; ++attempt) {
        patched = PatchAll(needle, aspect);
        if (patched != 0) break; // success or hard failure: stop retrying
        if (attempt < kMaxScanAttempts) {
            Log("No matches yet (attempt %d/%d); retrying in %lu ms...", attempt, kMaxScanAttempts,
                kRetryDelayMs);
            Sleep(kRetryDelayMs);
        }
    }

    // Surface the outcome as the thread exit code: the number of patched
    // occurrences, or PatchAll's -1 sentinel (0xFFFFFFFF) on failure.
    return static_cast<DWORD>(patched);
}

/**
 * @brief DLL entry point; the loader calls this when the .asi is mapped.
 *
 * On @c DLL_PROCESS_ATTACH it records the module handle, disables per-thread
 * attach/detach notifications, and spawns @ref MainThread to do the work so the
 * loader is not blocked. All other notification reasons are ignored.
 *
 * @param hModule   Handle to this module.
 * @param reason    The reason code for the call (attach/detach, process/thread).
 * @return @c TRUE to indicate successful load.
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        // ReSharper disable once CppLocalVariableMayBeConst
        if (HANDLE h = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr)) CloseHandle(h);
    }
    return TRUE;
}