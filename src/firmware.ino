#include <EEPROM.h>
#include "LibAPRS.h"
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include "Adafruit_VC0706.h" //Camera library
#include <SPI.h>            
#include "ssdv.h"
#include <SD.h>
#include "base91.h"
 
#define cameraconnection Serial1 
#define transmitter Serial2 
#define BUFFER_SIZE 2048
#define chipSelect 53

#define ADC_REFERENCE REF_5V
#define OPEN_SQUELCH false
#define debug

#define SATCALL_SIGN "VU3LLM"  
#define GSCALL_SIGN "VU2CWN"
#define SATTELEM_SSID 11
#define SATDIGI_SSID 6
#define GS_SSID 0       

#define COMMAND_INDICATOR '!'
#define APRS_TOCALL "APUVU1"

#define PTT_VHF (3)             
#define UHF_POWER_DOWN_PIN (2)  
#define THERM_1 (24)

#define TOTAL_RESETS_EEPROM_ADDR 0x00
#define WD_INDUCED_RESETS_EEPROM_ADDR 0x02
#define TX_STATUS 0x04
#define Rx_Frequency 0x06
#define Tx_Frequency 0x15
boolean transmission_is_on;
uint16_t total_resets;
uint16_t wd_induced_resets;
AX25Msg incomingPacket;


File imgFile;
ssdv_t ssdv_encoder;  // SSDV encoder state
char filename[13];
uint8_t ssdv_buffer[SSDV_PKT_SIZE];  // SSDV packet buffer
char base91_encoded[350];
size_t len;
Adafruit_VC0706 cam = Adafruit_VC0706(&cameraconnection);

ISR(WDT_vect) {
  wd_induced_resets++;
  EEPROM.write(WD_INDUCED_RESETS_EEPROM_ADDR, wd_induced_resets);
}

void watchdogSetup(void) {
  cli();
  wdt_reset();
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP3) | (0 << WDP2) | (0 << WDP1) | (1 << WDP0);
  sei();
}

void aprs_msg_callback(struct AX25Msg *msg) {
  
}
void printPacket(uint8_t *arr, size_t size) {
    Serial.print("[ ");
    for (size_t i = 0; i < size; i++) {
        Serial.print("0x");
        if (arr[i] < 0x10) Serial.print("0"); // Add leading zero for single-digit hex values
        Serial.print(arr[i], HEX);
        if (i < size - 1) Serial.print(", ");
        if ((i + 1) % 16 == 0) Serial.println(); // New line every 16 bytes for readability
    }
    Serial.println(" ]");
}

void send_packet() {
  wdt_reset();
   APRS_sendPkt(&base91_encoded[0], len);
   delay(200);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  pinMode(PTT_VHF, OUTPUT);
  pinMode(UHF_POWER_DOWN_PIN, OUTPUT);
  digitalWrite(UHF_POWER_DOWN_PIN, LOW); // Turn on UHF module

  //watchdogSetup();
  EEPROM.get(TOTAL_RESETS_EEPROM_ADDR, total_resets);
  total_resets++;  
  EEPROM.put(TOTAL_RESETS_EEPROM_ADDR, total_resets);
  EEPROM.get(WD_INDUCED_RESETS_EEPROM_ADDR, wd_induced_resets);
  EEPROM.get(TX_STATUS, transmission_is_on);

  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  APRS_setCallsign((char*)SATCALL_SIGN, SATTELEM_SSID);
  APRS_setDestination((char*)APRS_TOCALL, 0);

  APRS_setPower(9);
  APRS_setHeight(9);
  APRS_setGain(9);
  APRS_setDirectivity(1);
  APRS_setPreamble(700);
  APRS_setTail(500);

  APRS_printSettings();
  Serial.print(F("Free RAM:     "));
  Serial.println(freeMemory());


  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present"));
    // don't do anything more:
    return;
  }  
    if (cam.begin()) {
    Serial.println(F("Camera Found:"));
  } else {
    Serial.println(F("No camera found?"));
    return;
  }
  char *reply = cam.getVersion();
  if (reply == 0) {
    Serial.print(F("Failed to get version"));
  } else {
    Serial.println("-----------------");
    Serial.print(reply);
    Serial.println("-----------------");
  }
  cam.setImageSize(VC0706_640x480); 
  uint8_t imgsize = cam.getImageSize();
  Serial.print(F("Image size: "));
  if (imgsize == VC0706_640x480) Serial.println("640x480");

  cam.compressPicture(0x36);
  
  uint8_t image_id = 1;  
  int8_t quality = 4;  
  int pkt_size = SSDV_PKT_SIZE;  

  Serial.print(F("Free RAM after setup: "));
  Serial.println(freeMemory());
}

