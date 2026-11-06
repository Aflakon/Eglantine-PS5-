# Cold memory compression (OoT on a base PS5)

Notes on what's in this branch. Tested with Zelda: Ocarina of Time (Switch 2, 1.1.1) on a base PS5, 9.60.

## The problem

On the PS5 the game's memory, the emulator, the JIT and the GPU all share one 12 GB pool. In Kokiri Forest it looked like this: game ~5.9 GB, graphics ~2.5 GB, Eden's heap ~2 GB, JIT ~0.5 GB, other ~0.8 GB. It fills up a few minutes in and a Vulkan allocation fails (same as #21, #23).

## What I noticed

Most of the game's memory isn't touched for minutes at a time. I took access away from every 2 MiB guest slot each scan pass (`mprotect(PROT_NONE)`) and counted the ones that faulted back. Walking around Kokiri Forest, about 5 of ~5.8 GB stayed untouched for 2+ minutes. LZ4 packs those slots to 40-55%.

## What the branch does

It adds a `Cold` slot state to the guest memory service (`src/guest_memory_service.inc`), next to Zero/Own/Shared/Fine:

- Every scan pass, Own slots lose their access. A touch faults and the existing SIGSEGV handler gives it back right away (it already sees reads, not just writes). JIT fastmem and Eden's own code both go through it.
- A slot still untouched after 6 passes gets LZ4'd into a direct memory block of its own size (16 KiB pages). Its 2 MiB block is released and the address range is re-reserved with no access.
- Any access to a Cold slot faults. The handler gets a new 2 MiB block (pool, reserve, or it waits like writes already do), unpacks into it through a temp view, then maps it. Nobody sees a half-filled slot.
- When the pool's largest free block drops under 512 MiB, passes run every 5 s and compress after 1 untouched pass.
- If a Vulkan allocation still fails, `MemoryAllocator::RelieveMemory` asks the service to compress up to 256 MiB right then and retries (patch 1905).

The GPU never maps guest memory in this port, so taking access away doesn't affect it.

Switches (`dev-switches.txt`):

```
env=EDEN_NX2_CRAM_COLD=2            # 1 = only measure, 2 = compress
env=EDEN_NX2_CRAM_COLD_PASSES=6     # passes untouched before compressing
env=EDEN_NX2_CRAM_RESERVE_MIB=128   # guest reserve size, default 1024
```

Log lines: `EDEN_CRAM_COLD MiB untouched: ...` every 30 s, and `EDEN_CRAM_COLD reclaim for graphics: N slots, M MiB given back`.

## Results (docked 1x)

- Before: out of memory 4-16 min in.
- After: a 36 min session with no crash, more long runs after that. 27-30 fps most of the time.
- Guest memory ~5.9 GB down to ~5 GB, sometimes under 4.9. Usually 700-1200 slots cold.
- Thousands of compress/restore cycles, zero failed restores.

## Limits

- It moves the wall, it doesn't remove it. At 1.5x the GPU wants ~1.5 GB more and the game's working set doesn't fit anymore. Compression then thrashes (0.04 fps). For OoT, 1x is the ceiling on a base PS5.
- A syscall that reads or writes a cold slot directly would get EFAULT instead of a fault. Didn't see it in OoT, other games might.
- Compressing takes ~3 ms a slot, on the scanner thread, or on the thread that asked for GPU memory when things are tight.

## Other things in here

- Guest reserve size switch. The 1 GiB reserve had 840 MiB sitting idle when graphics ran out.
- Patch 1904: when memory is critical, the texture collector drops GPU-written images over 64 MiB without downloading them first. In OoT that was a 12288x12288 shadow map (288 MiB). The download needed a 288 MiB staging buffer plus a 288 MiB heap block right at the worst moment.
- Patch 1902: ASTC transcode memory budget, plus freeing temp transcode images and idle staging when an allocation fails.
- JIT cache 128 MiB for cores 3+, heap arenas at 2 MiB in their own region (headless/).
- Patches 1906/1907: Lichii's opt-in OoT fog/exposure workaround (`EDEN_NX2_OOT_VISUAL_WORKAROUND=1`) and two AMD forest fog fixes from Shakoquo/Lichii-sharkiix-version. With those, the `EDEN_NX2_REJECT_HASH` workaround for the white veil isn't needed anymore.

## Base

This is on 06f90c9 (v1.219.004) with the core moved to Lichii nx2 8813b447 (zortos293/Lichii). The submodule URL still points at Djihads80/Eglantine, so fetch 8813b447 from Lichii. It won't merge clean into current main, but most of the memory work is in `src/guest_memory_service.inc` and patches 1902, 1904, 1905.

Grass is still missing: mesh draws still make the app disappear while a save loads, so `no-mesh.txt` stays.
