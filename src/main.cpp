
// Signal K application template file.
//
// This application demonstrates core SensESP concepts in a very
// concise manner. You can build and upload the application as is
// and observe the value changes on the serial port monitor.
//
// You can use this source file as a basis for your own projects.
// Remove the parts that are not relevant to you, and add your own code
// for external hardware libraries.

#define SERIALDEBUG
#define INCLUDE_TFT // comment to remove tft support
// Lite only has 1 output and 1 ads1115
//#define LITE_V3 // uncomment to build for "lite" board

#include <Arduino.h>
#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/digital_output.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_listener.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include <sensesp/system/valueconsumer.h>
#include "sensesp_onewire/onewire_temperature.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/transforms/frequency.h"
#include "sensesp/transforms/analogvoltage.h"
#include "sensesp/transforms/hysteresis.h"
#include "sensesp/transforms/enable.h"
#include "sensesp_app_builder.h"

// specific to this design
#include "EngineMonitorHardware.h"
#include "EngineMonitorN2K.h"
#include <Adafruit_ADS1X15.h>
#include <NMEA2000_CAN.h>  // This will automatically choose right CAN library and create suitable NMEA2000 object
#include <N2kMessages.h>
#include <N2KDeviceList.h>
#include <lvgl.h>
#include "EngineMonitortft320x240.h"

using namespace sensesp;
reactesp::ReactESP app;

// ADS1115 Module Instantiation
// ADS1115 @ 0x48 I2C
Adafruit_ADS1115 adsB1;

// volts for adsB1
float read_adsB1_ch0_callback() { return (adsB1 . computeVolts ( adsB1 . readADC_SingleEnded ( 0 ) ));}
float read_adsB1_ch1_callback() { return (adsB1 . computeVolts ( adsB1 . readADC_SingleEnded ( 1 ) ));}
// invert since level voltage decreses as level increases. 
float read_adsB1_ch2_callback() { return (2.2 - (adsB1 . computeVolts ( adsB1 . readADC_SingleEnded ( 2 ) )));}
//float read_adsB1_ch2_callback() { return ((adsB1 . computeVolts ( adsB1 . readADC_SingleEnded ( 2 ) )));}

float read_adsB1_ch3_callback() { return (adsB1 . computeVolts ( adsB1 . readADC_SingleEnded ( 3 ) ));}

#ifndef LITE_V3
// ADS1115 @ 0x49 I2C
Adafruit_ADS1115 adsB2;

// volts for adsB2
float read_adsB2_ch0_callback() { return (adsB2 . computeVolts ( adsB2 . readADC_SingleEnded ( 0 ) ));}
float read_adsB2_ch1_callback() { return (adsB2 . computeVolts ( adsB2 . readADC_SingleEnded ( 1 ) ));}
float read_adsB2_ch2_callback() { return (adsB2 . computeVolts ( adsB2 . readADC_SingleEnded ( 2 ) ));}
float read_adsB2_ch3_callback() { return (adsB2 . computeVolts ( adsB2 . readADC_SingleEnded ( 3 ) ));}
#endif

// forward declarations
void setupADS1115();
void SendN2kEngine( void );
void setupN2K( void );
void SendN2kTemperature( void );
void doN2Kprocessing(void);

extern void do_lvgl_init(uint32_t );

// Set the information for other bus devices, which messages we support
const unsigned long TransmitMessages[] PROGMEM = { 127488L, // Engine Rapid / RPM
                                                   127489L, // Engine Dynamic, Oil Pressure and temperature
                                                   127505L, // Fluid Level
                                                   127508L, // Battery Status
                                                   130316L, // Temperature values (or alternatively 130311L or 130312L but they are depricated)                                                   
                                                   0
                                                 };

// Define schedulers for messages. Define schedulers here disabled. Schedulers will be enabled
// on OnN2kOpen so they will be synchronized with system.
// We use own scheduler for each message so that each can have different offset and period.
// Setup periods according PGN definition (see comments on IsDefaultSingleFrameMessage and
// IsDefaultFastPacketMessage) and message first start offsets. Use a bit different offset for
// each message so they will not be sent at same time.
tN2kSyncScheduler TemperatureScheduler(false,1000,2000);
tN2kSyncScheduler EngineRapidScheduler(false,750,500);
tN2kSyncScheduler EngineDynamicScheduler(false,500,1200);
tN2kSyncScheduler BatteryStatusScheduler(false,1000,1200);
tN2kSyncScheduler FuelTankLevelScheduler(false,500,500);
tN2kDeviceList *locN2KDeviceList;
uint8_t n2kConnected = 0;
DallasTemperatureSensors* dts;
int numberOfDevices = 3;
bool signalKConnected = false;

// unique chip id
int32_t chipId=0;

