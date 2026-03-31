# AGENTS.md
Guidance for agentic coding agents working in this repository.

Scope: entire repo (`dma_ip_drivers`), including `QDMA`, `XDMA`, `XVSEC`, legacy `linux-kernel`, and `QDMA/windows`.

## Repo Facts
- Primary languages: C, shell, Makefiles.
- Build system: GNU Make (Linux) and MSBuild (Windows).
- Linux modules are out-of-tree builds against configured kernel headers/source.
- Most runtime tests require FPGA hardware, loaded drivers, and root privileges.

## Cursor/Copilot Rules
- `.cursorrules`: not found.
- `.cursor/rules/`: not found.
- `.github/copilot-instructions.md`: not found.
- If these appear later, treat them as higher-priority instructions and update this file.

## Build Commands
Run commands from the component directory.

### QDMA Linux (`QDMA/linux-kernel`)
- Build driver + apps: `make`
- Build PF only: `make driver MODULE=mod_pf`
- Build VF only: `make driver MODULE=mod_vf`
- Build apps only: `make apps`
- Clean: `make clean`
- Install all: `sudo make install`
- Install modules only: `sudo make install-mods`
- Install apps only: `sudo make install-apps`
- Uninstall: `sudo make uninstall`

Common build knobs:
- `DEBUG=1`, `DEBUGFS=0|1`
- `EQDMA_CPM5_VF_GT_256Q_SUPPORTED=1`
- `KDIR=<kernel_build_dir>` or `KSRC=<src> KOBJ=<obj>`
- Cross-build: `CROSS_COMPILE=<triplet-> ARCH=<arch>`

### XDMA Linux (`XDMA/linux-kernel/xdma`)
- Build module: `make`
- Clean: `make clean`
- Install module: `sudo make install`
- Uninstall module: `sudo make uninstall`

Common knobs:
- `DEBUG=1`
- `xvc_bar_num=<n>`
- `xvc_bar_offset=<hex>`

### XDMA Tools (`XDMA/linux-kernel/tools`)
- Build tools: `make`
- Clean tools: `make clean`

### XVSEC Linux (`XVSEC/linux-kernel`)
- Build all: `make` or `make clean all`
- Driver only: `make drv`
- User library only: `make libxvsec`
- Tools only: `make tools`
- Clean: `make clean`
- Install: `sudo make install`
- Uninstall: `sudo make uninstall`

### Legacy Linux Tree (`linux-kernel`)
- Driver module: run `make` in `linux-kernel/xdma`
- Tools: run `make` in `linux-kernel/tools`
- Functional scripts: `linux-kernel/tests`

### QDMA Windows (`QDMA/windows`)
- Build solution: `msbuild /t:clean /t:build QDMA.sln`
- Visual Studio workflow and prerequisites: `QDMA/windows/README.md`

## Lint / Static Checks
- No in-tree formatter config (`.clang-format` not found).
- No dedicated lint target found.
- Use build warnings as primary lint signal.
- QDMA driver build includes strict flags (`-Wall -Werror`).
- Useful ad-hoc checks when available: `make W=1`, or external `scripts/checkpatch.pl --no-tree <patch>`.
- Avoid formatting-only churn unless requested.

## Test Commands (Single-Test First)
Most tests are integration/hardware tests and need:
- root/sudo
- loaded module
- expected `/dev` nodes
- supported FPGA design/bitstream

### XDMA tests (`XDMA/linux-kernel/tests`)
- Auto mode test: `./run_test.sh`
- Single MM test: `./dma_memory_mapped_test.sh xdma0 1024 1 1 1`
- Single ST test: `./dma_streaming_test.sh 1024 1 1`
- Broader MM script suite: `cd scripts_mm && ./xdma_mm.sh <bdf>`

### QDMA tests (`QDMA/linux-kernel/scripts`)
- PF script: `./qdma_run_test_pf.sh <bdf> <qid_start> <num_qs> <desc_bypass_en> <pftch_en> <pftch_bypass_en> <flr_on>`
- VF MM script: `./qdma_run_test_mm_vf.sh <bdf> <qid_start>`
- VF ST script: `./qdma_run_test_st_vf.sh <bdf> <qid_start>`

Single-test guidance for QDMA:
- Use minimal queue count where possible (for example, `num_qs=1`).
- For gtest-based VF flow (`qdma_vf_auto_tst.sh`), prefer a narrow filter:
  - `qdma_test --gtest_filter="*VF*"`

### XVSEC
- No dedicated automated test suite found in-tree.
- Validate via targeted `xvsecctl` workflows from `XVSEC/linux-kernel/docs/README`.

## Code Style Guidelines
Follow existing local style first; do not reformat unrelated code.

### Formatting
- Use tabs where existing C files use tabs.
- Keep brace/line-wrap style aligned with neighboring code.

### Includes / Imports
- Preserve include grouping and ordering used by each file.
- Keep `#define pr_fmt(...)` before includes when present.

### Naming
- Functions/variables: `snake_case`.
- Macros/constants: `UPPER_CASE`.
- Keep subsystem prefixes consistent (`qdma_`, `xdma_`, `xvsec_`, `xpdev_`).

### Types
- Kernel space: prefer kernel types (`u8/u16/u32/u64`, `bool`, `ssize_t`, etc.).
- User space: follow existing `<stdint.h>` style (`uint64_t`, etc.) where used.
- Avoid unnecessary signed/unsigned mixing.

### Error Handling
- Kernel code should return negative errno values (`-EINVAL`, `-ENOMEM`, ...).
- In user tools, follow local conventions (errno-style negatives vs nonzero exits).
- Check all return codes from syscalls/APIs.
- Use established cleanup labels (`out`, `err_out`) where appropriate.
- Release resources on every failure path.

### Logging
- Kernel: `pr_err`, `pr_warn`, `pr_info` (plus existing debug macros).
- User space: `fprintf(stderr, ...)`, `perror(...)`, concise actionable errors.
- Include key identifiers in logs when useful (BDF, queue id, channel id).

### Concurrency / Shared State
- Respect existing mutex/list/thread access patterns.
- Avoid lock order inversions.

### Shell Scripts
- Keep scripts consistent with existing Bash style.
- Validate arguments and print usage on bad input.
- Add explicit error checks after critical commands.

## Agent Workflow
- Identify subsystem before editing (`QDMA`, `XDMA`, `XVSEC`, `windows`).
- Build only touched targets first; avoid unnecessary full rebuilds.
- Prefer smallest single test that exercises your change.
- If hardware is unavailable, state that clearly and provide exact run commands.
- Do not mix unrelated cleanup/formatting with functional changes.

Keep this file current as build/test/style practices evolve.
