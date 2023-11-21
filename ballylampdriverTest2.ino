/* 
 * BallyLampDriverTest2.ino
 * Simple test Lamps on AS-2518-23 board using Serial commands to set state of 60 Lamps
 * On initial power on 1 lamp will be on each IC chip 
 * Created Nov 16, 2018 Author Todd Legere
 * Jan 12, 2019 Lamp driver is only pushing 33 Playfield lamps. Switched from 60 to 33 lamps
 * Feb 2, 2019 Updated interrupt to detect if FALLING
 * Apr 28,2020 Dumb ass fix the array to be a 4 by 15 (60 lamps) not 4 by 16 64 lamps
 * May 23,2020 Allow 60 lamps to be passed (Game only uses 50 of them
 */

/* 
 * The lampdriver AS-2518-23 uses an IC Chip 14514b (4 - 16 decoder and SCRs)
 * 4 Pins controls the turning on/off 16 Output pins, you may only turn one of the 16 on at a time
 * The board uses SCRs so an external a zero crossing circuit talks  to interupt pin #2 for each zero crossing.
 * A 2518-23 has 60 SCRs used to drive incandesent lamps and there are 4 14514b decoder chips
 * Think of the 4 AOutputs pins mapping to turning on one of the 16 output pins see table below
 * Since there are 4 IC14514b chips on the board each chip can turn on 15 lamps, 4 x 15 = 60 Lamps
 * set pins 4,5,6,7 0000 = All Off
 * set pins 4,5,6,7 0001 = Pin 1 on
 * set pins 4,5,6,7 0010 = Pin 2 on
 * set pins 4,5,6,7 0011 = Pin 3 on
 * set pins 4,5,6,7 0100 = Pin 4 on
 * set pins 4,5,6,7 0101 = Pin 5 on
 * set pins 4,5,6,7 0110 = Pin 6 on
 * set pins 4,5,6,7 0111 = Pin 7 on
 * set pins 4,5,6,7 1000 = Pin 8 on
 * set pins 4,5,6,7 1001 = Pin 9 on
 * set pins 4,5,6,7 1010 = Pin 10 on
 * set pins 4,5,6,7 1011 = Pin 11 on
 * set pins 4,5,6,7 1100 = Pin 12 on
 * set pins 4,5,6,7 1101 = Pin 13 on
 * set pins 4,5,6,7 1110 = Pin 14 on
 * set pins 4,5,6,7 1111 = Pin 15 on
 */

/*
 * See this link for schematic to AS-2518-23 board 
 * https://www.ipdb.org/files/4501/Bally_1978_Mata_Hari_Lamp_Driver_Schematic.jpg 
 * J4 Bally Connector Pins to Arduino Uno Pins (Confirm & Update this list)
 * J4 Pin 1  - Uno - Ground
 * J4 Pin 2  - Uno - Ground
 * J4 Pin 3  - Uno - 5vdc
 * J4 Pin 4  (P3) - Uno - Pin 11 (P3) Enable data to Chip U3
 * J4 Pin 5  (P2) - Uno - Pin 10 (P2) Enable data to Chip U2 
 * J4 Pin 6  (P1) - Uno - Pin 9 (P1) Enable data to Chip U1
 * J4 Pin 7  (P0) - Uno - Pin 8 (P0) Enable data to Chip U0
 * J4 Pin 8  Empty (This is the key pin)
 * J4 Pin 9  Empty 
 * J4 Pin 10 Empty
 * J4 Pin 11 - Uno - Ground
 * J4 Pin 12 - Uno - Ground
 * J4 Pin 13 (Strobe) - Uno - Pin 3 
 * J4 Pin 14 (A0) - Uno - Pin 4 Address 0
 * J4 Pin 15 (A1) - Uno - Pin 5
 * J4 Pin 16 (A2) - Uno - Pin 6
 * J4 Pin 17 (A3) - Uno - Pin 7
 */

/* 
 * Here is the part that will confuse you. You only turn on a SCR, it will turn off by itself at each zero cross
 * Thats why after each zero cross we need to tell each lamp over & over that it is on 
 */

