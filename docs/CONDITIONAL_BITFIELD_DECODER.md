Conditional Bitfield Decoder — Feature Documentation

Date: 20 May 2026
Project: PcapUdpExtractor
Feature version: V5.1

---

## 1. Why the Static Bitfield Decoder Is Not Enough

The existing static bitfield decoder assigns a fixed set of bit rules to a field. Every packet that matches the message definition uses the same rules to decode that field's bits.

This works correctly when a field's bit meaning is always the same. However, in real protocol data, one field's bit meaning can change depending on another field's value in the same payload.

Example problem:

A message contains a Sonar Mode field and a Sonar Sub State field.

When Sonar Mode is 0x01 (Live Mode), the bits of Sonar Sub State mean:
  Bit 0 = SSTT TX off
  Bit 1 = SSTT TX enable
  Bit 2 = Transmitting

When Sonar Mode is 0x02 (Simulation Mode), the same bits of Sonar Sub State mean:
  Bit 0 = Simulation

Using a single static rule set for Sonar Sub State would produce incorrect or meaningless output for rows where Sonar Mode is not Live.

---

## 2. What Conditional Bitfield Decoder Means

The conditional bitfield decoder allows a field (the dependent field) to be decoded using a different set of bit rules depending on the raw value of another field (the controller field) in the same payload.

A set of profiles is defined. Each profile is linked to one specific controller field raw value. When the extraction engine processes a packet, it reads the controller field's raw value, selects the matching profile, and decodes the dependent field's bits using that profile's rules.

If no profile matches the controller value, the output is marked with a configurable unknown label.

---

## 3. Key Concepts

Controller field:
The field whose raw value determines which profile is used to decode the dependent field.

Dependent field:
The field whose bits are decoded conditionally.

Profile:
A named configuration associated with one specific raw value of the controller field.
Each profile contains its own set of BitDecodeRule entries.

Exclusion rule:
An optional per-profile rule that checks whether a set of bits that should be mutually exclusive are all set simultaneously.
If more than one bit in the defined set is set, an invalid message is output in the validation column.

---

## 4. Sonar Mode and Sonar Sub State Example

Message layout:
Length: 30 bytes

Fields:
  Sonar_Mode
    Byte offset: configured by user
    Length: 1 byte
    Resolution: 1

  Sonar_Sub_State
    Byte offset: configured by user
    Length: 1 byte
    Resolution: 1

Sonar Mode raw values:
  0x01 = Live Mode
  0x02 = Simulation Mode
  0x04 = Replay Mode
  0x08 = Intrusive Maintenance Mode

Sonar Sub State conditional decoder:

  Controller field: Sonar_Mode

  Profile 1:
    Controller value: 0x01
    Profile name: Live
    Bit rules:
      Bit 0:  SSTT_TX_OFF
        0 = false
        1 = true
      Bit 1:  SSTT_TX_ENABLE
        0 = false
        1 = true
      Bit 2:  TRANSMITTING
        0 = No Ping
        1 = TX Ping in process
      Bit 3:  RESERVED
        0 = RESERVED
        1 = RESERVED
      Bits 4-7: RESERVED
    Exclusion rule:
      Bits: 0,1
      Validation label: SSTT_TX_state
      Invalid message: SSTT TX off and SSTT TX enable both true

  Profile 2:
    Controller value: 0x02
    Profile name: Simulation
    Bit rules:
      Bit 0:  SIMULATION
        0 = Not simulating
        1 = Simulating
      Bits 1-7: RESERVED

  Profile 3:
    Controller value: 0x04
    Profile name: Replay
    Bit rules:
      Bit 0:  REPLAY
        0 = Not replaying
        1 = Replaying
      Bits 1-7: RESERVED

  Profile 4:
    Controller value: 0x08
    Profile name: Intrusive_Maintenance
    Bit rules:
      Bit 0:  MAINTENANCE_MODE
        0 = Maintenance inactive
        1 = Maintenance active
      Bits 1-7: RESERVED

---

## 5. Configuration Steps

### Step 1: Create Message Definition

Open the application.
Select File Mode.
Import your PCAP or PCAPNG file.
Select Port filter.
Add the port number.
Click Manage Length Filters.
Add a message definition:
  Name: SonarMessage
  Payload Length: 30

### Step 2: Configure Fields

