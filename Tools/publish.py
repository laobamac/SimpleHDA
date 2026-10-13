#!/usr/bin/env python3
#
#  publish.py
#  SimpleHDA
#
#  Created by laobamac on 2026/8/28.
#  Copyright © 2026 laobamac. All rights reserved.
#
import argparse
import json
import os
from pathlib import Path
import plistlib
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--channel', choices=('pre', 'rel'), required=True)
args = parser.parse_args()
repository = os.environ['GITHUB_REPOSITORY']
sha = os.environ['GITHUB_SHA']
api = os.environ.get('GITHUB_API_URL', 'https://api.github.com')
token = os.environ['GH_TOKEN']
version = plistlib.loads((root / 'Info.plist').read_bytes())['CFBundleVersion']
if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
    raise SystemExit('Invalid repository name')
if not re.fullmatch(r'[0-9a-fA-F]{40}', sha):
    raise SystemExit('Invalid commit SHA')
suffix = f'-alpha-{sha[:7]}' if args.channel == 'pre' else ''
names = [f'SimpleHDA-Tahoe-v{version}-{configuration}{suffix}' for configuration in ('DEBUG', 'RELEASE')]
files = [root / 'Dist' / f'{name}.zip' for name in names]
if not all(path.is_file() for path in files):
    raise SystemExit('Release assets are missing')
base = f'/repos/{repository}'

def request(method, path, data=None, missing=False):
    body = None if data is None else json.dumps(data).encode()
    headers = {'Accept': 'application/vnd.github+json', 'Authorization': f'Bearer {token}',
               'X-GitHub-Api-Version': '2022-11-28', 'Content-Type': 'application/json'}
    query = urllib.request.Request(api + path, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(query, timeout=60) as response:
            text = response.read()
            return json.loads(text) if text else None
    except urllib.error.HTTPError as error:
        if error.code == 404 and missing:
            return None
        raise SystemExit(f'GitHub {method} {path} failed: HTTP {error.code}') from None

def pages(path):
    results = []
    page = 1
    while True:
        batch = request('GET', path + f'?per_page=100&page={page}')
        results.extend(batch)
        if len(batch) < 100:
            return results
        page += 1

if args.channel == 'rel':
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    release = event.get('release', {})
    tag = release.get('tag_name', '')
    if release.get('prerelease') or release.get('draft') or tag not in (version, f'v{version}'):
        raise SystemExit(f'Formal release tag must be {version} or v{version}, and match Info.plist')
    current = request('GET', base + '/releases/' + str(release['id']))
    if current['tag_name'] != tag or current['prerelease'] or current['draft']:
        raise SystemExit('Release state changed during the build')
else:
    tag = f'v{version}-alpha'
    encoded = urllib.parse.quote(tag, safe='')
    current = request('GET', base + '/releases/tags/' + encoded, missing=True)
    if current and (not current['prerelease'] or current['draft']):
        raise SystemExit('Alpha tag is already used by a formal or draft release')
    ref = request('GET', base + '/git/ref/tags/' + encoded, missing=True)
    if ref:
        request('PATCH', base + '/git/refs/tags/' + encoded, {'sha': sha, 'force': True})
    else:
        request('POST', base + '/git/refs', {'ref': 'refs/tags/' + tag, 'sha': sha})
    commits = request('GET', base + f'/commits?sha={sha}&per_page=5')
    repository_url = os.environ.get('GITHUB_SERVER_URL', 'https://github.com') + '/' + repository
    changes = []
    for commit in commits:
        subject = commit['commit']['message'].splitlines()[0]
        subject = re.sub(r'([\\`*_{}\[\]<>#|])', r'\\\1', subject)
        changes.append(f'- [`{commit["sha"][:7]}`]({repository_url}/commit/{commit["sha"]}) {subject}')
    description = 'Development build for macOS Tahoe.\n\n' + '\n'.join(changes) + '\n'
    values = {'tag_name': tag, 'target_commitish': sha, 'name': tag,
              'body': description, 'draft': False, 'prerelease': True, 'make_latest': 'false'}
    if current:
        current = request('PATCH', base + '/releases/' + str(current['id']), values)
    else:
        current = request('POST', base + '/releases', values)
subprocess.run(['gh', 'release', 'upload', tag, *(str(path) for path in files), '--repo', repository, '--clobber'], check=True)
if args.channel == 'pre':
    keep = {path.name for path in files}
    for asset in pages(base + f'/releases/{current["id"]}/assets'):
        if asset['name'] not in keep:
            request('DELETE', base + '/releases/assets/' + str(asset['id']))
    for release in pages(base + '/releases'):
        old_tag = release['tag_name']
        managed = old_tag == 'pre' or re.fullmatch(r'v\d+\.\d+\.\d+-alpha', old_tag)
        if release['id'] != current['id'] and release['prerelease'] and not release['draft'] and managed:
            request('DELETE', base + '/releases/' + str(release['id']))
            request('DELETE', base + '/git/refs/tags/' + urllib.parse.quote(old_tag, safe=''), missing=True)
print(f'Published {tag}: ' + ', '.join(names))
