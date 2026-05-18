# Bitfield Decoder Feature

## Purpose

The Bitfield Decoder feature adds protocol-level decoding for fields where status information is stored inside individual bits or grouped bit ranges.

The old simple bit specification idea only handled this case:

- Bit is `1` -> show one status name
- Bit is `0` -> show blank

That is not enough for real protocol messages. Many messages use both `0` and `1` meanings for a single bit, and many messages combine multiple bits into one logical value.

## Supported Cases

### 1. Single-bit status

Example:

| Bit | Value | Meaning |
|-----|-------|---------|
| 1 | 0 | Data Valid |
| 1 | 1 | Data Invalid |

This is configured as one rule:

- Label: `DATA_VALIDITY`
- Bits: `1`
- Type: `Single Bit`
- Mapping:
  - `0 = Data Valid`
  - `1 = Data Invalid`

### 2. Grouped bit status

Example for one byte, where bits are labeled `7 6 5 4 3 2 1 0`:

| Bits | Label | Binary | Meaning |
|------|-------|--------|---------|
| 0-2 | BITE | 000 | No failure |
| 0-2 | BITE | 001 | Anomaly |
| 0-2 | BITE | 010 | Warning |
| 0-2 | BITE | 011 | FAILURE-AHR(1) DATA NOT VALID |
| 0-2 | BITE | 100 | FAILURE-AHR DATA NOT VALID |
| 3-4 | MODE | 00 | NAVIGATION-SEA |
| 3-4 | MODE | 01 | NAVIGATION-QUAY |
| 3-4 | MODE | 10 | ALIGNMENT |
| 3-4 | MODE | 11 | MAINTENANCE |
| 5-6 | AHR_VALIDITY | 00 | AHR VALID |
| 5-6 | AHR_VALIDITY | 01 | AHR DATA DEGRADED |
| 5-6 | AHR_VALIDITY | 10 | AHR DATA NOT VALID |
| 7 | SPARE | 0 | RESERVED |
| 7 | SPARE | 1 | RESERVED |

This is configured as four rules:

1. `BITE`, bits `0-2`
2. `MODE`, bits `3-4`
3. `AHR_VALIDITY`, bits `5-6`
4. `SPARE`, bit `7`

## Bit Numbering Convention

Bit numbering is LSB-first inside the selected field.

For a 1-byte field:

- Bit 0 = least significant bit = `0x01`
- Bit 7 = most significant bit = `0x80`

For a multi-byte field:

- Bit 0 = least significant bit of byte 0
- Bit 7 = most significant bit of byte 0
- Bit 8 = least significant bit of byte 1
- Bit 15 = most significant bit of byte 1

## Grouped Bit Value Convention

For grouped bits, the first listed bit is treated as the least significant bit of the group.

Example:

Bits: `0,1,2`

If:

- Bit 0 = `1`
- Bit 1 = `0`
- Bit 2 = `1`

Then grouped binary display is:

```text
101
```

The internal integer value is:

```text
1 + 0*2 + 1*4 = 5
```

## How to Use

1. Define a normal payload field in the field table.
2. Select the field row.
3. Click `Bitfield Decoder`.
4. Click `Add Rule`.
5. Enter:
   - Label / output name
   - Bit position or range
   - Rule type
   - Unknown value behavior
   - Binary-to-meaning mappings
6. Save the rule.
7. Save the Bitfield Decoder dialog.
8. Export normally.

The original raw field column is preserved. Extra decoded columns are added after that field.

## CSV Output Format

For a field named `Msg2` with four decode rules:

- `BITE`
- `MODE`
- `AHR_VALIDITY`
- `SPARE`

CSV headers become:

```text
Msg2, Msg2_BITE, Msg2_MODE, Msg2_AHR_VALIDITY, Msg2_SPARE
```

The software does not create unnecessary columns for every bit unless the user creates individual rules.

## Unknown Value Behavior

Each rule has one unknown value behavior:

| Option | Output |
|--------|--------|
| UNKNOWN(binary) | `UNKNOWN(101)` |
| Blank | empty cell |
| Raw Binary | `101` |

Recommended default is `UNKNOWN(binary)` because it exposes unmapped protocol values.

## Reserved / Spare Bits

Reserved or spare bits should be configured as a rule with type:

```text
Reserved / Spare
```

Common mapping:

```text
0 = RESERVED
1 = RESERVED
```

Reserved bits do not block export and are not treated as errors.

## Files Changed

| File | Change |
|------|--------|
| `headers/AppTypes.h` | Added `BitDecodeRule`, `hasBitfieldDecoder`, and `bitDecodeRules` |
| `headers/BitfieldDecoder.h` | New helper for JSON, bit parsing, validation, and decoding |
| `sources/BitfieldDecoder.cpp` | New implementation for bitfield logic |
| `headers/BitfieldRuleDialog.h` | New Add/Edit Rule dialog |
| `sources/BitfieldRuleDialog.cpp` | New rule editor UI and validation |
| `headers/BitfieldDecoderDialog.h` | New main Bitfield Decoder dialog |
| `sources/BitfieldDecoderDialog.cpp` | New dialog for managing rules |
| `headers/ui_MainWindow.h` | Added `Bitfield Decoder` button |
| `headers/MainWindow.h` | Added `onBitfieldDecoderClicked()` slot |
| `sources/MainWindow.cpp` | Wired UI, stored decoder JSON in row data, collected rules into `FieldDefinition`, and added decoded CSV headers |
| `sources/ExtractionEngine.cpp` | Appended decoded values during extraction |
| `PcapUdpExtractor.pro` | Registered new files |

## Testing Checklist

### Basic Build

- [ ] Run qmake
- [ ] Run clean build
- [ ] Confirm zero compile errors

### Existing Behavior

- [ ] Add normal field without decoder
- [ ] Export CSV
- [ ] Confirm old extraction still works
- [ ] Confirm old port/header filtering still works

### Single-bit Test

Field: `Msg1`, length `1`

Rule:

- Label: `DATA_VALIDITY`
- Bits: `1`
- Mapping:
  - `0 = Data Valid`
  - `1 = Data Invalid`

Expected:

| Byte | Output |
|------|--------|
| `0x00` | Data Valid |
| `0x02` | Data Invalid |

### Grouped-bit Test

Field: `Msg2`, length `1`

Rules:

- `BITE`, bits `0-2`
- `MODE`, bits `3-4`
- `AHR_VALIDITY`, bits `5-6`
- `SPARE`, bit `7`

Expected examples:

| Byte | Expected Output |
|------|-----------------|
| `0x00` | BITE=No failure, MODE=NAVIGATION-SEA, AHR_VALIDITY=AHR VALID, SPARE=RESERVED |
| `0x01` | BITE=Anomaly |
| `0x02` | BITE=Warning |
| `0x03` | BITE=FAILURE-AHR(1) DATA NOT VALID |
| `0x04` | BITE=FAILURE-AHR DATA NOT VALID |
| `0x08` | MODE=NAVIGATION-QUAY |
| `0x10` | MODE=ALIGNMENT |
| `0x18` | MODE=MAINTENANCE |
| `0x20` | AHR_VALIDITY=AHR DATA DEGRADED |
| `0x40` | AHR_VALIDITY=AHR DATA NOT VALID |
| `0x80` | SPARE=RESERVED |

### Multi-byte Test

Field length: `2`

Rule:

- Label: `SECOND_BYTE_GROUP`
- Bits: `8-11`

Confirm the rule reads from the second byte.

## Notes

- Decoder configuration is stored as JSON in the field row during the current application session.
- During export, the JSON is parsed into `FieldDefinition.bitDecodeRules` and the extraction engine uses only that model-level data.
- CSV escaping already exists in `CsvExporter`, including commas, quotes, and newlines.
