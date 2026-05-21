 Single-Shot Optimization Plan — Zero Behavior Change

## Context

The PcapUdpExtractor application slows down noticeably on captures with many configured messages and/or large UDP payloads. A previous Codex pass already trimmed three dead-file headers and added a "Verify all configured messages before export" checkbox (Option C). What remains is a set of internal hot-path inefficiencies that can be fixed in a single PR without changing **any** externally observable behavior — same CSV bytes, same column order, same error messages, same preview rows, same UI state transitions, same validation rules. The goal is to maximize speed on big captures while making the change reviewable as one unit.

## Non-Negotiable Behavioral Contract

Every change in this plan MUST preserve, byte-for-byte:

1. CSV file content (cells, escaping, formula-protection prefix, line endings).
2. Column order and column count for both modes and live capture.
3. All `QMessageBox` text, status-bar text, dialog titles.
4. Preview table content and order (offline + live).
5. Match semantics: port mode uses `(sourcePort == port || destPort == port) && payloadLength == messageLength`; header mode uses `commonPort` + prefix `left(headerSize) == header`.
6. Field validation results (including all field-length/decoder validations).
7. The 500 MB capture limit, the verify-messages checkbox, all dialog flows.
8. Public APIs of `UdpPacketParser`, `ExtractionEngine`, `BitfieldDecoder`, `ConditionalBitfieldDecoder`, `CsvExporter`, `CsvStreamWriter`, `PcapFileReader` — signatures unchanged.

The PR explicitly excludes: threading, UI architecture changes, struct field changes, public API changes, payload zero-copy refactors, PCAP reader buffer-reuse refactors, removing the 500 MB cap. Those belong to a later phase.

## Critical Files To Modify

- [sources/ExtractionEngine.cpp](sources/ExtractionEngine.cpp)
- [sources/BitfieldDecoder.cpp](sources/BitfieldDecoder.cpp)
- [sources/ConditionalBitfieldDecoder.cpp](sources/ConditionalBitfieldDecoder.cpp)
- [sources/CsvExporter.cpp](sources/CsvExporter.cpp)
- [sources/MainWindow.cpp](sources/MainWindow.cpp) — preview-only changes

No header file changes. No `.pro` file changes. No `.ui` file changes.

---

## Change Set

### 1. ExtractionEngine — single-pass raw decode, drop string-keyed maps

