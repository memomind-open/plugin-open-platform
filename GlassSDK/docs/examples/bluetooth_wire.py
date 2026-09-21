#!/usr/bin/env python3
"""Public business-protocol examples. No radio I/O or GMP installer operations.

Python 3.8+. Run --check for offline vectors, --markdown for byte annotations,
or without arguments for complete request hex. Values in JSON are decimal.
"""
import argparse
import json
import struct
import zlib

MAX_LOGICAL = 80 * 1024


def tlv(kind, value):
    value = bytes(value)
    if len(value) > MAX_LOGICAL - 13:
        raise ValueError('TLV exceeds example logical limit')
    return bytes([kind]) + len(value).to_bytes(3, 'big') + value


def uint8(value):
    return tlv(8, bytes([value]))


def json_tlv(value):
    return tlv(5, json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode('utf-8'))


def frames(event, service, command, body=b'', frame_bytes=512):
    """Return complete physical frames, each with its own additive checksum.

    On BLE frame_bytes is the validated characteristic write limit (<=512).
    The caller paces writes/ACKs; this function does not send frames.
    """
    if not 10 <= frame_bytes <= 512:
        raise ValueError('frame_bytes must be 10..512')
    body = bytes(body)
    if len(body) + 9 > MAX_LOGICAL:
        raise ValueError('logical packet too large')
    size = frame_bytes - 9
    chunks = [body[i:i + size] for i in range(0, len(body), size)] or [b'']
    # Avoid the App/firmware historical wrap-boundary difference on small-MTU links.
    if len(chunks) > 239:
        raise ValueError('use smaller business messages or a larger negotiated MTU')
    result = []
    for i, chunk in enumerate(chunks):
        raw = bytes([0xFA if i == 0 else i]) + (len(body) + 9).to_bytes(3, 'big')
        raw += bytes([event, service, command]) + chunk
        result.append(raw + (sum(raw) & 0xFFFF).to_bytes(2, 'big'))
    return result


def parse_frame(packet):
    """Parse one complete frame, NOT an arbitrary socket read."""
    packet = bytes(packet)
    if not 9 <= len(packet) <= 512:
        raise ValueError('invalid physical size')
    logical = int.from_bytes(packet[1:4], 'big')
    if not len(packet) <= logical <= MAX_LOGICAL:
        raise ValueError('invalid logical size')
    if sum(packet[:-2]) & 0xFFFF != int.from_bytes(packet[-2:], 'big'):
        raise ValueError('bad additive checksum')
    if packet[0] != 0xFA and not 1 <= packet[0] <= 239:
        raise ValueError('invalid fragment head')
    return packet[0], logical, packet[4], packet[5], packet[6], packet[7:-2]


def reassemble(packets):
    """Reassemble a supplied sequence of complete, ordered physical frames."""
    if not packets:
        raise ValueError('missing frames')
    body = bytearray()
    identity = None
    for i, packet in enumerate(packets):
        head, logical, event, service, command, chunk = parse_frame(packet)
        if head != (0xFA if i == 0 else i):
            raise ValueError('out-of-order fragment')
        current = logical, event, service, command
        if identity is None:
            identity = current
        if identity != current:
            raise ValueError('fragment identity changed')
        body.extend(chunk)
        if i < len(packets) - 1 and len(body) >= logical - 9:
            raise ValueError('extra fragment after complete message')
    if len(body) != identity[0] - 9:
        raise ValueError('incomplete or oversized logical body')
    return bytes(body)


def parse_tlvs(body):
    result = []
    pos = 0
    while pos < len(body):
        if len(body) - pos < 4:
            raise ValueError('truncated TLV header')
        kind = body[pos]
        size = int.from_bytes(body[pos + 1:pos + 4], 'big')
        pos += 4
        if pos + size > len(body):
            raise ValueError('truncated TLV value')
        value = body[pos:pos + size]
        if kind in {3: 4, 6: 1, 7: 2, 8: 1} and size != {3: 4, 6: 1, 7: 2, 8: 1}[kind]:
            raise ValueError('invalid integer/status width')
        if kind in (2, 5):
            value.decode('utf-8')
        if kind == 5:
            json.loads(value.rstrip(b'\0').decode('utf-8'))
        result.append((kind, value))
        pos += size
    return result


def application_body(channel, payload):
    if not payload:
        raise ValueError('running-plugin message payload must be nonempty')
    return tlv(7, struct.pack('>H', channel)) + tlv(1, payload)


