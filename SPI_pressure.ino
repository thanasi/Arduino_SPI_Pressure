// example to communicate with SPI pressure sensor
// Sensor: Honeywell HSCDRRD2.5MDSA3
//			-- These are half-duplex
//			-- they transition on the falling edge
//			-- output up to 4 bytes (as long as SS is active)
//				-- 2 bits status + 6 bits data
//				-- 8 bits data
//				-- 8 bits temp
//				-- 3 bits temp (optional) + 5bits DNC (do not care)
//			-- output is ordered MSB first
//			-- clock frequency 50kHz - 800kHz
//			-- 10%-90% VCC output range

#include <SPI.h>
#include <Chrono.h>

// get new data every 1ms
#define UPDATE_PERIOD 1

/******************************
Arduino Nano (5V, ATMega328)
******************************/
// SPI Pins
#define SS_PIN 10
#define MOSI_PIN 11
#define MISO_PIN 12
#define SCK_PIN 13

// arduino nano clock speed is 16MHz
// so pick divider to put the SPI clock
// in the operating range of the sensor
// 16MHz / 32 = 500kHz
#define SPI_CLOCK_DIV SPI_CLOCK_DIV32

/******************************/


/*******************************************
Sensor Specs (Honeywell HSCDRRD2.5MDSA3)
*******************************************/
// max/min pressure in millibar (differential)
float PMAX = 2.5;
float PMIN = -2.5;
uint32_t OUT_MAX = 0x3999;
uint32_t OUT_MIN = 0x0666;

/******************************************/

Chrono timer = Chrono();
Chrono sampler = Chrono();

uint8_t st = 0;
uint32_t t = 0;
uint16_t data = 0;

// masks for separating data for sending
uint8_t LOWER_MASK = B11111111;

// 14-bit data
// 0b0011111111111111
uint16_t DATA_MASK = (B00111111 << 8) | B11111111;


void setup() {

	// set up slave select pin and disable sensor to start
	pinMode(SS_PIN, OUTPUT);
	digitalWrite(SS_PIN, HIGH);

	// set up the SPI pins
	SPI.begin();

	// MSB first
	SPI.setBitOrder(MSBFIRST);

	// set clock divider based on which board we're using
	SPI.setClockDivider(SPI_CLOCK_DIV);

	// clock base value is 0
	// read data on rising edge
	SPI.setDataMode(SPI_MODE0);

	// establish serial connection with the computer
	Serial.begin(115200);

	// make sure we can communicate with the pressure sensor
	// and that it's not throwing any errors
	st = data_status((uint8_t) read_SPI(1));

	// if we get an error code, don't go any further
	while (st==3) {
		Serial.write("Sensor Error --- status 3");
	}

	// start the timer clock
	timer.restart();

	send_init();

}

void loop() {

	if (sampler.metro(UPDATE_PERIOD)) {
		// read sensor
		// ignore last two bytes of temperature data for now
		data = (uint16_t) read_SPI(2);
		st = data_status((uint8_t) (data >> 8));

		// just get the relevant bits - get rid of the status bits
		data = data & DATA_MASK;

		// make sure the data is new, otherwise it's useless
		if (st==0) {
			// get time
			t = timer.get();
			
			// write time and raw sensor bytes to serial
			send_packet(t,data);

			// Serial.print(t);
			// Serial.print(',');
			// Serial.println(get_pressure(data));
		} 
	}

}

void send_init(){
	Serial.print("start");
}

void send_close(){
	Serial.print("done");
}

// send data with most significant bits first
void send_packet(uint32_t tt, uint16_t dd){
	// send 4 bytes of time variable
	// by selecting a different byte of the data each time
	// // starting with the uppermost byte and working our way down
	// for (int i=24; i>=0; i-=8){
	// 	Serial.write( tt & (LOWER_MASK << i));
	// }

	// // pad the uint16 so that it looks like a uint32
	// Serial.write(0x00);
	// Serial.write(0x00);
	// // send 2 bytes of data
	// for (int i=8; i>=0; i-=8){
	// 	Serial.write( dd & (LOWER_MASK << i));
	// }

	Serial.print(tt, DEC);
	Serial.print('\t');
	Serial.print(dd, DEC);
	Serial.print('\n');
}

// read n bytes from SPI
uint32_t read_SPI(uint8_t bytesToRead){

	// max 4 bytes from this sensor
	if (bytesToRead > 4) {
		return 0;
	}

	// ignore silly requests
	if (bytesToRead <= 0) {
		return 0;
	}

	uint8_t inByte = 0;
	uint32_t result = 0;

	// incoming data can be at most 4 bytes long
	// tell the sensor to start transmitting
	digitalWrite(SS_PIN, LOW);

	// read the first byte
	result = SPI.transfer(0x00);
	bytesToRead--;

	// if we want more bytes
	while (bytesToRead > 0) {
		// shift result to next byte
		result = result << 8;
		// read byte
		inByte = SPI.transfer(0x00);
		// concatenate byte
		result = result | inByte;
		bytesToRead--;
	}

	// turn off the sensor by setting select pin high
	digitalWrite(SS_PIN, HIGH);

	return result;	
}

// status of 0 means ok
// status of 1 means command mode
// status of 2 means stale data
// status of 3 means error
uint8_t data_status(uint8_t b) {
	return (b >> 6);
}

float get_pressure(uint16_t dat) {
	return PMIN + (dat - OUT_MIN)*(PMAX - PMIN)/(OUT_MAX - OUT_MIN);
}
