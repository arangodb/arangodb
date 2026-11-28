# GitHub Actions Workflow Generator

This directory contains `gen_gh_actions.py`, a script to generate GitHub Actions CI workflows from the same configuration logic used for Travis CI.

## Usage

The script can generate workflows for different platforms:

```bash
# Generate Linux CI workflow (default)
./scripts/gen_gh_actions.py linux > .github/workflows/linux-ci.yml

# Generate macOS CI workflow
./scripts/gen_gh_actions.py macos > .github/workflows/macos-ci.yml

# Generate Windows CI workflow
./scripts/gen_gh_actions.py windows > .github/workflows/windows-ci.yml

# Generate FreeBSD CI workflow
./scripts/gen_gh_actions.py freebsd > .github/workflows/freebsd-ci.yml

# Generate combined workflow with all platforms
./scripts/gen_gh_actions.py all > .github/workflows/ci-all.yml
```

## Generated Workflows

### Linux CI (`linux-ci.yml`)
- **test-linux** (AMD64): `ubuntu-latest` (x86_64)
  - ~96 configurations covering GCC, Clang, various flags
- **test-linux-arm64** (ARM64): `ubuntu-24.04-arm` (aarch64)
  - ~14 configurations including large hugepage tests
  - **Note:** Free ARM64 runners (Public Preview) - may have longer queue times during peak hours

**Total:** 110 configurations

### macOS CI (`macos-ci.yml`)
- **test-macos** (Intel): `macos-15-intel` (x86_64)
  - ~10 configurations with GCC compiler
- **test-macos-arm64** (Apple Silicon): `macos-latest` (arm64)
  - ~11 configurations including large hugepage tests

**Total:** 21 configurations

### Windows CI (`windows-ci.yml`)
- **test-windows** (AMD64): `windows-latest` (x86_64)
  - 10 configurations covering MinGW-GCC and MSVC compilers
  - 32-bit and 64-bit builds
  - Uses MSYS2 for build environment

**Total:** 10 configurations

### FreeBSD CI (`freebsd-ci.yml`)
- **test-freebsd** (AMD64): Runs in FreeBSD VM on `ubuntu-latest`
  - Matrix testing: debug (on/off), prof (on/off), arch (32/64-bit), uncommon configs
  - 16 total configuration combinations
  - Uses FreeBSD 15.0 via `vmactions/freebsd-vm@v1`
  - Uses `gmake` (GNU Make) instead of BSD make

**Total:** 16 configurations

## Architecture Verification

Each workflow includes a "Show OS version" step that prints:

**Linux:**
```bash
=== System Information ===
uname -a              # Kernel and architecture
=== Architecture ===
uname -m              # x86_64, aarch64, etc.
arch                 # Architecture type
=== CPU Info ===
lscpu                # Detailed CPU information
```

**macOS:**
```bash
=== macOS Version ===
sw_vers              # macOS version and build
=== Architecture ===
uname -m             # x86_64 or arm64
arch                # i386 or arm64
=== CPU Info ===
sysctl machdep.cpu.brand_string  # CPU model
```

**Windows:**
```cmd
=== Windows Version ===
systeminfo           # OS name and version
ver                 # Windows version
=== Architecture ===
PROCESSOR_ARCHITECTURE  # AMD64, x86, ARM64
```

## GitHub Runner Images

| Platform | Runner Label | Architecture | OS Version | Strategy |
|----------|--------------|--------------|------------|----------|
| Linux AMD64 | ubuntu-latest | x86_64 | Ubuntu 22.04+ | Auto-update |
| Linux ARM64 | ubuntu-24.04-arm | aarch64 | Ubuntu 24.04 | Free (Public Preview) |
| macOS Intel | macos-15-intel | x86_64 | macOS 15 Sequoia | Pinned |
| macOS Apple Silicon | macos-15 | arm64 | macOS 15 Sequoia | Pinned |
| Windows | windows-latest | x86_64 | Windows Server 2022+ | Auto-update |
| FreeBSD | ubuntu-latest (VM) | x86_64 | FreeBSD 15.0 in VM | VM-based |

### Runner Strategy Explained

We use a **hybrid approach** to balance stability and maintenance:

**Auto-update runners (`-latest`):**
- **Linux AMD64**: `ubuntu-latest` - Very stable, rarely breaks, auto-updates to newest Ubuntu LTS
- **Windows**: `windows-latest` - Backward compatible, auto-updates to newest Windows Server

**Pinned runners (specific versions):**
- **Linux ARM64**: `ubuntu-24.04-arm` - **Free for public repos** (Public Preview, may have queue delays)
- **macOS Intel**: `macos-15-intel` - Last Intel macOS runner (EOL **August 2027**)
- **macOS Apple Silicon**: `macos-15` - Pin for control over macOS upgrades

**Why this approach?**
-  Reduces maintenance (auto-update where safe)
-  Prevents surprise breakages (pin where needed)
-  Balances stability and staying current
-  Uses free ARM64 runners for public repositories

### ARM64 Queue Times

**If you experience long waits for ARM64 jobs:**

The `ubuntu-24.04-arm` runner is **free for public repositories** but is in **Public Preview**. GitHub warns: *"you may experience longer queue times during peak usage hours"*.

To reduce wait times we should upgrade to Team/Enterprise plan - then we could use `ubuntu-24.04-arm64` for faster, paid runners

### Important Deprecation Timeline

| Date | Event | Action Required |
|------|-------|------------------|
| **August 2027** | macOS Intel runners removed | Must drop Intel macOS testing or use self-hosted |
| **TBD** | ARM64 runners leave Public Preview | May see improved queue times |

**Note:** `macos-15-intel` is the **last Intel-based macOS runner** from GitHub Actions. After August 2027, only Apple Silicon runners will be available.

## Platform-Specific Details

### Windows Build Process
The Windows workflow uses:
1. **MSYS2** setup via `msys2/setup-msys2@v2` action
2. **MinGW-GCC**: Standard autotools build process in MSYS2 shell
3. **MSVC (cl.exe)**: Requires `ilammy/msvc-dev-cmd@v1` for environment setup
   - Uses `MSYS2_PATH_TYPE: inherit` to inherit Windows PATH
   - Exports `AR=lib.exe`, `NM=dumpbin.exe`, `RANLIB=:`
4. **mingw32-make**: Used instead of `make` (standard in MSYS2)

### macOS Build Process
- Uses Homebrew to install `autoconf`
- Tests on both Intel (x86_64) and Apple Silicon (ARM64)
- Standard autotools build process
- Excludes certain malloc configurations not supported on macOS

### Linux Build Process
- Ubuntu Latest for AMD64, Ubuntu 24.04 for ARM64
- Installs 32-bit cross-compilation dependencies when needed
- Most comprehensive test matrix (110 configurations)

## Relationship to Travis CI

This script mirrors the logic from `gen_travis.py` but generates GitHub Actions workflows instead of `.travis.yml`. The test matrices are designed to provide equivalent coverage to the Travis CI configuration.

## Regenerating Workflows

To regenerate all workflows after modifying `gen_gh_actions.py`:

```bash
./scripts/gen_gh_actions.py linux > .github/workflows/linux-ci.yml
./scripts/gen_gh_actions.py macos > .github/workflows/macos-ci.yml
./scripts/gen_gh_actions.py windows > .github/workflows/windows-ci.yml
```

**Note**: The generated files should not be edited by hand. All changes should be made to `gen_gh_actions.py` and then regenerated.