// globals used for N2K Messages
double AltRPM = 0;
double EngRPM = 0;
double OilPres = 0;
double AltVolts = 0;
double HouseVolts = 0;
double FuelLevel = 0;
double B1A3 = 0;
double B2A0 = 0;
double B2A1 = 0;
double B2A2 = 0;
double B2A3 = 0;
bool chkEng = 0;
bool digIn2 = 0;
double engineCoolantTemp = 0;
double engineBlockTemp = 0;
double engineRoomTemp = 0;
double engineExhaustTemp = 0;

// *****************************************************************************
// Call back for NMEA2000 open. This will be called, when library starts bus communication.
// See NMEA2000.SetOnOpen(OnN2kOpen); on setup()
void OnN2kOpen() {
  // Start schedulers now.
  TemperatureScheduler.UpdateNextTime();
  BatteryStatusScheduler.UpdateNextTime();
  EngineRapidScheduler.UpdateNextTime();
  EngineDynamicScheduler.UpdateNextTime();
  FuelTankLevelScheduler.UpdateNextTime();
} // OnN2kOpen

// The setup function performs one-time application initialization.
void setup() {

  // seupt outputs at start
  pinMode(Out1, OUTPUT_OPEN_DRAIN); // output 1
  digitalWrite(Out1, true);

  #ifndef LITE_V3
  pinMode(Out2, OUTPUT_OPEN_DRAIN); // output 1
  digitalWrite(Out2, true);
  #endif
  // used to create a unique id
  uint8_t chipid [ 6 ];

#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif

  // derive a unique chip id from the burned in MAC address
  esp_efuse_mac_get_default ( chipid );
  for ( int i = 0 ; i < 6 ; i++ )
    chipId += ( chipid [ i ] << ( 7 * i ));

  #ifdef SERIALDEBUG
  // dump chip id at startup
  Serial.printf ( "\nPoweron reset - Chip ID: %x\n" , chipId );
  #endif

  // create a unique hostname based on constant + chipid
  char locBuf[128], locChipId[16];
  #ifndef LITE_V3
    strcpy(locBuf, "EM_V3");
  #else
    strcpy(locBuf, "EM_V3_Lite");
  #endif
  sprintf(locChipId,"-%x", chipId);
  strcat(locBuf, locChipId);

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname(locBuf)
                    // Optionally, hard-code the WiFi and Signal K server
                    // ->set_wifi("ssid","password")
                    //->set_sk_server("ip address", port#)
                    // ota password
                    ->enable_ota("1Qwerty!")
                    // use GPIO as settings reset
                    ->set_button_pin(0)
                    ->get_app();

  //************* ads1115 I2C section **************************************************
  // setup ads1115 on I2C bus, ads1115 is 16bit a/d. Raw inputs should not exceed 3.3V
  // Bank 1 (B1) has resitor divider networks on each input to allow for a maximum input voltage
  // of 18V. Vchip = (3.3K/(14.7K + 3.3K)) * Vin. @18V Vin = 3.3V Vchip
  // Bank 2 (B2) (non Lite only) has resitor divider networks to allow for a maximum input voltage
  // of 5V. Vchip = (3.3K/(1.7K + 3.3K)) * Vin. @5V Vin = 3.3V Vchip
  setupADS1115();

  // Create a new sensor for adsB1_A0
  const char *kAlternatorVoltageLinearConfigPath = "/propulsion/alternator voltage/calibrate";
  const char *kAlternatorVoltageSKPath = "/propulsion/alternator voltage/sk_path";
  const float AltVmultiplier = 5.64; // resitor divider ratio, measured
  const float AltVoffset = 0;

  auto* analog_input_adsB1_0 = new RepeatSensor<float>(
      500, read_adsB1_ch0_callback);

  analog_input_adsB1_0    
      ->connect_to(new Linear(AltVmultiplier, AltVoffset, kAlternatorVoltageLinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/propulsion/alternator voltage/sk_path",         // Signal K path
          kAlternatorVoltageSKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "Alternator Voltage")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float AltVoltsValue) {
          AltVolts = AltVoltsValue;
        }));                     

      #ifdef SERIALDEBUG
      // debug - add an observer that prints out the current value of the analog input
      // every time it changes.
      analog_input_adsB1_0->attach([analog_input_adsB1_0]() {
        debugD("Analog input adsB1_0 value: %f Raw Volts", analog_input_adsB1_0->get());
      });
      #endif

  // Create a new sensor for adsB1 channel 1
  const char *kHouseVoltageLinearConfigPath = "/electrical/house voltage/calibrate";
  const char *kHouseVoltageSKPath = "/electrical/house voltage/sk_path";
  const float HouseVmultiplier = 5.64; // resitor divider ratio, measured
  const float HouseVoffset = 0;

  auto* analog_input_adsB1_1 = new RepeatSensor<float>(
      500, read_adsB1_ch1_callback);

  analog_input_adsB1_1    
      ->connect_to(new Linear(HouseVmultiplier, HouseVoffset, kHouseVoltageLinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/electrical/house voltage/voltage",         // Signal K path
          kHouseVoltageSKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "House Battery Voltage")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float HouseVoltsValue) {
          HouseVolts = HouseVoltsValue;
        }));                     

      #ifdef SERIALDEBUG
      // debug - add an observer that prints out the current value of the analog input
      // every time it changes.
      analog_input_adsB1_1->attach([analog_input_adsB1_1]() {
        debugD("Analog input adsB1_1 value: %f Raw Volts", analog_input_adsB1_1->get());
      });
      #endif


  // Create a new sensor for adsB1 channel 2
  const char *kFuelLevelLinearConfigPath = "/vessel/tank/fuel level/calibrate/volts";
  const char *kFuelLevelLinearConfigPath1 = "/vessel/tank/fuel level/calibrate/level";
  const char *kFuelLevelSKPath = "/vessel/tank/fuel level/sk_path";
  const float FuelLevelmultiplier = 5.64; // convert analog inout to raw volts
  const float FuelLeveloffset = 0;
  auto* analog_input_adsB1_2 = new RepeatSensor<float>(
      500, read_adsB1_ch2_callback);

  analog_input_adsB1_2
      ->connect_to(new Linear(FuelLevelmultiplier, FuelLeveloffset, kFuelLevelLinearConfigPath)) // convert to raw volts
      ->connect_to(new Linear(140.0, -93.0, kFuelLevelLinearConfigPath1)) // convert to level, 4.5V = 100%, 8.5V = 0%
      ->connect_to(new SKOutputFloat(
          "/vessel/tank/fuel/level",         // Signal K path
          kFuelLevelSKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Percent",                     // Define output units
                        "Propulsion Fuel Level")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float FuelLevelValue) {
          FuelLevel = FuelLevelValue;
        }));                     

      #ifdef SERIALDEBUG
      // debug - add an observer that prints out the current value of the analog input
      // every time it changes.
      analog_input_adsB1_2->attach([analog_input_adsB1_2]() {
        debugD("Analog input adsB1_2 value: %f Raw Volts", analog_input_adsB1_2->get());
      });
      #endif

  // Create a new sensor for adsB1 channel 3
  const char *kAnalogInputB1A3LinearConfigPath = "/sensors/analog_input_adsB1_3/calibrate";
  const char *kAnalogInputB1A3SKPath = "/sensors/analog_input_adsB1_3/voltage/sk_path";
  const float AnalogInputB1A3multiplier = 5.64; // just read in volts for now
  const float AnalogInputB1A3offset = 0;

  auto* analog_input_adsB1_3 = new RepeatSensor<float>(
      500, read_adsB1_ch3_callback);

  // Connect the analog input to Signal K output. This will publish the
  // analog input value to the Signal K server every time it changes.
  analog_input_adsB1_3
      ->connect_to(new Linear(AnalogInputB1A3multiplier, AnalogInputB1A3offset, kAnalogInputB1A3LinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/sensors/analog_input_adsB1_3/voltage",         // Signal K path
          kAnalogInputB1A3SKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "Analog Input Volts")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float B1A3Value) {
          B1A3 = B1A3Value;
        }));                     

      #ifdef SERIALDEBUG
      // debug - add an observer that prints out the current value of the analog input
      // every time it changes.
      analog_input_adsB1_3->attach([analog_input_adsB1_3]() {
        debugD("Analog input adsB1_3 value: %f Raw Volts", analog_input_adsB1_3->get());
      });
      #endif

