# Mario Kart World title screen: frame rate on the console

Status: 2026-11-02. The title screen went from about 11 FPS to about 24 FPS on the same core
(Zortos `3140648a`) and driver (RADV, pinned Mesa). 40 FPS is not reached; what is left is
measured below. Every number is one console run of a 45-60 s window at the title (the attract
scene varies between runs by about 2 FPS), with the `EDEN_VULKAN_FRAME` counter of `heap.log`.

## What was wrong

The console has no fastmem arena for 64-bit games: every guest read of a page the GPU caches
track leaves the JIT, and when the per-core area cache misses, it was answered by the GPU
thread (an "ordered read": the guest core sleeps, the GPU thread wakes, answers, the core
wakes). With the port's high GPU accuracy every fence also waited for the console's GPU and the
core's split GPU thread stayed off. Profiles of the baseline (ps5-perf, precise and off-CPU):

- the process used 2.6 of 8 CPUs; the GPU thread idled 40-47% waiting for commands while the
  guest's main core spent 54% of its wall time in `HostCommandCompletion::Wait`;
- the Vulkan driver got 1,000-3,700 submissions a second (one per fence), each a cache flush,
  an AGC submit and a 0.3 ms suspend point;
- 12% of the GPU thread's wall time was `fopen`/`fclose`/`_write` of per-process dump files
  (patch 0005 gates them behind `EDEN_NX2_READBACK_DIAGNOSTICS`);
- the texture collector sat in aggressive mode (the port measures GPU memory as the budget
  less the shared pool's largest free block, and the guest holds 4.3-4.8 GB of the 11.1 GB
  pool at the title), re-uploading ~3,000-4,500 textures per 600 frames.

## What changed

| Change | Where | Title FPS |
|---|---|---|
| Baseline: high accuracy, reactive flushing, no split thread | - | 10.9 |
| Low accuracy (fast GPU fences), reactive flushing off | performance settings | 12.0 |
| + the core's split GPU thread (needs the two above) | fixed a crash, below | 13.1 |
| + guest reads with nothing to flush answered on the guest core (`EDEN_NX2_UNSAFE_PREEMPTIVE_READS=1`, as fastmem builds do) | main.cpp default | 19.9 |
| + the split's front checks the written-page bits before draining the back for a command list | patch 0006 | 21.8-24.2 |
| + 300 us spin waits before the three sleeps of a read round trip | patch 0006, main.cpp default | within noise |

The split GPU thread crashed on the console: the port presented its loading screen from the
front thread while the back thread composited the guest's frames, both recording into one
scheduler (a descriptor set allocation failed, then `Layer::ConfigureDraw` dereferenced the
empty set). The present now goes through the split's ring (`headless/graphics.cpp`).

Switch 2 titles (program ids `0400...`) default to fast GPU fences and no reactive flushing
in the port's performance settings; the game's own values in the Library still win.

## What is left (profile of the 24 FPS configuration)

- The split's back thread is the limit: ~0.7-0.9 of a core at 22 FPS, of which engine method
  calls ~20% (2 million a second), draw setup ~35% (26,000-30,000 draws a second), texture
  uploads ~15% (re-uploads of collected ASTC images), fences ~3%, ring dispatch ~19%.
- The front still waits for the back for command lists on GPU-written pages. Checking a
  semaphore acquire against the guest's own word before draining is on (23.1 FPS, within the
  run-to-run noise of the full drain; `EDEN_NX2_ACQUIRE_DRAIN_FIRST=1` restores the drain).
  Waiting only up to the last compute, copy or upload method crashed the game while loading on
  two candidates (a list read before its writer ran, then a clear with a garbage render
  target), so it is off unless `EDEN_NX2_LIST_DRAIN_TO_WRITER=1`: something the front does not
  count as a writer fills those lists.
- Memory: the guest's mapped memory grows through the title screen (4.3 -> 4.8 GB in two
  minutes) while the texture cache holds ~2-4 GB of host copies and the heap ~1.9 GB in an
  11.1 GB pool; low-accuracy runs end with `VK_ERROR_OUT_OF_DEVICE_MEMORY` after 1.5-3
  minutes. The 20 s memory report now also prints the kernel's count of the guest's mapped
  memory to tell real growth from sparse-slot commits. `EDEN_NX2_TC_COSTLY_MIN_AGE` keeps
  recently used decoded ASTC images out of aggressive collection (fewer re-uploads, less
  headroom).
- The core's `EDEN_NX2_GPU_PROFILE` costs ~25% of the GPU thread's CPU on the console (clock
  reads and emulated TLS); never read final frame rates with it on.

## Reproduce

Build in the usual container (`docs/BUILDING.md`), with `EDEN_PS5_VULKAN=ON` and
`EDEN_VULKAN_DRIVER=RADV`, and package into `build/<name>/PPSA98515`. Then, under the console
lease:

```
python3 tools/console-lease.py --host 192.168.0.190 run --wait -- \
  env MKW_CANDIDATE=<name> python3 tools/mkw-title/run.py .local/exp \
  "prod:fast_gpu=true,reactive_flushing=false,measure=60;control:fast_gpu=true,reactive_flushing=false,env=EDEN_NX2_LIST_DRAIN_FULL=1|EDEN_NX2_ACQUIRE_DRAIN_FIRST=1"
python3 tools/mkw-title/analyze.py .local/exp/<run>
python3 tools/mkw-title/stacks.py .local/exp/<run>/prod precise GPU,GPUBack,CPUCore_0 3
```

`run.py` deploys the candidate's `eboot.bin`, writes the game's performance settings and an
`env.txt` (the port's development switch file: `NAME=VALUE` lines read before the core starts,
`EDEN_LOG_FILTER` names a log filter), launches the game through `launch.elf` (built from
`launch.c` with the payload SDK), waits for the title (800 presented frames), measures,
records ps5-perf precise and off-CPU profiles with the build's matched ELF, keeps live log
tails (the crash handler's restart truncates them), and restores the settings file and the
original `eboot.bin`.

AI assistance: the investigation, the changes and this document were made with Claude Code.
