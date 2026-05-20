# Field Data-Type Selector

## Purpose

The field extractor previously decoded every selected byte range as an unsigned big-endian integer, then multiplied that value by the Resolution column.

That works for protocols that send scaled integer values, but it gives wrong values for protocols that send native IEEE 754 floating-point values.

Example:

| Payload bytes | Intended type | Correct value | Old unsigned integer value |
|---------------|---------------|---------------|-----------------------------|
| `43 34 00 00` | `float` / IEEE 754 binary32 big-endian | `180` | `1127481344` |

This update adds a per-field Type selector so each field can be decoded using the correct big-endian data type.

## What Changed

### 1. Field definition now stores a data type

File:

- `headers/AppTypes.h`

Added:

- `FieldDataType` enum
- `FieldDefinition::dataType`
- fixed-width helper for typed fields

Supported types:

| UI Label | Internal type | Length |
|----------|---------------|--------|
| Raw Unsigned BE | `RawUnsignedBE` | editable, 1 to 8 bytes |
| bool | `Bool` | 1 byte |
| uchar | `Uint8` | 1 byte |
| char | `Int8` | 1 byte |
| ushort | `Uint16` | 2 bytes |
| short | `Int16` | 2 bytes |
| uint | `Uint32` | 4 bytes |
| int | `Int32` | 4 bytes |
| ulong | `Uint64` | 8 bytes |
| long | `Int64` | 8 bytes |
| float | `Float32` | 4 bytes |
| double | `Float64` | 8 bytes |

Default type is `RawUnsignedBE`, so existing field configurations keep the old behavior.

### 2. Extraction engine now branches by type

File:

- `sources/ExtractionEngine.cpp`

Behavior:

- `RawUnsignedBE`, `uchar`, `ushort`, `uint`, `ulong`: decoded as unsigned big-endian integers.
- `char`, `short`, `int`, `long`: decoded as signed big-endian integers using sign extension.
- `float`: decoded as IEEE 754 binary32 big-endian.
- `double`: decoded as IEEE 754 binary64 big-endian.
- `bool`: decoded from 1 byte as `false` for `0`, `true` for any nonzero value.

Resolution behavior:

- blank Resolution means `1.0`
- `1.0` means no scaling
- any other positive resolution multiplies the decoded numeric value

The extraction engine continues to use `byteOffsetcorrect`, which keeps the user-visible byte offset as 1-indexed.

### 3. Field Configuration dialog has a Type column

Files:

- `sources/FieldConfigurationDialog.cpp`
- `headers/FieldConfigurationDialog.h`

The field table now has this column order:

| Column | Meaning |
|--------|---------|
| Field Name | output column name |
| Byte Offset | user-entered byte offset |
| Length | byte count |
| Type | data type selector |
| Resolution | optional scale expression |
| Bit Decoder | static bit decoder status |
| Cond. Decoder | conditional bit decoder status |

When a fixed-width type is selected, Length is automatically set and made read-only.

Examples:

- selecting `float` sets Length to `4`
- selecting `double` sets Length to `8`
- selecting `bool` sets Length to `1`
- switching back to `Raw Unsigned BE` makes Length editable again

### 4. Resolution can be blank

File:

- `sources/InputValidator.cpp`

Blank Resolution is now treated as `1.0`.

This allows native fields such as `float` and `double` to be configured without a meaningless scale expression.

### 5. Type-specific length validation was added

File:

- `sources/InputValidator.cpp`

Fixed-width types must match their natural size.

Examples:

- `float` requires Length `4`
- `double` requires Length `8`
- `short` requires Length `2`
- `bool` requires Length `1`

`Raw Unsigned BE` still supports editable lengths from `1` to `8`.

## What Is Working

- Existing unsigned integer fields continue to use the legacy behavior when Type is `Raw Unsigned BE`.
- Big-endian integer decoding is supported for unsigned and signed 8/16/32/64-bit values.
- Big-endian `float` and `double` decoding is implemented.
- `bool` decoding is implemented as a 1-byte field.
- Blank Resolution is accepted and stored as `1.0`.
- Fixed-width type selection auto-updates the Length cell.
- Fixed-width Length cells are locked in the UI.
- The conditional bitfield decoder raw-value lookup still uses the raw unsigned byte value, so controller matching behavior is preserved.
- Static and conditional bitfield decoders continue to operate on the selected raw byte slice.
- Payload bounds checks now use `byteOffsetcorrect`, so a user-visible field ending on the last payload byte is accepted correctly.

## What Is Not Working / Not Verified

- Build verification could not be completed in this environment because the generated makefile points to a missing Qt kit path:

```text
C:\Qt\6.11.0\mingw_64
```

- `qmake.exe` was not found in the available local Qt folders, so the project could not be rebuilt here.
- Runtime UI verification was not performed because the app could not be built/launched from this environment.
- Little-endian decoding is not implemented. All new typed decoders are big-endian only.
- Multi-byte bool is not supported. `bool` is intentionally fixed at 1 byte.
- Byte Offset `0` is still accepted by the generic field validator for compatibility with the older UI behavior, but it becomes invalid for the corrected 1-indexed extraction path because `byteOffsetcorrect` becomes `-1`.

## Expected Manual Verification

After the correct Qt kit is available, verify these cases:

1. Float happy path:
   - Payload bytes at the configured field offset: `43 34 00 00`
   - Type: `float`
   - Length: `4`
   - Resolution: blank
   - Expected output: `180`

2. Legacy unsigned regression:
   - Type: `Raw Unsigned BE`
   - Same field settings as before this change
   - Expected output: unchanged from the previous version

3. Signed integer wrap:
   - `FF FF` as `short` should output `-1`
   - `80 00` as `short` should output `-32768`

4. Resolution with float:
   - `43 34 00 00` as `float`
   - Resolution: `2`
   - Expected output: `360`

5. Bool:
   - `00` as `bool` should output `false`
   - `01` or any nonzero byte as `bool` should output `true`

6. Length locking:
   - selecting `double` should set Length to `8` and make it read-only
   - switching back to `Raw Unsigned BE` should make Length editable again