Click Configure Fields for the SonarMessage entry.
Add two fields:

  Sonar_Mode
    Byte Offset: enter the correct byte offset for Sonar Mode
    Length: 1
    Resolution: 1

  Sonar_Sub_State
    Byte Offset: enter the correct byte offset for Sonar Sub State
    Length: 1
    Resolution: 1

Important: do not use 1-based protocol byte numbering directly.
If the protocol documentation says Sonar Mode is at byte 18 using 1-based numbering, the byte offset in this software is 17 (0-based).
Always verify the correct 0-based offset for your protocol.

### Step 3: Configure Conditional Decoder

Select the Sonar_Sub_State row in the field table.
Click Conditional Decoder.
The Conditional Decoder dialog opens.

Select controller field: Sonar_Mode
Select unknown behavior: UNKNOWN_CONTROLLER

Click Add Profile.
Enter controller value: 0x01
Enter profile name: Live
Click Configure Bit Rules.
Add bit decode rules as described in section 4.
Return to profile dialog.
In the Mutual Exclusion Rules section, click Add Rule.
Enter:
  Bits: 0,1
  Validation Label: SSTT_TX_state
  Invalid Message: SSTT TX off and SSTT TX enable both true
Click Save.

Repeat Add Profile for values 0x02, 0x04, 0x08 with their respective profiles.

Click Save in the Conditional Decoder dialog.
The Cond. Decoder column now shows: Yes (4 profiles)

Click Save in the Field Configuration dialog.

### Step 4: Export

Click Start Export.
The application validates fields and message existence.
One CSV file is exported per message definition.

---

## 6. CSV Column Format

The conditional decoder produces a stable set of columns. All columns appear in every exported CSV regardless of which profile matched each row.

Example columns for Sonar_Sub_State with 4 profiles:

  Sonar_Sub_State
  Sonar_Sub_State_Profile
  Sonar_Sub_State_Live_SSTT_TX_OFF
  Sonar_Sub_State_Live_SSTT_TX_ENABLE
  Sonar_Sub_State_Live_TRANSMITTING
  Sonar_Sub_State_Live_RESERVED
  Sonar_Sub_State_Live_Validation_SSTT_TX_state
  Sonar_Sub_State_Simulation_SIMULATION
  Sonar_Sub_State_Simulation_RESERVED
  Sonar_Sub_State_Replay_REPLAY
  Sonar_Sub_State_Replay_RESERVED
  Sonar_Sub_State_Intrusive_Maintenance_MAINTENANCE_MODE
  Sonar_Sub_State_Intrusive_Maintenance_RESERVED

For a row where Sonar Mode = 0x01 (Live):
  Sonar_Sub_State_Profile = Live
  Live profile columns are filled with decoded values.
  Simulation, Replay, Maintenance columns are blank.

For a row where Sonar Mode = 0x02 (Simulation):
  Sonar_Sub_State_Profile = Simulation
  Simulation profile columns are filled.
  All other profile columns are blank.

For a row where Sonar Mode = 0x10 (unknown):
  Sonar_Sub_State_Profile = UNKNOWN_CONTROLLER(0x10)
  All rule columns are blank.

If unknown behavior is set to BLANK:
  Sonar_Sub_State_Profile = (empty)
  All rule columns are blank.

---

## 7. Unknown Controller Behavior

When the controller field's raw value does not match any configured profile, the Profile column output is determined by the Unknown Behavior setting:

UNKNOWN_CONTROLLER (default):
  Profile column = UNKNOWN_CONTROLLER(0xXX)
  where XX is the hex representation of the controller value.

BLANK:
  Profile column = (empty string)

All bit rule columns are always blank when no profile matches.

---

## 8. Validation Rules

Before saving, the following are validated:

Message-level:
  Controller field name cannot be empty.
  Controller field must exist in the same message field list.
  Controller field cannot be the same field as the dependent field.
  Controller field length must be 1 to 8 bytes.
  At least one profile must be defined.
  Controller values must be unique across profiles.
  Profile names must be unique.
  Profile names must be unique after label sanitization.

Profile-level:
  Profile name cannot be empty.
  Bit positions must fit inside the dependent field length.
  Bit rules must pass the existing BitfieldDecoder validation rules.
  Exclusion rules: at least 2 bit positions per rule.
  Exclusion rules: bit positions must be in range 0 to (dependent field length * 8 - 1).
  Exclusion rules: validation label cannot be empty.

Re-validation on field configuration save:
  When the Field Configuration dialog saves, it re-validates all conditional decoders against the final collected field list.
  This catches cases where the controller field was renamed or deleted after the conditional decoder was configured.

