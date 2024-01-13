#define DL_FREQ 485e6  // Downling frequency
#define UL_FREQ 495e6  // Uplink frequency

#define RATE_SCALING 4
#define FFT_SIZE (RATE_SCALING * 128) 
//#define RATE ((float)RATE_SCALING * 1920000.0) //2000000.0) // Sampling rate
#define RATE ((float)RATE_SCALING * 2000000.0) // Sampling rate
#define DL_RATE RATE  // Downling frequency
#define UL_RATE RATE  // Uplink frequency

#define DEVICE_TYPE_BS 0
#define DEVICE_ADDR_BS "serial=32711EE" //"serial=30CBE77"
#define DEVICE_TYPE_UE 0
//#define DEVICE_ADDR_UE "serial=3104B0C"
#define DEVICE_ADDR_UE "serial=3271162" //serial=30FB566"
// #define NRBDL (RATE_SCALING * 6)
// #define PACKET_SIZE (NRBDL * 12/2) // NRBDL * 12/2


