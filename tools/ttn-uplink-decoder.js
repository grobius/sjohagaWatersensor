// Paste into the device's JavaScript uplink payload formatter in The Things Stack.
function decodeUplink(input) {
  if (input.fPort !== 1 || !input.bytes || input.bytes.length !== 4) {
    return { errors: ["Expected four sensor bytes on FPort 1"] };
  }
  var bytes = input.bytes;
  var rawTemperature = (bytes[2] << 8) | bytes[3];
  return {
    data: {
      vcc: ((bytes[0] << 8) | bytes[1]) / 1000,
      temperature: rawTemperature === 0xFFFF ? null : rawTemperature / 100 - 50,
      sensor_ok: rawTemperature !== 0xFFFF
    }
  };
}
