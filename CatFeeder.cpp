/*
 Sainsmart LCD Shield for Arduino
 Key Grab v0.2
 Written by jacky

 www.sainsmart.com

 Displays the currently pressed key on the LCD screen.

 Key Codes (in left-to-right order):

 None   - 0
 Select - 1
 Left   - 2
 Up     - 3
 Down   - 4
 Right  - 5

 */

//#include <DateTime.h>
//#include <DateTimeStrings.h>
#include <LiquidCrystal.h>
#include <DFR_Key.h>
#include <Wire.h>
#include <DS1307new.h>
#include <EEPROM.h>
//#include <Ports.h>
//#include <PortsLCD.h>

#define DS1307_I2C_ADDRESS 0x68                          // I2C Adress
#define NORMAL_MODE		 0
#define SET_CLOCK_MODE   1
#define SET_FEED_MODE    2
#define LOW_POWER_MODE   3
#define FEED_MODE        4

#define POS_DAY    0
#define POS_MONTH  1
#define POS_YEAR   2
#define POS_HOUR   3
#define POS_MINUTE 4
#define POS_CUP    5

#define ADDR_START   0
#define ADDR_TIME_ENTRY_1 2
#define ADDR_TIME_ENTRY_2 6
#define ADDR_TIME_ENTRY_3 10
#define ADDR_TIME_ENTRY_4 14

#define END_SWITCH_PIN 12
#define RELAIS_PIN 13

//Pin assignments for SainSmart LCD Keypad Shield
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//---------------------------------------------

DFR_Key keypad;

//int localKey = 0;
//String keyString = "";

uint16_t startAddr = 0x0000;            // Start address to store in the NV-RAM
uint16_t lastAddr;                      // new address for storing in NV-RAM
uint16_t TimeIsSet = 0xaa55;            // Helper that time must not set again
uint8_t parts;
uint8_t seconds;
uint8_t minutes;
uint8_t hours;
uint8_t keyPressed;
uint8_t mode;
uint8_t position;
uint32_t feedTimes[4];
uint8_t feedIndex;
int prevKey;
int waitTime;
int partsPerSec;
byte feedHour;
byte feedMinute;
byte feedCup;
uint8_t remainingCups;
uint8_t displayTimeout;
char buffer[20] = "";                  // Speicherbereich für Datenkonvertierung

byte bcdToDec(byte val)
{
	return ((val / 16 * 10) + (val % 16));
}

void showFeedTime(int index)
{
	if (index < 1 || index > 4)
		return;

	byte *ptr = (byte *) &feedTimes[index - 1];
	feedMinute = ptr[0];
	feedHour = ptr[1];
	feedCup = ptr[2];

	Serial.println(itoa(ptr[0], buffer, 10));
	Serial.println(itoa(ptr[1], buffer, 10));

	/*
	 lcd.setCursor(0, 1);
	 lcd.write( itoa (i, buffer, 10 ) );
	 lcd.write("> ");

	 if ( hour < 10 )
	 lcd.write( "0" );
	 lcd.write( itoa( hour, buffer, 10 ) );
	 lcd.write( ":" );
	 if ( minute < 10 )
	 lcd.write ( "0" );
	 lcd.write( itoa( minute, buffer, 10 ) );
	 lcd.write( "| ");
	 lcd.write( itoa ( cups, buffer, 10 ) );
	 */
}

