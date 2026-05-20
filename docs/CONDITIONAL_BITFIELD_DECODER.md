# Conditional Bitfield Decoder

## Why the static bitfield decoder is insufficient

The existing **Bitfield Decoder** assigns one fixed set of bit decode rules to a field, regardless of any other value in the same payload. This is adequate when bit meanings are protocol-stable, but real protocols frequently reuse the same byte for different purposes depending on context.

**Example — Sonar Sub State:** In a sonar protocol the byte at offset 3 means completely different things depending on offset 2 (Sonar Mode):

| Sonar Mode value | Sonar Sub State bit meaning |
|---|---|
| `0x01` (Live) | Bits 0–3: TX_OFF / TX_ENABLE / TRANSMITTING / RESERVED |
| `0x02` (Simulation) | Bit 0: SIMULATION active |
| `0x04` (Replay) | Bit 0: REPLAY active |
| `0x08` (Maintenance) | Bit 0: MAINTENANCE_MODE active |

A static decoder could only express one of these interpretations at a time. The Conditional Bitfield Decoder expresses all of them in a single configuration and selects the correct one at extraction time.

## Concepts

**Controller field** — the field whose raw integer value determines which set of bit rules to apply. In the example above, `Sonar_Mode` is the controller field.

**Dependent field** — the field whose bits are decoded conditionally. `Sonar_Sub_State` is the dependent field.

**Profile** — one named set of bit decode rules paired with a specific controller value. Each profile corresponds to one mode or context.

Both fields must be in the same message definition. The controller field may appear before or after the dependent field in the field list — a two-pass extraction algorithm handles both orderings.

## Configuration steps (Sonar example)

1. Open **Message Definitions** and add a 30-byte message on the appropriate port.
2. Add two fields: `Sonar_Mode` (byte offset 2, length 1) and `Sonar_Sub_State` (byte offset 3, length 1).
3. Select `Sonar_Sub_State` → click **Conditional Decoder**.
4. In the **Conditional Bitfield Decoder** dialog:
   - **Controller Field**: select `Sonar_Mode` from the dropdown.
   - **Unknown Controller Behavior**: choose `UNKNOWN_CONTROLLER` or `BLANK`.
5. Click **Add Profile**:
   - **Controller Value**: `0x01`
   - **Profile Name**: `Live`
   - Click **Configure Bit Rules** to open the standard Bitfield Decoder dialog and add rules for the Live mode bits.
6. Repeat step 5 for profiles `Simulation` (value `0x02`), `Replay` (`0x04`), and `Maintenance` (`0x08`).
7. Click **Save**. The 6th column in the field list shows `Yes (4 profiles)`.

## CSV output format

For a dependent field named `Sonar_Sub_State` with profiles `Live` (3 rules), `Simulation` (2 rules), `Replay` (2 rules), and `Maintenance` (2 rules), the CSV column headers are:

```
Sonar_Sub_State
Sonar_Sub_State_Profile
Sonar_Sub_State_Live_SSTT_TX_OFF
Sonar_Sub_State_Live_SSTT_TX_ENABLE
Sonar_Sub_State_Live_TRANSMITTING
Sonar_Sub_State_Simulation_SIMULATION
Sonar_Sub_State_Simulation_RESERVED
Sonar_Sub_State_Replay_REPLAY
Sonar_Sub_State_Replay_RESERVED
Sonar_Sub_State_Maintenance_MAINTENANCE_MODE
Sonar_Sub_State_Maintenance_RESERVED
```

**Stable headers guarantee:** The column list is always identical regardless of which profile matches in any given row. Headers are computed from the stored configuration, not from the data.

**Per-row fill:** Only the matching profile's rule columns contain decoded values. All other profiles' rule columns are blank (`""`).

### Example rows

| Sonar_Mode | Sonar_Sub_State | Sonar_Sub_State_Profile | Live_SSTT_TX_OFF | Live_SSTT_TX_ENABLE | Live_TRANSMITTING | Simulation_SIMULATION | … |
|---|---|---|---|---|---|---|---|
| 1 | 2 | Live | 0 | 1 | 0 | | |
| 2 | 1 | Simulation | | | | active | |
| 16 | 0 | UNKNOWN_CONTROLLER(0x10) | | | | | |

## Unknown controller value behavior

When the decoded value of the controller field does not match any profile's controller value, the `_Profile` column is filled with either:

- `UNKNOWN_CONTROLLER(0xNN)` — the hex value of the unrecognised controller value (default).
- `` (blank) — if **Unknown Controller Behavior** is set to `BLANK`.

All rule columns remain blank when the controller value is unknown.

## Validation rules

The dialog enforces the following before saving:

1. Controller field name is non-empty.
2. Controller field is not the same field as the dependent field.
3. Controller field exists in the message's field list.
4. Controller field length is between 1 and 8 bytes.
5. At least one profile is configured.
6. No two profiles share the same controller value.
7. No two profiles share the same name (case-insensitive).
8. No two profile names produce the same sanitized CSV column prefix (e.g. `Live Mode` and `Live-Mode` both sanitize to `Live_Mode`).
9. Each profile's bit rules pass the same validation as the static Bitfield Decoder: no empty labels, no duplicate labels, no duplicate bit positions, all bit positions within `[0, fieldLength*8-1]`.

`FieldConfigurationDialog::onSaveClicked()` re-validates all conditional decoders against the final field list, so controller-field-existence checks are accurate even if another field was renamed or removed.

`MainWindow::validateMessageDefinitions()` performs a final check at export time.

## Current limitations

- **One message per port+length combination** — if two message definitions share the same UDP port and payload length, only one can be configured. This is a pre-existing constraint unrelated to conditional decoding.
- **No mutual-exclusion bit validation within a profile** — for example, the Live profile cannot currently enforce that bits 0 and 1 are mutually exclusive (only one of TX_OFF / TX_ENABLE can be set simultaneously). This validation would require an additional UI in `ConditionalProfileDialog` and is tracked as a future enhancement.
- **Lenient JSON load** — `ConditionalBitfieldDecoder::fromJson()` loads rules with a permissive field length of 8 bytes to avoid needing the dependent field's actual length at deserialization time. The real length is enforced by `validate()`.

## Manual test checklist

| # | Test | Expected result |
|---|---|---|
| 1 | Configure 4 profiles; start export with packets where controller=0x01 | `_Profile` = `Live`; Live rule columns filled; all other profile columns blank |
| 2 | Packets with controller=0x02 | `_Profile` = `Simulation`; Simulation rule columns filled; others blank |
| 3 | Packets with controller=0x04 | `_Profile` = `Replay`; Replay columns filled |
| 4 | Packets with controller=0x08 | `_Profile` = `Maintenance`; Maintenance columns filled |
| 5 | Packets with controller=0x10 (no matching profile) | `_Profile` = `UNKNOWN_CONTROLLER(0x10)`; all rule columns blank |
| 6 | Set unknown behavior to BLANK; send unmatched controller | `_Profile` column is empty string |
| 7 | CSV headers across 100 rows | All rows have identical column count; no variation |
| 8 | Static bitfield decoder field in same message | Static decoder columns export identically to pre-feature behavior |
| 9 | Field with no conditional decoder | No extra columns emitted |
| 10 | Delete controller field after configuring conditional decoder; try to save | Error: controller field does not exist |
| 11 | Set controller field = dependent field | Error: controller cannot be the same as dependent |
| 12 | Live capture with conditional decoder configured | Live CSV matches port-mode CSV column layout; preview table columns match |
