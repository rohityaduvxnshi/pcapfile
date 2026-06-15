#!/usr/bin/env python3
"""Generate a small .pcap with UDP packets that match the Nav_Status message
(UDP port 5000, 32-byte payload starting with header 0xAA55). Sample values
vary per packet so the exported table shows realistic, changing columns."""
import struct, os, socket

def ip_checksum(hdr):
    s = 0
    for i in range(0, len(hdr), 2):
        w = (hdr[i] << 8) + (hdr[i+1] if i+1 < len(hdr) else 0)
        s += w
    s = (s >> 16) + (s & 0xffff)
    s += (s >> 16)
    return (~s) & 0xffff

def udp_packet(payload, src_port=4001, dst_port=5000):
    # UDP header
    udp_len = 8 + len(payload)
    udp = struct.pack('>HHHH', src_port, dst_port, udp_len, 0) + payload
    # IPv4 header (20 bytes), protocol 17 = UDP
    total_len = 20 + udp_len
    ver_ihl = 0x45
    src_ip = socket.inet_aton('192.168.1.50')
    dst_ip = socket.inet_aton('239.0.0.1')
    iph = struct.pack('>BBHHHBBH4s4s', ver_ihl, 0, total_len, 0x1234, 0, 64, 17, 0, src_ip, dst_ip)
    chk = ip_checksum(iph)
    iph = struct.pack('>BBHHHBBH4s4s', ver_ihl, 0, total_len, 0x1234, 0, 64, 17, chk, src_ip, dst_ip)
    # Ethernet header (14 bytes), ethertype 0x0800 = IPv4
    eth = bytes.fromhex('010005000001') + bytes.fromhex('aabbccddeeff') + struct.pack('>H', 0x0800)
    return eth + iph + udp

def nav_payload(i):
    b = bytearray(32)
    b[0] = 0xAA; b[1] = 0x55                      # header / MessageID region
    lat = int((37.0 + i * 0.001) / 1e-7) & 0xFFFFFFFF
    lon = int((-122.0 - i * 0.001) / 1e-7) & 0xFFFFFFFF
    struct.pack_into('>I', b, 1, lat & 0xFFFFFFFF) # Latitude int32 @ offset2 (0-based 1)
    # keep header byte intact
    b[0] = 0xAA; b[1] = 0x55
    struct.pack_into('>i', b, 5, lon if lon < 0x80000000 else lon - 0x100000000)  # Longitude @ offset6
    struct.pack_into('>f', b, 9, 100.0 + i)        # Altitude float32 @ offset10
    struct.pack_into('>H', b, 13, 1200 + i * 7)    # Speed uint16 @ offset14
    struct.pack_into('>H', b, 15, (i * 4096) & 0xFFFF)  # Heading uint16 @ offset16
    struct.pack_into('>H', b, 17, i % 4)           # StatusWord uint16 @ offset18 (Mode bits cycle 0..3)
    cs = ('NAV%03d' % i).encode('ascii')           # CallSign string @ offset20 (len 8)
    b[19:19+len(cs)] = cs
    return bytes(b)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sample_capture.pcap')
with open(out, 'wb') as f:
    # pcap global header: magic, ver 2.4, zone, sigfigs, snaplen, network=1 (Ethernet)
    f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
    ts = 1700000000
    for i in range(15):
        pkt = udp_packet(nav_payload(i))
        usec = (i * 100000) % 1000000
        f.write(struct.pack('<IIII', ts + i // 10, usec, len(pkt), len(pkt)))
        f.write(pkt)
print('wrote', out, os.path.getsize(out), 'bytes,', 15, 'packets')
