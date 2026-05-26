# Editing the PcapUdpExtractor JSON Files by Hand

This guide is for the workflow:

1. **Import CSV** with field definitions into `FieldConfigurationDialog`.
2. Click **Export JSON…** — get a `fields.json` containing those fields with empty (`null`) decoder slots.
3. **Hand-edit `fields.json`** in any text editor (VS Code, Notepad++, Sublime, Vim) to fill in the bit decoders and conditional decoders.
4. Click **Import JSON…** — fields, bit rules, and conditional rules all load into the dialog at once.

You can also hand-edit the **project sidecar** (`<pcap-name>.pcproj.json`) produced by **File → Save Project**. The two file formats are slightly different — both are documented below.

> **Always make a backup of the JSON file before hand-editing.** The app validates on import; an invalid file will be rejected. A backup means you can recover quickly from a typo.

---

## 1. Two JSON file formats — when each appears

| File | Produced by | Scope | Top-level shape |
|---|---|---|---|
| **`fields.json`** | *Field Configuration → Export JSON…* | One message's fields | `{ "version": 1, "kind": "PcapUdpExtractorFieldList", "fields": [ … ] }` |
| **`<pcap>.pcproj.json`** | *File → Save Project* (Ctrl+S) | The entire app session: filters, all messages, header fields, live fields | `{ "version": 1, "appVersion": "…", "filterConfig": {…}, "portMessages": [ … ], "headerFields": [ … ], "live": {…} }` |

The **field object structure** (described in §3 below) is identical in both files. So **once you learn how to write a `bitfieldDecoder` for one field, you can do it in either file.**

---

## 2. Where to open and edit

