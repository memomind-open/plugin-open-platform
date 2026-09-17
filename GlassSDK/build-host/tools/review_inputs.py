"""Capture the SDK build inputs and commands alongside a completed GMP.

The snapshot is hash-bound to the GMP. It is evidence of the local build inputs,
not a signature or a substitute for a reviewer rebuilding those inputs.
"""
import hashlib
import io
import json
import lzma
import os
from pathlib import Path
import subprocess
import tempfile
import tarfile
from review_crypto import encrypt_source

MAX_SOURCE = 32 * 1024 * 1024
MAX_EXPANDED = 64 * 1024 * 1024


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def review_record(sdk):
    path = Path(sdk).parent / 'build.json'
    if not path.is_file():
        return {}
    record = json.loads(path.read_text())
    return record if isinstance(record, dict) and record.get('format') == 'gm-build-inputs' else {}


def embedded_include_dirs(sdk):
    record = review_record(sdk)
    if record.get('header_mode') != 'embedded-full':
        return []
    names = record.get('system_include_dirs')
    if not isinstance(names, list) or not names or len(names) > 4096:
        raise RuntimeError('Invalid embedded system include directories')
    root = Path(sdk).resolve().parent
    result = []
    for name in names:
        if (not isinstance(name, str) or not name.startswith('toolchain/')
                or any(c in name for c in ('\\', ':', ';'))
                or any(part in ('', '.', '..') for part in name.split('/'))):
            raise RuntimeError('Unsafe embedded system include directory')
        directory = (root / name).resolve()
        if root / 'toolchain' not in directory.parents or not directory.is_dir():
            raise RuntimeError('Missing or unsafe embedded system include directory: ' + name)
        result.append(directory)
    return result


def compiler_include_dirs(compiler):
    # GCC reports include_next search order; preserve it in the portable snapshot.
    environment = dict(os.environ, LC_ALL='C')
    process = subprocess.run([str(compiler), '-E', '-x', 'c', '-v', '-'], input=b'',
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             check=True, env=environment)
    result = []
    inside = False
    for line in process.stderr.decode(errors='replace').splitlines():
        if '#include <...> search starts here:' in line:
            inside = True
        elif 'End of search list.' in line:
            inside = False
        elif inside:
            result.append(Path(line.strip()).resolve())
    if not result:
        raise RuntimeError('Cannot discover GCC system include directories')
    return result


def verify_review_toolchain(sdk, bin_dir):
    record = review_record(sdk)
    if record.get('header_mode') == 'embedded-full':
        embedded_include_dirs(sdk)
        root = Path(sdk).resolve().parent
        for name, checksum in record.get('files', {}).items():
            if not name.startswith('toolchain/'):
                continue
            path = (root / name).resolve()
            if (root / 'toolchain' not in path.parents or not path.is_file()
                    or sha256(path.read_bytes()) != checksum):
                raise RuntimeError('Embedded toolchain dependency is missing or changed: ' + name)
        return
    entries = record.get('external_toolchain_headers', {})
    if not isinstance(entries, dict):
        raise RuntimeError('Invalid review toolchain header list')
    root = Path(bin_dir).resolve().parent
    for relative, entry in entries.items():
        if (not relative.endswith('.h') or '\\' in relative or ':' in relative
                or any(part in ('', '.', '..') for part in relative.split('/'))):
            raise RuntimeError('Unsafe review toolchain header path')
        path = (root / relative).resolve()
        if root not in path.parents or not path.is_file():
            raise RuntimeError('Review toolchain header is unavailable: ' + relative)
        checksum = entry if isinstance(entry, str) else entry.get('sha256')
        size = None if isinstance(entry, str) else entry.get('size')
        if size is not None and path.stat().st_size != size:
            raise RuntimeError('Review toolchain header size mismatch: ' + relative)
        if sha256(path.read_bytes()) != checksum:
            raise RuntimeError('Review toolchain header SHA-256 mismatch: ' + relative)