// add second bank for non Lite version
#ifndef LITE_V3
  // Create a new sensor for adsB2 channel 0
  const char *kAnalogInputB2A0LinearConfigPath = "/sensors/analog_input_adsB2_0/calibrate";
  const char *kAnalogInputB2A0SKPath = "/sensors/analog_input_adsB2_0/voltage/sk_path";
  const float AnalogInputB2A0multiplier = 1.515; // just read in volts for now
  const float AnalogInputB2A0offset = 0;

  auto* analog_input_adsB2_0 = new RepeatSensor<float>(
      500, read_adsB2_ch0_callback);

  // Connect the analog input to Signal K output. This will publish the
  // analog input value to the Signal K server every time it changes.
  analog_input_adsB2_0
      ->connect_to(new Linear(AnalogInputB2A0multiplier, AnalogInputB2A0offset, kAnalogInputB2A0LinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/sensors/analog_input_adsB2_0/voltage",         // Signal K path
          kAnalogInputB2A0SKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "Analog Input Volts")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float B2A0Value) {
          B2A0 = B2A0Value;
        }));                     

  // Create a new sensor for adsB2 channel 1
  const char *kAnalogInputB2A1LinearConfigPath = "/sensors/analog_input_adsB2_1/calibrate";
  const char *kAnalogInputB2A1SKPath = "/sensors/analog_input_adsB2_1/voltage/sk_path";
  const float AnalogInputB2A1multiplier = 1.515; // just read in volts for now
  const float AnalogInputB2A1offset = 0;

  auto* analog_input_adsB2_1 = new RepeatSensor<float>(
      500, read_adsB2_ch1_callback);

  // Connect the analog input to Signal K output. This will publish the
  // analog input value to the Signal K server every time it changes.
  analog_input_adsB2_1
      ->connect_to(new Linear(AnalogInputB2A1multiplier, AnalogInputB2A1offset, kAnalogInputB2A1LinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/sensors/analog_input_adsB2_1/voltage",         // Signal K path
          kAnalogInputB2A1SKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "Analog Input Volts")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float B2A1Value) {
          B2A1 = B2A1Value;
        }));                     

  // Create a new sensor for adsB2 channel 2
  const char *kAnalogInputB2A2LinearConfigPath = "/sensors/analog_input_adsB2_2/calibrate";
  const char *kAnalogInputB2A2SKPath = "/sensors/analog_input_adsB2_2/voltage/sk_path";
  const float AnalogInputB2A2multiplier = 1.515; // just read in volts for now
  const float AnalogInputB2A2offset = 0;

  auto* analog_input_adsB2_2 = new RepeatSensor<float>(
      500, read_adsB2_ch2_callback);

  // Connect the analog input to Signal K output. This will publish the
  // analog input value to the Signal K server every time it changes.
  analog_input_adsB2_2
      ->connect_to(new Linear(AnalogInputB2A2multiplier, AnalogInputB2A2offset, kAnalogInputB2A2LinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/sensors/analog_input_adsB2_2/voltage",         // Signal K path
          kAnalogInputB2A2SKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "Analog Input Volts")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float B2A2Value) {
          B2A2 = B2A2Value;
        }));                     

  // Create a new sensor for adsB2 channel 3
  const char *kAnalogInputB2A3LinearConfigPath = "/sensors/analog_input_adsB2_3/calibrate";
  const char *kAnalogInputB2A3SKPath = "/sensors/analog_input_adsB2_3/voltage/sk_path";
  const float AnalogInputB2A3multiplier = 1.515; // just read in volts for now
  const float AnalogInputB2A3offset = 0;

  auto* analog_input_adsB2_3 = new RepeatSensor<float>(
      500, read_adsB2_ch3_callback);

  // Connect the analog input to Signal K output. This will publish the
  // analog input value to the Signal K server every time it changes.
  analog_input_adsB2_3
      ->connect_to(new Linear(AnalogInputB2A3multiplier, AnalogInputB2A3offset, kAnalogInputB2A3LinearConfigPath))
  
      ->connect_to(new SKOutputFloat(
          "/sensors/analog_input_adsB2_3/voltage",         // Signal K path
          kAnalogInputB2A3SKPath,        
                                                  // web UI and for storing the
                                                  // configuration
          new SKMetadata("Volts",                     // Define output units
                        "Analog Input Volts")))  // Value description
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float B2A3Value) {
          B2A2 = B2A3Value;
        }));                     
