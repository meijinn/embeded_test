/* ----- Pin setting -----
   PC4 - Analog   4 - Encoder SDA
   PC5 - Analog   5 - Encoder SCL
   PD2 - Digital  2 - Encoder A - INT0
   PD3 - Digital  3 - Encoder B - INT1
*/
/*sampling 150us
  pulse par revolution 2048ppr*/
/*//////////////////////////////////////////*/

#include <stdint.h>
#include <Wire.h>
#define AS5600_AS5601_DEV_ADDRESS       0x36
#define AS5600_AS5601_REG_CONF          0x07
#define AS5600_AS5601_REG_STATUS        0x0B
#define AS5601_REG_ABN                  0x09
#define d 6
#define k 0.001885
#define A 0.6

float a, rpm, km, km_new, last_micros, current_micros;
long t, t_old, count, count_old;

volatile int32_t EncoderCount;

void Encoder_GPIO_init(void) {
  DDRD  &= ~((1 << PD2) | (1 << PD3));  // Set PD2 and PD3 as input
  EICRA = 0b00000101; // Trigger event of INT0 and INT1 : Any Logic Change
  EIMSK = 0b00000011; // Enable interrupt INT0 and INT1
  sei();              //Enable Global Interrupt
}

// Encoder "A" pin logic change interrupt callback function
ISR(INT0_vect) {
  updateEncoderCount();
}

// Encoder "B" pin logic change interrupt callback function
ISR(INT1_vect) {
  updateEncoderCount();
}

void updateEncoderCount(void) {
  const static int8_t EncoderIndexTable[] =
    {0, -1, 1, 0,  1, 0, 0, -1,  -1, 0, 0, 1,  0, 1, -1, 0};
  static uint8_t EncoderPinState_Now, EncoderPinState_Prev = 0;

  EncoderPinState_Now = (PIND >> 2) & 0x03; // Bit1 : PD3 (Encoder B), Bit0 : PD2 (Encoder A)
  EncoderCount += EncoderIndexTable[EncoderPinState_Prev << 2 | EncoderPinState_Now];
  EncoderPinState_Prev = EncoderPinState_Now;
}

void Encoder_I2C_init(void) {
  byte error;
  uint8_t data;

  // Read AS5601 status register
  Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
  Wire.write(AS5600_AS5601_REG_STATUS);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_AS5601_DEV_ADDRESS, 1);
  data = Wire.read();
  data &= 0x38;
  if (data != 0x20)
  {
    Serial.print("Magnet error : ");
    if ( !(data & 0x20) )
      Serial.println("Magnet was not detected");
    if (data & 0x10)
      Serial.println("Magnet too weak");
    if (data & 0x08)
      Serial.println("Magnet too strong");
    Serial.println("Stop");
    for (;;);
  }
  else
    Serial.println("Magnet : OK");

  // Set AS5601 resolution 2048ppr
  Serial.println("Change Encoder resolution 2048ppr");
  Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
  Wire.write(AS5601_REG_ABN);
  Wire.write(0b00001000);   // ABN(3:0)
  error = Wire.endTransmission();
  if (error){
    Serial.print("error=");Serial.println(error);
  }
  delay(1);

  // Write AS5601 conf
  Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
  Wire.write(AS5600_AS5601_REG_CONF);
  Wire.write(0b00000111);   // WD = 0b0, FTH = 0b001, SF = 0b11
  error = Wire.endTransmission();
  if (error){
    Serial.print("error=");Serial.println(error);
  }
  delay(1);
}


/*//////////////////////////////////////////*/


void setup() {
  /* I2C init */
  Wire.begin();
  Wire.setClock(400000);

  /* Communication function init */
  Serial.begin(115200);
  while (!Serial);

  /* Peripheral init */
  Encoder_I2C_init();
  Encoder_GPIO_init();
}

void loop() {
  current_micros = micros();
  count = EncoderCount;
  a = (float)(150000000*(count - count_old))/2048/(current_micros - last_micros);
  rpm = 60*a;
  km = d*k*rpm;
  km_new = A*km_new + (1-A)*km;
  Serial.println(km_new);
  count_old = count;
  last_micros = current_micros;
}
