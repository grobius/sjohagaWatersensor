// used in LoRaMAC.cpp for randomizing and create DevNonce
#define ANALOGINPUTFORRANDOMIZING PA2

//Uncomment for debug. DEBUG_MAIN must be defined for Serial port init
// #define DEBUG_LORA
//#define DEBUG_LOW_LEVEL

#ifdef DEBUG_LORA
  // for debugging redirect to hardware Serial2
  // Tx on PA2
  // HardwareSerial Serial2(USART2);   // or HardWareSerial Serial2 (PA3, PA2);
  // #define DEBUG_BEGIN(...) Serial2.begin(__VA_ARGS__)
  #define DEBUG_PRINT(...) Serial2.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial2.println(__VA_ARGS__)
#else
  // #define DEBUG_BEGIN(...)
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

// Define max payload size used for this node
#define MAX_UPLINK_PAYLOAD_SIZE    220
#define MAX_DOWNLINK_PAYLOAD_SIZE   220

#if !defined(AS_923) && !defined(EU_868) && !defined(US_915) && !defined(AU_915)

    //LoRaWAN freq band
    #define EU_868
    //#define AU_915
    //#define US_915
    //#define AS_923
   
    #ifdef US_915
        //Select the subband youre working on
        // make sure your gateway is working in one of these bands
        #define SUBND_0     // 902.3 - 903.7 Mhz
        //#define SUBND_1     // 903.9 - 905.3 Mhz TTN
        //#define SUBND_2     // 905.5 - 906.9 Mhz
        //#define SUBND_3     // 907.1 - 908.5 Mhz
        //#define SUBND_4     // 908.7 - 910.1 Mhz
        //#define SUBND_5     // 910.3 - 911.7 Mhz
        //#define SUBND_6     // 911.9 - 913.3 Mhz
        //#define SUBND_7     // 913.5 - 914.9 Mhz
    #endif

    #ifdef AU_915
        //Select the subband youre working on
        // make sure your gateway is working in one of these bands
        #define SUBND_0     // 915.2 - 916.6 Mhz
        //#define SUBND_1     // 916.8 - 918.2 Mhz
        //#define SUBND_2     // 918.4 - 919.8 Mhz
        //#define SUBND_3     // 920.0 - 921.4 Mhz
        //#define SUBND_4     // 921.6 - 923.0 Mhz
        //#define SUBND_5     // 923.2 - 924.6 Mhz
        //#define SUBND_6     // 924.8 - 926.2 Mhz
        //#define SUBND_7     // 926.4 - 927.8 Mhz
    #endif
#endif