#endif

  // debug - add an observer that prints out the current value of the analog input
  // every time it changes.
  //analog_input_adsB1_1->attach([analog_input_adsB1_1]() {
  //  debugD("Analog input adsB1_1 value: %f Volts", analog_input_adsB1_1->get());
  //});
//*****************end ads115 I2C

  // **************** engine rpm
  //const char *kEngineRPMInputConfig = "/Engine RPM/DigialInput";
  const char *kEngineRPMCalibrate = "/propulsion/engine RPM/calibrate";
  const char *kEngineRPMSKPath = "/propulsion/engine RPM/sk_path";
  const float multiplier = ((1.0/numberPoles)*60.0);
  const float engmultiplier = multiplier * (1.63/pulleyRatio);
  const unsigned int rpm_read_delay = 500;

  // setup input pin for rpm pulse
  pinMode ( Engine_RPM_Pin , INPUT_PULLUP );
  //auto* rpmSensor = new DigitalInputCounter(Engine_RPM_Pin, INPUT_PULLUP, RISING, rpm_read_delay, kEngineRPMInputConfig);
  auto* rpmSensor = new DigitalInputCounter(Engine_RPM_Pin, INPUT_PULLUP, RISING, rpm_read_delay);

  // alternator rpm converted to engine rpm converted to hz for sk
  rpmSensor
      ->connect_to(new Frequency(
         engmultiplier, kEngineRPMCalibrate))  // connect the output of sensor))

      ->connect_to(new SKOutputFloat(
        "/propulsion/engine RPM", 
        kEngineRPMSKPath, 
        new SKMetadata("RPM",                     // Define output units
                     "Engine RPM")))
      
      // this is how we get values out of sensesp :-)
      // connect for n2k purposes
      ->connect_to(
        new LambdaConsumer<float>([](float engRpmValue) {
          EngRPM = engRpmValue;
        }));
  
    // This is a relay that will activate when RPM is high enough: ie) enable a charger at higher rpm
    // Out 1 is for the relay
    pinMode(Out1, OUTPUT_OPEN_DRAIN); // output 1
    const char *kEngineRPMRelayHystresisConfig = "/propulsion/engine RPM relay/config";
    rpmSensor 
        ->connect_to(new Frequency(
          engmultiplier, kEngineRPMCalibrate))  // connect the output of sensor))
        ->connect_to(new Hysteresis<float, bool>(1400, 1500, true, false, kEngineRPMRelayHystresisConfig))
        ->connect_to(new DigitalOutput(Out1));
