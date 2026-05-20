# Conditional Bitfield Decoder — Implementation Record

**Branch:** `claude/add-project-documentation-R2qmF`
**Date:** 2026-05-20
**Revision:** 2026-05-20 (post-implementation fixes applied — see Corrections section)

---

## What was built

A generic **Conditional Bitfield Decoder** feature for PcapUdpExtractor. It allows one field's bits to be decoded differently depending on the raw value of another field in the same payload. The existing static bitfield decoder is preserved completely unchanged.

---

## Files created (8 new files)

### 1. `headers/ConditionalBitfieldDecoder.h` + `sources/ConditionalBitfieldDecoder.cpp`

Static utility class — mirrors the existing `BitfieldDecoder` pattern.

| Method | Description |
|---|---|
| `toJson(config)` | Serialises a `ConditionalBitfieldDecoderConfig` to a compact JSON string. Each profile's rules are stored using `BitfieldDecoder::rulesToJson()`. |
| `fromJson(json, config, error)` | Deserialises JSON. Rules are loaded with a lenient field length of 8; `validate()` enforces the real length. |
| `validate(config, allFields, depName, depLength, error)` | Full validation: non-empty controller name, controller ≠ dependent field, controller exists in `allFields`, controller length 1–8 bytes, at least one profile, no duplicate controller values, no duplicate profile names, no duplicate sanitised profile names, each profile's rules pass `BitfieldDecoder::validateRules()`. |
| `columnHeaders(depFieldName, config)` | Returns a stable ordered `QStringList` of CSV column names — `DepField_Profile`, then for every profile in order all its enabled rule columns as `DepField_ProfileName_RuleLabel`. |
| `decode(depBytes, ctrlValue, ctrlFound, config)` | Returns values in the same order as `columnHeaders()`. Only the matching profile's rule columns are filled; all others are empty strings. |

### 2. `headers/ConditionalProfileDialog.h` + `sources/ConditionalProfileDialog.cpp` + `forms/ConditionalProfileDialog.ui`

Dialog for adding or editing one profile. Fields:
- **Controller Value** — accepts `1`, `255`, `0x01`, `0xFF` (parsed with `toULongLong(&ok, 0)`)
- **Profile Name** — free text
- **Configure Bit Rules** button — opens the existing `BitfieldDecoderDialog` unchanged
- Label showing current rule count

On OK: validates controller value is parseable, profile name is non-empty, and rules pass `BitfieldDecoder::validateRules()`.

### 3. `headers/ConditionalBitfieldDecoderDialog.h` + `sources/ConditionalBitfieldDecoderDialog.cpp` + `forms/ConditionalBitfieldDecoderDialog.ui`

Dialog for configuring the full conditional decoder on one dependent field. Features:
- Info label showing dependent field name, byte length, and available bit range
- **Controller Field** combo — populated from all other fields in the same message (dependent field is excluded)
- **Unknown Controller Behavior** combo — `UNKNOWN_CONTROLLER` or `BLANK`
- Profile table (3 columns: Controller Value in hex | Profile Name | Rule count)
- Add / Edit / Remove profile buttons, each opening `ConditionalProfileDialog`
- Save calls `ConditionalBitfieldDecoder::validate()` before accepting

### 4. `docs/CONDITIONAL_BITFIELD_DECODER.md`

User-facing documentation covering:
- Why static bitfield decoding is insufficient for real protocol data
- Controller/dependent field concepts with the Sonar Mode / Sonar Sub State worked example
- Step-by-step configuration walkthrough
- CSV column format and stable-header guarantee
- Unknown controller value behaviour
- All validation rules enforced by the dialogs
- Current limitations (no mutual-exclusion bit validation within a profile)
- 12-test manual test checklist

---

## Files modified (6 existing files)

### `headers/AppTypes.h`

Added two new structs before `FieldDefinition`:

```cpp
struct ConditionalBitDecodeProfile {
    QString profileName;
    quint64 controllerValue;      // default 0
    QList<BitDecodeRule> bitDecodeRules;
};

struct ConditionalBitfieldDecoderConfig {
    QString controllerFieldName;
    QString unknownBehavior;      // default "UNKNOWN_CONTROLLER"
    QList<ConditionalBitDecodeProfile> profiles;
};
```

Extended `FieldDefinition` with two new members (default values ensure zero change to existing data):
```cpp
bool hasConditionalBitfieldDecoder;         // default false
ConditionalBitfieldDecoderConfig conditionalDecoder;
```

### `headers/ExtractionEngine.h` + `sources/ExtractionEngine.cpp`

**New method:**
```cpp
static QStringList columnHeaders(const QList<FieldDefinition>& fields);
```
Produces the CSV column header list in the exact same order and count as `valuesFromPayload()`. For each field: the field name, then (if `hasBitfieldDecoder`) each enabled rule's column, then (if `hasConditionalBitfieldDecoder`) `ConditionalBitfieldDecoder::columnHeaders()`.

**`valuesFromPayload()` converted to two-pass:**
- Pass 1 builds `QMap<QString, quint64> rawValues` and `QMap<QString, bool> fieldValid` for all fields whose byte range is valid in the payload. This ensures the controller field can be looked up even when it appears after the dependent field.
- Pass 2 produces the output row. After the existing static bitfield decoder block, a new conditional decoder block calls `ConditionalBitfieldDecoder::decode()` using the controller value from the pre-built map.

### `forms/FieldConfigurationDialog.ui`