/*
 * What pin to use for input from external zero crossing circuit
 * Arduino Uno use pin 2
 * Arduino Mega choose 1 (2,18,19,20,21)
 */

/* 
 * Lets do the Math... Our Sinewave is 60Hz (50Hz in Europe)
 * At 60 Hz that interupt is every 8.333ms or 8333 uS
 * This happens 100 times per second
 */

/*
 * Serial Command = Prefix (Constant "L") + Lamp# (1-60) for lamp off and Lamp# 65 -124 Lamp on 
 * Example "L1 Lamp1 off, L65 = Lamp 1 on (or lampnum + 64 = Lamp on)
 * Example "A 10000000000000000000000000000000000000000000000001" Provides state of the lamp array (60 lamps)
 * Prefix A will contain 60 elements 0 = off, 1 = on
 */

/*    Pin Lamp
 * J1 01  07
 * J1 28  30      
 * J2  2  40
 * J2  9  44
 */
// #define DEBUG 1             // Set to 0 to remove debug
#define ZERO_DETECT   2     // Your arduino interrupt pin
#define BOARD_LED     13    // on board LED

// volatile required if you are going to reference this variable
// outside of the interrupt procedure
volatile  byte zeroCrossCounter = 0;

String newarray;
String newstate; 
String newchip;
String newpin;
int Lampnum;
int Chipnum;
int Chippinstate;
int i;
int a;
int b;
int z;
int Chippin;
String zz;
String tmp;

int Strobe_Pin = 3;	// Strobe the decode chip that is enabled (U1 - U4)
int AOutputs[] = {4,5,6,7}; // Which arduino pin to turn on (1-15)
int UOutputs[] = {8,9,10,11}; // Which arduino pin to decoder (U1 - U4) for its lamp turn on (pins 1-15)
String readString;

char inData[64]; // Our serial data coming in
char inChar;
byte index = 0; 
/*
 * Lamp Array 4 decoder chips with 15 lamps each 4 x 15 = 60 lamps
 */
volatile byte counter = 0;
 
