/*

	See changelog.

*/

#include "Arduino.h"
#include "Si4703_Breakout.h"
#include "Wire.h"

Si4703_Breakout::Si4703_Breakout(uint8_t resetPin, uint8_t sdioPin, uint8_t sclkPin)
{
	_resetPin = resetPin;
	_sdioPin = sdioPin;
	_sclkPin = sclkPin;
	_poweredDown = false;

	// Default EUROPE, overwrite later on powerOn function.
	_fmL = 875;
	_fmS = 1;
}

void Si4703_Breakout::powerOn()
{
	if (!_poweredDown) si4703_init();
	else rePowerOn();
}

void Si4703_Breakout::powerOff()
{
	// Si4702/03-C19, Section 4.11, page 20
	// AN230 - Low Power, Bus Accessible - 2.1.2 page 5-6
	readRegisters();

	si4703_registers[SYSCONFIG1] |= (1<<GPIO21);            // GPIOX set to (Low-0x10), to reduce current consumption:
	si4703_registers[SYSCONFIG1] &= ~(1<<GPIO20);           // AN230 Table 4- Powerdown Sequence
	si4703_registers[SYSCONFIG1] |= (1<<GPIO11);            // GPIO3 not set because it mess up XOSCEN
	si4703_registers[SYSCONFIG1] &= ~(1<<GPIO10);

	si4703_registers[POWERCFG] &= ~(1<<DMUTE); 				// Mute enabled
	si4703_registers[TEST1] |= (1<<AHIZEN);                 // Enable AHIZEN (opt)
	si4703_registers[SYSCONFIG1] &= ~(1<<RDS);              // Disable RDS
	si4703_registers[POWERCFG] |= (1<<ENABLE);       	  	// Enable to 1
	si4703_registers[POWERCFG] |= (1<<DISABLE);				// Disable to 1

	updateRegisters();

	delay(2); 	// 1.5 ms max until the device powers down (enable and disable set to 0)
				// AN230 - Figure 9. Powerdown Timing

	_poweredDown = true;
}

void Si4703_Breakout::rePowerOn()
{
	// Si4702/03-C19, Section 4.11, page 21
	// AN230 - Low Power, Bus Accessible - 2.1.2 page 5-6
	readRegisters();

	si4703_registers[POWERCFG] |= (1<<DMUTE);				// Mute disabled
	si4703_registers[TEST1] &= ~(1<<AHIZEN);				// Disable AHIZEN (opt)
	si4703_registers[SYSCONFIG1] |= (1<<RDS);				// Enable RDS
	si4703_registers[POWERCFG] |= (1<<ENABLE);				// Enable to 1
	si4703_registers[POWERCFG] &= ~(1<<DISABLE);			// Disable to 0

	updateRegisters();
}

// 31 Volume levels AN230 - 3.3.3. VOLEXT
void Si4703_Breakout::setVolume(uint8_t volume)
{
	boolean setLowRange = false;
	setLowRange = (volume < 16);
	if (volume > 30) volume = 15;
	if (volume > 15) volume -= 15;

	readRegisters(); // Read the current register set
	
	si4703_registers[SYSCONFIG2] &= 0xFFF0; // Clear volume bits
	si4703_registers[SYSCONFIG2] |= volume; // Set new volume
	if (setLowRange) si4703_registers[SYSCONFIG3] |= (1 << VOLEXT);
	else si4703_registers[SYSCONFIG3] &= ~(1 << VOLEXT);

	updateRegisters(); // Update
}

uint8_t Si4703_Breakout::getVolume()
{
	readRegisters();
	boolean lowRange = (si4703_registers[SYSCONFIG3] & (1 << VOLEXT));
	uint8_t volume = (si4703_registers[SYSCONFIG2] & 0x000F);
	if (!lowRange) volume+=15; // Upper range
	return volume;
}

// 31 Volume levels
uint8_t Si4703_Breakout::increaseVolume()
{
	readRegisters();
	uint8_t volume = (si4703_registers[SYSCONFIG2] & 0x000F);
	++volume;
	if (volume > 15) {
		if (si4703_registers[SYSCONFIG3] & (1 << VOLEXT)) {
			si4703_registers[SYSCONFIG3] &= ~(1 << VOLEXT);
			volume = 1;
		}
		else volume = 15;
	}
	si4703_registers[SYSCONFIG2] &= 0xFFF0; // Clear volume bits
	si4703_registers[SYSCONFIG2] |= volume; // Set new volume

	updateRegisters();

	return volume;
}

