#pragma once

#include <stddef.h>
#include <stdint.h>

// OTAA identity stored in the STM32L0 data EEPROM, separate from the firmware.
// Written once per device by tools/provision.py; firmware updates leave it intact.
//
// Record layout (76 bytes, values as upper- or lower-case ASCII hex, MSB first):
//   0  magic "WSID"
//   4  version (1), then 3 reserved bytes
//   8  DevEUI, 16 characters
//  24  AppEUI/JoinEUI, 16 characters
//  40  AppKey, 32 characters
//  72  CRC-32 (IEEE, as zlib.crc32) of bytes 0-71, least significant byte first
namespace DeviceIdentity {
constexpr uint32_t EEPROM_ADDRESS = 0x08080000;
constexpr uint8_t VERSION = 1;
constexpr size_t DEV_EUI_OFFSET = 8;
constexpr size_t APP_EUI_OFFSET = 24;
constexpr size_t APP_KEY_OFFSET = 40;
constexpr size_t EUI_CHARS = 16;
constexpr size_t KEY_CHARS = 32;
constexpr size_t CRC_OFFSET = 72;
constexpr size_t RECORD_SIZE = 76;

struct Credentials {
    char devEui[EUI_CHARS + 1];
    char appEui[EUI_CHARS + 1];
    char appKey[KEY_CHARS + 1];
};

constexpr uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
        }
    }
    return ~crc;
}

constexpr bool isHex(uint8_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

constexpr bool copyHex(const uint8_t *record, size_t offset, size_t chars, char *out) {
    for (size_t i = 0; i < chars; ++i) {
        if (!isHex(record[offset + i])) return false;
        out[i] = char(record[offset + i]);
    }
    out[chars] = '\0';
    return true;
}

// Returns false for a blank, corrupt or unknown record; credentials are then
// unspecified and must not be used.
constexpr bool parse(const uint8_t *record, size_t length, Credentials &credentials) {
    if (record == nullptr || length < RECORD_SIZE) return false;
    if (record[0] != 'W' || record[1] != 'S' || record[2] != 'I' || record[3] != 'D' ||
        record[4] != VERSION) {
        return false;
    }
    const uint32_t storedCrc = uint32_t(record[CRC_OFFSET]) |
                               (uint32_t(record[CRC_OFFSET + 1]) << 8) |
                               (uint32_t(record[CRC_OFFSET + 2]) << 16) |
                               (uint32_t(record[CRC_OFFSET + 3]) << 24);
    if (crc32(record, CRC_OFFSET) != storedCrc) return false;
    return copyHex(record, DEV_EUI_OFFSET, EUI_CHARS, credentials.devEui) &&
           copyHex(record, APP_EUI_OFFSET, EUI_CHARS, credentials.appEui) &&
           copyHex(record, APP_KEY_OFFSET, KEY_CHARS, credentials.appKey);
}
} // namespace DeviceIdentity
