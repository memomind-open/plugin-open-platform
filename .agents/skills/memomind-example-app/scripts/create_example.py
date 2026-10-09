#!/usr/bin/env python3
"""Create a small Web/Glass project from current repository inputs, never overwrite it."""
import argparse
import html
import json
from pathlib import Path
import re
import shutil
import shlex
import subprocess
import tempfile

SKILL = Path(__file__).resolve().parents[1]
REPO = SKILL.parents[2]


def bundle_sdk(destination):
    vendor = destination / 'vendor'
    if vendor.is_symlink() or (vendor / 'gm-plugin-web-sdk.esm.js').is_symlink():
        raise ValueError('Refusing to write SDK through a symbolic link')
    code = '''
import { pathToFileURL } from 'node:url';
import { readFile } from 'node:fs/promises';
const root = process.argv[1];
const { bundlePhoneSdk } = await import(pathToFileURL(root + '/tools/bundle-phone-sdk.mjs'));
const metadata = JSON.parse(await readFile(root + '/package.json', 'utf8'));
process.stdout.write(await bundlePhoneSdk(root, metadata.version));
'''
    result = subprocess.run(
        ['node', '--input-type=module', '-e', code, str(REPO / 'PhoneSDK')],
        check=True, capture_output=True, text=True, encoding='utf-8',
    )
    vendor.mkdir(exist_ok=True)
    (vendor / 'gm-plugin-web-sdk.esm.js').write_text(result.stdout, encoding='utf-8')


def create(args):
    sdk = 'PhoneSDK' if args.kind == 'web' else 'GlassSDK'
    target = Path(args.output).absolute() if args.output else REPO / sdk / 'examples' / args.name
    if target.exists() or target.is_symlink():
        raise ValueError('Destination already exists: ' + str(target))
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.memomind-starter-', dir=str(target.parent)) as temporary:
        stage = Path(temporary) / 'project'
        stage.mkdir()
        if args.kind == 'web':
            shutil.copytree(SKILL / 'assets/web', stage, dirs_exist_ok=True)
            p = stage / 'index.html'
            p.write_text(p.read_text(encoding='utf-8').replace('MemoMind Starter', html.escape(args.title)), encoding='utf-8')
            p = stage / 'plugin.js'
            p.write_text(p.read_text(encoding='utf-8').replace("'starter.count'", json.dumps(args.id + '.count')), encoding='utf-8')
            manifest = {
                'id': args.id, 'name': args.title, 'version': '0.1.0', 'entry': 'index.html',
                'bridgeVersion': '2.0', 'schemaVersion': 2, 'permissionPolicyVersion': 1,
                'permissions': [{'name': 'storage', 'required': True}, {'name': 'display', 'required': True}],
                'deviceRequirements': {'preferredPluginId': 'com.gm.example.web-bridge',
                                       'protocols': [{'id': 'gm.scene', 'minVersion': '1.0'}]},
            }
            bundle_sdk(stage)
        else:
            source = REPO / 'GlassSDK/examples/lvgl_ui'
            c_source = (source / 'lvgl_ui.c').read_text(encoding='utf-8')
            c_source = c_source.replace('../common/call_ui.h', 'call_ui.h')
            c_source = c_source.replace('if (screen == 0) return GM_PLUGIN_ESTATE;',
                'if (screen == 0) { example_call_ui_stop(&s_call_ui); return GM_PLUGIN_ESTATE; }')
            c_source = c_source.replace('        return GM_PLUGIN_ENOMEM;',
                '        example_call_ui_stop(&s_call_ui);\n        return GM_PLUGIN_ENOMEM;')
            c_source = c_source.replace('"LVGL UI example"', json.dumps(args.title, ensure_ascii=True))
            (stage / (args.name + '.c')).write_text(c_source, encoding='utf-8')
            shutil.copy2(REPO / 'GlassSDK/examples/common/call_ui.h', stage / 'call_ui.h')
            manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
            manifest.update(id=args.id, name=args.title, version=1)
        (stage / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        # Recheck before moving the completed project; failure leaves no half-created project.
        if target.exists() or target.is_symlink():
            raise ValueError('Destination appeared during generation: ' + str(target))
        stage.rename(target)
    display_target = shlex.quote(str(target))
    print('Created: ' + str(target))
    if args.kind == 'web':
        print('Preview: node PhoneSDK/tools/run-browser-studio.mjs --plugin ' + display_target)
        print('Package: node PhoneSDK/tools/build-mmpkg.mjs ' + display_target + ' <release-path>.mmpkg')
        print('Pair with com.gm.example.web-bridge in Desktop Studio; a generated LVGL Glass starter does not implement gm.scene.')
    else:
        try:
            example = target.resolve().relative_to((REPO / 'GlassSDK/examples').resolve())
        except ValueError:
            print('Build: python3 GlassSDK/build.py build --project ' + display_target)
            package = target / '.build' / target.name / (target.name + '.gmp')
            print('After building, use Import package for this standalone GMP; keep its matching manifest.json beside it.')
        else:
            print('Build: python3 GlassSDK/build.py build --example ' + shlex.quote(example.as_posix()))
            package = REPO / 'GlassSDK/build-host/.build' / example / (example.name + '.gmp')
            print('After building, import this repository as the Studio workspace and refresh the glasses catalog.')
        print('Expected GMP after build: ' + str(package))
        print('Verify the application appears in the glasses catalog before checking pairing; generation alone does not build it.')
        print('Glasses-only starter: disable the phone component in Studio. Custom pairing requires a matching phone implementation and message handlers.')
    print('This is a runnable starter, not a completed application.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind', choices=['web', 'glass'])
    parser.add_argument('--name', help='new directory slug (lowercase letters, digits and hyphens)')
    parser.add_argument('--id', help='independent plugin identity, for example com.example.focus-timer')
    parser.add_argument('--title', help='application display title')
    parser.add_argument('--output', help='optional project directory instead of SDK/examples/<name>')
    parser.add_argument('--refresh-sdk', metavar='PROJECT', help='refresh only the standalone SDK of an existing Web project')
    args = parser.parse_args()
    try:
        if args.refresh_sdk:
            if any([args.kind, args.name, args.id, args.title, args.output]):
                raise ValueError('--refresh-sdk cannot be combined with creation arguments')
            target = Path(args.refresh_sdk).resolve()
            manifest = json.loads((target / 'manifest.json').read_text(encoding='utf-8'))
            if manifest.get('bridgeVersion') != '2.0':
                raise ValueError('Expected an existing Bridge 2.0 Web project')
            bundle_sdk(target)
            print('Refreshed standalone SDK: ' + str(target / 'vendor/gm-plugin-web-sdk.esm.js'))
            return
        if not all([args.kind, args.name, args.id, args.title]):
            raise ValueError('Creation requires --kind, --name, --id and --title')
        if not re.fullmatch(r'[a-z][a-z0-9]*(?:-[a-z0-9]+)*', args.name):
            raise ValueError('--name must be a lowercase directory slug without paths')
        if not re.fullmatch(r'[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+', args.id):
            raise ValueError('--id must be a lowercase dotted/hyphenated identity')
        create(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