// 31 Volume levels
uint8_t Si4703_Breakout::decreaseVolume() 
{
	readRegisters();

	uint8_t volume = (si4703_registers[SYSCONFIG2] & 0x000F);
	--volume;
	if (volume == 255 || volume == 0) {
		if (si4703_registers[SYSCONFIG3] & (1 << VOLEXT)) volume = 0;
		else {
			si4703_registers[SYSCONFIG3] |= (1 << VOLEXT);
			volume = 15;
		}
	}
	si4703_registers[SYSCONFIG2] &= 0xFFF0; // Clear volume bits
	si4703_registers[SYSCONFIG2] |= volume; // Set new volume

	updateRegisters();

	return volume;
}

boolean Si4703_Breakout::toggleMute() 
{
	readRegisters();
	si4703_registers[POWERCFG] ^= (1<<DMUTE);
	updateRegisters();

	return isMuted();
}

boolean Si4703_Breakout::isMuted() 
{
	return !(si4703_registers[POWERCFG] & (1<<DMUTE));
}

//To get the Si4703 inito 2-wire mode, SEN needs to be high and SDIO needs to be low after a reset
//The breakout board has SEN pulled high, but also has SDIO pulled high. Therefore, after a normal power up
//The Si4703 will be in an unknown state. RST must be controlled
void Si4703_Breakout::si4703_init()
{
	pinMode(_resetPin, OUTPUT);
	pinMode(_sdioPin, OUTPUT); //SDIO is connected to A4 for I2C
	digitalWrite(_sdioPin, LOW); //A low SDIO indicates a 2-wire interface
	digitalWrite(_resetPin, LOW); //Put Si4703 into reset
	delay(1); //Some delays while we allow pins to settle
	digitalWrite(_resetPin, HIGH); //Bring Si4703 out of reset with SDIO set to low and SEN pulled high with on-board resistor
	delay(1); //Allow Si4703 to come out of reset

	Wire.begin(); //Now that the unit is reset and I2C inteface mode, we need to begin I2C

	readRegisters();
	si4703_registers[TEST1] = 0x8100; //Enable the oscillator, from AN230 page 12, rev 0.9
	updateRegisters();

	delay(600); //Wait for clock to settle 500 min - from AN230 page 12

	readRegisters();

	si4703_registers[POWERCFG] |= (1<<ENABLE); //Enable bit to 1
	si4703_registers[POWERCFG] &= ~(1<<DISABLE); //Disable bit to 0
	si4703_registers[POWERCFG] |= (1<<DMUTE); //Disable Mute
	si4703_registers[SYSCONFIG1] |= (1<<RDS); //Enable RDS

	updateRegisters();
	delay(110); //Max powerup time, from datasheet (Table 8. FM Receiver Characteristics), page 13
	readRegisters();

	#if defined(EUROPE)
	si4703_registers[SYSCONFIG1] |= (1<<DE); //50micros Europe/Japan setup
	si4703_registers[SYSCONFIG2] &= ~(1<<BAND1); //87.5 - 108 Mhz (US/Europe) (Default)
	si4703_registers[SYSCONFIG2] &= ~(1<<BAND0); //87.5 - 108 Mhz (US/Europe) (Default)
	si4703_registers[SYSCONFIG2] &= ~(1<<SPACE1); //100kHz channel spacing for Europe/Japan
	si4703_registers[SYSCONFIG2] |= (1<<SPACE0); //100kHz channel spacing for Europe/Japan
	_fmL = 875;
	_fmS = 1;
	#endif

	#if defined(USA)
	si4703_registers[SYSCONFIG1] &= ~(1<<DE); //75micros USA/Australia setup
	si4703_registers[SYSCONFIG2] &= ~(1<<BAND1); //87.5 - 108 Mhz (US/Europe) (Default)
	si4703_registers[SYSCONFIG2] &= ~(1<<BAND0); //87.5 - 108 Mhz (US/Europe) (Default)
	si4703_registers[SYSCONFIG2] &= ~(1<<SPACE1); //100kHz channel spacing for US
	si4703_registers[SYSCONFIG2] &= ~(1<<SPACE0); //100kHz channel spacing for US
	_fmL = 875;
	_fmS = 2;
	#endif

	#if defined(JAPAN)
	si4703_registers[SYSCONFIG1] |= (1<<DE); //50micros Europe/Japan setup
	si4703_registers[SYSCONFIG2] &= ~(1<<BAND1); //76 - 108 Mhz (Japan wide range)
	si4703_registers[SYSCONFIG2] |= (1<<BAND0); //76 - 108 Mhz (Japan wide range)
	si4703_registers[SYSCONFIG2] &= ~(1<<SPACE1); //100kHz channel spacing for Europe/Japan
	si4703_registers[SYSCONFIG2] |= (1<<SPACE0); //100kHz channel spacing for Europe/Japan
	_fmL = 760;
	_fmS = 1;
	#endif

	si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
	si4703_registers[SYSCONFIG2] |= 0x0001; //Set volume to lowest

	updateRegisters();
}