---

## 9. Backwards Compatibility

The existing static bitfield decoder is fully preserved.
  hasBitfieldDecoder and bitDecodeRules on FieldDefinition are unchanged.
  BitfieldDecoder, BitfieldDecoderDialog, and BitfieldRuleDialog are unchanged.
  Existing CSV exports using static bitfield rules continue to work exactly as before.

A field can have both a static bitfield decoder and a conditional bitfield decoder at the same time if required.
In that case, the CSV output contains both static decoder columns followed by conditional decoder columns.

---

## 10. Current Limitations

Port and payload length identification only:
  The software matches messages using UDP port plus UDP payload length.
  Two different messages on the same port with the same payload length cannot currently be distinguished.
  Future enhancement: Port plus length plus header bytes, or Port plus message ID byte.

Controller field ordering:
  The extraction engine reads all field raw values in phase 1 before building output.
  The controller field can appear before or after the dependent field in the configured field list.

No partial controller matching:
  If the controller value exactly matches a profile, that profile is used.
  Partial matching, range matching, or bitmask matching are not supported.
  Each profile must be keyed to a specific exact controller value.

Validation column scope:
  Exclusion rule validation (mutually exclusive bits) currently checks whether more than one bit in the defined set is set.
  This covers the binary mutual-exclusion case (at most one bit should be set).
  It does not currently support other cross-rule constraint types.

---

## 11. Manual Test Checklist

Build test:
  Clean project.
  Run qmake.
  Rebuild.
  Confirm no missing ui_*.h errors.
  Launch application.

Field configuration UI test:
  Open Field Configuration dialog.
  Add Sonar_Mode and Sonar_Sub_State fields.
  Select Sonar_Sub_State.
  Click Conditional Decoder.
  Confirm controller field dropdown contains Sonar_Mode but not Sonar_Sub_State.
  Add 4 profiles as described.
  Save. Confirm Cond. Decoder column shows Yes (4 profiles).

Acceptance tests:

Test 1:
  Sonar_Mode = 0x01, Sonar_Sub_State = 0x01
  Expected: Profile = Live, SSTT_TX_OFF = true, SSTT_TX_ENABLE = false, TRANSMITTING = No Ping

Test 2:
  Sonar_Mode = 0x01, Sonar_Sub_State = 0x02
  Expected: Profile = Live, SSTT_TX_OFF = false, SSTT_TX_ENABLE = true

Test 3:
  Sonar_Mode = 0x01, Sonar_Sub_State = 0x04
  Expected: Profile = Live, TRANSMITTING = TX Ping in process

Test 4:
  Sonar_Mode = 0x01, Sonar_Sub_State = 0x03
  Expected: Profile = Live, SSTT_TX_OFF = true, SSTT_TX_ENABLE = true
  Validation column: SSTT TX off and SSTT TX enable both true

Test 5:
  Sonar_Mode = 0x02, Sonar_Sub_State = 0x01
  Expected: Profile = Simulation, SIMULATION = Simulating

Test 6:
  Sonar_Mode = 0x04, Sonar_Sub_State = 0x01
  Expected: Profile = Replay, REPLAY = Replaying

Test 7:
  Sonar_Mode = 0x08, Sonar_Sub_State = 0x01
  Expected: Profile = Intrusive_Maintenance, MAINTENANCE_MODE = Maintenance active

Test 8:
  Sonar_Mode = 0x10, Sonar_Sub_State = 0x01
  Expected: Profile = UNKNOWN_CONTROLLER(0x10), all rule columns blank

Test 9:
  Configure conditional decoder for Sonar_Sub_State with Sonar_Mode as controller.
  Delete Sonar_Mode field.
  Click Save in Field Configuration.
  Expected: Save is blocked. Error message says controller field not found.

Test 10:
  Open an existing message with a static bitfield decoder field.
  Export.
  Confirm static decoder columns appear unchanged.

Test 11:
  Normal numeric field (no decoder).
  Export.
  Confirm raw resolved value still appears correctly.

Test 12:
  Configure port plus length message.
  Start export.
  Confirm port plus length workflow still works.

CSV column stability test:
  Export 50 packets containing all 4 Sonar Mode values.
  Open CSV file.
  Confirm all conditional decoder columns appear in every row.
  Confirm non-matching profile columns are blank (not absent).
  Confirm column count is stable across all rows.