void storeFeedTime(int index)
{
	byte * p = (byte*) &feedTimes[index - 1];
	uint8_t addr;

	p[0] = feedMinute; //((byte) feedMinute / 10) << 4 | (byte) feedMinute % 10;
	p[1] = feedHour; //((byte) feedHour / 10) << 4 | (byte) feedHour % 10;
	p[2] = feedCup;
	p[3] = 0;

	switch (index)
	{
	case 1:
		addr = ADDR_TIME_ENTRY_1;
		break; //RTC.setRAM(ADDR_TIME_ENTRY_1, (uint8_t *)&feedTimes[0], sizeof(uint32_t)); break;
	case 2:
		addr = ADDR_TIME_ENTRY_2;
		break; //RTC.setRAM(ADDR_TIME_ENTRY_2, (uint8_t *)&feedTimes[1], sizeof(uint32_t)); break;
	case 3:
		addr = ADDR_TIME_ENTRY_3;
		break; //RTC.setRAM(ADDR_TIME_ENTRY_3, (uint8_t *)&feedTimes[2], sizeof(uint32_t)); break;
	case 4:
		addr = ADDR_TIME_ENTRY_4;
		break; //RTC.setRAM(ADDR_TIME_ENTRY_4, (uint8_t *)&feedTimes[3], sizeof(uint32_t)); break;
	default:
		return;
	}

	for (int i = 0; i < 4; i++)
	{
		Serial.println(itoa(p[i], buffer, 10));

		EEPROM.write(addr + i, p[i]);
	}
}

void setup()
{
	byte * ptr;

	Serial.begin(9600);
	Serial.println("Started");

	lcd.begin(16, 2);
	lcd.clear();
	//lcd.write( "DS1307 - Uhr" );
	//RTC.setRAM(0, (uint8_t *)&startAddr, sizeof(uint16_t));// Store startAddr in NV-RAM address 0x08

	/*
	 Uncomment the next 2 lines if you want to SET the clock
	 Comment them out if the clock is set.
	 DON'T ASK ME WHY: YOU MUST UPLOAD THE CODE TWICE TO LET HIM WORK
	 AFTER SETTING THE CLOCK ONCE.
	 */
//  TimeIsSet = 0xffff;
//  RTC.setRAM(54, (uint8_t *)&TimeIsSet, sizeof(uint16_t));
	/*
	 Control the clock.
	 Clock will only be set if NV-RAM Address does not contain 0xaa.
	 DS1307 should have a battery backup.
	 */
	//RTC.getRAM(54, (uint8_t *)&TimeIsSet, sizeof(uint16_t));
	//if (TimeIsSet != 0xaa55)
	//{
	//  RTC.stopClock();
	//
	//  RTC.fillByYMD(2011,4,8);
	//  RTC.fillByHMS(22,7,0);
	//
	//  RTC.setTime();
	//  TimeIsSet = 0xaa55;
	//  RTC.setRAM(54, (uint8_t *)&TimeIsSet, sizeof(uint16_t));
	//  RTC.startClock();
	//}
	//else
	//{
	if ( !RTC.isPresent() )
	{
		lcd.write("No clock detected");
	}

	RTC.getTime();
	//}

	/*
	 Control Register for SQW pin which can be used as an interrupt.
	 */
	RTC.ctrl = 0x00;                      // 0x00=disable SQW pin, 0x10=1Hz,
										  // 0x11=4096Hz, 0x12=8192Hz, 0x13=32768Hz
	RTC.setCTRL();

	uint8_t MESZ = RTC.isMEZSummerTime();

	mode = NORMAL_MODE;
	seconds = RTC.second;
	minutes = RTC.minute;
	hours = RTC.hour;
	position = POS_DAY;
	parts = 0;
	partsPerSec = 5;
	waitTime = 200;
	keyPressed = 0;
	feedIndex = 1;
	feedHour = 0;
	feedMinute = 0;
	feedCup = 0;
	displayTimeout = 0;
	remainingCups = 0;

	for (int i = 0; i < sizeof(feedTimes) / sizeof(feedTimes[0]); i++)
	{
		uint8_t addr;
		uint8_t * ptr = (uint8_t *) &feedTimes[i];

		switch (i)
		{
		case 0:
			addr = ADDR_TIME_ENTRY_1;
			break;
		case 1:
			addr = ADDR_TIME_ENTRY_2;
			break;
		case 2:
			addr = ADDR_TIME_ENTRY_3;
			break;
		case 3:
			addr = ADDR_TIME_ENTRY_4;
			break;
		default:
			break;
		}

		for (int x = 0; x < 4; x++)
		{
			uint8_t val = EEPROM.read(addr + x);

			if (val == 255)
			{
				val = 0;
			}

			ptr[x] = val;

			Serial.println ( itoa ( ptr[x], buffer, 10 ) );
		}
	}

	showFeedTime(1);

	pinMode(RELAIS_PIN, OUTPUT);
	pinMode(END_SWITCH_PIN, INPUT);
	digitalWrite(RELAIS_PIN, HIGH);
}