def capture(sdk, source, gmp, build_dir, ninja, compiler, project_root=None, include_dirs=()):
    sdk, source, gmp, build_dir = [Path(p).resolve() for p in (sdk, source, gmp, build_dir)]
    compiler = Path(compiler).resolve()
    toolchain = compiler.parent.parent
    embedded = embedded_include_dirs(sdk)
    embedded_root = sdk.parent / 'toolchain' if embedded else None
    project_root = Path(project_root).resolve() if project_root is not None else None
    example = source.name if project_root is not None else source.relative_to(sdk / 'examples').as_posix()
    target = 'gm_plugin_' + example.replace('/', '_')
    workspace = 'workspace/' + source.name if project_root == source else 'workspace'

    def portable(path):
        path = Path(path)
        if embedded_root is not None and (path == embedded_root or embedded_root in path.parents):
            relative = path.relative_to(embedded_root).as_posix()
            return 'toolchain' if relative == '.' else 'toolchain/' + relative
        if path == toolchain or toolchain in path.parents:
            relative = path.relative_to(toolchain).as_posix()
            return 'toolchain' if relative == '.' else 'toolchain/' + relative
        if project_root is not None and (path == project_root or project_root in path.parents):
            relative = path.relative_to(project_root).as_posix()
            return workspace if relative == '.' else workspace + '/' + relative
        if path == sdk or sdk in path.parents:
            relative = path.relative_to(sdk).as_posix()
            return 'SDK' if relative == '.' else 'SDK/' + relative
        raise RuntimeError('Dependency outside SDK/toolchain/source root: ' + str(path)
                           + '. Use --source-root to include shared project dependencies.')

    def query(*args):
        return subprocess.check_output([str(ninja), '-C', str(build_dir), '-t', *args], text=True)

    compdb = json.loads(query('compdb'))
    units = [unit for unit in compdb if unit['command'] and (
        unit['output'].replace('\\', '/').startswith('CMakeFiles/' + target + '_objects.dir/')
        or Path(unit['file']).resolve() == sdk / 'build-host/tools/build/abi_check.c')]
    if not units:
        raise RuntimeError('No compilation database entries for review snapshot')
    dependencies = set()
    for unit in units:
        listing = query('deps', unit['output'])
        if '(VALID)' not in listing:
            raise RuntimeError('Missing or stale compiler dependencies; rebuild before packaging')
        for line in listing.splitlines():
            if line.startswith('    '):
                dependency = Path(line[4:])
                dependencies.add((build_dir / dependency).resolve())
    files = {}
    total = 0

    def add(path, name=None):
        nonlocal total
        path = Path(path)
        if path.is_symlink() or not path.is_file():
            raise RuntimeError('Review inputs must be regular files: ' + str(path))
        name = name or portable(path)
        if name in files:
            return
        if path.stat().st_size + total > MAX_EXPANDED or len(files) >= 4095:
            raise RuntimeError('Review source exceeds limits')
        data = path.read_bytes()
        total += len(data)
        if total > MAX_EXPANDED:
            raise RuntimeError('Review source exceeds limits')
        files[name] = data

    # Capture compiler-discovered dependencies, including transitive/system .h files.
    for path in sorted(dependencies):
        add(path)
    for directory in (source, sdk / 'include'):
        for parent, dirs, names in os.walk(directory, followlinks=False):
            dirs[:] = [d for d in dirs if d not in {'.git', '.build', 'build', '__pycache__'}]
            if any((Path(parent) / d).is_symlink() for d in dirs):
                raise RuntimeError('Source directory symbolic links are not supported')
            for name in names:
                path = Path(parent) / name
                if (name == '.env' or name.startswith('.env.')
                        or path.suffix.lower() in {'.key', '.pem', '.gmp', '.zip'}
                        or name.endswith(('.review.json', '.review-source.tar.xz', '.review-source.enc'))):
                    continue
                # Only omit the plugin README when it is not a compiler input.
                if path == source / 'README.md' and path.resolve() not in dependencies:
                    continue
                add(path, 'SDK/' + path.relative_to(sdk).as_posix() if directory == sdk / 'include' else None)
    for relative in ('build.py', 'build-host/tools/gm_build.py', 'build-host/tools/gmp_pack.py', 'build-host/tools/gmp_xip_pack.py',
                     'build-host/tools/gmp_elf.py', 'build-host/tools/xip_compile.py',
                     'build-host/tools/build/gm_plugin_xip.ld',
                     'build-host/tools/review_inputs.py', 'build-host/tools/review_crypto.py',
                     'build-host/tools/review_public_key.json', 'build-host/tools/requirements-review.txt',
                     'build-host/tools/build/CMakeLists.txt',
                     'build-host/tools/build/gm_plugin.ld', 'build-host/tools/build/abi_check.c'):
        add(sdk / relative, 'SDK/' + relative)
    gmp_bytes = gmp.read_bytes()
    system_dirs = [portable(path) for path in (embedded or compiler_include_dirs(compiler))]
    if any(not name.startswith('toolchain/') for name in system_dirs):
        raise RuntimeError('System search path is outside the toolchain; use --include-dir for project headers')
    rebuild = ['python3', 'SDK/build.py', 'build', '--example', example]
    if project_root is not None:
        rebuild = ['python3', 'SDK/build.py', 'build', '--project', portable(source),
                   '--source-root', portable(project_root)]
        for directory in include_dirs:
            rebuild.extend(['--include-dir', portable(directory)])
    build = {
        'format': 'gm-build-inputs', 'version': 1, 'example': example,
        'gmp_sha256': sha256(gmp_bytes),
        'compiler_version': subprocess.check_output([str(compiler), '--version'], text=True).strip(),
        'commands': query('commands', target).splitlines(),
        'working_directory': str(build_dir),
        'rebuild': rebuild,
        'header_mode': 'embedded-full',
        'system_include_dirs': system_dirs,
        'source_directories': system_dirs,
        'note': 'Compiler-read headers are embedded verbatim. Rebuild uses these copies with -nostdinc; no local header restoration is needed. Use the matching compiler and compare the GMP.',
        'files': {name: sha256(data) for name, data in sorted(files.items())},
    }
    if project_root is not None:
        build.pop('example')
        build['project'] = portable(source)
        build['source_root'] = portable(project_root)
        # Preserve even empty search directories: their order is part of the build.
        build['source_directories'] = sorted({portable(project_root), portable(source)}
                                             | {portable(p) for p in include_dirs} | set(system_dirs))
    files['build.json'] = json.dumps(build, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()
    # A shared XZ dictionary compresses repeated SDK/header text across files.
    # All retained source/header bytes are preserved, including system headers.
    out = io.BytesIO()
    with tarfile.open(fileobj=out, mode='w', format=tarfile.PAX_FORMAT) as archive:
        for name, data in sorted(files.items()):
            info = tarfile.TarInfo(name)
            info.size = len(data)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(data))
    content = lzma.compress(out.getvalue(), format=lzma.FORMAT_XZ, preset=6)
    if len(content) > MAX_SOURCE:
        raise RuntimeError('Review source archive exceeds 32 MiB')
    content, encryption = encrypt_source(content, sha256(gmp_bytes))
    metadata = {'format': 'gm-review', 'version': 2,
                'gmp': {'name': gmp.name, 'sha256': sha256(gmp_bytes)},
                'source': {'name': 'source.enc', 'sha256': sha256(content), 'external_headers': False,
                           'encryption': encryption}}
    # Publish metadata last; readers reject any interrupted/mixed snapshot by hash.
    for path, data in ((gmp.with_suffix('.review-source.enc'), content),
                       (gmp.with_suffix('.review.json'), json.dumps(metadata, sort_keys=True).encode())):
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        try:
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)
    return gmp.with_suffix('.review-source.enc')
