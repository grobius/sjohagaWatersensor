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
const char *appKey = "ABCDEFABCDEFABCDEFABCDEFABCDEFAB";
// incease this time to shorten time between send and receive, depends on micro-controller
const double calTime =  7980.0;
