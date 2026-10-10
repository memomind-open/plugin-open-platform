#!/usr/bin/env python3
"""Check declared Web/Glass pairing and optional GMP identity metadata, without running plugins."""
import argparse
import json
from pathlib import Path
import re
import sys
import zipfile

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO / 'GlassSDK/build-host/tools'))
from gmp_xip_pack import HEADER


def read_manifest(path):
    path = Path(path)
    if path.is_dir():
        path = path / 'manifest.json'
    if path.suffix == '.mmpkg':
        with zipfile.ZipFile(path) as archive:
            if archive.namelist().count('manifest.json') != 1:
                raise ValueError('MMPKG must contain exactly one root manifest.json')
            result = json.loads(archive.read('manifest.json'))
    else:
        result = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(result, dict) or not isinstance(result.get('id'), str) or not result['id']:
        raise ValueError('Manifest must contain a nonempty id: ' + str(path))
    return result


def version(value):
    if not isinstance(value, str) or not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,3}', value):
        raise ValueError('Expected a numeric version with one to four components: ' + repr(value))
    parts = tuple(int(part) for part in value.split('.'))
    return parts + (0,) * (4 - len(parts))


def protocols(entries, version_key):
    if not isinstance(entries, list):
        raise ValueError('protocols must be an array')
    result = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get('id'), str) or not entry['id']:
            raise ValueError('Protocol entries require a nonempty id')
        if entry['id'] in result:
            raise ValueError('Duplicate protocol: ' + entry['id'])
        result[entry['id']] = version(entry.get(version_key))
    return result


def check_pair(web, glass):
    requirements = web.get('deviceRequirements')
    if not isinstance(requirements, dict):
        raise ValueError('No deviceRequirements: declare the intended pair or treat this as a phone-only application')
    provides = glass.get('provides', {})
    if not isinstance(provides, dict):
        raise ValueError('Glass provides must be an object')
    needed = protocols(requirements.get('protocols', []), 'minVersion')
    available = protocols(provides.get('protocols', []), 'version')
    errors, notes = [], []
    required_id = requirements.get('requiredPluginId')
    if required_id is not None:
        if not isinstance(required_id, str) or not required_id:
            raise ValueError('requiredPluginId must be a nonempty string')
        if required_id != glass['id']:
            errors.append('Required plugin ' + required_id + '; selected ' + glass['id'])
        if 'minPluginVersion' in requirements:
            if version(str(glass.get('version'))) < version(requirements['minPluginVersion']):
                errors.append('Glass package version is below minPluginVersion')
    for protocol_id, minimum in needed.items():
        if protocol_id not in available:
            errors.append('Missing protocol ' + protocol_id)
        elif available[protocol_id] < minimum:
            errors.append('Protocol version too old: ' + protocol_id)
    preferred = requirements.get('preferredPluginId')
    if preferred and preferred != glass['id']:
        notes.append('Preferred plugin is ' + str(preferred) + '; a different provider must satisfy all requirements')
    if not needed and not required_id:
        notes.append('No required protocol or identity is declared; this is weak evidence of compatibility')
    return errors, notes


def check_gmp(path, glass):
    path = Path(path)
    with path.open('rb') as stream:
        data = stream.read(HEADER.size)
    if len(data) != HEADER.size:
        raise ValueError('Truncated GMP header: ' + str(path))
    fields = HEADER.unpack(data)
    if fields[0] != b'GMPK' or fields[1] != 2 or fields[3] != path.stat().st_size:
        raise ValueError('Expected a GMP v2 header with matching file size')
    name = fields[15].split(b'\0', 1)[0].decode('utf-8')
    if (name, fields[14], fields[2]) != (glass.get('name'), glass.get('version'), glass.get('abi_version')):
        raise ValueError('GMP name/version/ABI differs from the selected Glass manifest; rebuild or select the matching package')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--web', required=True, help='Web source manifest/directory or the actual .mmpkg to deliver')
    parser.add_argument('--glass', required=True, help='manifest/directory for the Glass plugin actually selected in Studio')
    parser.add_argument('--gmp', help='actual GMP file; checks existence and name/version/ABI header metadata only')
    args = parser.parse_args()
    try:
        web, glass = read_manifest(args.web), read_manifest(args.glass)
        errors, notes = check_pair(web, glass)
        if args.gmp:
            check_gmp(args.gmp, glass)
        print('Web: ' + web['id'] + ' -> Glass: ' + glass['id'])
        for note in notes:
            print('NOTE: ' + note)
        if errors:
            for error in errors:
                print('FAIL: ' + error)
            return 1
        print('PASS: declared pairing' + (' and GMP identity metadata' if args.gmp else ' (manifest only; no GMP checked)'))
        print('This does not validate GMP code, CRC, source freshness, channel handlers, permissions, Studio selection, or runtime behavior.')
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