void loop() {
  
  if (! cam.takePicture()) 
    Serial.println(F("Failed to snap!"));
  else 
    Serial.println(F("Picture taken!"));
  
  // Create an image with the name IMAGExx.JPG
  strcpy(filename, "IMAGE000.JPG");
  for (int i = 0; i < 500; i++) {
    filename[5] = '0' + (i/100);
    filename[6] = '0' + (i/10)%10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
    if (i == 499) {
    Serial.println("No available filenames left.");
    return;
    }
  }
 
  // Open the file for writing
  imgFile = SD.open(filename, FILE_WRITE);

  // Get the size of the image (frame) taken  
  uint32_t jpglen = cam.frameLength();
  Serial.print("Storing ");
  Serial.print(jpglen, DEC);
  Serial.print(" byte image.");

  int32_t time = millis();
  // Read all the data up to # bytes!
  byte wCount = 0; // For counting # of writes
  while (jpglen > 0) {
    // read 32 bytes at a time;
    uint8_t *buffer;
   uint8_t bytesToRead = min((uint32_t)32, jpglen); // change 32 to 64 for a speedup but may not work with all setups!
    buffer = cam.readPicture(bytesToRead);
    imgFile.write(buffer, bytesToRead);
    if(++wCount >= 64) { // Every 2K, give a little feedback so it doesn't appear locked up
      Serial.print('.');
      wCount = 0;
    }
    //Serial.print("Read ");  Serial.print(bytesToRead, DEC); Serial.println(" bytes");
    jpglen -= bytesToRead;
  }
  imgFile.close();

  delay(1000);
  
  imgFile = SD.open(filename, FILE_READ);
  if (!imgFile) {
  Serial.println(F("Failed to open image for reading."));
  return;
  }
  
  uint8_t buffer[BUFFER_SIZE];
  int bytesRead = 0;
  ssdv_enc_init(&ssdv_encoder, SSDV_TYPE_NOFEC, SATCALL_SIGN, 1, SSDV_PKT_SIZE);
  
// Set buffer for SSDV encoding
  ssdv_enc_set_buffer(&ssdv_encoder, ssdv_buffer);
  while ((bytesRead = imgFile.read(buffer, BUFFER_SIZE)) > 0) {
    if (ssdv_enc_feed(&ssdv_encoder, buffer, bytesRead) != SSDV_OK) {
        Serial.println(F("Error feeding data to SSDV encoder."));
        imgFile.close();
        return;
    } else {
        Serial.println(F("Feeded to encoder"));
    }

    while (ssdv_enc_get_packet(&ssdv_encoder) == SSDV_OK) {
        Serial.print(F("Sent SSDV packet to transmitter.\n"));
        // Encode packet to Base91 ASCII
        len = base91_encode(ssdv_buffer, SSDV_PKT_SIZE, base91_encoded);
        base91_encoded[len] = '\0';  // Null-terminate the string

    // Print encoded packet
//    Serial.print("Base91 length: ");
//    Serial.println(len);
//    Serial.println("Base91-encoded SSDV packet:");
//    Serial.println(base91_encoded); 
      send_packet();
    }
}

//  Flush out remaining packets after feeding entire image
while (ssdv_enc_get_packet(&ssdv_encoder) == SSDV_OK) {
    Serial.print(F("Sent SSDV packet to transmitter (flushed).\n"));
    printPacket(ssdv_buffer, SSDV_PKT_SIZE);
    delay(2000);
}

  // Close the image file after reading all data
  imgFile.close();
  time = millis() - time;
  Serial.println("done!");
  Serial.print(time); Serial.println(" ms elapsed");
  cam.cameraFrameBuffCtrl(VC0706_RESUMEFRAME);
  delay(30000);  // Add delay to avoid flooding the transmitter
}
