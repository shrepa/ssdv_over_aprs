#include "Adafruit_VC0706.h"

// Initialization code used by all constructor types
void Adafruit_VC0706::common_init(void) {
  hwSerial = NULL;
  frameptr = 0;
  bufferLen = 0;
  serialNum = 0;
}

Adafruit_VC0706::Adafruit_VC0706(HardwareSerial *ser) {
  common_init();  // Set everything to common state, then...
  hwSerial = ser; // ...override hwSerial with value passed.
}

boolean Adafruit_VC0706::begin(uint32_t baud) {
    hwSerial->begin(baud);
  return reset();
}

boolean Adafruit_VC0706::reset() {
  uint8_t args[] = {0x0};

  return runCommand(VC0706_RESET, args, 1, 5);
}

char *Adafruit_VC0706::getVersion(void) {
  uint8_t args[] = {0x01};

  sendCommand(VC0706_GEN_VERSION, args, 1);
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

boolean Adafruit_VC0706::setImageSize(uint8_t x) {
  uint8_t args[] = {0x05, 0x04, 0x01, 0x00, 0x19, x};

  if (x < VC0706_1024x768)
    // standard image resolution
    args[1] = 0x04;
  else
    // extended image resolution
    args[1] = 0x05;

  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

uint8_t Adafruit_VC0706::getDownsize(void) {
  uint8_t args[] = {0x0};
  if (!runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6))
    return -1;

  return camerabuff[5];
}

boolean Adafruit_VC0706::setDownsize(uint8_t newsize) {
  uint8_t args[] = {0x01, newsize};

  return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5);
}

char *Adafruit_VC0706::setBaud115200() {
  uint8_t args[] = {0x03, 0x01, 0x0D, 0xA6};

  sendCommand(VC0706_SET_PORT, args, sizeof(args));
  // get reply
  if (!readResponse(CAMERABUFFSIZ, 200))
    return 0;
  camerabuff[bufferLen] = 0; // end it!
  return (char *)camerabuff; // return it!
}

boolean Adafruit_VC0706::setCompression(uint8_t c) {
  uint8_t args[] = {0x5, 0x1, 0x1, 0x12, 0x04, c};
  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

uint8_t Adafruit_VC0706::getCompression(void) {
  uint8_t args[] = {0x4, 0x1, 0x1, 0x12, 0x04};
  runCommand(VC0706_READ_DATA, args, sizeof(args), 6);
  printBuff();
  return camerabuff[5];
}

boolean Adafruit_VC0706::cameraFrameBuffCtrl(uint8_t command) {
  uint8_t args[] = {0x1, command};
  return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5);
}

uint32_t Adafruit_VC0706::frameLength(void) {
  uint8_t args[] = {0x01, 0x00};
  if (!runCommand(VC0706_GET_FBUF_LEN, args, sizeof(args), 9))
    return 0;

  uint32_t len;
  len = camerabuff[5];
  len <<= 8;
  len |= camerabuff[6];
  len <<= 8;
  len |= camerabuff[7];
  len <<= 8;
  len |= camerabuff[8];

  return len;
}
boolean Adafruit_VC0706::takePicture() {
  frameptr = 0;
  return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME);
}

uint8_t Adafruit_VC0706::available(void) { return bufferLen; }

uint8_t *Adafruit_VC0706::readPicture(uint8_t n) {
  uint8_t args[] = {0x0C,
                    0x0,
                    0x0A,
                    (uint8_t)((frameptr >> 24) & 0xFF),
                    (uint8_t)((frameptr >> 16) & 0xFF),
                    (uint8_t)((frameptr >> 8) & 0xFF),
                    (uint8_t)(frameptr & 0xFF),
                    0,
                    0,
                    0,
                    n,
                    CAMERADELAY >> 8,
                    CAMERADELAY & 0xFF};

  if (!runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, false))
    return 0;

  // read into the buffer PACKETLEN!
  if (readResponse(n + 5, CAMERADELAY) == 0)
    return 0;

  frameptr += n;
  return camerabuff;
}

boolean Adafruit_VC0706::runCommand(uint8_t cmd, uint8_t *args, uint8_t argn,
                                    uint8_t resplen, boolean flushflag) {
  // flush out anything in the buffer?
  if (flushflag) {
    readResponse(100, 10);
  }

  sendCommand(cmd, args, argn);
  if (readResponse(resplen, 200) != resplen)
    return false;
  if (!verifyResponse(cmd))
    return false;
  return true;
}

void Adafruit_VC0706::sendCommand(uint8_t cmd, uint8_t args[] = 0,
                                  uint8_t argn = 0) {
    hwSerial->write((byte)0x56);
    hwSerial->write((byte)serialNum);
    hwSerial->write((byte)cmd);

    for (uint8_t i = 0; i < argn; i++) {
      hwSerial->write((byte)args[i]);
      // Serial.print(" 0x");
      // Serial.print(args[i], HEX);
    }
  // Serial.println();
}

uint8_t Adafruit_VC0706::readResponse(uint8_t numbytes, uint8_t timeout) {
  uint8_t counter = 0;
  bufferLen = 0;
  int avail;

  while ((timeout != counter) && (bufferLen != numbytes)) {
    avail = hwSerial->available();
    if (avail <= 0) {
      delay(1);
      counter++;
      continue;
    }
    counter = 0;
    // there's a byte!
    camerabuff[bufferLen++] = hwSerial->read();
  }
  // printBuff();
  // camerabuff[bufferLen] = 0;
  // Serial.println((char*)camerabuff);
  return bufferLen;
}

boolean Adafruit_VC0706::verifyResponse(uint8_t command) {
  if ((camerabuff[0] != 0x76) || (camerabuff[1] != serialNum) ||
      (camerabuff[2] != command) || (camerabuff[3] != 0x0))
    return false;
  return true;
}

void Adafruit_VC0706::printBuff() {
  for (uint8_t i = 0; i < bufferLen; i++) {
    Serial.print(" 0x");
    Serial.print(camerabuff[i], HEX);
  }
  Serial.println();
}

boolean Adafruit_VC0706::compressPicture(uint8_t factor) {
  uint8_t args[] = {0x05, 0x01, 0x01, 0x12, 0x04, factor};
  return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

uint8_t Adafruit_VC0706::getImageSize() {
  uint8_t args[] = {0x4, 0x4, 0x1, 0x00, 0x19};
  if (!runCommand(VC0706_READ_DATA, args, sizeof(args), 6))
    return -1;

  return camerabuff[5];
}
