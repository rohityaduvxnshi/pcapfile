# Working Manual

## 1. Open Project

Open `PcapUdpExtractor.pro` in Qt Creator.

Recommended setup:

- Qt Widgets kit
- qmake build system
- C++11 compatible compiler
- No external libraries required

## 2. Build Project

From Qt Creator, click **Build**.

From terminal on Windows MinGW:

```bash
qmake PcapUdpExtractor.pro
mingw32-make
```

From Linux:

```bash
qmake PcapUdpExtractor.pro
make
```

## 3. Use Application

1. Click **Browse**.
2. Select a `.pcap` or `.pcapng` file.
3. Enter the UDP port number.
4. Click **Add Field** for every UDP payload value you want to extract.
5. Fill these columns:
   - Field: output column name
   - Byte: byte offset inside UDP payload
   - Length: number of bytes to read, from 1 to 8
   - Resolution: multiplier applied to raw value
6. Click **Start**.
7. Select where to save the CSV file.
8. Open the exported CSV in Excel.

## 4. Field Example

Suppose the UDP payload starts like this:

```text
00 64 00 C8 01 2C
```

If field settings are:

| Field | Byte | Length | Resolution |
|---|---:|---:|---:|
| Speed | 0 | 2 | 0.1 |

The application reads:

```text
00 64 = 100 decimal
100 * 0.1 = 10.0
```

CSV value becomes:

```text
10
```

## 5. Important Rule

Byte offset is counted from the UDP payload start, not from the Ethernet frame start.

## 6. Output

Output CSV columns:

```text
Packet No, Timestamp, Source IP, Destination IP, Source UDP Port, Destination UDP Port, Payload Size, user fields...
```

## 7. Stability Notes

- The file is read packet by packet.
- The full file is not loaded into RAM.
- Preview table is limited to 5000 rows.
- All matching packets are still exported to CSV.
- Malformed packets are skipped safely.
