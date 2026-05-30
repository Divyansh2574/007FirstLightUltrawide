# 007 First Light — Ultrawide Cutscene Patch (ASI)

Removes the 16:9 pillarboxing (black bars on the sides) during cutscenes on
ultrawide / super-ultrawide monitors, without editing the executable via an hex editor.

It does the same thing as the manual HxD fix — it just does it in memory, at
runtime, every launch. **No file edits, so it should survive game updates**.

## How it works

The Glacier engine stores the cutscene aspect ratio as a raw 32-bit float.
For 16:9 that value is `1.777778`, which in memory is the bytes `39 8E E3 3F`.
On startup this ASI scans the game's loaded executable image, finds every copy
of that constant, and overwrites it with the float for your aspect ratio
(`Width / Height`). Identical result to the hex edit, computed automatically.

The scan runs on a short retry schedule (up to 20 attempts, 500 ms apart) so it
still lands if the engine is slow to map the constant — it succeeds as soon as
the value appears and stops, so there's no fixed startup delay to wait out.

## What you need

1. **An ASI loader.** The standard one is [**Ultimate ASI Loader**](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest)
   (ThirteenAG, on GitHub). Download the **x64** build.
2. Rename the loader's `dll` to a name the game already loads, and drop it in
   the game's `Retail` folder (next to `007FirstLight.exe`). Good choices for a
   Glacier/Bink title:
   - `bink2w64.dll`  (Glacier uses Bink for video — natural fit), or
   - `dinput8.dll`, or `version.dll`, or `winhttp.dll`.
   Any one of these works; pick whichever the loader's docs recommend if one
   doesn't take.

## Install

Place these next to `007FirstLight.exe` (…\Steam\steamapps\common\007 First
Light\Retail\):

```
Retail\
├─ 007FirstLight.exe
├─ <asi-loader>.dll                  (e.g. bink2w64.dll / dinput8.dll)
├─ 007FirstLightUltrawide.asi        ← this patch
└─ 007FirstLightUltrawide.ini        ← your settings
```

Some loader configs look for `.asi` files in a `scripts\` or `plugins\`
subfolder instead — if so, put the `.asi` (and `.ini`) there.

## Configure

Open `007FirstLightUltrawide.ini` and set your resolution:

```ini
[General]
Enabled = 1          ; 0/false/no/off = ASI loads but does nothing

[Aspect]
Width  = 3440
Height = 1440
AspectRatio = 0      ; or set a float here to override Width/Height

[Advanced]
SearchAspect = 0     ; 0 = search for the built-in 16:9 value (normal)
```

`Enabled = 0` is a quick way to turn the fix off without removing any files
(handy for A/B testing or if a future update breaks it). Common aspect values
are pre-listed in the ini comments.

`[Advanced] SearchAspect` controls the value the patch *searches for* in memory
(as opposed to `[Aspect]`, which is the value it writes). Leave it at `0` for
normal use — that means "look for the game's stock 16:9 constant". You'd only
ever set it if a game update changed that stored value (see Troubleshooting).

**Normal use: leave it at `0`.** Zero means "use the built-in 16:9 value (1.777778 → `39 8E E3 3F`)." That's the value the game ships with, so you never touch this for a working patch.

**When you'd change it:** only if a game update relocates *and changes* the locked ratio so the scan reports `Occurrences patched: 0`. At that point the game is no longer storing 16:9, so searching for 16:9 finds nothing. You point `SearchAspect` at the new locked ratio instead — and because it's in the ini, you do it without recompiling.

| `SearchAspect` | float | search bytes |
|---|---|---|
| `0` (default) | 1.777778 (16:9) | `39 8E E3 3F` |
| `1.85` | 1.850000 | `CD CC EC 3F` |
| `2.0` | 2.000000 | `00 00 00 40` |
| `2.39` | 2.390000 | `C3 F5 18 40` |

## Verify

Launch the game and play a cutscene — it should fill the screen. A log file
`007FirstLightUltrawide.log` is written next to the `.asi`; open it to confirm.
A good run looks like:

```
Replace bytes: 8E E3 18 40 (2.388889)
Patched occurrence #1 at 00007FF6...
Patched occurrence #2 at 00007FF6...
Finished. Occurrences patched: 2
```

You may also see one or more `No matches yet (attempt N/20); retrying...` lines
before the patch succeeds — that's normal when the engine hasn't finished
mapping the constant yet, and the retry loop simply waits for it.

## Troubleshooting

- **`Occurrences patched: 0`** — the scan locates the value wherever an update
  moves it, so this almost always means the *value itself* changed (a different
  baked constant, or stored/computed differently). Re-derive the 16:9 search
  bytes in HxD, convert them to a decimal aspect (or just use the new ratio),
  and set `[Advanced] SearchAspect` in the ini — no recompile needed. Timing is
  rarely the cause now: the patch re-scans automatically (20 attempts, 500 ms
  apart, ~10 s total), so it still lands even if the engine is slow to map the
  constant.
- **Log file never appears** — the ASI isn't being loaded. Check that the
  loader proxy dll name is one the game actually imports, and that you used the
  **x64** loader. Some titles need the proxy in the same folder as the exe.
- **Cutscenes stretch instead of crop** — that's the engine's choice for how it
  fits the wider ratio; this fix changes the ratio it targets, not the fit mode.
- **Want to disable it** — set `Enabled = 0` in the ini, or just remove the
  `.asi` (or the loader dll). Nothing on disk in the game files was changed.

## Rebuilding from source

Requires CMake (3.15+) and a C++ compiler — either Visual Studio Build Tools
(MSVC) or MinGW/GCC. The `CMakeLists.txt` works with both, and it **generates
`007FirstLightUltrawide.ini` automatically** next to the built `.asi` (the
template lives inside `CMakeLists.txt`, so there's no separate file to copy).
The project targets 64-bit only.

### On Windows with Visual Studio Build Tools

Do **not** pass a toolchain file (those are for Linux cross-compiling).

```bat
cmake -B build
cmake --build build --config Release
```

Output: `build\Release\007FirstLightUltrawide.asi` (with the `.ini` beside it).
MSVC is multi-config, so the `--config Release` is what selects the optimized
build — there's no `-DCMAKE_BUILD_TYPE` step.

### On Windows with MinGW (e.g. an MSYS2 MINGW64 shell)

Pick a non-Visual-Studio generator so CMake uses your MinGW `gcc`. No toolchain
file is needed — your shell's environment already provides the right compiler.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release    # or -G "MinGW Makefiles"
cmake --build build
```