// *************** end engine rpm

// Oil Pressure Sender Config //
// transducer has .5V - 4.5V for 0-100psi (689476 Pa).
// Analog input is 3.3V raw with a resister divider of 7.5K and 11K.
// analog input returns 0raw=0V and 4095raw=3.3V @ the pin.
// thru resister divider, .5V xducer ~= .2V at chip = .2*4095/3.3 ~= 248 (125 actual)
// 4.5V xducer ~= 1.82V at chip = 1.82*4095/3.3 ~= 2258
const char *kOilPressureADCConfigPath = "/propulsion/Engine Oil Pressure/ADC/scale";
const char *kOilPressureLinearConfigPath = "/propulsion/oil pressure/calibrate";
const char *kOilPressureSKPath = "/propulsion/oil pressure/sk_path";
const float OilPmultiplier = 228;
const float OilPoffset = -82080;

// Oil Pressure ESP32 Analog Input
auto * analog_input = new AnalogInput(OIL_PRESSURE, 500, kOilPressureADCConfigPath, 4096);

analog_input
      // scale using linear transform
      ->connect_to(new Linear(OilPmultiplier, OilPoffset, kOilPressureLinearConfigPath))
      // send to SK, value is in Pa
      ->connect_to(new SKOutputFloat("/propulsion/oil pressure", kOilPressureSKPath))
      // for N2K use, value is in Pa
      ->connect_to(
        new LambdaConsumer<float>([](float engOilPresValue) {
          OilPres = engOilPresValue;
          // if negative then make zero
          if (OilPres <= 0.0) OilPres = 0; // for some reason n2k does not like negative oil pressure??
        }));


// *********** digital inputs
// use pull down so a 1 is the input on - non inverted
// Use In1 for High Temp/Low Oil, when on (+12V) we are in alarm
const uint8_t DigInput1 = In1;
pinMode ( DigInput1 , INPUT_PULLDOWN );

// sampling interval for digital inputs
const unsigned int kDigitalInput1Interval = 250;
const char *kCheckEngineEnableSKPath = "/propulsion/check engine/enable";
const char *kCheckEngineSKPath = "/propulsion/check engine/sk_path";
// Digital input 1  - connect to high temp/low oil alarm
// when input low, no alarm
auto* digitalInput1 = new RepeatSensor<bool>(
    kDigitalInput1Interval,
    [DigInput1]() { return !digitalRead(DigInput1); });

// INput 1 send out to SK
digitalInput1
  
  // bool to enable/disable the check engine
  ->connect_to( 
    new Enable<bool>(true, kCheckEngineEnableSKPath))
  
  ->connect_to(new SKOutputBool(
      "/propulsion/check engine",          // Signal K path
      kCheckEngineSKPath,         // configuration path
      new SKMetadata("",                       // No units for boolean values
                    "Digital Input High Temp/Low Oil Pres")  // Value description
    ))
  
  ->connect_to(
    new LambdaConsumer<bool>([](bool checkEngineValue) {
      chkEng = checkEngineValue;
    }));


  // Digital input 2, sample periodically
  const unsigned int kDigitalInput2Interval = 250;
  const uint8_t DigInput2 = In2;
  pinMode ( DigInput2 , INPUT_PULLDOWN );
  const char *kDigitalInput2SKPath = "/sensors/digital in2/sk_path";
  auto* digitalInput2 = new RepeatSensor<bool>(
      kDigitalInput2Interval,
      [DigInput2]() { return digitalRead(DigInput2); });

  // Input 2 send out to SK
  digitalInput2->connect_to(new SKOutputBool(
      "/sensors/digital in2",          // Signal K path
      kDigitalInput2SKPath,         // configuration path
      new SKMetadata("",                       // No units for boolean values
                     "Digital input 2 value")  // Value description
      ))

    ->connect_to(
      new LambdaConsumer<bool>([](bool digInput2Value) {
        digIn2 = digInput2Value;
      }));