int LampArray [4] [15] = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                          {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                          {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                          {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
                          
void setup()
{
	Serial.begin(115200); // For debugging purposes
  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

	pinMode (ZERO_DETECT, INPUT_PULLUP); // External Interupt pin Zero Crossing
	attachInterrupt(digitalPinToInterrupt(ZERO_DETECT), zero_cross_int, FALLING);  
	// Which of the 15 lamps for this chip to turn on
	pinMode (AOutputs[0], OUTPUT);
	pinMode (AOutputs[1], OUTPUT);
	pinMode (AOutputs[2], OUTPUT);
	pinMode (AOutputs[3], OUTPUT);
	// Which IC (U1 - U4) chip to speak to
	pinMode (UOutputs[0], OUTPUT);
	pinMode (UOutputs[1], OUTPUT);
	pinMode (UOutputs[2], OUTPUT);
	pinMode (UOutputs[3], OUTPUT);

  // Turn off all 60 output lamp (15 x 4)
  digitalWrite(UOutputs[0], HIGH); 
  digitalWrite(UOutputs[1], HIGH);
  digitalWrite(UOutputs[2], HIGH);
  digitalWrite(UOutputs[3], HIGH);

  // Set address to Binary 0
  digitalWrite (AOutputs[0], LOW); 
  digitalWrite (AOutputs[1], LOW); 
  digitalWrite (AOutputs[2], LOW);
  digitalWrite (AOutputs[3], LOW);

  // Strobe the address in
  digitalWrite(Strobe_Pin,HIGH);
  digitalWrite(Strobe_Pin,LOW);

  // Enable all for chips
  digitalWrite(UOutputs[0], LOW); 
  digitalWrite(UOutputs[1], LOW);
  digitalWrite(UOutputs[2], LOW);
  digitalWrite(UOutputs[3], LOW);

  inData[0] = '\0';
  
#if DEBUG  
        Serial.write("Lamp Driver Version 1.0\n");
        Serial.write("Drives 60 Lamps on Balley Lamp driver board.\n");
        Serial.write("Expected Message examples\n");
        Serial.write("L1 = Turn Lamp 1 off, L61 = Turn Lamp 1 On (n + 60 = On Lamp state)\n");
        Serial.write("L60 = Turn Lamp 60 off, L120 = Turn Lamp 60 On (n + 60 = On Lamp state)\n");
        Serial.write("A + 60 Lamp states 0 = Off, 1 = On\n");
        Serial.write("A000000000000000000000000000000000000000000000000000000000000\n");
        Serial.println();
#endif     
        /* 
         * Initialize Lamps to off
         */
     
        for (a = 0; a < 4; a++) {
          for (b = 0; b < 16; b++) {
            LampArray [a] [b] = 0;
          }
        }


#if DEBUG        
        Serial.println("Lamp Array Now:");
        for (a = 0; a < 4; a++) {
          for (b = 0; b < 15; b++) {
            Serial.print(LampArray [a] [b]);
      	    Serial.print(' ');
          }
          Serial.println();
        }
#endif


}

void loop(){
    newarray = "";
    while (Serial.available() > 0) // Don't read unless
    {
      inChar = Serial.read(); // Read a character
        if(inChar != '\n') 
        {
            inData[index] = inChar; // Store it
            index++; // Increment where to write next
            inData[index] = '\0'; // Null terminate the string
        } else {
            newarray = parseMsg(inData);
#if DEBUG          
            Serial.println();
            Serial.println("inData  [" + String(inData) + "]");
            Serial.println("newarray [" + newarray + "]");
            Serial.println("newarray len [" + String(newarray.length()) + "]"); 
#endif      

            inData[0] = '\0';        
            /* 
             * All 60 Lamps passed
             */
            if (newarray.length() == 60) { // There are 60 lamps pgm passes 64
              
              /*
               * Init lamp Array
               */
               
               for (a = 0; a < 4; a++) {
                for (b = 0; b < 16; b++) {
                  LampArray [a] [b] = 0;
                }
               }
               
               for (a = 0; a < newarray.length(); a++) { 
                 
                 Lampnum = a;
                 
                 zz = newarray.substring(a,a + 1);
                 if (zz != "1") {
                  zz = "0"; // Get rid of noise
                 }
                 
                 Chippinstate = zz.toInt();
                 
                 Chippin = int(Lampnum % 15);
                 if (Chippin == 0) {
                   Chippin = 15;
                 }               
                
                 if(int(Lampnum % 15) == 0) {
                  Chipnum = int(Lampnum / 15) - 1;
                 } else {
                  Chipnum = int(Lampnum / 15);
                 }

                 LampArray[Chipnum][Chippin ] = Chippinstate;                 
               }  // End for a
               LampArray [3] [1] = 0;

#if DEBUG 
               Serial.println("Lamp Array Now:");
               for (a = 0; a < 4; a++) {
                 for (b = 0; b < 16; b++) {
                   Serial.print(LampArray [a] [b]);
      	           Serial.print(' ');
                 }
                 Serial.println();
               }
#endif
               
               index = 0;
               inData[index] = {'\0'};               
            } // End newarray.length() == 60
            
            
            /*
             * Invalid data received
             */
             
            if (newarray == "BadData") {
#if DEBUG               
              Serial.println("Bad data request [" + String(newarray) + "]:");
#endif              
              index = 0;
              inData[index] = {'\0'};
            } 
              
            /* 
             * Individual Lamp Changes
             * Turn Lamp off = 1 - 60 
             * Turn Lamp on = 61 - 120 (Really Lampnum - 60)
             */ 
             
            if (newarray.length() < 5) {
              Lampnum = newarray.toInt(); 
              if ((Lampnum >= 0 && Lampnum < 121))
              {
                Chippinstate = 0; // 0 - 59 Lamp is off
                if (Lampnum > 60) {
                  Lampnum = Lampnum - 60; // Lamp 60 - 119 is on
                  Chippinstate = 1;
                }
              

                 /* 
                  * Which Pin #
                  */
                Chippin = int(Lampnum % 15);
                if (Chippin == 0) {
                  Chippin = 15;
                }
                
                /* 
                 * Which Chip
                 * Our Chip Number begin at 0 not 1 so subtract 1 from Chipnum pin Arrary 0 - 3 not 1 - 4
                 */               
                if (int(Lampnum % 15) == 0) {
                  Chipnum = int(Lampnum/15) - 1; 
                  } else {
                  Chipnum = int(Lampnum/15);
                }


#if DEBUG               
                Serial.println("Chip [" + String(Chipnum) + "] Pin [" + String(Chippin) + "] State [" + String(Chippinstate) + "]");
#endif                
                LampArray[Chipnum][Chippin - 1] = Chippinstate;

#if DEBUG
                Serial.println("Lamp Array Now:");
                for (a = 0; a < 4; a++) {
                  for (b = 0; b < 15; b++) {
                    if (LampArray[a][b] > 1) {
                      Serial.print("\nLampArray [" + String(a) + "][" + String(b)+"] is ["+String(LampArray[a][b]) + "]\n");
                    }
                    Serial.print(LampArray [a] [b], HEX);
      	            Serial.print(' ');
                  }
                  Serial.println();
                }
#endif
                
              } else {
#if DEBUG                
                Serial.println("Invalid Lamp Range must be (1 - 60) or (61 - 120) :[" + newarray + "]");
                Serial.println();
#endif                
              } // End Lampnum range check
              
              index = 0;
              inData[index] = {'\0'};
            } // End newarray.length 
            

        }
        
    }
}

String parseMsg(String Msg)
{
	char t;
	t = Msg[0];
        
	switch (t) {
		case 'A':
      newarray = Msg.substring(1);

#if DEBUG   
      Serial.println("Lamp Array reset request:[" + Msg + "]");
      Serial.println("Size of message:[" + String(newarray.length()) + "]");
#endif    
  
			if (newarray.length() < 60 ) {

#if DEBUG   
        Serial.println("Lamp Array reset request:[" + Msg + "]");        
				Serial.println("Invalid Lamp Array needs to be 60 chars long:[" + Msg + "]");
        Serial.println();
#endif
        
        return "BadData";
			} else {

#if DEBUG         
				Serial.println("Lamp Array:[" + Msg + "]");
        Serial.println();  
#endif
        
				newarray = Msg.substring(1);
        return newarray;

			} // End Msg.length
			break;

		case 'L':
   
#if DEBUG    
			Serial.println("Individual Lamp change:[" + Msg + "]");
#endif     

			if (Msg.length() > 4) {

#if DEBUG       
				Serial.println("Invalid Lamp change:[" + Msg + "]");
        Serial.println();
#endif  
        return "BadData";
      
			} else {
			  newarray = Msg.substring(1);
			  return newarray;	          
			} // End Msg.length
			break;

		default:
    
#if DEBUG     
			Serial.println("Invalid Message requested type [" + String(Msg) + "] Must begin with A or L: ");
      Serial.println();
#endif

      return "BadData";
			break;

	} // End switch
       
}


void zero_cross_int()
{ 
  ++counter;
  if ( counter == 60) {
    counter = 0;
    digitalWrite(BOARD_LED,!digitalRead(LED_BUILTIN));
  }

  int y;

  digitalWrite(UOutputs[0], HIGH); 
  digitalWrite(UOutputs[1], HIGH);
  digitalWrite(UOutputs[2], HIGH);
  digitalWrite(UOutputs[3], HIGH);

  
	// For each U1 - U4 Chip
	for (y = 0; y < 4; y++) { 
		// For each of the 16 output pins on chip U1 - U4
		
		for (byte binary = 0; binary <= 15; binary++) {
      digitalWrite(UOutputs[y], HIGH); 
      if (LampArray[y][binary] == 1) {
        for (int i = 0; i<4; i++) {
          if (bitRead(binary,i) == 1) {
            digitalWrite(AOutputs[i],HIGH);
					} else {
						digitalWrite(AOutputs[i],LOW);
				  }
			  } // End for int i
        digitalWrite(Strobe_Pin,HIGH);
        digitalWrite(Strobe_Pin, LOW); // Latch/strobe the address to the chip
        digitalWrite(UOutputs[y], LOW);
      } // Set the address if it is on

		} // End checking all 16 pins
	}
  
} // End zero_cross_int