def parse_record_packet(packet, mode):
    """Extract (stream_command, [40-byte Opus packets]) from a complete packet.

    Caller handles 52/11 and 52/13, buffering, session identity and format negotiation.
    These examples do not claim current firmware implements recording over BLE.
    """
    if mode not in (0, 1):
        raise ValueError('negotiate recording mode first')
    if len(packet) < 12 or packet[0] != 0x52 or packet[1] not in range(0x90, 0x95):
        raise ValueError('not an audio packet')
    if packet[2:4] != b'\0\0' or packet[11] != 0xFF or not 1 <= packet[6] <= 8:
        raise ValueError('invalid audio header')
    stride = 49 if mode == 0 else 40
    if len(packet) != 12 + packet[6] * stride:
        raise ValueError('incomplete/wrong-format audio packet')
    result = []
    for i in range(packet[6]):
        start = 12 + i * stride
        if mode == 0:
            if packet[start + 4] != 40:
                raise ValueError('bad legacy frame marker')
            start += 9
        result.append(bytes(packet[start:start + 40]))
    return packet[1], result


def examples():
    # name, service, command, body, semantic note; all data is synthetic.
    items = [
        ('Heartbeat', 1, 1, b'', 'Expect a STATUS response. No plugin required.'),
        ('Device information', 1, 2, b'', 'Expect STRING JSON, not a JSON-type TLV.'),
        ('Device status', 1, 6, b'', 'Expect STRING JSON. Ignore unknown fields.'),
        ('Brightness 5', 1, 14, uint8(5), 'Brightness level 5; expect STATUS.'),
        ('Automatic brightness on', 1, 15, uint8(1), '0 off / 1 on; expect STATUS.'),
        ('HUD vertical position 4', 1, 10, uint8(4), 'Display position level 4, not a pixel coordinate.'),
        ('HUD optical distance 4', 1, 11, uint8(4), 'Optical distance level 4; expect STATUS.'),
        ('Audio ENC and forward pickup', 1, 47, uint8(0) + uint8(0), 'Send on GM command link before raw recording start.'),
        ('Audio raw processing', 1, 47, uint8(1), 'ENC off; pickup omitted for compatibility.'),
        ('Media play', 1, 118, uint8(0), 'Native media control; does not carry audio or establish HFP.'),
        ('Media pause', 1, 118, uint8(1), 'Pause operation is 1 in the handler enum.'),
        ('Media volume 50 percent', 1, 118, uint8(5) + uint8(50), 'Operation 5 followed by volume 50 decimal.'),
        ('Translation start', 3, 1, tlv(2, b'Demo'), 'Open a native translation session; expect STATUS.'),
        ('Translation final text', 3, 4, json_tlv(['Hello', 'Hi']), 'Within an active native translation session: translated text first, source second.'),
        ('Translation stop', 3, 2, b'', 'Close the native translation session.'),
        ('Teleprompter start', 4, 1, json_tlv([600, 350, 3, 1, 1, 2, 0]), 'Example product geometry; manual mode, small text, medium line width, preview scene.'),
        ('Teleprompter text', 4, 5, uint8(1) + json_tlv(['Hello']), 'Initial text content in an active teleprompter session.'),
        ('Teleprompter highlight', 4, 6, json_tlv([0, 5]), 'Text indexes, not GM byte offsets; validate indexing with the renderer.'),
        ('Teleprompter stop', 4, 2, b'', 'Stop native teleprompter session.'),
        ('Notification', 5, 1, uint8(1) + json_tlv({'id': 1, 'a': 'Demo', 'type': 0, 'ts': 0, 'c': 'Hello', 'ti': 'SDK', 'pkg_name': 'example.sdk'}), 'Detailed notification with synthetic timestamp; replace with the documented App-local seconds convention.'),
        ('Screenshot request', 1, 101, b'', 'Receive asynchronous command 66 upload markers/chunks; see PROTOCOL.md screenshot contract.'),
        ('Web Bridge clear', 15, 40, application_body(1, b'\0'), 'Requires running Web Bridge. Clear payload contains a compatibility byte.'),
        ('Web Bridge text Hi', 15, 40, application_body(2, struct.pack('>BHHHHBB', 1, 10, 20, 200, 40, 0, 0) + b'Hi'), 'Object 1; x=10, y=20, width=200, height=40, border/radius=0.'),
        ('Web Bridge rectangle', 15, 40, application_body(3, struct.pack('>BHHHHBB', 2, 10, 20, 200, 40, 1, 0)), 'Object 2; border=1, radius=0.'),
        ('Web Bridge line', 15, 40, application_body(5, struct.pack('>BHHHHB', 3, 0, 0, 100, 50, 1)), 'Object 3; from (0,0) to (100,50), stroke=1.'),
        ('Web Bridge bitmap', 15, 40, application_body(6, struct.pack('>HHHHH', 0, 0, 2, 2, 1) + b'\xf0\x0f'), '2x2 GRAY_4 checkerboard, one byte per row.'),
        ('Web Bridge delete object', 15, 40, application_body(4, b'\x01'), 'Delete object 1.'),
        ('Web Bridge LZ4 bitmap', 15, 40, application_body(7, struct.pack('>HHHHHI', 0, 0, 2, 2, 1, 2) + b'\x20\xf0\x0f'), 'Raw LZ4 block: token 20 means two literal bytes F0 0F; no match or frame header. Requires Host LZ4.'),
        ('Web Bridge frame begin', 15, 40, application_body(8, struct.pack('>IH', 1, 1)), 'Frame ID 1, one tile. Wait for channel 0104 begin status before sending tile.'),
        ('Web Bridge framed LZ4 tile', 15, 40, application_body(9, struct.pack('>IHHHHHHI', 1, 0, 0, 0, 2, 2, 1, 2) + b'\x20\xf0\x0f'), 'Frame ID 1, tile 0, final 2x2 tile. Wait for application frame status, not just delivery ACK.'),
        ('HOGP capabilities', 16, 1, json_tlv({'op': 'query_cap'}), 'Response is JSON capability data; not an ordinary success STATUS.'),
        ('HOGP status', 16, 1, json_tlv({'op': 'query_status'}), 'Response is a JSON state snapshot.'),
        ('HOGP scan start', 16, 1, json_tlv({'op': 'scan_start', 'report': True, 'duration_ms': 10000}), 'ACK first; scan lifecycle and batches arrive asynchronously.'),
        ('HOGP scan stop', 16, 1, json_tlv({'op': 'scan_stop'}), 'Wait for scan-ended event as well as ACK.'),
        ('HOGP connect', 16, 1, json_tlv({'op': 'connect', 'addr': 'AA:BB:CC:DD:EE:FF', 'addr_type': 0}), 'Synthetic address only. Replace with address/type from the same scan.'),
        ('HOGP enable normalized HID events', 16, 1, json_tlv({'op': 'enable_hid_event', 'enable': True, 'raw': False}), 'Enable normalized forwarding only; matches the independent HOGP quick start. Returns JSON, not a STATUS success.'),
        ('HOGP enable HID events', 16, 1, json_tlv({'op': 'enable_hid_event', 'enable': True, 'raw': True}), 'Returns enable/raw/rate_hz JSON. Raw reports are a separate opt-in.'),
        ('HOGP GATT list', 16, 1, json_tlv({'op': 'query_gatt_list'}), 'Requires HID readiness; collect all list fragments from command 05.'),
        ('HOGP descriptor', 16, 1, json_tlv({'op': 'query_hid_desc', 'include_usages': True}), 'Collect command 0A descriptor fragments.'),
        ('HOGP read', 16, 3, json_tlv({'seq': 12, 'op': 'read', 'list_id': 1, 'value_handle': 37}), 'Example handles only. Await GM ACK and separate read_result with seq=12.'),
        ('HOGP write', 16, 3, json_tlv({'seq': 13, 'op': 'write', 'list_id': 1, 'value_handle': 37}) + tlv(1, b'\x01\x02'), 'Two raw write bytes in BYTES TLV. Use current discovered handles and properties.'),
        ('HOGP subscribe', 16, 3, json_tlv({'seq': 14, 'op': 'subscribe', 'list_id': 1, 'value_handle': 37, 'cccd_handle': 38, 'subscribe_type': 'notify'}), 'Glasses configure the accessory CCCD. This is not the phone-facing CCCD.'),
        ('HOGP unsubscribe', 16, 3, json_tlv({'seq': 15, 'op': 'unsubscribe', 'list_id': 1, 'value_handle': 37, 'cccd_handle': 38}), 'Await asynchronous unsubscribe_result.'),
        ('HOGP input map', 16, 11, json_tlv({'version': 1, 'rules': [{'type': 'usage_key', 'page': 12, 'usage': 205, 'key': 10}]}), 'Example maps Consumer Play/Pause usage to logical PLAY_PAUSE. Replaces the whole map.'),
        ('HOGP disconnect', 16, 1, json_tlv({'op': 'disconnect'}), 'Queued disconnect; wait for link state.'),
        ('HOGP unbond', 16, 1, json_tlv({'op': 'unbond'}), 'Removes accessory bond; do not send merely to probe connectivity.'),
        ('HOGP read result', 16, 4, json_tlv({'seq': 12, 'event': 'read_result', 'list_id': 1, 'value_handle': 37, 'att_err': 0}) + tlv(1, b'\x01\x02'), 'Synthetic glasses-to-phone success result. Correlate JSON seq=12, not this illustrative GM event ID.'),
        ('HOGP notification data', 16, 4, json_tlv({'event': 'notify', 'seq': 0, 'list_id': 1, 'value_handle': 37}) + tlv(1, b'\x01'), 'Synthetic unsolicited accessory value. Not a pending read response.'),
        ('HOGP control success', 16, 1, tlv(6, b'\0'), 'Synthetic response to control event 1: queue accepted, not scan/connect/disconnect completion. Do not send this as a request.'),
        ('HOGP scan result', 16, 2, json_tlv({'seq': 0, 'scan_id': 1, 'devices': [{'adv_addr': 'AA:BB:CC:DD:EE:FF', 'adv_addr_type': 0, 'rssi': -48, 'name': 'Demo HID', 'service_uuids': ['1812']}]}), 'Synthetic glasses-to-controller notification. Copy adv_addr and adv_addr_type into connect addr and addr_type. Not a response correlated by GM event ID.'),
        ('HOGP link ready', 16, 7, json_tlv({'state': 2, 'reason': 1, 'err': 0, 'connected': 1, 'bonded': 1, 'hid_ready': 1, 'gatt_ready': 0, 'addr': 'AA:BB:CC:DD:EE:FF', 'addr_type': 0}), 'Synthetic unsolicited HID-ready event. Numeric readiness flags; private GATT is not ready in this example.'),
        ('HOGP link disconnected', 16, 7, json_tlv({'state': 4, 'reason': 5, 'err': 0, 'connected': 0, 'bonded': 1, 'hid_ready': 0, 'gatt_ready': 0, 'addr': 'AA:BB:CC:DD:EE:FF', 'addr_type': 0}), 'Synthetic manual-disconnect event; bond retained. The address may be empty depending on state.'),
        ('HOGP normalized Up press', 16, 6, json_tlv({'type': 'hid_key', 'key': 0, 'phase': 0, 'src': 1, 'axis': 0, 'rid': 1, 'app': 65542, 'val': 1, 'norm': 1, 'x': 0, 'y': 0, 'mod': 0, 'page': 7, 'usage': 82, 'ts': 1000}), 'Synthetic normalized keyboard Up press; key and phase zero are valid. Firmware-generated event, not a command for injecting a key. Release uses phase 1; actual report ID and timestamp depend on the accessory/session.'),
        ('HOGP normalized Up release', 16, 6, json_tlv({'type': 'hid_key', 'key': 0, 'phase': 1, 'src': 1, 'axis': 0, 'rid': 1, 'app': 65542, 'val': 0, 'norm': 0, 'x': 0, 'y': 0, 'mod': 0, 'page': 7, 'usage': 82, 'ts': 1100}), 'Synthetic normalized keyboard Up release matching the press fixture. Do not treat it as a second navigation press or transmit it as a command.'),
        ('HOGP raw HID report', 16, 6, json_tlv({'type': 'hid_raw', 'rid': 1, 'len': 2}) + tlv(1, b'\x01\x00'), 'Synthetic two-byte report. Decode its bits using the actual accessory descriptor.'),
    ]
    return items