// ******** end digital input

//************* start onewire temperature sensors
/*
     Tell SensESP where the sensor is connected to the board
     ESP32 pins are specified as just the X in GPIOX
  */
  uint8_t OneWirePin = ONE_WIRE_BUS;
  // Define how often SensESP should read the sensor(s) ms
  uint OneWireReadDelay = 1250;

  // start onewire
  dts = new DallasTemperatureSensors(OneWirePin);

  // Below are temperatures sampled and sent to Signal K server
  // To find valid Signal K Paths that fits your need you look at this link:
  // https://signalk.org/specification/1.4.0/doc/vesselsBranch.html


  // Measure engine temperature
  const char *kOWEngineTempConfig = "/propulsion/engine temperature/config";
  const char *kOWEngineTempLinearConfig = "/propulsion/engine temperature/linear";
  const char *kOWEngineTempSKPath = "/propulsion/engine temperature/sk_path";

  auto* engineTemp =
      new OneWireTemperature(dts, OneWireReadDelay, kOWEngineTempConfig);

  engineTemp->connect_to(new Linear(1.0, 0.0, kOWEngineTempLinearConfig))
      ->connect_to(new SKOutputFloat("/propulsion/engine temperature",
                                     kOWEngineTempSKPath))

      ->connect_to(
      new LambdaConsumer<float>([](float engineBlockTempValue) {
        engineBlockTemp = engineBlockTempValue;
      }));

  // Measure engine exhaust temperature
  const char *kOWExhaustTempConfig = "/propulsion/exhaust temperature/config";
  const char *kOWExhaustTempLinearConfig = "/propulsion/exhaust temperature/linear";
  const char *kOWExhaustTempSKPath = "/propulsion/exhaust temperature/sk_path";

  auto* exhaustTemp =
      new OneWireTemperature(dts, OneWireReadDelay, kOWExhaustTempConfig);

  exhaustTemp->connect_to(new Linear(1.0, 0.0, kOWExhaustTempLinearConfig))
      ->connect_to(new SKOutputFloat("/propulsion/exhaust temperature",
                                     kOWExhaustTempSKPath))

      ->connect_to(
            new LambdaConsumer<float>([](float engineExhaustTempValue) {
              engineExhaustTemp = engineExhaustTempValue;
            }));

  // Engine Compartment Temp....sensor floating in the comaprtment
  const char *kOWEngineRoomTempConfig = "/sensors/engine room temperature/config";
  const char *kOWEngineRoomTempLinearConfig = "/sensors/engine room temperature/linear";
  const char *kOWEngineRoomTempSKPath = "/sensors/engine room temperature/sk_path";

  auto* engineRoom =
      new OneWireTemperature(dts, OneWireReadDelay, kOWEngineRoomTempConfig);

  engineRoom->connect_to(new Linear(1.0, 0.0, kOWEngineRoomTempLinearConfig))
      ->connect_to(new SKOutputFloat("/sensors/engine room temperature",
                                     kOWEngineRoomTempSKPath))

      ->connect_to(
          new LambdaConsumer<float>([](float engineRoomTempValue) {
            engineRoomTemp = engineRoomTempValue;
          }));

  // Measure engine coolant temperature
  const char *kOWCoolantTempConfig = "/propulsion/coolant temperature/config";
  const char *kOWCoolantTempLinearConfig = "/propulsion/coolant temperature/linear";
  const char *kOWCoolantTempSKPath = "/propulsion/coolant temperature/sk_path";

  auto* coolantTemp =
      new OneWireTemperature(dts, OneWireReadDelay, kOWCoolantTempConfig);

  coolantTemp->connect_to(new Linear(1.0, 0.0, kOWCoolantTempLinearConfig))
      ->connect_to(new SKOutputFloat("/propulsion/coolant temperature",
                                     kOWCoolantTempSKPath))

      ->connect_to(
      new LambdaConsumer<float>([](float engineCoolantTempValue) {
        engineCoolantTemp = engineCoolantTempValue;
      }));

  // Measure engine coolant temperature
  const char *kOWPCBTempConfig = "/sensors/PCB temperature/config";
  const char *kOWPCBTempLinearConfig = "/sensors/PCB temperature/linear";
  const char *kOWPCBTempSKPath = "/sensors/PCB temperature/sk_path";

  auto* PCBTemp =
      new OneWireTemperature(dts, OneWireReadDelay, kOWPCBTempConfig);

  PCBTemp->connect_to(new Linear(1.0, 0.0, kOWPCBTempLinearConfig))
      ->connect_to(new SKOutputFloat("/sensors/PCB temperature",
                                     kOWPCBTempSKPath));