Added `btnConditionalDecoder` ("Conditional Decoder") button to the button row, immediately after the existing `btnBitfieldDecoder`.

### `headers/FieldConfigurationDialog.h`

- Added private slot `void onConditionalDecoderClicked()`
- Added private helper `void setConditionalDecoderCell(int row, const ConditionalBitfieldDecoderConfig& config, bool hasDecoder)`

### `sources/FieldConfigurationDialog.cpp`

| Change | Detail |
|---|---|
| Column count | Changed from 5 to 6; 6th header is "Cond. Decoder" |
| `FIELD_COL_COND_DECODER = 5` | New column constant |
| Includes | Added `ConditionalBitfieldDecoder.h`, `ConditionalBitfieldDecoderDialog.h` |
| `refreshFieldTable()` | Stores conditional decoder JSON at `Qt::UserRole + 1` on the name item; calls `setConditionalDecoderCell()` for every row |
| `onAddFieldClicked()` | Initialises 6th column with `setConditionalDecoderCell(..., false)` |
| `setConditionalDecoderCell()` | New helper: displays "No" or "Yes (N profiles)" with controller field tooltip |
| `onConditionalDecoderClicked()` | New slot: reads field name/length from table, builds `QList<FieldDefinition>` of other fields (for controller dropdown), reads/writes `Qt::UserRole + 1`, opens `ConditionalBitfieldDecoderDialog` |
| `collectFields()` | After reading static decoder JSON (`Qt::UserRole`), also reads `Qt::UserRole + 1`, calls `ConditionalBitfieldDecoder::fromJson()`, populates `field.hasConditionalBitfieldDecoder` and `field.conditionalDecoder` |
| `onSaveClicked()` | After `collectFields()`, runs a second pass that calls `ConditionalBitfieldDecoder::validate()` for every field with a conditional decoder — this catches controller-field-existence errors using the final complete field list |

### `sources/MainWindow.cpp`

| Change | Detail |
|---|---|
| Includes | Added `ConditionalBitfieldDecoder.h`, `QMap` |
| `buildLiveFieldHeaders()` | Replaced manual loop with single call to `ExtractionEngine::columnHeaders(fields)` — headers and values are now guaranteed to stay in sync |
| `fieldStatusText()` | Now also counts and reports fields with `hasConditionalBitfieldDecoder` |
| `validateMessageDefinitions()` | Added block that calls `ConditionalBitfieldDecoder::validate()` for each field with a conditional decoder; uses `message.fields` as the full field list for controller-field-existence check |
| `extractLiveRowValues()` | **Corrected after initial implementation** — replaced the 60-line manual loop with a 10-line implementation that delegates entirely to `ExtractionEngine::valuesFromPayload()`. Short-packet detection is a separate pre-check that sets the flag only; the engine handles all value production including conditional decoders. See Corrections section. |

### `PcapUdpExtractor.pro`

Added to SOURCES, HEADERS, and FORMS:
```
SOURCES += ConditionalBitfieldDecoder.cpp
SOURCES += ConditionalBitfieldDecoderDialog.cpp
SOURCES += ConditionalProfileDialog.cpp

HEADERS += ConditionalBitfieldDecoder.h
HEADERS += ConditionalBitfieldDecoderDialog.h
HEADERS += ConditionalProfileDialog.h

FORMS   += ConditionalBitfieldDecoderDialog.ui
FORMS   += ConditionalProfileDialog.ui
```

---

## Backwards compatibility

| Item | Status |
|---|---|
| `FieldDefinition.hasBitfieldDecoder` + `bitDecodeRules` | Untouched |
| `BitfieldDecoder` class | No changes |
| `BitfieldDecoderDialog` / `BitfieldRuleDialog` | Reused unchanged; `ConditionalProfileDialog` opens `BitfieldDecoderDialog` directly |
| `ExtractionEngine::valueFromPayload()` | Unchanged |
| `valuesFromPayload()` — existing column order | Preserved; conditional output appended after existing output per field |
| All existing CSV exports | Unchanged; `buildLiveFieldHeaders()` now delegates to `ExtractionEngine::columnHeaders()` which produces identical output for fields without a conditional decoder |

---

## Design decisions

**Two-pass extraction** — the controller field may appear after the dependent field in the field list (e.g. field order added by user). Both `ExtractionEngine::valuesFromPayload()` and `extractLiveRowValues()` do a pre-pass to build all raw values before the output pass.

**Column stability** — `columnHeaders()` and `decode()` iterate profiles and rules in identical order. `columnHeaders()` is the single source of truth; `decode()` mirrors its logic without calling it. This avoids per-row recomputation of the header list.

**Lenient JSON load, strict validate** — `fromJson()` uses `fieldLengthBytes=8` when calling `rulesFromJson()` so it can load any stored config without needing the dependent field length at deserialization time. `validate()` always re-validates with the real field length.

**Controller dropdown excludes dependent field** — enforced in `ConditionalBitfieldDecoderDialog` constructor when populating `cmbControllerField`, and again in `validate()`.

**Mutual-exclusion bit validation not implemented** — documented as a known limitation. The Live mode sonar protocol requirement (bits 0 and 1 cannot both be true) cannot currently be expressed in the UI. A future `ConditionalProfileDialog` extension would add a mutual-exclusion rule table.

---

## Key invariant

> `ExtractionEngine::columnHeaders(fields).size()` == `ExtractionEngine::valuesFromPayload(payload, fields).size()`  
> for any payload size (including empty/short — `valueFromPayload` returns `"N/A"`, decode returns empty strings).