**File:** [sources/ExtractionEngine.cpp:153-208](sources/ExtractionEngine.cpp#L153)

**Current behavior:** `valuesFromPayload` does two passes:
- Phase 1 reads every field's raw `quint64` into `QMap<QString,quint64> rawValues` (keyed by field name) plus `QMap<QString,bool> fieldValid`.
- Phase 2 calls `valueFromPayload(payload, field)` for each field — which internally re-runs `readUnsignedBigEndianRawValue` on the same bytes. **Every field is decoded twice.**

**Change (internals only):**
- Replace the two `QMap` instances with two `QVarLengthArray<quint64, 16> raw` and `QVarLengthArray<bool, 16> ok`, indexed by field position.
- Build a small `QHash<QString,int> nameToIndex` once at function entry, mapping field name → index, for the conditional decoder's controller-field lookup.
- Add a private file-scope helper `formatRawValue(quint64 rawValue, const FieldDefinition& field)` that mirrors the `switch` in `valueFromPayload` but does NOT re-read bytes. It produces a byte-identical string.
- Phase 2 calls `formatRawValue(raw[i], field)` instead of `valueFromPayload(payload, field)`.
- For fields that fail the bounds check (`ok[i] == false`), produce `"N/A"` exactly as today.
- Keep `valueFromPayload(payload, field)` (single-field accessor) unchanged — still re-reads, since callers rely on the signature. It is no longer used by `valuesFromPayload`.

**Why safe:** `formatRawValue` is a faithful extraction of the existing `switch` body. The bounds checks remain identical. The phase-1 vector replaces the `QMap` but stores the same numbers under the same indices. The controller lookup that used `rawValues.value(ctrlName, 0)` becomes `raw[nameToIndex.value(ctrlName, -1)]` with the same fallback semantics (controller-not-found → `ctrlFound = false`, raw = 0).

**Wins:** Eliminates one `readUnsignedBigEndianRawValue` per field, plus two `QMap` insertions per field, plus name-keyed `QString` allocations for hash keys. Expected: ~30–50% reduction in CPU time inside `valuesFromPayload` for typical 5–20 field configurations.

---

### 2. ExtractionEngine — remove `QRegularExpression` from `formatCalculatedValue`

**File:** [sources/ExtractionEngine.cpp:26-31](sources/ExtractionEngine.cpp#L26)

**Current:**
```cpp
return QString::number(value, 'f', 6)
    .remove(QRegularExpression("0+$"))
    .remove(QRegularExpression("\\.$"));
```
Two `QRegularExpression` constructions per formatted floating-point value.

**Change:**
```cpp
QString s = QString::number(value, 'f', 6);
int end = s.size();
while (end > 0 && s.at(end - 1) == QLatin1Char('0')) --end;
if (end > 0 && s.at(end - 1) == QLatin1Char('.')) --end;
s.truncate(end);
return s;
```

**Why safe:** The two regex patterns `0+$` and `\.$` strip trailing zeros then a single trailing dot. The loop matches that exactly: strip all trailing `'0'` chars, then strip one trailing `'.'` if present. Output bytes are identical for every input.

**Wins:** Removes per-row PCRE engine init. ~5× faster on this function. Adds up significantly when many float fields are configured.

---

### 3. BitfieldDecoder::decodeRule — single QMap lookup + cheaper unknown-behavior dispatch

**File:** [sources/BitfieldDecoder.cpp:346-374](sources/BitfieldDecoder.cpp#L346)

**Current:** Calls `rule.valueMeanings.contains(value)` then `rule.valueMeanings.value(value)` (two O(log n) lookups). Then calls the local `normalizedUnknownBehavior(rule.unknownBehavior)` which does `trimmed().toUpper()` allocating a temporary on every packet.

**Change (internals only):**
- Replace the `contains`/`value` pair with a single `constFind`:
  ```cpp
  auto it = rule.valueMeanings.constFind(value);
  if (it != rule.valueMeanings.constEnd()) return it.value();
  ```
- Replace `normalizedUnknownBehavior(rule.unknownBehavior)` with case-insensitive direct compares:
  ```cpp
  const QString& b = rule.unknownBehavior;
  if (b.compare(QLatin1String("BLANK"), Qt::CaseInsensitive) == 0) return QString();
  if (b.compare(QLatin1String("RAW_BINARY"), Qt::CaseInsensitive) == 0) return binary;
  return QStringLiteral("UNKNOWN(") + binary + QLatin1Char(')');
  ```
  Same three-way result, no `toUpper()` temporary, no extra string allocation for the unknown case.

**Why safe:** Both the lookup and the unknown-behavior cases produce the same outputs. `QString::compare(..., Qt::CaseInsensitive)` is the same semantic check as `trimmed().toUpper() == "BLANK"` for inputs that have no leading/trailing whitespace; the only inputs flow from `normalizedUnknownBehavior` at config time, which already trims — so there is no whitespace in the stored value at runtime.

**Wins:** Removes a per-row `QString::toUpper()` allocation. Halves QMap lookup cost.

---

### 4. ConditionalBitfieldDecoder::decode — single-pass result, cached unknown-behavior

**File:** [sources/ConditionalBitfieldDecoder.cpp:268-341](sources/ConditionalBitfieldDecoder.cpp#L268)

**Current:** Pre-counts `totalCols` by iterating profiles, then allocates a `QStringList` of N empty strings using `<<` in a loop, then walks profiles a second time to fill the matching profile's slots. Also calls `decoder.unknownBehavior.toUpper() != "BLANK"` (temporary allocation) every call.

**Change (internals only):**
- Compute `useUnknownLabel` once via `QString::compare(decoder.unknownBehavior, "BLANK", Qt::CaseInsensitive) != 0`.
- Use `QStringList result; result.reserve(totalCols);` and pre-fill with `result.fill(QString(), totalCols)` so allocation happens once. (Same end state as today.)
- Keep the two-walk structure (count then fill) — the count walk is cheap and the fill walk only does real work for the matching profile. The only changes are: avoid the `<<` insertion loop and avoid the `toUpper()`.

**Why safe:** Output is the same `QStringList`. The profile-matching logic, the validation-rule logic, the `UNKNOWN_CONTROLLER(0x...)` formatting, and the column ordering are all unchanged.

**Wins:** Removes per-row temporary `QString` allocations. Smaller win than #1–#3 but free.

---

### 5. CsvExporter — reusable UTF-8 buffer, no `QStringList` per row

**File:** [sources/CsvExporter.cpp](sources/CsvExporter.cpp) (entire `writeRow` and `open` paths)

**Current:** `writeRow` builds a `QStringList cells`, calls `escapeCell(safeCell(...))` per cell, calls `cells.join(',')`, then writes through `QTextStream` configured with `setCodec("UTF-8")`. File opened with `QIODevice::Text` so Qt translates `\n` to `\r\n` on Windows. Each row triggers: N `QString` allocs + 1 join + QTextStream codec encode.

**Change (internals only, signature unchanged):**
- Add a private `QByteArray m_lineBuffer` member. (Header file edit: add the field — but this header is **not** part of the public API surface; no other compilation unit uses internals of `CsvExporter`. Confirmed: only `MainWindow.cpp` includes it and only via constructor + `open()` + `writeRow()` + `close()`.)
  - **Decision:** to honor "no header changes," declare the buffer at function scope inside `writeRow` instead, OR add it as a static thread-local in the .cpp. Use **function-scope `static thread_local QByteArray buf;`** to keep `CsvExporter.h` untouched. Each export thread keeps one buffer; reused across rows.
- Replace `writeRow` body with:
  1. Clear `buf` (keeps capacity).
  2. For each cell: call new helper `appendEscapedCellUtf8(buf, cell)` that:
     - Applies the same formula-protection (prepend `'` if first char is `=`, `+`, `-`, or `@`).
     - Detects need-to-quote (contains `,`, `"`, `\n`, `\r`) in a **single scan**.
     - If quoted: append `"`, then append the UTF-8 bytes while doubling each `"`.
     - If not quoted: append the UTF-8 bytes directly.
  3. Append `,` between cells, `\n` at end (NOT `\r\n` — file remains opened in `QIODevice::Text` so Qt translates on Windows; output bytes stay identical to current).
  4. `m_file.write(buf)`; check return value matches `buf.size()`; on mismatch set the same error string as today (`"Failed while writing CSV row."` — keep wording byte-identical).
- Replace `open()` body's header write to use the same path (still goes through `writeRow(headers, ...)`).
- Remove the `m_stream` member usage in this file by stopping the codec setup — but **keep the member declaration** in `CsvExporter.h` untouched (no header changes). Just stop calling `setDevice` / `setCodec` / `<<`. The `QTextStream` member becomes a do-nothing reservation.
  - Alternative if we want zero risk: keep the `m_stream` member fully wired but stop using it in `writeRow` only. `flush()` on close still works because `m_file.close()` flushes the OS handle.
  - Verify with `close()`: replace `m_stream.flush(); m_stream.setDevice(0);` with `m_file.flush();` (or leave the stream lines harmless against an unused stream).

**Why safe:**
- The character escaping rules are byte-for-byte the same: same trigger characters, same `""` doubling, same surrounding `"`.
- The formula-protection prefix logic is identical.
- The UTF-8 encoding is identical (`QString::toUtf8()` ≡ `QTextStream` with `setCodec("UTF-8")` and no BOM, which the current code already configures).
- Line endings: keep `QIODevice::Text` and emit `\n` to inherit the platform's translation behavior — bytes on disk stay identical to today on each platform.
- The error string written into `errorMessage` on failure is preserved verbatim.

**Wins:** Removes ~N `QString` allocations + 1 `QStringList::join` + 1 QTextStream codec round-trip per row. For 1M rows × 15 cols, this is one of the larger cumulative wins.

---

### 6. ExtractionEngine — drop the per-row `Q_ASSERT` columnHeaders rebuild

**File:** [sources/ExtractionEngine.cpp:205](sources/ExtractionEngine.cpp#L205)

**Current:**
```cpp
Q_ASSERT(values.size() == columnHeaders(fields).size());
```
In debug builds this rebuilds the entire `QStringList` of headers — including calls into `BitfieldDecoder::sanitizeColumnLabel` and `ConditionalBitfieldDecoder::columnHeaders` — **per row**.

**Change (internals only):**
- Cache the expected count in a local before the loop:
  ```cpp
  #ifndef QT_NO_DEBUG
  const int expected = computeExpectedColumnCount(fields); // small inline that counts without building strings
  #endif
  ...
  Q_ASSERT(values.size() == expected);
  ```
- Add a private file-scope helper `computeExpectedColumnCount(fields)` that walks fields once and sums `1 + (hasBitfieldDecoder ? rules.size() : 0) + (hasConditional ? conditionalColCount : 0)`. The conditional col count is `1 + sum_over_profiles(rules.size() + exclusionRules.size())`.

**Why safe:** Release builds are unaffected (`Q_ASSERT` is a no-op). Debug builds get the same assertion, just without rebuilding string lists each row. The expected count formula mirrors `columnHeaders` exactly — verified by reading the two functions side by side.

**Wins:** Improves debug-build performance dramatically (sometimes 10×). Pure dev quality-of-life.

---

### 7. Live preview — delta append instead of full rebuild

**File:** [sources/MainWindow.cpp:1378-1388](sources/MainWindow.cpp#L1378)

**Current:** `refreshLivePreview()` fires every 250 ms and does `ui->tblOutput->setRowCount(0);` then `appendPreviewRow` for every row in `m_livePreviewRows` (up to 200).

**Change:**
- Add a new `MainWindow` member `int m_liveLastRenderedRows = 0;` — declared **inside MainWindow.cpp at file scope as a `static` only if header edits are disallowed**, OR as a private member of `MainWindow` (which **does** touch `MainWindow.h`).
  - **Decision:** the user said header cleanup already happened and minor header additions are not behavioral. To keep this PR header-touch-free, store the counter as a `static int` inside `refreshLivePreview()` (function-local static — survives across calls, reset by `prepareOutputTable()` indirectly when row count drops).
- New refresh flow:
  1. Update counter labels (unchanged).
  2. If `m_livePreviewRows.size() < lastCount`: the buffer wrapped (capped at LIMIT and dropped from front). Remove top `lastCount - m_livePreviewRows.size() + newlyAdded` rows in batch via `ui->tblOutput->removeRow(0)` loop. Simpler implementation: if `m_livePreviewRows.size() == LIMIT && tableRowCount == LIMIT`, remove the topmost row before appending the latest one — but this requires tracking the "oldest" pointer.
  3. Easiest correct approach: append rows for indices `[lastCount, m_livePreviewRows.size())`. If `lastCount > m_livePreviewRows.size()` (wrap happened), recompute: clear and rebuild fully **only this iteration** (rare event when buffer is at LIMIT and trim removed from front).

**Refined approach (simpler and correct):**
- Maintain a `static int lastRenderedTotal` that counts **total rows pushed into `m_livePreviewRows` since start**, not just current size. Set this counter inside `onLiveDatagramReceived` whenever a row is appended (use another small static).
- Better: keep refresh purely view-side. On each tick:
  - Let `cur = m_livePreviewRows.size();`
  - Let `tbl = ui->tblOutput->rowCount();`
  - If `tbl > cur` (paranoia): set row count to `cur`.
  - For the trailing rows where row buffer changed identity, we can't know without tracking. So compromise:
    - Track `static int previewRunSeq = 0;` incremented inside `onLiveDatagramReceived` by 1 per appended row.
    - Track another `static int lastRenderedSeq = 0;`.
    - Compute `delta = previewRunSeq - lastRenderedSeq`. If delta ≤ 0, nothing new — only update labels and return.
    - If delta > 0: the last `delta` entries of `m_livePreviewRows` are new. Append them. If `tbl + delta > LIVE_PREVIEW_ROW_LIMIT`, also `removeRow(0)` enough times to keep the table at the limit.
    - Update `lastRenderedSeq`.
  - Reset both seq counters in `startLiveCapture()` (before listening).

**Why safe:**
- The final visible rows are identical: same rows in same order, capped at the same limit.
- The labels logic is unchanged.
- The slight difference is internal: the table is no longer rebuilt from scratch. Visually you cannot tell the difference; if anything, no more 4 Hz flicker.

**Wins:** Goes from ~8 000 `QTableWidgetItem` allocations per second when idle to a few per second when packets actually arrive. Major perceived-responsiveness improvement during live capture.

---

### 8. Offline preview — wrap hot loop with `setUpdatesEnabled(false)`

**File:** [sources/MainWindow.cpp:647-712](sources/MainWindow.cpp#L647) (header mode hot loop) and [sources/MainWindow.cpp:1059-1136](sources/MainWindow.cpp#L1059) (port-message hot loop)

**Current:** Each matched preview row triggers `ui->tblOutput->insertRow` and N `setItem` calls, which schedule paints. With `processEvents` every 500 packets, those paints actually run mid-loop.

**Change:**
- Right before entering the export loop, call `ui->tblOutput->setUpdatesEnabled(false);`.
- After the loop completes (success or failure), in the same block where `closePartitions()` runs, call `ui->tblOutput->setUpdatesEnabled(true);` and then `ui->tblOutput->viewport()->update();`.
- No change to which rows are added or in what order.

**Why safe:** `setUpdatesEnabled(false)` only defers repaints — it does not skip any insertion. After re-enabling, the widget paints once with all rows present. Matches the existing behavior exactly.

**Wins:** Removes per-row repaint cost (which is non-trivial with 5 000 preview rows × wide columns) and quiets the UI flicker every 500 packets.

---

## Execution Order (single PR, single commit-worthy change)

Land all eight items in one branch. Order within the branch (so partial bisects work cleanly):

1. ExtractionEngine `formatCalculatedValue` regex → manual trim (Change #2). Smallest, isolated.
2. BitfieldDecoder::decodeRule micro-opt (Change #3).
3. ConditionalBitfieldDecoder::decode micro-opt (Change #4).
4. ExtractionEngine single-pass raw decode + `formatRawValue` (Change #1). Largest engine change.
5. ExtractionEngine `Q_ASSERT` debug-build fix (Change #6).
6. CsvExporter buffer rewrite (Change #5). Touches I/O path.
7. Offline preview `setUpdatesEnabled` (Change #8).
8. Live preview delta append (Change #7). UI-state-track change last.

Build after each, run the verification step against a small reference capture to confirm CSV output stays byte-identical.

## Reused Existing Functions and Utilities

- [BitfieldDecoder::decodeRule](sources/BitfieldDecoder.cpp#L346) — keep, just optimize internals.
- [BitfieldDecoder::sanitizeColumnLabel](sources/BitfieldDecoder.cpp#L404) — used only in column-header generation, untouched.
- [ConditionalBitfieldDecoder::columnHeaders](sources/ConditionalBitfieldDecoder.cpp#L241) — used by `ExtractionEngine::columnHeaders`; untouched.
- [ExtractionEngine::valueFromPayload](sources/ExtractionEngine.cpp#L84) — kept as public single-field accessor; new `formatRawValue` is a private extraction of its `switch` body.
- [CsvStreamWriter::escapeCsv](sources/CsvStreamWriter.cpp#L182) and [protectFormula](sources/CsvStreamWriter.cpp#L198) — semantics mirrored in the new `CsvExporter` UTF-8 buffer path, ensuring the two writers stay byte-identical for the same inputs.

## Verification

Before/after correctness check (must pass, otherwise revert):

1. Pick a representative capture file (e.g. one of the files in [test_files/](test_files/) or the user's own test pcap with multiple configured messages — both port mode and header mode).
2. With the current `main` branch (pre-change) build, run an export with:
   - Port mode, ≥2 messages, ≥1 message with a bitfield decoder, ≥1 message with a conditional bitfield decoder. Use the "Verify all configured messages before export" checkbox both ON and OFF.
   - Header mode, ≥2 filters, ≥1 field with `Float32` resolution (exercises `formatCalculatedValue`).
   - Live mode (using [tools/udp_test_sender.py](tools/udp_test_sender.py)) for ~30 seconds with bitfield-decoded fields.
3. Save all generated CSVs as `before/`.
4. Build the optimized branch. Re-run the same scenarios with **identical** inputs into `after/`.
5. Diff every CSV: on Windows PowerShell:
   ```powershell
   Get-ChildItem before -Filter *.csv | ForEach-Object {
     $a = $_.FullName
     $b = Join-Path after $_.Name
     if ((Get-FileHash $a).Hash -ne (Get-FileHash $b).Hash) {
       Write-Host "MISMATCH: $($_.Name)"
     }
   }
   ```
   Any mismatch is a release blocker — revert that change.
6. Visual check: preview tables should contain the same rows. Status/summary message text should be identical (compare screenshots side by side).
7. Performance check: time a known large capture with `QElapsedTimer`-wrapped sections (add temporarily, remove before commit). Expect end-to-end export time to drop noticeably (target: ≥40% faster on a capture with 10+ configured messages and decoders).
8. Smoke test the `Verify messages` checkbox both states still behave the same (still pre-scans when checked, still warns at end when unchecked and a message produced 0 rows).
9. Live mode UI should no longer flicker every 250 ms during idle listening.

## Out of Scope (Explicitly Deferred)

- Threading for export or live capture.
- Removing the 500 MB validator cap.
- Zero-copy payload view in `ParsedUdpPacket`.
- `PcapFileReader` buffer-reuse and PCAPNG block skip-ahead.
- Replacing `QTableWidget` with a model-backed view.
- Splitting `MainWindow.cpp` into an `ExportEngine` class.
- The dual-port routing hash and compiled `MessagePlan`.

These remain valid future work but each carries some behavioral risk or requires architectural changes that exceed the "one go, no compromise" scope.