// ************* end onewire temp sensors

  // Start networking, SK server connections and other SensESP internals
  sensesp_app->start();

  // now start n2k
  setupN2K();

  // No need to parse the messages at every single loop iteration; 100 ms will do
  app.onRepeat(100, []() { doN2Kprocessing(); });

  // if tft then init the screen
  #if defined (INCLUDE_TFT)
    // setup tft
    #ifndef LITE_V3
      do_lvgl_init(1); // 1 for regular, 3 for lite
    #else
      do_lvgl_init(3); // 1 for regular, 3 for lite
    #endif

    // setup an onrepeat to update the display regularly
    app.onRepeat(500, []() { lv_timer_handler(); });
  #endif

} // end setup

// main processing loop
void loop() { 
  // Sensesp update
  app.tick();
} // end loop


// Initialize the ADS1115 module(s)
void setupADS1115 ( void )
{
  // ads1115 stuff
  uint16_t locBank1Present;
  uint16_t locBank2Present;

  #if defined ( SERIALDEBUG )
    Serial . println ( "Getting single-ended readings from AIN0..3" );
    Serial . println ( "ADC Range: +/- 6.144V (1 bit = 3mV/ADS1015, 0.1875mV/ADS1115)" );
  #endif

  // Set the ADC gain to 1. By design we expect input range @ the ADS1115 from 0-3.3V 
  // based on the onboard resistor values.
  //
  // Values at terminals: Bank 1 will divide 0-18V into 0-3.3V
  // Values at terminals: Bank 2 will divide 0-5V into 0-3.3V at the ADC
  //
  //                                                                   ADS1115
  //                                                                   -------
  adsB1 .setGain ( GAIN_ONE );        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  if (!  adsB1  .  begin  ( ADS1115_BANK1_ADDR )) {
    Serial . println ( "Failed to initialize ADS @0x48." );
    locBank1Present = 0; // flag to indicate if bank 1 is available
  } else {
  #if defined ( SERIALDEBUG )
    Serial . println ( "Initialized ADS @0x48 - Bank 1" );
  #endif
    locBank1Present = 1; // flag to indicate if bank 1 is available
  } // end if

#ifndef LITE_V3
  adsB2 .setGain ( GAIN_ONE );        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  if ( ! adsB2 . begin ( ADS1115_BANK2_ADDR )) {
    Serial . println ( "Failed to initialize ADS @0x49." );
    locBank2Present = 0; // flag to indicate if bank 1 is available
  } else {
  #if defined ( SERIALDEBUG )
    Serial . println ( "Initialized ADS @0x49 - bANK 2" );  
  #endif
    locBank2Present = 1; // flag to indicate if bank 1 is available
} // end if
#endif

#if defined ( SERIALDEBUG )
  Serial . println ( "ADS1115 module setup complete." );
#endif

} // end setupADS1115

// setup for N2k
void setupN2K() {
  
  // Set Product information
  // Tweaked to display properly on Raymarine...
  NMEA2000.SetProductInformation("EM V3", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "SN00000001",  // Manufacturer's Model ID
                                 "1.0.0.01 (2024-01-15)",  // Manufacturer's Software version code
                                 #ifndef LITE_V3
                                 "EM V3" // Manufacturer's Model version
                                 #else
                                 "EM V3 Lite" // Manufacturer's Model version
                                 #endif
                                 );
  // Set device information
  NMEA2000.SetDeviceInformation(chipId, // Unique number. Use e.g. Serial number.
                                132, // Device function=Analog -> NMEA2000. See codes on https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25, // Device class=Sensor Communication Interface. See codes on https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2040 // Just choosen free from code list on https://web.archive.org/web/20190529161431/http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );
  // Uncomment 2 rows below to see, what device will send to bus. Use e.g. OpenSkipper or Actisense NMEA Reader                           
  //Serial.begin(115200);
  //#if defined (SERIALDEBUG)
  //NMEA2000.SetForwardStream(&Serial);
  // If you want to use simple ascii monitor like Arduino Serial Monitor, uncomment next line
  //NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.
  //#endif
  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode/*N2km_NodeOnly*/,22);
  locN2KDeviceList = new tN2kDeviceList(&NMEA2000);
  NMEA2000.EnableForward(false); // Disable all msg forwarding to USB (=Serial)
  // Here we tell library, which PGNs we transmit
  NMEA2000.ExtendTransmitMessages(TransmitMessages);
  // Define OnOpen call back. This will be called, when CAN is open and system starts address claiming.
  NMEA2000.SetOnOpen(OnN2kOpen);

  // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
  NMEA2000 . SetN2kCANMsgBufSize ( 32 );
  NMEA2000 . SetN2kCANReceiveFrameBufSize ( 500 );
  NMEA2000 . SetN2kCANSendFrameBufSize ( 500 );

  NMEA2000.Open();
} // end setupN2K

