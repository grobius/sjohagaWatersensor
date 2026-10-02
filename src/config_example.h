/**
 * config_example.h
 * Example file with OTAA credentials
 * Please rename this file to config.h and replace the credential information
 * 
 * @author Leo Korbee | Leo.Korbee@xs4all.nl
 * @date 2024-06-17 
*/

// OTAA credential - all start with msb
const char *appEui = "0000000000000123";
const char *devEui = "1234567890123456";
const char *appKey = "0123456789ABCDEF0123456789ABCDEF";
// The application measures RTC drift against SysTick during startup.
// The old calTimeDivider setting is no longer used by the application.
