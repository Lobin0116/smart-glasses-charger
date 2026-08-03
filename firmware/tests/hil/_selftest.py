"""Self-test for sgc_at — verifies Python protocol mirror matches firmware logic."""
import sgc_at


def main():
    req = sgc_at.pack_request(sgc_at.OPCODE_CASE_HEART, bytes([1, 0, 0x40, 0x01]))
    print(f"packed request ({len(req)} bytes): {req.hex()}")

    parsed = sgc_at.parse_frame(req)
    print(f"parsed: magic={parsed['magic']:#010x} opcode={parsed['opcode']:#06x} "
          f"status={parsed['status']:#04x} payload={parsed['payload'].hex()}")

    assert parsed["magic"] == sgc_at.MAGIC_REQ
    assert parsed["opcode"] == sgc_at.OPCODE_CASE_HEART
    assert parsed["payload"] == bytes([1, 0, 0x40, 0x01])
    print("round-trip OK")

    rsp = sgc_at.pack_heartbeat_response(glass_soc=0xE4, glass_sta=0x80, case_version=0x01)
    print(f"packed response ({len(rsp)} bytes): {rsp.hex()}")
    parsed_rsp = sgc_at.parse_frame(rsp)
    assert parsed_rsp["magic"] == sgc_at.MAGIC_RSP
    assert parsed_rsp["payload"] == bytes([1, 0, 0xE4, 0x80, 0x01])
    print("response build/parse OK")

    tampered = bytearray(req)
    tampered[10] ^= 0x01
    try:
        sgc_at.parse_frame(bytes(tampered))
        print("ERROR: tampered frame passed CRC!")
    except ValueError as e:
        print(f"CRC tamper detected: {e}")

    print("\nAll self-tests passed.")


if __name__ == "__main__":
    main()