def labels_for_body(body, service, command):
    labels = []
    kinds = {1: 'BYTES', 2: 'STRING', 3: 'INT32', 5: 'JSON', 6: 'STATUS', 7: 'INT16', 8: 'INT8'}
    fields = parse_tlvs(body)
    channel = None
    for index, (kind, value) in enumerate(fields):
        if kind == 7 and service == 15 and command in (40, 41):
            channel = int.from_bytes(value, 'big')
        labels.extend(['TLV %d type: %s' % (index + 1, kinds.get(kind, str(kind))),
                       'Value length, high byte', 'Value length, middle byte', 'Value length, low byte (%d bytes)' % len(value)])
        drawing = {
            1: ['Clear compatibility byte'],
            2: ['Object ID', 'X high', 'X low', 'Y high', 'Y low', 'Width high', 'Width low', 'Height high', 'Height low', 'Border width', 'Corner radius'],
            3: ['Object ID', 'X high', 'X low', 'Y high', 'Y low', 'Width high', 'Width low', 'Height high', 'Height low', 'Border width', 'Corner radius'],
            4: ['Object ID'],
            5: ['Object ID', 'X1 high', 'X1 low', 'Y1 high', 'Y1 low', 'X2 high', 'X2 low', 'Y2 high', 'Y2 low', 'Stroke width'],
            6: ['X high', 'X low', 'Y high', 'Y low', 'Width high', 'Width low', 'Height high', 'Height low', 'Stride high', 'Stride low'],
            7: ['X high', 'X low', 'Y high', 'Y low', 'Width high', 'Width low', 'Height high', 'Height low', 'Stride high', 'Stride low', 'Decoded size byte 3', 'Decoded size byte 2', 'Decoded size byte 1', 'Decoded size byte 0', 'LZ4 token: two literals, no match', 'First literal pixel pair', 'Second literal pixel pair'],
            8: ['Frame ID byte 3', 'Frame ID byte 2', 'Frame ID byte 1', 'Frame ID byte 0', 'Tile count high', 'Tile count low'],
            9: ['Frame ID byte 3', 'Frame ID byte 2', 'Frame ID byte 1', 'Frame ID byte 0', 'Tile index high', 'Tile index low', 'X high', 'X low', 'Y high', 'Y low', 'Width high', 'Width low', 'Height high', 'Height low', 'Stride high', 'Stride low', 'Decoded size byte 3', 'Decoded size byte 2', 'Decoded size byte 1', 'Decoded size byte 0', 'LZ4 token: two literals, no match', 'First literal pixel pair', 'Second literal pixel pair'],
        }.get(channel, [])
        for i, byte in enumerate(value):
            if kind in (2, 5):
                label = '%s UTF-8 byte %d: `%s`' % (kinds[kind], i, chr(byte) if 32 <= byte < 127 else '\\x%02X' % byte)
            elif kind == 1 and drawing:
                label = drawing[i] if i < len(drawing) else ('UTF-8 text' if channel == 2 else 'GRAY_4 pixel pair')
            elif kind in (3, 6, 7, 8):
                label = '%s byte %d; whole unsigned value = %d' % (kinds[kind], i, int.from_bytes(value, 'big'))
            else:
                label = 'Application byte %d' % i
            labels.append(label)
    return labels


