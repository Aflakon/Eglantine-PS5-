#!/usr/bin/env python3
"""Write and sign the update catalog the app reads (branch `catalog` of this repository).

    publish-update-catalog.py RELEASE.zip TAG NOTES.txt OUTPUT_FOLDER

RELEASE.zip is the file attached to the GitHub release TAG. NOTES.txt is what the update dialog
shows as What's new: plain text, list items starting "- ". The private key is
~/.config/eglantine-ps5/update-signing.pem; it never goes in the repository.

Check the result, then commit OUTPUT_FOLDER's contents to the branch `catalog`.
"""
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

REPOSITORY = 'viejunerd/Eglantine-PS5-'
OPENSSL = shutil.which('openssl', path='/opt/homebrew/bin:' + os.environ.get('PATH', ''))
KEY = pathlib.Path.home() / '.config/eglantine-ps5/update-signing.pem'
MAX_NOTES = 4000  # what the app's offer takes from a catalog (self_update.h)

if len(sys.argv) != 5:
    sys.exit(__doc__.strip())
archive, tag, notes_file, output = pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3]), pathlib.Path(sys.argv[4])

with zipfile.ZipFile(archive) as zip_file:
    param = json.loads(zip_file.read('PPSA98515/sce_sys/param.json'))
title, content = param['titleId'], param['contentVersion']
if not re.fullmatch(r'\d{2}\.\d{3}\.\d{3}', content):
    sys.exit(f'contentVersion must be NN.NNN.NNN, not {content}')
notes = notes_file.read_text().strip()
if len(notes) > MAX_NOTES:
    sys.exit(f'{notes_file} has {len(notes)} characters; the app takes {MAX_NOTES}')

app = {
    'title_id': title,
    'name': param['localizedParameters']['en-US']['titleName'] if 'localizedParameters' in param else 'EglantinePS5',
    'status': 'available',
    'content_version': content,
    'version': 'v' + content[3:],
    'format': 'zip',
    'artifact_url': f'https://github.com/{REPOSITORY}/releases/download/{tag}/{archive.name}',
    'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
    'size': archive.stat().st_size,
    'page': f'https://github.com/{REPOSITORY}/releases/tag/{tag}',
    'release_notes': notes,
    'release_notes_truncated': False,
}
app_bytes = (json.dumps(app, indent=2, ensure_ascii=False) + '\n').encode()
# The time, not a counter: the app refuses a sequence lower than the highest it has seen, and
# consoles still remember the one from the catalog they used before this one.
manifest = {'sequence': int(time.time()), 'files': {f'apps/{title}.json': hashlib.sha256(app_bytes).hexdigest()}}
manifest_bytes = (json.dumps(manifest, indent=2) + '\n').encode()

(output / 'apps').mkdir(parents=True, exist_ok=True)
(output / 'apps' / f'{title}.json').write_bytes(app_bytes)
(output / 'manifest.json').write_bytes(manifest_bytes)
with tempfile.NamedTemporaryFile() as message:
    message.write(manifest_bytes)
    message.flush()
    subprocess.run([OPENSSL, 'pkeyutl', '-sign', '-rawin', '-inkey', str(KEY), '-in', message.name,
                    '-out', str(output / 'manifest.sig')], check=True)
if (output / 'manifest.sig').stat().st_size != 64:
    sys.exit('The signature is not 64 bytes')
print(f'{title} {content} ({app["version"]}) -> {app["artifact_url"]}')
