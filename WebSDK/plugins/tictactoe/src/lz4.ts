// Raw LZ4 block encoder matching GMPluginPhoneApp PluginPackage.compressLz4.
const MATCH_FIND_LIMIT = 12;
const LAST_LITERALS = 5;

export function compressLz4(input: Uint8Array): Uint8Array {
  const output: number[] = [];
  const table = new Int32Array(1 << 16);
  table.fill(-1);
  let anchor = 0;
  let cursor = 0;

  const hashAt = (position: number): number => {
    const value = (
      input[position]
      | (input[position + 1] << 8)
      | (input[position + 2] << 16)
      | (input[position + 3] << 24)
    ) >>> 0;
    return (Math.imul(value, 2654435761) >>> 16) & 0xffff;
  };

  const appendLength = (value: number): void => {
    while (value >= 255) {
      output.push(255);
      value -= 255;
    }
    output.push(value);
  };

  while (cursor + MATCH_FIND_LIMIT <= input.length) {
    const hash = hashAt(cursor);
    const match = table[hash];
    table[hash] = cursor;
    if (
      match < 0
      || cursor - match > 0xffff
      || input[match] !== input[cursor]
      || input[match + 1] !== input[cursor + 1]
      || input[match + 2] !== input[cursor + 2]
      || input[match + 3] !== input[cursor + 3]
    ) {
      cursor += 1;
      continue;
    }

    let matchLength = 4;
    while (
      cursor + matchLength < input.length - LAST_LITERALS
      && input[match + matchLength] === input[cursor + matchLength]
    ) {
      matchLength += 1;
    }
    const literals = cursor - anchor;
    const encodedMatch = matchLength - 4;
    output.push(
      (Math.min(literals, 15) << 4) | Math.min(encodedMatch, 15),
    );
    if (literals >= 15) appendLength(literals - 15);
    output.push(...input.subarray(anchor, cursor));
    const distance = cursor - match;
    output.push(distance & 0xff, distance >>> 8);
    if (encodedMatch >= 15) appendLength(encodedMatch - 15);
    cursor += matchLength;
    anchor = cursor;
  }

  const literals = input.length - anchor;
  output.push(Math.min(literals, 15) << 4);
  if (literals >= 15) appendLength(literals - 15);
  output.push(...input.subarray(anchor));
  return Uint8Array.from(output);
}
