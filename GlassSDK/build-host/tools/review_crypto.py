"""Encrypt source archives for reviewers using libsodium sealed boxes.

Public-key encryption provides confidentiality, not author identity or approval.
This module is mirrored in GlassSDK/build-host/tools and Studio/scripts.
"""
import hashlib
import json
import os
from pathlib import Path
import re

ALGORITHM = 'libsodium-sealed-box'
MAX_SOURCE = 32 * 1024 * 1024
DEFAULT_PRIVATE_KEY = Path(__file__).with_name('review_private_key.json')


def _crypto():
    try:
        from nacl.public import PrivateKey, PublicKey, SealedBox
        from nacl.exceptions import CryptoError
    except ImportError as error:
        raise ValueError('Source encryption requires PyNaCl: python3 -m pip install PyNaCl') from error
    return PrivateKey, PublicKey, SealedBox, CryptoError


def _read_key(path, private=False):
    path = Path(path).expanduser()
    if path.stat().st_size > 4096:
        raise ValueError('Review key file exceeds limit')
    record = json.loads(path.read_bytes())
    field = 'private_key' if private else 'public_key'
    if (not isinstance(record, dict) or record.get('format') != 'gm-review-key'
            or type(record.get('version')) is not int or record['version'] != 1
            or not isinstance(record.get(field), str)
            or not re.fullmatch('[0-9a-f]{64}', record[field])):
        raise ValueError('Invalid review key file')
    return bytes.fromhex(record[field])


def encrypt_source(archive, gmp_sha256, public_key=None):
    _, PublicKey, SealedBox, _ = _crypto()
    public_key = public_key or os.environ.get('GM_REVIEW_PUBLIC_KEY') or Path(__file__).with_name('review_public_key.json')
    key = _read_key(public_key)
    # Bind the encrypted payload to the runtime digest as well as the outer JSON.
    plaintext = b'GM-REVIEW-SOURCE\x00' + bytes.fromhex(gmp_sha256) + archive
    ciphertext = SealedBox(PublicKey(key)).encrypt(plaintext)
    if len(ciphertext) > MAX_SOURCE:
        raise ValueError('Encrypted source exceeds 32 MiB')
    return ciphertext, {'algorithm': ALGORITHM, 'key_id': hashlib.sha256(key).hexdigest(),
                        'archive': 'tar.xz'}


def decrypt_source(ciphertext, gmp_sha256, descriptor, private_key=None):
    if (not isinstance(descriptor, dict) or descriptor.get('algorithm') != ALGORITHM
            or descriptor.get('archive') != 'tar.xz'
            or not isinstance(descriptor.get('key_id'), str)
            or not re.fullmatch('[0-9a-f]{64}', descriptor['key_id'])):
        raise ValueError('Invalid source encryption descriptor')
    private_key = private_key or os.environ.get('GM_REVIEW_PRIVATE_KEY')
    if not private_key and DEFAULT_PRIVATE_KEY.is_file():
        private_key = DEFAULT_PRIVATE_KEY
    if not private_key:
        raise ValueError('Encrypted source requires --private-key or GM_REVIEW_PRIVATE_KEY')
    PrivateKey, _, SealedBox, CryptoError = _crypto()
    key = PrivateKey(_read_key(private_key, private=True))
    if hashlib.sha256(bytes(key.public_key)).hexdigest() != descriptor['key_id']:
        raise ValueError('Review private key does not match source key_id')
    try:
        plaintext = SealedBox(key).decrypt(ciphertext)
    except CryptoError as error:
        raise ValueError('Source decryption failed: ciphertext is damaged or key is incorrect') from error
    prefix = b'GM-REVIEW-SOURCE\x00' + bytes.fromhex(gmp_sha256)
    if not plaintext.startswith(prefix):
        raise ValueError('Encrypted source does not match the GMP digest')
    return plaintext[len(prefix):]


def generate_keys(private_path, public_path):
    PrivateKey, _, _, _ = _crypto()
    private_path, public_path = Path(private_path).expanduser(), Path(public_path).expanduser()
    if private_path.exists() or public_path.exists() or private_path.resolve() == public_path.resolve():
        raise ValueError('Key output paths must be distinct and must not exist')
    key = PrivateKey.generate()
    base = {'format': 'gm-review-key', 'version': 1}
    private_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    public_path.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(str(private_path), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as stream:
        json.dump(dict(base, private_key=bytes(key).hex()), stream)
        stream.write('\n')
    with public_path.open('x') as stream:
        json.dump(dict(base, public_key=bytes(key.public_key).hex()), stream)
        stream.write('\n')