def markdown():
    out = ['# Annotated Bluetooth business-protocol examples', '',
           'Generated by `python3 GlassSDK/docs/examples/bluetooth_wire.py --markdown`.', '',
           'Every example is a **synthetic encoding vector**, not a captured device response.',
           'All GM examples below use event ID 1 for readability; allocate distinct pending IDs in a real client.',
           'Do not send this entire catalog in sequence. Read each prerequisite and response contract first.',
           'HOGP addresses/handles are placeholders. Some commands change state or remove a bond.',
           'Only the characteristic value is shown, not ATT opcodes, radio headers, or encryption.',
           'Long examples require a sufficient BLE write limit or GM fragmentation; they are not single writes at MTU 23.',
           'See [transport and release limitations](BLUETOOTH_DEVELOPER_GUIDE.md), [audio](AUDIO_PROTOCOL.md),',
           '[HUD](HUD_PROTOCOL.md), and [HOGP](BLE_ACCESSORY_PROTOCOL.md). No GMP installer frames are included.', '',
           '## Index', '']
    rows = examples()
    for name, *_ in rows:
        out.append('- [%s](#%s)' % (name, name.lower().replace(' ', '-')))
    for name, service, command, body, note in rows:
        packet = frames(1, service, command, body)[0]
        assert len(frames(1, service, command, body)) == 1
        labels = ['First physical frame marker', 'Logical length, high byte', 'Logical length, middle byte',
                  'Logical length, low byte (%d total)' % len(packet), 'Event ID = 1',
                  'Service = 0x%02X' % service, 'Command = 0x%02X' % command]
        labels += labels_for_body(body, service, command)
        labels += ['Additive checksum high byte', 'Additive checksum low byte']
        assert len(labels) == len(packet)
        out.extend(['', '## ' + name, '', note, '', '```text', packet.hex(' ').upper(), '```', '',
                    '| Offset (decimal) | Hex byte | Meaning |', '| ---: | --- | --- |'])
        out.extend('| %d | `%02X` | %s |' % (i, byte, labels[i]) for i, byte in enumerate(packet))
    out.extend(['', '## Response examples', '',
                'For brightness event 1, STATUS success (14 bytes) is:', '', '```text',
                frames(1, 1, 14, tlv(6, b'\0'))[0].hex(' ').upper(), '```', '',
                'Offsets 0–6 repeat frame marker, length, event/service/command. Offset 7 is `06` (STATUS);',
                '8–10 are `00 00 01` (one value byte); 11 is `00` (success); 12–13 are the additive sum.', '',
                'For Web Bridge delivery, the normal success response has INT8 status and INT32 reserved next_offset:', '',
                '```text', frames(1, 15, 40, uint8(0) + tlv(3, b'\0' * 4))[0].hex(' ').upper(), '```', '',
                'Offset 7 is INT8; 8–10 encode length 1; 11 is status 0. Offset 12 is INT32;',
                '13–15 encode length 4; 16–19 are next_offset=0; 20–21 are the sum.',
                'Early dispatch failures can instead use generic STATUS; inspect the TLV type before interpreting status.', '',
                '## Raw recording controls', '',
                '| Operation | Complete hex |', '| --- | --- |',
                '| Request legacy | `52 12 00` |', '| Request compact | `52 12 01` |',
                '| Compact response | `52 13 01 31 28 00` |', '| Start default | `52 01 00` |',
                '| Example start notification | `52 11 00 01 02 03` |', '| Stop | `52 00 00` |', '',
                'These have no GM header or checksum. Every byte is explained in [Audio protocol](AUDIO_PROTOCOL.md#recording-control-bytes).',
                'The start-notification timestamp here is synthetic, not a fixed value to expect.', ''])
    return '\n'.join(out)


