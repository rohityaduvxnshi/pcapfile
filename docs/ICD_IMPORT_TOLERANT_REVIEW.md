# ICD Import Tolerant Review Rows

This note documents the ICD import change made on branch `claude/practical-mendel-dM2RL` after the multi-table merge work.

## Problem fixed

Previously, the ICD builder skipped an entire field row when any one required-looking property was missing or invalid. Examples:

- no field name
- no byte offset
- byte offset text that could not be parsed
- no data type
- unrecognised data type text
- no length
- invalid length text

That was unsafe for real ICD tables because many Word ICDs contain sparse or imperfect rows. The user should be able to see the row, edit the missing cell, or untick it manually.

## New behaviour

The Build step now preserves reviewable ICD rows even when one property is missing or invalid. Only the affected property is left blank in the Build & review tree.

For example:

| ICD row issue | Review result |
|---|---|
| missing length | field row remains; Length cell is empty |
| invalid length text | field row remains; Length cell is empty and warning is shown |
| unmapped ByteOffset column | field row remains; ByteOffset cell is empty |
| unknown DataType text | field row remains; DataType cell is empty and warning is shown |
| missing field name | field row remains; Name cell is empty |

The final OK step is still strict. A ticked field must be valid before it is imported into the real `MessageDefinition`. The user can fix the empty cells in the review tree or untick the bad rows.

## Files changed

- `headers/IcdImportTypes.h`
  - Added `IcdFieldDraftRow` for editable review-time field rows.
  - `IcdMessageDraft` now carries `fieldRows` beside the existing `MessageDefinition`.
  - Column mapping comments now state that Name, ByteOffset, DataType, and Length can be left unmapped.

- `headers/IcdReviewDraftBuilder.h`
  - New loose review builder API.

- `sources/IcdReviewDraftBuilder.cpp`
  - New builder that converts selected/merged ICD tables into editable review rows.
  - Missing or invalid field properties are converted to empty cells plus warnings, not row skips.
  - Child table offset restart detection still works when ByteOffset is mapped.
  - If ByteOffset is not mapped, child tables are merged without offset auto-adjustment and a warning is shown.

- `sources/IcdImportDialog.cpp`
  - Build now calls `IcdReviewDraftBuilder::buildGroupedDrafts()` instead of the stricter importer builder.
  - Review tree columns now serve both message rows and field rows:
    - Message / Field
    - Port / ByteOffset
    - Payload Len / Length
    - Optional Header / DataType
    - Preview / Resolution
  - Field rows are editable directly in the review tree.
  - OK parses edited field rows into real `FieldDefinition`s and then runs existing `InputValidator::validateFields()`.

- `forms/IcdTableSettingsDialog.ui`
  - Removed the mandatory `*` markers from Name, ByteOffset, and DataType mapping labels.
  - The combo boxes still support `(not mapped)`.

- `PcapUdpExtractor.pro`
  - Added the new builder source/header to qmake.

## Important design rule

This change does not weaken final validation. It only moves invalid-row handling from build-time skipping to review-time editing.

Build step: tolerant, keeps rows visible.

OK step: strict, imports only valid ticked fields.

## Test checklist

1. Import an ICD where one field row has no Length.
   - Expected: row is visible; Length cell is empty; user can edit it.

2. Import an ICD where ByteOffset is not mapped.
   - Expected: rows are visible; ByteOffset cells are empty; warnings mention unmapped ByteOffset.

3. Import an ICD where DataType text is invalid.
   - Expected: row is visible; DataType cell is empty; warning lists accepted labels.

4. Tick an incomplete field and click OK.
   - Expected: import is blocked with a validation message.

5. Fix the empty field cells and click OK again.
   - Expected: valid message imports normally.

6. Untick incomplete rows and click OK.
   - Expected: only ticked valid rows import.

7. Test merged tables where child offsets restart from 0.
   - Expected: offset auto-append still applies when ByteOffset is mapped.

8. Test merged tables where ByteOffset is not mapped.
   - Expected: no crash; warning says offset auto-adjustment was skipped.
