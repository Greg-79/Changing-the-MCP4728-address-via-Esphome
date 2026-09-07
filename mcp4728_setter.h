// my_components/mcp4728_setter.h
#include <Arduino.h>
#include "esphome.h"

#define I2C_READ 1
#define I2C_WRITE 0

class TwoWireBase {
public:
  virtual uint8_t read(uint8_t last) = 0;
  virtual uint8_t restart(uint8_t addressRW) = 0;
  virtual uint8_t start(uint8_t addressRW) = 0;
  virtual void stop(void) = 0;
  virtual uint8_t write(uint8_t data) = 0;
};

#define I2C_DELAY_USEC 10

class SoftI2cMaster : public TwoWireBase {
  uint8_t sclPin_;
  uint8_t sdaPin_;
public:
  void init(uint8_t sclPin, uint8_t sdaPin);
  uint8_t read(uint8_t last);
  uint8_t restart(uint8_t addressRW);
  uint8_t start(uint8_t addressRW);
  void stop(void);
  uint8_t write(uint8_t b);
  uint8_t ldacwrite(uint8_t b, uint8_t);
};

// ----- Реализация методов -----
void SoftI2cMaster::init(uint8_t sclPin, uint8_t sdaPin) {
  sclPin_ = sclPin;
  sdaPin_ = sdaPin;
  pinMode(sclPin_, OUTPUT);
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, OUTPUT);
  digitalWrite(sclPin_, HIGH);
  digitalWrite(sdaPin_, HIGH);
}

uint8_t SoftI2cMaster::read(uint8_t last) {
  uint8_t b = 0;
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, INPUT);
  for (uint8_t i = 0; i < 8; i++) {
    b <<= 1;
    delayMicroseconds(I2C_DELAY_USEC);
    digitalWrite(sclPin_, HIGH);
    if (digitalRead(sdaPin_)) b |= 1;
    digitalWrite(sclPin_, LOW);
  }
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, OUTPUT);
  digitalWrite(sdaPin_, last);
  digitalWrite(sclPin_, HIGH);
  delayMicroseconds(I2C_DELAY_USEC);
  digitalWrite(sclPin_, LOW);
  digitalWrite(sdaPin_, HIGH);
  return b;
}

uint8_t SoftI2cMaster::restart(uint8_t addressRW) {
  digitalWrite(sclPin_, HIGH);
  return start(addressRW);
}

uint8_t SoftI2cMaster::start(uint8_t addressRW) {
  digitalWrite(sdaPin_, LOW);
  delayMicroseconds(I2C_DELAY_USEC);
  digitalWrite(sclPin_, LOW);
  return write(addressRW);
}

void SoftI2cMaster::stop(void) {
  digitalWrite(sdaPin_, LOW);
  delayMicroseconds(I2C_DELAY_USEC);
  digitalWrite(sclPin_, HIGH);
  delayMicroseconds(I2C_DELAY_USEC);
  digitalWrite(sdaPin_, HIGH);
  delayMicroseconds(I2C_DELAY_USEC);
}

uint8_t SoftI2cMaster::write(uint8_t b) {
  for (uint8_t m = 0X80; m != 0; m >>= 1) {
    digitalWrite(sdaPin_, m & b);
    digitalWrite(sclPin_, HIGH);
    delayMicroseconds(I2C_DELAY_USEC);
    digitalWrite(sclPin_, LOW);
  }
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, INPUT);
  digitalWrite(sclPin_, HIGH);
  b = digitalRead(sdaPin_);
  digitalWrite(sclPin_, LOW);
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, OUTPUT);
  return b == 0;
}

uint8_t SoftI2cMaster::ldacwrite(uint8_t b, uint8_t ldacpin) {
  for (uint8_t m = 0X80; m != 0; m >>= 1) {
    digitalWrite(sdaPin_, m & b);
    digitalWrite(sclPin_, HIGH);
    delayMicroseconds(I2C_DELAY_USEC);
    digitalWrite(sclPin_, LOW);
  }
  digitalWrite(ldacpin, LOW);
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, INPUT);
  digitalWrite(sclPin_, HIGH);
  b = digitalRead(sdaPin_);
  digitalWrite(sclPin_, LOW);
  digitalWrite(sdaPin_, HIGH);
  pinMode(sdaPin_, OUTPUT);
  return b == 0;
}

// ----- Глобальный объект I2C и функции -----
SoftI2cMaster i2c;

// ----- Исправленная функция чтения адреса -----
uint8_t readAddress(int LDACpin) {
  digitalWrite(LDACpin, HIGH);
  int ack1 = i2c.start(0B00000000);
  int ack2 = i2c.ldacwrite(0B00001100, LDACpin);
  int ack3 = i2c.restart(0B11000001);
  uint8_t raw = i2c.read(true);
  i2c.stop();
  delay(100);
  digitalWrite(LDACpin, HIGH);
  uint8_t addr = (raw >> 1) & 0x07;   // правильный адрес (0..7)
  ESP_LOGI("mcp", "ACKs read: %d %d %d", ack1, ack2, ack3);
  ESP_LOGI("mcp", "Raw: 0x%02X, Address: %d (0x%02X)", raw, addr, 0x60 + addr);
  return addr;   // теперь возвращаем корректный адрес
}

void writeAddress(int oldAddress, int newAddress, int LDACpin) {
  digitalWrite(LDACpin, HIGH);
  int ack1 = i2c.start(0B11000000 | (oldAddress << 1));
  int ack2 = i2c.ldacwrite(0B01100001 | (oldAddress << 2), LDACpin);
  int ack3 = i2c.write(0B01100010 | (newAddress << 2));
  int ack4 = i2c.write(0B01100011 | (newAddress << 2));
  i2c.stop();
  delay(100);
  ESP_LOGI("mcp", "ACKs write: %d %d %d %d", ack1, ack2, ack3, ack4);
}

void init_i2c(int sda_pin, int scl_pin, int ldac_pin) {
  pinMode(ldac_pin, OUTPUT);
  digitalWrite(ldac_pin, HIGH);
  i2c.init(scl_pin, sda_pin);
  ESP_LOGI("mcp", "I2C initialized (SDA=%d, SCL=%d, LDAC=%d)", sda_pin, scl_pin, ldac_pin);
}

void change_address(int old_addr, int new_addr, int ldac_pin) {
  ESP_LOGI("mcp", "Changing address from %d to %d...", old_addr, new_addr);
  writeAddress(old_addr, new_addr, ldac_pin);
  ESP_LOGI("mcp", "New address set to: 0x%02X", 0x60 + new_addr);
}