- The exported JSON file is at whatever path you chose in the *Export JSON…* dialog.
- The project sidecar lives next to your `.pcap` file as `<basename>.pcproj.json`, OR — if your pcap folder is read-only — under `%APPDATA%\PcapUdpExtractor\` (Windows).
- Open it in a text editor with **JSON syntax highlighting**. VS Code, Sublime Text, Notepad++ with a JSON plugin, or even `code .` from any folder all work.
- After editing, save the file as **UTF-8 without BOM**, with normal `LF` or `CRLF` line endings — both are fine.

---

## 3. Field object structure

This is what one entry in the `fields` array looks like:

```json
{
  "name": "Status",
  "byteOffset": 1,
  "byteOffsetCorrect": 0,
  "length": 1,
  "dataType": "Uint8",
  "resolution": 1.0,
  "resolutionExpression": "1",
  "bitfieldDecoder": null,
  "conditionalDecoder": null
}
```

| Key | Type | Notes |
|---|---|---|
| `name` | string | Required. Non-empty. Used as the CSV column header in the extracted output. |
| `byteOffset` | int | Required. **1-based** offset into the UDP payload (matches the dialog). The very first byte is `1`. |
| `byteOffsetCorrect` | int | Optional. **0-based** copy of `byteOffset - 1`. If you change `byteOffset`, also change this (or just remove it — the app will recompute). |
| `length` | int | Required. Byte length of the field. For fixed-size types the value is enforced (e.g. `Uint16` ⇒ `length` must be `2`). For `RawUnsignedBE` it is freely user-defined. |
| `dataType` | string | Required. One of: `RawUnsignedBE`, `Bool`, `Uint8`, `Int8`, `Uint16`, `Int16`, `Uint32`, `Int32`, `Uint64`, `Int64`, `Float32`, `Float64`, `String`. Case-sensitive. `String` is UTF-8 text, `length` may exceed 8; all other types are capped to 8 bytes. |
| `resolution` | number | Numeric scale applied to the raw value. Default `1.0`. |
| `resolutionExpression` | string | Human-readable formula (e.g. `"raw*0.01"`, `"raw/256"`). The app evaluates this. Default `"1"`. |
| `bitfieldDecoder` | object \| null | The bit-decoding rules for this field — see §4. `null` means "no bit decoding." |
| `conditionalDecoder` | object \| null | Conditional bit-decoding driven by another field's value — see §5. `null` means "no conditional decoding." |

---

## 4. Adding a bitfield decoder by hand

A `bitfieldDecoder` object has one key, `rules`, holding an array. Each rule decodes a contiguous (or non-contiguous) set of bits into a meaningful label.

### 4.1 Minimum example — a single 1-byte flag

A `Uint8` field where bit 0 means "Power" (on/off) and bit 1 means "Heater" (on/off):

```json
{
  "name": "FlagsByte",
  "byteOffset": 1,
  "length": 1,
  "dataType": "Uint8",
  "resolution": 1.0,
  "resolutionExpression": "1",
  "bitfieldDecoder": {
    "rules": [
      {
        "label": "Power",
        "type": "SINGLE_BIT",
        "bits": [0],
        "reserved": false,
        "enabled": true,
        "unknownBehavior": "UNKNOWN",
        "mappings": [
          { "value": "0", "binary": "0", "meaning": "Off" },
          { "value": "1", "binary": "1", "meaning": "On" }
        ]
      },
      {
        "label": "Heater",
        "type": "SINGLE_BIT",
        "bits": [1],
        "reserved": false,
        "enabled": true,
        "unknownBehavior": "UNKNOWN",
        "mappings": [
          { "value": "0", "binary": "0", "meaning": "Off" },
          { "value": "1", "binary": "1", "meaning": "On" }
        ]
      }
    ]
  },
  "conditionalDecoder": null
}
```

### 4.2 Grouped bits — a 3-bit "Mode" code

Same byte, bits 2-4 together encode a mode (8 possible values):

```json
{
  "label": "Mode",
  "type": "GROUPED_BITS",
  "bits": [2, 3, 4],
  "reserved": false,
  "enabled": true,
  "unknownBehavior": "UNKNOWN",
  "mappings": [
    { "value": "0", "binary": "000", "meaning": "Idle" },
    { "value": "1", "binary": "001", "meaning": "Boot" },
    { "value": "2", "binary": "010", "meaning": "Run" },
    { "value": "3", "binary": "011", "meaning": "Pause" },
    { "value": "4", "binary": "100", "meaning": "Fault" }
  ]
}
```

Add this object to the same `rules: []` array, alongside the `Power` and `Heater` rules.

### 4.3 Reserved (unused) bits

If bits 5-7 are reserved and should never be touched by the decoder:

```json
{
  "label": "ReservedBlock",
  "type": "RESERVED",
  "bits": [5, 6, 7],
  "reserved": true,
  "enabled": false,
  "unknownBehavior": "UNKNOWN",
  "mappings": []
}
```

A reserved rule still must declare its bits (so the validator knows they aren't used by another rule), but `mappings` can be empty.

### 4.4 Rule keys explained

| Key | Type | Notes |
|---|---|---|
| `label` | string | Required, unique within the field. Becomes a column suffix in the extracted CSV (e.g. `FlagsByte_Power`). |
| `type` | string | One of `SINGLE_BIT`, `GROUPED_BITS`, `RESERVED`. Cosmetic + drives a few UI hints; if `reserved: true` the type is `RESERVED`. If `bits` has one entry the type is `SINGLE_BIT`. Otherwise `GROUPED_BITS`. |
| `bits` | array of int | Required. **Bit indices** within the field, where bit 0 is the LSB of the **first** byte. For a 2-byte (Uint16) field, valid bits are `0`–`15`. For 4 bytes, `0`–`31`. Bits do not have to be contiguous (`[0, 3, 5]` is fine), but inside one rule they cannot duplicate, and across rules in the same field a bit may only be used once. |
| `reserved` | bool | `true` for placeholder rules covering unused bits. `false` for decoded rules. |
| `enabled` | bool | If `false`, the rule is skipped during validation and decoding. Useful for temporarily turning a rule off without deleting it. |
| `unknownBehavior` | string | What to print in the output CSV when the decoded value has no mapping entry. One of: `"UNKNOWN"` (writes the word "UNKNOWN"), `"BLANK"` (writes nothing), `"RAW_BINARY"` (writes the raw bit string like `010`). |
| `mappings` | array | A list of `{ value, binary, meaning }` objects. See below. |

### 4.5 Mapping objects

Each mapping translates one numeric value of the bit group into a human-readable string:

```json
{ "value": "5", "binary": "101", "meaning": "Calibration" }
```

| Key | Type | Notes |
|---|---|---|
| `value` | string of decimal digits | The decimal value of the bit pattern. **Must be a string** (JSON), but contain only digits. The app parses it as a `quint64`. |
| `binary` | string of `0`/`1` | Optional but **highly recommended for clarity**. Length must equal the number of bits in this rule. If both `value` and `binary` are present and disagree, the importer treats `binary` as authoritative. |
| `meaning` | string | What appears in the extracted CSV when this pattern is decoded. |

**Rule:** for a rule covering N bits, `value` may range from `0` to `(2^N) - 1` inclusive. A 3-bit rule allows `0`–`7`; a 16-bit rule allows up to `65535`. The validator rejects values out of range.

### 4.6 Bit position syntax — the difference between the CSV and JSON columns

- In the **CSV** (`Bits` column), you may write `0-2` (range), `0;1;2` (list), `3` (single). Semicolons are used because comma is the CSV separator.
- In the **JSON**, you always write a plain JSON array of integers: `[0, 1, 2]`. No ranges, no special syntax.

---

## 5. Adding a conditional bitfield decoder

A `conditionalDecoder` makes the bit-decoding depend on the value of *another field* (called the **controller field**). When the controller's raw value matches a `controllerValue`, the corresponding profile's rules are applied.

### 5.1 Structure

```json
"conditionalDecoder": {
  "controllerFieldName": "ModeByte",
  "unknownBehavior": "UNKNOWN_CONTROLLER",
  "profiles": [
    {
      "profileName": "FlightMode",
      "controllerValue": 1,
      "bitDecodeRules": [
        { "label": "GearDown", "type": "SINGLE_BIT", "bits": [0], "reserved": false, "enabled": true, "unknownBehavior": "UNKNOWN",
          "mappings": [ { "value": "0", "binary": "0", "meaning": "Up" }, { "value": "1", "binary": "1", "meaning": "Down" } ] },
        { "label": "FlapsPos", "type": "GROUPED_BITS", "bits": [1, 2], "reserved": false, "enabled": true, "unknownBehavior": "UNKNOWN",
          "mappings": [
            { "value": "0", "binary": "00", "meaning": "Retracted" },
            { "value": "1", "binary": "01", "meaning": "Approach" },
            { "value": "2", "binary": "10", "meaning": "Landing" }
          ] }
      ],
      "exclusionRules": []
    },
    {
      "profileName": "GroundMode",
      "controllerValue": 2,
      "bitDecodeRules": [
        { "label": "ParkingBrake", "type": "SINGLE_BIT", "bits": [0], "reserved": false, "enabled": true, "unknownBehavior": "UNKNOWN",
          "mappings": [ { "value": "0", "binary": "0", "meaning": "Released" }, { "value": "1", "binary": "1", "meaning": "Set" } ] }
      ],
      "exclusionRules": []
    }
  ]
}
```

### 5.2 Keys

| Key | Type | Notes |
|---|---|---|
| `controllerFieldName` | string | Required. Must match the `name` of another field in the same message. |
| `unknownBehavior` | string | `"UNKNOWN_CONTROLLER"` (write "UNKNOWN_CONTROLLER") or `"BLANK"`. |
| `profiles` | array | One entry per controller-value branch. |

Each profile:

| Key | Type | Notes |
|---|---|---|
| `profileName` | string | Free-text. Becomes part of CSV column headers. |
| `controllerValue` | number (integer) | The controller field's decoded numeric value that triggers this profile. |
| `bitDecodeRules` | array | Same shape as the rules in §4. They decode the *current* field, not the controller. |
| `exclusionRules` | array | Optional mutual-exclusivity checks. Each entry has `mutuallyExclusiveBits: [bit, bit, …]`, `validationLabel`, `invalidMessage`. Used to flag illegal bit combinations at decode time. |

### 5.3 Exclusion rule example

To declare that bits 0, 1, and 2 of this field must never have more than one set simultaneously:

```json
"exclusionRules": [
  {
    "mutuallyExclusiveBits": [0, 1, 2],
    "validationLabel": "FlapsConflict",
    "invalidMessage": "Multiple flap positions reported simultaneously"
  }
]
```

---

## 6. Putting it together — a complete `fields.json`

```json
{
  "version": 1,
  "kind": "PcapUdpExtractorFieldList",
  "exportedAt": "2026-05-21T14:32:11Z",
  "fields": [
    {
      "name": "ModeByte",
      "byteOffset": 1,
      "length": 1,
      "dataType": "Uint8",
      "resolution": 1.0,
      "resolutionExpression": "1",
      "bitfieldDecoder": null,
      "conditionalDecoder": null
    },
    {
      "name": "StatusByte",
      "byteOffset": 2,
      "length": 1,
      "dataType": "Uint8",
      "resolution": 1.0,
      "resolutionExpression": "1",
      "bitfieldDecoder": {
        "rules": [
          { "label": "Power",  "type": "SINGLE_BIT",   "bits": [0],       "reserved": false, "enabled": true, "unknownBehavior": "UNKNOWN",
            "mappings": [ { "value": "0", "binary": "0", "meaning": "Off" }, { "value": "1", "binary": "1", "meaning": "On" } ] },
          { "label": "Heater", "type": "SINGLE_BIT",   "bits": [1],       "reserved": false, "enabled": true, "unknownBehavior": "UNKNOWN",
            "mappings": [ { "value": "0", "binary": "0", "meaning": "Off" }, { "value": "1", "binary": "1", "meaning": "On" } ] },
          { "label": "Mode",   "type": "GROUPED_BITS", "bits": [2, 3, 4], "reserved": false, "enabled": true, "unknownBehavior": "UNKNOWN",
            "mappings": [
              { "value": "0", "binary": "000", "meaning": "Idle" },
              { "value": "1", "binary": "001", "meaning": "Boot" },
              { "value": "2", "binary": "010", "meaning": "Run" }
            ] },
          { "label": "Reserved", "type": "RESERVED", "bits": [5, 6, 7], "reserved": true, "enabled": false, "unknownBehavior": "UNKNOWN", "mappings": [] }
        ]
      },
      "conditionalDecoder": null
    }
  ]
}
```

Save → click **Import JSON…** in the dialog → both fields appear, the second one already wired up with three decode rules + one reserved block.

---

## 7. Common mistakes (and what the app will tell you)

| Mistake | Symptom |
|---|---|
| Bit index out of range (e.g. `[8]` on a 1-byte field) | "Decode rule X uses invalid bit position 8. Valid range is 0–7." |
| Same bit used by two rules in the same field | "Bit position N is used by more than one decode rule." |
| Two rules with the same `label` | "Duplicate decode rule label: X" |
| `value` greater than `(2^N) - 1` for an N-bit rule | "Decode rule X has mapping value M outside N-bit range." |
| `value` is a JSON number instead of a string | Parses but may overflow silently for large values — **always quote `value` as a string**. |
| `binary` length doesn't match bit count | "Binary 'XXX' length N does not match bit count M." |
| Trailing comma after the last array element | JSON parse error pointing at the offending offset. |
| Missing comma between adjacent objects | JSON parse error. |
| Forgetting `"mappings": []` in a reserved rule | Parses fine — empty mappings are explicitly allowed for reserved rules. |

When the importer reports an error it lists every problem at once — no need to re-run import per bad row.

---

## 8. Quick reference — JSON skeleton you can copy

```json
{
  "label": "MyLabel",
  "type": "GROUPED_BITS",
  "bits": [0, 1, 2, 3],
  "reserved": false,
  "enabled": true,
  "unknownBehavior": "UNKNOWN",
  "mappings": [
    { "value": "0",  "binary": "0000", "meaning": "Zero" },
    { "value": "1",  "binary": "0001", "meaning": "One"  },
    { "value": "15", "binary": "1111", "meaning": "Max"  }
  ]
}
```

Drop this object into any field's `bitfieldDecoder.rules` array, change the bits / meanings to match your ICD, save the file, click **Import JSON…**.

---

## 9. Round-trip rules of thumb

- **Export → Edit → Import** is the supported flow. Re-importing a file the app exported preserves every field and decoder byte-for-byte (modulo whitespace and key order, which the JSON parser is indifferent to).
- The project sidecar (`<pcap>.pcproj.json`) stores bit/conditional decoders **as escaped JSON strings** for backward compatibility with the internal API. The hand-editable `fields.json` (Export JSON in the field dialog) stores them as **nested objects**. The Import path on both accepts either form, so you can paste a decoder object from one file into the other and it will work.
- **Validation runs on every import** — the same gate the manual *Save* button uses inside the bit decoder dialog. An invalid file is rejected with the full error list; the dialog's current state is not touched.