// Function for debugging purposes over Serial
void Si4703_Breakout::printRegisters()
{
	Serial.println(">Registres:");
	for(int x = 0x00 ; x<0x10; x++) {
		Serial.print(x,HEX);
		Serial.print(": ");
		Serial.println(si4703_registers[x],HEX);
	}
}

//Read the entire register control set from 0x00 to 0x0F
void Si4703_Breakout::readRegisters()
{
	//Si4703 begins reading from register upper register of 0x0A and reads to 0x0F, then loops to 0x00.
	Wire.requestFrom(SI4703, 32); //We want to read the entire register set from 0x0A to 0x09 = 32 bytes.

	while(Wire.available() < 32) ; //Wait for 16 words/32 bytes to come back from slave I2C device
	//We may want some time-out error here

	//Remember, register 0x0A comes in first so we have to shuffle the array around a bit
	for(int x = 0x0A ; ; x++) { //Read in these 32 bytes
		if(x == 0x10) x = 0; //Loop back to zero
		si4703_registers[x] = Wire.read() << 8;
		si4703_registers[x] |= Wire.read();
		if(x == 0x09) break; //We're done!
	}
}

//Write the current 9 control registers (0x02 to 0x07) to the Si4703
//It's a little weird, you don't write an I2C addres
//The Si4703 assumes you are writing to 0x02 first, then increments
byte Si4703_Breakout::updateRegisters() {

	Wire.beginTransmission(SI4703);
	//A write command automatically begins with register 0x02 so no need to send a write-to address
	//First we send the 0x02 to 0x07 control registers
	//In general, we should not write to registers 0x08 and 0x09
	for(int regSpot = 0x02 ; regSpot < 0x08 ; regSpot++) {
		byte high_byte = si4703_registers[regSpot] >> 8;
		byte low_byte = si4703_registers[regSpot] & 0x00FF;

		Wire.write(high_byte); //Upper 8 bits
		Wire.write(low_byte); //Lower 8 bits
	}

	//End this transmission
	byte ack = Wire.endTransmission();
	if(ack != 0) return(0); // fail

	return(1); //success
}

uint16_t Si4703_Breakout::seekUp()
{
	return seek(SEEK_UP);
}

uint16_t Si4703_Breakout::seekDown()
{
	return seek(SEEK_DOWN);
}

//Seeks out the next available station
//Returns the freq if it made it
//Returns zero if failed
uint16_t Si4703_Breakout::seek(byte seekDirection)
{
	readRegisters();

	//Set seek mode wrap bit	
	// TODO CHECK, reference says it's the other way
	si4703_registers[POWERCFG] |= (1<<SKMODE); //Allow wrap
	//si4703_registers[POWERCFG] &= ~(1<<SKMODE); //Disallow wrap - if you disallow wrap, you may want to tune to 87.5 first


	if(seekDirection == SEEK_DOWN) si4703_registers[POWERCFG] &= ~(1<<SEEKUP); //Disabled to Seek down
	else si4703_registers[POWERCFG] |= 1<<SEEKUP; //Set to Seek up

	si4703_registers[POWERCFG] |= (1<<SEEK); //Start seek
	updateRegisters(); //Seeking will now start

	//Poll to see if STC is set
	do {
		readRegisters();
	}while((si4703_registers[STATUSRSSI] & (1<<STC)) == 0); //Seek complete?

	//readRegisters();
	int valueSFBL = si4703_registers[STATUSRSSI] & (1<<SFBL); //Store the value of SFBL
	si4703_registers[POWERCFG] &= ~(1<<SEEK); //Clear the seek bit after seek has completed
	updateRegisters();

	//Wait for the si4703 to clear the STC as well
	do {
		readRegisters();
	}while((si4703_registers[STATUSRSSI] & (1<<STC)) != 0); //STC complete?

	if(valueSFBL) { //The bit was set indicating we hit a band limit or failed to find a station
		return(0);
	}
	// TODO: check exceptions AN230 page 19

	return getChannel();
}