def checks():
    def rejects(function, *args):
        try:
            function(*args)
        except (ValueError, OverflowError):
            return
        raise AssertionError('malformed input accepted')
    assert frames(1, 1, 14, uint8(5))[0].hex() == 'fa00000e01010e08000001050126'
    assert frames(1, 15, 40, application_body(1, b'ABC'))[0].hex() == 'fa000016010f2807000002000101000003414243021c'
    fragmented = frames(1, 15, 40, application_body(1, b'A' * 600))
    assert [len(p) for p in fragmented] == [512, 116]
    assert [p[-2:].hex() for p in fragmented] == ['7f31', '1bd1']
    for _, service, command, body, _ in examples():
        for limit in (20, 64, 244, 512):
            assert reassemble(frames(1, service, command, body, limit)) == body
        assert b''.join(tlv(k, v) for k, v in parse_tlvs(body)) == body
    rejects(parse_frame, b'\xfa')
    damaged = bytearray(fragmented[0]); damaged[-1] ^= 1
    rejects(parse_frame, damaged)
    rejects(reassemble, fragmented[:1])
    rejects(reassemble, fragmented[::-1])
    rejects(reassemble, fragmented + fragmented[1:])
    rejects(parse_tlvs, bytes.fromhex('08000002ff'))
    rejects(parse_tlvs, bytes.fromhex('080000020001'))
    opus = bytes(range(40))  # Synthetic framing fixture, not valid recorded audio.
    header = bytes.fromhex('5292000009ff0100000000ff')
    legacy = header + b'\0' * 4 + b'\x28' + b'\0' * 4 + opus
    compact = header + opus
    assert parse_record_packet(legacy, 0) == (0x92, [opus])
    assert parse_record_packet(compact, 1) == (0x92, [opus])
    rejects(parse_record_packet, legacy, 1)
    rejects(parse_record_packet, compact[:-1], 1)
    rejects(parse_record_packet, bytes.fromhex('521301312800'), 1)
    assert zlib.crc32(b'123456789') & 0xFFFFFFFF == 0xCBF43926
    notification = next(item for item in examples() if item[0] == 'Notification')
    assert isinstance(json.loads(parse_tlvs(notification[3])[1][1])['id'], int)
    print('PASS: golden GM vectors, %d business vectors at four frame limits, malformed input, legacy/compact extraction, CRC32 check vector' % len(examples()))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--markdown', action='store_true')
    args = parser.parse_args()
    if args.check:
        checks()
    elif args.markdown:
        print(markdown(), end='')
    else:
        for name, service, command, body, _ in examples():
            print(name + ': ' + frames(1, service, command, body)[0].hex(' ').upper())
