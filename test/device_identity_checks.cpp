// Compile with C++14 or newer. Static assertions execute the real EEPROM record
// parser during compilation, without an MCU or host runtime.
#include "DeviceIdentity.h"

using namespace DeviceIdentity;

struct Record {
    uint8_t bytes[RECORD_SIZE];
};

constexpr const char *DEV_EUI = "70B3D57ED0012345";
constexpr const char *APP_EUI = "0000000000000000";
constexpr const char *APP_KEY = "0123456789abcdef0123456789ABCDEF";

constexpr void put(Record &record, size_t offset, const char *text, size_t chars) {
    for (size_t i = 0; i < chars; ++i) record.bytes[offset + i] = uint8_t(text[i]);
}

constexpr void seal(Record &record) {
    const uint32_t crc = crc32(record.bytes, CRC_OFFSET);
    for (size_t i = 0; i < 4; ++i) record.bytes[CRC_OFFSET + i] = uint8_t(crc >> (8 * i));
}

constexpr Record validRecord() {
    Record record{};
    put(record, 0, "WSID", 4);
    record.bytes[4] = VERSION;
    put(record, DEV_EUI_OFFSET, DEV_EUI, EUI_CHARS);
    put(record, APP_EUI_OFFSET, APP_EUI, EUI_CHARS);
    put(record, APP_KEY_OFFSET, APP_KEY, KEY_CHARS);
    seal(record);
    return record;
}

constexpr bool sameText(const char *a, const char *b, size_t chars) {
    for (size_t i = 0; i < chars; ++i) {
        if (a[i] != b[i]) return false;
    }
    return a[chars] == '\0';
}

constexpr bool parsesValid() {
    const Record record = validRecord();
    Credentials credentials{};
    return parse(record.bytes, RECORD_SIZE, credentials) &&
           sameText(credentials.devEui, DEV_EUI, EUI_CHARS) &&
           sameText(credentials.appEui, APP_EUI, EUI_CHARS) &&
           sameText(credentials.appKey, APP_KEY, KEY_CHARS);
}

constexpr bool rejectsWithByte(size_t offset, uint8_t value, bool reseal) {
    Record record = validRecord();
    record.bytes[offset] = value;
    if (reseal) seal(record);
    Credentials credentials{};
    return !parse(record.bytes, RECORD_SIZE, credentials);
}

constexpr bool rejectsFilled(uint8_t value) {
    Record record{};
    for (size_t i = 0; i < RECORD_SIZE; ++i) record.bytes[i] = value;
    Credentials credentials{};
    return !parse(record.bytes, RECORD_SIZE, credentials);
}

constexpr bool rejectsShortOrNull() {
    const Record record = validRecord();
    Credentials credentials{};
    return !parse(record.bytes, RECORD_SIZE - 1, credentials) &&
           !parse(nullptr, RECORD_SIZE, credentials);
}

constexpr uint8_t CHECK_STRING[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

static_assert(crc32(CHECK_STRING, sizeof(CHECK_STRING)) == 0xCBF43926, "standard CRC-32 check value");
static_assert(parsesValid(), "valid record yields all three credentials");
static_assert(rejectsFilled(0x00) && rejectsFilled(0xFF), "erased EEPROM is unprovisioned");
static_assert(rejectsShortOrNull(), "truncated or missing record");
static_assert(rejectsWithByte(0, 'X', true), "wrong magic");
static_assert(rejectsWithByte(4, 2, true), "unknown version");
static_assert(rejectsWithByte(DEV_EUI_OFFSET + 3, 'G', true), "non-hex DevEUI");
static_assert(rejectsWithByte(APP_EUI_OFFSET, ' ', true), "non-hex AppEUI");
static_assert(rejectsWithByte(APP_KEY_OFFSET + KEY_CHARS - 1, 0, true), "non-hex AppKey");
static_assert(rejectsWithByte(APP_KEY_OFFSET + 5, '0', false), "corrupted key fails CRC");
static_assert(rejectsWithByte(CRC_OFFSET + 3, uint8_t(validRecord().bytes[CRC_OFFSET + 3] ^ 0xFF), false),
              "corrupted CRC");