Output: `build/007FirstLightUltrawide.asi` (with the `.ini` beside it).

### Cross-compiling from Linux

This is the only situation where the toolchain file applies:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-x64.cmake
cmake --build build
```

### Versioning a local build

The build accepts the semantic version as a cache variable; it is compiled into
the `.asi` (file-version resource + the `.log` header). Omit it and you get
`0.0.0` / `dev`:

```bat
cmake -B build -DPROJECT_VERSION=1.4.0
```

## Releases (CI)

Pushing to `main` runs `.github/workflows/release.yml`, which builds an **MSVC
x64 Release** and publishes a GitHub Release. The version is derived
automatically from the commit history using
[Conventional Commits](https://www.conventionalcommits.org/):

| Commit prefix | Example | Bump |
|---|---|---|
| `fix:` | `fix: handle 0-width ini value` | patch — `1.2.3 → 1.2.4` |
| `feat:` | `feat: ini-configurable retry budget` | minor — `1.2.3 → 1.3.0` |
| `feat!:` / `BREAKING CHANGE:` | `feat!: drop 32-bit support` | major — `1.2.3 → 2.0.0` |

Commits that aren't `fix`/`feat`/breaking (e.g. `docs:`, `chore:`) build but
publish **no** release. Each release attaches
`007FirstLightUltrawide-vX.Y.Z.zip` (the `.asi` + generated `.ini`), and that
version is the one embedded in the binary.

**Seeding or forcing a version.** Commit-based bumping needs a prior version tag
to count from, so the **first** release (and any time you want a specific
version) is cut by pushing a tag:

```bash
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

The workflow builds that exact version and publishes it. After a version tag
exists, pushes to `main` take over and auto-increment from it.

> For releases to publish, the repo's **Settings → Actions → General → Workflow
> permissions** must be set to *Read and write* (lets the default `GITHUB_TOKEN`
> create tags and releases).

## Contributing / linting

Two layers keep the code consistent:

- **Locally — [pre-commit](https://pre-commit.com/).** Fast format + hygiene
  checks, plus a commit-message check that enforces the Conventional Commits the
  release workflow depends on. One-time setup:

  ```bash
  pip install pre-commit
  pre-commit install            # installs the pre-commit and commit-msg hooks
  pre-commit run --all-files    # optional: lint the whole tree now
  ```

  Hooks: `clang-format` (shares `.clang-format`), whitespace/EOF/line-ending
  fixers, and `conventional-pre-commit` on the commit message. clang-tidy is
  *not* run here (it needs a build database) — CI handles it.

- **In CI — [cpp-linter](https://github.com/cpp-linter/cpp-linter-action).**
  `.github/workflows/cpp-linter.yml` runs `clang-format` **and** `clang-tidy`
  (using `.clang-format` / `.clang-tidy`) on every PR and push to `main`,
  annotating findings inline. It configures an MSVC build first so clang-tidy
  can resolve `windows.h`.

Style/check config lives in `.clang-format` and `.clang-tidy` at the repo root;
both the local hooks and CI read the same files, so results match.

## Notes
- This patches **every** occurrence of the `39 8E E3 3F` byte pattern in the
  main module. If the engine happens to use that same float for something
  unrelated, it would be changed too.
