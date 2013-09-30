/* 
	
	See changelog.

*/

#ifndef Si4703_Breakout_h
#define Si4703_Breakout_h

#define EUROPE
//#define USA
//#define JAPAN


#include "Arduino.h"

class Si4703_Breakout
{
  public:
	Si4703_Breakout(uint8_t resetPin, uint8_t sdioPin, uint8_t sclkPin);
	void powerOn();								// call in setup
	void powerOff();

	void setVolume(uint8_t volume); 			// 0 to 30
	uint8_t getVolume();
	uint8_t increaseVolume();
	uint8_t decreaseVolume();
	boolean toggleMute();
	boolean isMuted();

	uint16_t seekUp(); 							// returns the tuned channel or 0
	uint16_t seekDown(); 				
	void setChannel(uint16_t freq);  			// 3 digit channel number
	uint16_t getChannel();

	void readRDS(char* message, long timeout);	
												// message should be at least 9 chars
												// result will be null terminated
												// timeout in milliseconds
	void printRegisters();
  private:
	uint8_t _resetPin;
	uint8_t _sdioPin;
	uint8_t _sclkPin;
	uint8_t _poweredDown;
	uint16_t _fmL;         						// Band lower limit. L = {875, 760}
	uint16_t _fmS;        						// Space between channels. S = { 1, 2, 0.5 }  Note: 0.5 not implemented
	void si4703_init();
	void rePowerOn();
	uint16_t seek(byte seekDirection);

	void readRegisters();
	byte updateRegisters();

	uint16_t si4703_registers[16]; 				//There are 16 registers, each 16 bits large

	static const int  SI4703 = 0x10; 			//0b._001.0000 = I2C address of Si4703 - note that the Wire function assumes non-left-shifted I2C address, not 0b.0010.000W
	static const uint16_t  I2C_FAIL_MAX = 10; 	//This is the number of attempts we will try to contact the device before erroring out
	static const uint16_t  SEEK_DOWN = 	0; 		//Direction used for seeking. Default is down
	static const uint16_t  SEEK_UP = 1;

	//Define the register names
	static const uint16_t  DEVICEID = 	0x00;
	static const uint16_t  CHIPID = 	0x01;
	static const uint16_t  POWERCFG = 	0x02;
	static const uint16_t  CHANNEL = 	0x03;
	static const uint16_t  SYSCONFIG1 = 0x04;
	static const uint16_t  SYSCONFIG2 = 0x05;
	static const uint16_t  SYSCONFIG3 = 0x06;
	static const uint16_t  TEST1 = 		0x07;
	static const uint16_t  TEST2 = 		0x08;
	static const uint16_t  BOOTCONFIG = 0x09;
	static const uint16_t  STATUSRSSI = 0x0A;
	static const uint16_t  READCHAN = 	0x0B;
	static const uint16_t  RDSA = 		0x0C;
	static const uint16_t  RDSB = 		0x0D;
	static const uint16_t  RDSC = 		0x0E;
	static const uint16_t  RDSD = 		0x0F;

	//Register 0x02 - POWERCFG
	static const uint16_t  SMUTE = 		15;
	static const uint16_t  DMUTE = 		14;
	static const uint16_t  SKMODE = 	10;
	static const uint16_t  SEEKUP = 	9;
	static const uint16_t  SEEK = 		8;
	static const uint16_t  DISABLE = 	6;
	static const uint16_t  ENABLE = 	0;

	//Register 0x03 - CHANNEL
	static const uint16_t  TUNE =		15;

	//Register 0x04 - SYSCONFIG1
	static const uint16_t  RDS = 		12;
	static const uint16_t  DE = 		11;
	static const uint16_t  GPIO31 = 	5;
	static const uint16_t  GPIO30 = 	4;
	static const uint16_t  GPIO21 = 	3;
	static const uint16_t  GPIO20 = 	2;
	static const uint16_t  GPIO11 = 	1;
	static const uint16_t  GPIO10 = 	0;

	//Register 0x05 - SYSCONFIG2
	static const uint16_t  BAND1 = 		7;
	static const uint16_t  BAND0 = 		6;
	static const uint16_t  SPACE1 =		5;
	static const uint16_t  SPACE0 = 	4;

	//Register 0x06 - SYSCONFIG3
	static const uint16_t  VOLEXT = 	8;

	//Register 0x07 - TEST1
	static const uint16_t  XOSCEN = 	15;
	static const uint16_t  AHIZEN = 	14;

	//Register 0x0A - STATUSRSSI
	static const uint16_t  RDSR = 		15;
	static const uint16_t  STC = 		14;
	static const uint16_t  SFBL = 		13;
	static const uint16_t  AFCRL = 		12;
	static const uint16_t  RDSS = 		11;
	static const uint16_t  STEREO =		8;
};

#endif