// *****************************************************************************
double ReadCabinTemp() {
  int tmptmp=random(0,300);
  return CToKelvin((float)(tmptmp)/10); // Read here the true temperature e.g. from analog input
}

// *****************************************************************************
double ReadWaterTemp() {
  int tmptmp=random(100,900);
  return CToKelvin((float)(tmptmp)/10); // Read here the true temperature e.g. from analog input
}

// *****************************************************************************
void SendN2kEngine() {
  tN2kMsg N2kMsg;
  tN2kEngineDiscreteStatus1 Status1=0;
  tN2kEngineDiscreteStatus2 Status2=0;
  if ( EngineRapidScheduler.IsTime() ) {
    EngineRapidScheduler.UpdateNextTime();
    SetN2kEngineParamRapid ( N2kMsg , 0 , EngRPM, N2kDoubleNA, N2kInt8NA );
    NMEA2000.SendMsg(N2kMsg);
  } // endif

  if ( EngineDynamicScheduler.IsTime() ) {
    EngineDynamicScheduler.UpdateNextTime();
    
    // if this bit is set then check engine alarm pops up on the screen
    // this is double reversed, in PCB h/w and on the engine.
    // low at the terminal is high in the processor
    // low at the terminal is alarm state.
    Status1.Bits.CheckEngine = chkEng;
    Status2.Bits.MaintenanceNeeded = 0;

    // use engine temp as oil temp for now
    SetN2kEngineDynamicParam ( N2kMsg , 0 , OilPres, engineBlockTemp, engineCoolantTemp, AltVolts,
                               N2kDoubleNA , N2kDoubleNA , N2kDoubleNA , N2kDoubleNA , N2kInt8NA , N2kInt8NA , Status1 , Status2 );

    NMEA2000.SendMsg(N2kMsg);
  } // endif

} // end sendn2kenginerapid

// *****************************************************************************
void SendN2kBattery() {
  tN2kMsg N2kMsg;
  if ( BatteryStatusScheduler.IsTime() ) {
    BatteryStatusScheduler.UpdateNextTime();
    SetN2kDCBatStatus(N2kMsg, 0, HouseVolts, 0/*BatteryCurrent=N2kDoubleNA*/,
                     0/*BatteryTemperature=N2kDoubleNA*/, 0xff);
    NMEA2000.SendMsg(N2kMsg);
  } // endif

} // end SendN2kBattery

// *****************************************************************************
void SendN2kTemperature() {
  tN2kMsg N2kMsg;

  if ( TemperatureScheduler.IsTime() ) {
    TemperatureScheduler.UpdateNextTime();
    //SetN2kTemperature(N2kMsg, 1, 1, N2kts_MainCabinTemperature, ReadCabinTemp());
    SetN2kTemperatureExt(N2kMsg, 255, 1, N2kts_EngineRoomTemperature, engineRoomTemp, N2kDoubleNA);
    NMEA2000.SendMsg(N2kMsg);
    delay(50); // probably not neccessary
    SetN2kTemperatureExt(N2kMsg, 255, 1, N2kts_ExhaustGasTemperature, engineExhaustTemp, N2kDoubleNA);
    NMEA2000.SendMsg(N2kMsg);
  } // endif

} // SendN2kTemperature

// *****************************************************************************
void SendN2kTankLevel() {
  tN2kMsg N2kMsg;

  if ( FuelTankLevelScheduler.IsTime() ) {
    // fuel level
    FuelTankLevelScheduler.UpdateNextTime();
    SetN2kFluidLevel(N2kMsg, 0, N2kft_Fuel, FuelLevel, 75.7);
    NMEA2000.SendMsg(N2kMsg);
  } // end if
} // end send2ktanklevel

// does all N2K send/recieve processing. Send message cadence is controlled with timers.
// This helper is called on a regular timer, Send messages are senbt when thier 
// specifiec timer is done.
void doN2Kprocessing(){
  SendN2kTemperature();
  SendN2kEngine();
  SendN2kBattery();
  SendN2kTankLevel();
  NMEA2000.ParseMessages();
  // record the number of devices on n2k bus
  n2kConnected = locN2KDeviceList->Count();
} // end doN2kprocessing

