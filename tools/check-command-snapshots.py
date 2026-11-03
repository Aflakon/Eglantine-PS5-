#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual submission copy and DMA selection with mock backing memory."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = root / 'Eglantine/src'
submit = (source / 'core/hle/service/nvdrv/devices/nvhost_gpu.cpp').read_text()
anchor = 'if (snapshot && channel_state->memory_manager) {'
assert submit.count(anchor) == 1, 'Submission snapshot anchor changed'
footer = '\n        }\n    }\n\n    auto& flags'
assert submit.count(footer) == 1, 'Submission snapshot boundary changed'
body = submit.split(anchor, 1)[1].split(footer, 1)[0]
assert submit.count('const bool defer_empty = ') == 1, 'Title policy anchor changed'
policy = submit.split('const bool defer_empty = ', 1)[1].split(';', 1)[0]
dma = (source / 'video_core/dma_pusher.cpp').read_text()
assert dma.count('const bool submitted = ') == 1, 'DMA selection anchor changed'
selection = dma.split('const bool submitted = ', 1)[1].split(';', 1)[0]
fixture = (root / 'headless/command_snapshot_check.cpp.in').read_text()
assert fixture.count('@SNAPSHOT_BODY@') == fixture.count('@SNAPSHOT_SELECTION@') == fixture.count('@SNAPSHOT_POLICY@') == 1
fixture = (fixture.replace('@SNAPSHOT_BODY@', body).replace('@SNAPSHOT_SELECTION@', selection)
           .replace('@SNAPSHOT_POLICY@', policy))
with tempfile.TemporaryDirectory(prefix='eden-command-snapshots-') as directory:
    directory = Path(directory)
    cpp, executable = directory / 'check.cpp', directory / 'check'
    cpp.write_text(fixture)
    subprocess.run(['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-I' + str(source),
                    str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=10)
print('Actual command snapshots: title gate, empty startup fetch, preserved words on reuse, ordinary entries, '
      'GPU-written/dirty/resumed overrides and empty lists PASS (mock backing memory)')