void Si4703_Breakout::setChannel(uint16_t freq)
{
	// Freq = Scale * Channel + Limit
	// ex: Freq = 973, Scale = 2, Limit = 875
	// Channel = (Freq-Limit)/S = (973-875)/2 = 49
	uint16_t channel;

	channel = (freq-_fmL)/_fmS;

	//These steps come from AN230 page 20 rev 0.5
	readRegisters();
	si4703_registers[CHANNEL] &= 0xFE00; //Clear out the channel bits
	si4703_registers[CHANNEL] |= channel; //Mask in the new channel
	si4703_registers[CHANNEL] |= (1<<TUNE); //Set the TUNE bit to start
	updateRegisters();

	delay(60); // Not required, but used to reduce the Wire transmissions.

	//Poll to see if STC is set
	do {
		readRegisters();
	}while((si4703_registers[STATUSRSSI] & (1<<STC)) == 0); //Tuning complete?

	si4703_registers[CHANNEL] &= ~(1<<TUNE); //Clear the tune after a tune has completed
	updateRegisters();

	delay(1); // Not required, but used to reduce the Wire transmissions.

	//Wait for the si4703 to clear the STC as well
	do {
		readRegisters();
	}while((si4703_registers[STATUSRSSI] & (1<<STC)) != 0); //STC complete?
}

uint16_t Si4703_Breakout::getChannel()
{
	uint16_t channel, freq;

	//Freq(MHz) = 0.100(in Europe) * Channel + 87.5MHz
	//X = 0.1 * Chan + 87.5

	readRegisters();
	channel = si4703_registers[READCHAN] & 0x03FF;
	freq = (channel*_fmS)+_fmL;

	return freq;
}

void Si4703_Breakout::readRDS(char* buffer, long timeout)
{ 
	long endTime = millis() + timeout;
	boolean completed[] = {false, false, false, false};
	int completedCount = 0;
	while(completedCount < 4 && millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)) {
			// ls 2 bits of B determine the 4 letter pairs
			// once we have a full set return
			// if you get nothing after 20 readings return with empty string
			uint16_t b = si4703_registers[RDSB];
			int index = b & 0x03;
			if (! completed[index] && b < 500) {
				completed[index] = true;
				completedCount ++;
				char Dh = (si4703_registers[RDSD] & 0xFF00) >> 8;
				char Dl = (si4703_registers[RDSD] & 0x00FF);
				buffer[index * 2] = Dh;
				buffer[index * 2 +1] = Dl;
				// Serial.print(si4703_registers[RDSD]); Serial.print(" ");
				// Serial.print(index);Serial.print(" ");
				// Serial.write(Dh);
				// Serial.write(Dl);
				// Serial.println();
			}
			delay(40); //Wait for the RDS bit to clear
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime) {
		buffer[0] ='\0';
		return;
	}

	buffer[8] = '\0';
}

/* CODE written by me long ago and not re-tested, the "readRDS" function on this lib does not work, as it seems. Will try to merge with the code beneath.

void Si4703_Breakout::llegirReg(byte& Bh, byte& Bl, byte& Ch, byte& Cl, byte& Dh, byte& Dl) {
	Bh = (si4703_registers[RDSB] & 0xFF00) >> 8;
	Bl = (si4703_registers[RDSB] & 0x00FF);
	Ch = (si4703_registers[RDSC] & 0xFF00) >> 8;
	Cl = (si4703_registers[RDSC] & 0x00FF);
	Dh = (si4703_registers[RDSD] & 0xFF00) >> 8;
	Dl = (si4703_registers[RDSD] & 0x00FF);
}

void Si4703_Breakout::llegeixRDS(char* buffer, long timeout) {
	long endTime = millis() + timeout;
	byte Bh, Bl, Ch, Cl, Dh, Dl, p;
	p = 0;
	boolean go = false;
	while (not go && millis() < endTime) {
		readRegisters();
		if (si4703_registers[STATUSRSSI] & (1<<RDSR)) {
			llegirReg(Bh,Bl,Ch,Cl,Dh,Dl);
			go = ((Bh & 0xF0) == 0x20) && ((Bl & 0x0F) == 0x00);
		}
		delay(40);
	}
	if (millis() < endTime) {
		buffer[p++] = byte(Ch);
		buffer[p++] = byte(Cl);
		buffer[p++] = byte(Dh);
		buffer[p++] = byte(Dl);
	}
	boolean ready = false;
	while(not ready && millis() < endTime && p < 64) {
        readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			llegirReg(Bh,Bl,Ch,Cl,Dh,Dl);

			if ((Bh & 0xF0) == 0x20) {
				if ((Bl & 0x0F) == 0x00) {
					ready = true;
				}
				else {
					buffer[p++] = byte(Ch);
					buffer[p++] = byte(Cl);
					buffer[p++] = byte(Dh);
					buffer[p++] = byte(Dl);
				}
			}
		}
		delay(40);
	}
}

*/