void loop()
{

	int value = analogRead(0);

	Serial.print ( itoa ( value, buffer, 10 ) );

	int key = keypad.getKey();

	if ( key != NO_KEY && mode == LOW_POWER_MODE )
	{
		mode = NORMAL_MODE;
	}

	switch ( key )
	{
	case NO_KEY:

		keyPressed = 0;
		if ( mode != SET_FEED_MODE )
		{
			if ( prevKey == DOWN_KEY )
			{
				feedIndex++;
				if (feedIndex == 5)
					feedIndex = 1;
				showFeedTime(feedIndex);
			}
			else if ( prevKey == UP_KEY )
			{
				feedIndex--;
				if ( feedIndex == 0 )
					feedIndex = 4;
				showFeedTime(feedIndex);
			}
		}

		prevKey = NO_KEY;
		break;

	case SELECT_KEY:

		Serial.println("Select");

		if ( mode == SET_CLOCK_MODE && keyPressed == 0 )
		{
			Serial.println("Leaving edit mode");

			mode = NORMAL_MODE;

			//RTC.isMEZSummerTime();
			Serial.print(RTC.day);
			Serial.print(RTC.month);
			Serial.print(RTC.year);
			Serial.print(RTC.hour);
			Serial.print(RTC.minute);
			Serial.print(RTC.dow);

			RTC.second = 0;
			RTC.setTime();
			RTC.startClock();
		}
		else if ( mode == SET_FEED_MODE && keyPressed == 0 )
		{
			Serial.println("Leaving set mode");

			storeFeedTime(feedIndex);
			showFeedTime(feedIndex);
			mode = NORMAL_MODE;
		}
		else
		{
			keyPressed++;
			if ( keyPressed == 10 )
			{
				Serial.println("Entering edit mode");
				mode = SET_CLOCK_MODE;
				//keyPressed = 0;
				prevKey = NO_KEY;
				position = POS_DAY;
				RTC.stopClock();
				lcd.setCursor(0, 0);
			}
		}
		break;

	case RIGHT_KEY:
		Serial.println("Right");

		if ( mode == SET_CLOCK_MODE )
		{
			switch ( position )
			{
			case POS_DAY:
				position = POS_MONTH;
				break;
			case POS_MONTH:
				position = POS_YEAR;
				break;
			case POS_YEAR:
				position = POS_HOUR;
				break;
			case POS_HOUR:
				position = POS_MINUTE;
				break;
			case POS_MINUTE:
				position = POS_DAY;
				break;
			}

			lcd.setCursor(0, 0);
		}
		else if ( mode == SET_FEED_MODE )
		{
			switch ( position )
			{
			case POS_HOUR:
				position = POS_MINUTE;
				break;
			case POS_MINUTE:
				position = POS_CUP;
				break;
			case POS_CUP:
				position = POS_HOUR;
				break;
			}

			lcd.setCursor(0, 1);
		}
		break;

	case UP_KEY:
	case DOWN_KEY:

		Serial.println(key == UP_KEY ? "Up" : "Down");

		if ( mode == SET_CLOCK_MODE )
		{
			switch ( position )
			{
			case POS_DAY:
				if ( key == UP_KEY )
					RTC.day++;
				else
					RTC.day--;
				if ( RTC.day == 0 )
					RTC.day = 31;
				if ( RTC.day == 31 )
					RTC.day = 1;
				break;

			case POS_MONTH:
				if ( key == UP_KEY )
					RTC.month++;
				else
					RTC.month--;
				if ( RTC.month == 0 )
					RTC.month = 12;
				if ( RTC.month == 12 )
					RTC.month = 1;
				break;

			case POS_YEAR:
				if ( key == UP_KEY )
					RTC.year++;
				else
					RTC.year--;
				break;

			case POS_HOUR:
				if (key == UP_KEY)
					RTC.hour++;
				else
					RTC.hour--;
				if ( RTC.hour == 255 )
					RTC.hour = 23;
				if ( RTC.hour == 24 )
					RTC.hour = 0;
				hours = RTC.hour;
				break;

			case POS_MINUTE:
				if ( key == UP_KEY )
					RTC.minute++;
				else
					RTC.minute--;
				if ( RTC.minute == 255 )
					RTC.minute = 59;
				if ( RTC.minute == 60 )
					RTC.minute = 0;
				minutes = RTC.minute;
				break;
			}
		}
		else if ( mode == SET_FEED_MODE )
		{
			switch ( position )
			{
			case POS_HOUR:
				if (  key == UP_KEY )
					feedHour++;
				else
					feedHour--;
				if ( feedHour == 255 )
					feedHour = 23;
				if ( feedHour == 24 )
					feedHour = 0;
				break;

			case POS_MINUTE:
				if ( key == UP_KEY )
					feedMinute++;
				else
					feedMinute--;
				if ( feedMinute == 255 )
					feedMinute = 59;
				if ( feedMinute == 60 )
					feedMinute = 0;
				break;

			case POS_CUP:
				if ( key == UP_KEY )
					feedCup++;
				else
					feedCup--;
				if ( feedCup == 255 )
					feedCup = 9;
				if ( feedCup == 10 )
					feedCup = 0;
				break;
			}
		}
		else
		{
			prevKey = key;
			if ( key == DOWN_KEY )
			{
				keyPressed++;
				if ( keyPressed == 10 )
				{
					Serial.println("Entering set feed mode");

					mode = SET_FEED_MODE;

					prevKey = NO_KEY;
					//keyPressed = 0;
					position = POS_HOUR;
				}
			}
		}

		break;
	}

	int timeout = mode == LOW_POWER_MODE ? 1 : 50;
	if ( key == NO_KEY )
	{
		displayTimeout++;
		if ( displayTimeout == timeout ) // 10 seconds timeout
		{
			pinMode(10, OUTPUT);
			digitalWrite(10, LOW);

			if ( mode == SET_CLOCK_MODE )
			{
				RTC.getTime();
				seconds = RTC.second;
				minutes = RTC.minute;
				hours = RTC.hour;
				mode = NORMAL_MODE;
			}
			else if ( mode == SET_FEED_MODE )
			{
				showFeedTime(feedIndex);
				mode = NORMAL_MODE;
			}
		}
	}
	else
	{
		if ( displayTimeout >= timeout )
		{
			prevKey = NO_KEY;
		}

		displayTimeout = 0;
		pinMode(10, INPUT);
	}

	lcd.setCursor(0, 0);              // Datum und Uhrzeit in 1. Zeile schreiben

	int blinkClock = mode == SET_CLOCK_MODE && seconds % 2
			&& (key == NO_KEY || key == SELECT_KEY);

	if ( blinkClock && position == POS_DAY )
	{
		lcd.write("  ");
	}
	else
	{
		if (RTC.day < 10)
			lcd.write("0");
		lcd.write ( itoa ( RTC.day, buffer, 10 ) );
	}

	lcd.write(".");

	if ( blinkClock && position == POS_MONTH )
	{
		lcd.write("  ");
	}
	else
	{
		if (RTC.month < 10)
			lcd.write("0");
		lcd.write ( itoa ( RTC.month, buffer, 10 ) );
	}

	lcd.write(".");

	if ( blinkClock && position == POS_YEAR )
	{
		lcd.write ("    ");
	}
	else
	{
		lcd.write ( itoa ( RTC.year, buffer, 10 ) );
	}

	lcd.write(" ");
	//lcd.setCursor(0, 1);                                // Datum und Uhrzeit in 2. Zeile schreiben

	if ( blinkClock && position == POS_HOUR )
	{
		lcd.write("  ");
	}
	else
	{
		if (hours < 10)
			lcd.write("0");
		lcd.write(itoa(hours, buffer, 10));
	}

	if ( mode == SET_CLOCK_MODE || seconds % 2 )
	{
		lcd.write(":");
	}
	else
	{
		lcd.write (" ");
	}

	if ( blinkClock && position == POS_MINUTE )
	{
		lcd.write ("  ");
	}
	else
	{
		if ( minutes < 10 )
			lcd.write("0");

		lcd.write( itoa ( minutes, buffer, 10 ) );
	}

	lcd.setCursor(0, 1);

	lcd.write ( itoa ( feedIndex, buffer, 10 ) );
	lcd.write ("> ");

	int blinkTime = mode == SET_FEED_MODE && seconds % 2
			&& (key == NO_KEY || key == DOWN_KEY);

	if ( blinkTime && position == POS_HOUR )
	{
		lcd.write ("  ");
	}
	else
	{
		if ( feedHour < 10 )
			lcd.write("0");
		lcd.write ( itoa ( feedHour, buffer, 10 ) );
	}

	lcd.write(":");

	if ( blinkTime && position == POS_MINUTE )
	{
		lcd.write("  ");
	}
	else
	{
		if ( feedMinute < 10 )
			lcd.write("0");
		lcd.write ( itoa ( feedMinute, buffer, 10 ) );
	}

	lcd.write("| ");

	if ( blinkTime && position == POS_CUP )
	{
		lcd.write (" ");
	}
	else
	{
		lcd.write ( itoa ( feedCup, buffer, 10 ) );
	}

	switch ( mode )
	{
	case LOW_POWER_MODE:
		waitTime = 1000;
		partsPerSec = 1;
		Serial.println("Low power mode");
		break;

	case FEED_MODE:
		waitTime = 50;
		partsPerSec = 20;
		break;

	default:
		//Serial.println("Edit mode");
		waitTime = 200;
		partsPerSec = 5;
		break;
	}

	delay(waitTime);
	if ( ++parts == partsPerSec )
	{
		parts = 0;
		seconds++;
	}

	if ( seconds >= 60 && parts == 0 && mode != SET_CLOCK_MODE )
	{
		seconds -= 60;
		minutes++;

		if ( minutes == 60 )
		{
			minutes = 0;
			hours++;
		}

		if ( mode == FEED_MODE && remainingCups == 0 )
		{
			mode = NORMAL_MODE;
		}

		if ( mode == NORMAL_MODE || mode == LOW_POWER_MODE )
		{
			for (int i = 0; i < sizeof(feedTimes) / sizeof(feedTimes[0]); i++)
			{
				byte * ptr = (byte*) &feedTimes[i];
				byte fm = ptr[0];
				byte fh = ptr[1];
				byte fc = ptr[2];

				if ( hours == fh && minutes == fm && fc > 0 )
				{
					Serial.println("It's feeding time");

					mode = FEED_MODE;
					remainingCups = fc;
					digitalWrite ( RELAIS_PIN, LOW );
					delay(10000);
					seconds += 10; // correct time
					break;
				}
			}
		}

		// sync every hour;
		if ( minutes == 0 )
		{
			RTC.getTime();
			seconds = RTC.second;
			minutes = RTC.minute;
			hours = RTC.hour;
		}
	}

	if ( mode == FEED_MODE )
	{
		if ( remainingCups > 0 )
		{
			if ( digitalRead ( END_SWITCH_PIN ) == LOW )
			{
				remainingCups--;
				if ( remainingCups == 0 )
				{
					digitalWrite( RELAIS_PIN, HIGH);
				}
			}
		}
	}
}
