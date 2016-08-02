#include <LiquidCrystal.h>

//Sample using LiquidCrystal library
#include <LiquidCrystal.h>
#include <util\delay.h>
/*******************************************************

This program will test the LCD panel and the buttons
Mark Bramwell, July 2010

********************************************************/

#define JACK_PIN 1

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// define some values used by the panel and buttons
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// read the buttons
int read_LCD_buttons()
{

 int adc_prev = 0;
  
 int adc_key_in  = analogRead(0);; 

 // Debounce the input. It takes a sec to slew to the final value, dont want the intermediates. 

 do {
  delay(2);
  adc_prev = adc_key_in;
  adc_key_in = analogRead(0);      // read the value from the sensor   
 } while (adc_prev != adc_key_in );
 
 
 // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
 // we add approx 50 to those values and check to see if we are close
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 // For V1.1 us this threshold
 
 /*
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 250)  return btnUP; 
 if (adc_key_in < 450)  return btnDOWN; 
 if (adc_key_in < 650)  return btnLEFT; 
 if (adc_key_in < 850)  return btnSELECT;  
*/
 // For V1.0 comment the other threshold and use the one below:

 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 195)  return btnUP; 
 if (adc_key_in < 380)  return btnDOWN; 
 if (adc_key_in < 520)  return btnLEFT; 
 if (adc_key_in < 790)  return btnSELECT;   



 return btnNONE;  // when all others fail, return this...
}

// Call this from loop to check that the values are right.

void printKey() {
       lcd.clear();
      lcd.print( analogRead(0) );
      delay(100);
      return;

}

char buff[10];

#define MODE_MOTOR 1
#define MODE_LED 2

uint8_t mode=MODE_MOTOR;    

#define MAX_DUTY 200
uint8_t duty = 0;

#define MAX_TOP 255
uint16_t top  = 255;

uint8_t green = 0;

uint8_t red=0;

// Refresh the Screen

void refreshLabels() {
  
 lcd.setCursor(0,0);
 
 if (mode==MODE_MOTOR) {
   lcd.print("   Duty    Top  "); // print a simple message
   
 } else if (mode==MODE_LED) {
   lcd.print("    Red  Green  "); // print a simple message
 }
 
}
  
// left padded, 5 digit string

void digitstring( uint16_t x ) {
  
  uint8_t place = 5;
   
  while (place--) {
    
    uint16_t next = x/10;
    
    buff[place] = (x-(next*10)) + '0';
    
    if (!next) {
      
      while (place--) {
        
        buff[place] = ' ';
        
      }
      
      return;
      
    }
    
    x=next;
    
  }
    
  
}

void refreshValues() {

 if (mode==MODE_MOTOR) {
     lcd.setCursor(2,1); 
  
     digitstring(duty); 
     lcd.print(buff); 
  
     lcd.setCursor(9,1); 
 
     digitstring(top);
     lcd.print(buff); 
  
  } else if (mode==MODE_LED) {
     lcd.setCursor(2,1);   

     digitstring(red); 
     lcd.print(buff); 
  
     lcd.setCursor(9,1); 
 
     digitstring(green);
     lcd.print(buff); 

    
  }
}

void refresh() {
  refreshLabels();
  refreshValues();
}


void showKeyPress() {
  
 uint16_t a = analogRead(0);
  
 lcd.setCursor(2,1); 
  
 digitstring(a); 
 lcd.print(buff); 
  
  _delay_ms(1000);
  
 return;
}  


// We are using an PNP, so driving the gate low will energise the plug

#define ON()    digitalWrite(JACK_PIN,LOW)
#define OFF()   digitalWrite(JACK_PIN,HIGH)


void setupJack() {

  // Pin 8 connected to PNP transistor that controls power to the plug  
  pinMode( JACK_PIN , OUTPUT );
  OFF();

}


// Returns 0 or 1

uint8_t parity( uint8_t b ) {

  uint8_t p=0;

  while (b) {

    if (b & 1) p^=1;
    b >>=1;
  }

  return(p);
}

// Commands defined in the firmware source

int sendCommand( uint8_t command, uint8_t a, uint8_t b ) {

  // Sanity check field lengths

  if (command > 0b111 )   return(1);
  if (a       > 0b1111 )  return(2);

  // Did you know that the xor of two bytes together has the same partity of the two bytes seporately?

  unsigned packet = ( command << 13U ) | ( a << 9 ) | ( b << 1 ) | parity( command ^ a ^ b );
   
    
  unsigned  bitmask = 1U<<15;

  // Create the top of the first sync pulse. 
  // This has to be long enough to get the recievers attention and have them ready to see the 1st sync fall

  ON();
  _delay_us(10000);   // With the 2000us below we wil get a full 22ms before the first drop
                      // Since each main loop on the unit takes ~13ms, we need this to be long enough to 
                      // get the data decoders attention and have it ready and waiting for the falling edge, but
                      // Anything more than 16ms will be read as charging
  
  while (bitmask) {

    // Sync pulse       
    ON();
    _delay_us(3000);  
    OFF();              // We sync to this trailing edge
                        // Trailing is better becase the CIP responds almost instantly 
                        // when the voltage drops.

    _delay_us(1000);     // The trigger valley.                     

   // Next comes the actual data bit window. It must be 2000us long.
   // The first 1ms is rise time (if the bit is 1), then the next 1ms is sample window.
   // Sample targets the center of the window, 2500us after the falling sync edge (1500us after the rise)

    if (packet & bitmask) {

      // Do nothing on most bits - the next sync pulse will be sampled and read as high. Saves 2ms on 1 bits.

      if (bitmask==1) {       // Are we on the final bit?

          ON();
          _delay_us(3000);    // we have to send ourselves
          OFF();
      
      }
            
    } else {

      // stay off for 3000us. 
      _delay_us(3000);

    }
    
    bitmask >>= 1;
    
  }

  // we actually do not need to do anything here. we are always guaranteed to be low when we get here.

   return(0);
  
}


void setup()
{
  setupJack();
  lcd.begin(16, 2);              // start the library
  
  lcd.clear();    // Clear the "not connected" message
 
  refresh();    
}


#define INITIAL_DELAY 500    // Delay between 1st and 2nd repeat of a keypress

uint16_t keyDelay=INITIAL_DELAY;      // start with 1 second delay btween ticks

unsigned long nextKeyTime=0;        // Next time key will repeat

uint8_t lastkey=0;
  
void loop()
{

 int lcd_key  = read_LCD_buttons();  // read the buttons
 
 if (lcd_key== btnNONE ) {
    return;
 }
    
 if (lcd_key == lastkey) {     // Key held down
 
   if (nextKeyTime > millis() ) {      // Still waiting
   
       return;
       
   }
   
   if (keyDelay) {
   
     keyDelay /=2;
     
   } 

 } else {     // Different key
 
   keyDelay = INITIAL_DELAY;    // Reset delay
   lastkey = lcd_key;
      
 }
 
  nextKeyTime = millis() + keyDelay; 
 
 switch (lcd_key)               // depending on which button was pushed, we perform an action
 {
   case btnRIGHT:

        if (mode==MODE_MOTOR) {
          
          if (top<MAX_TOP) {
            top++;
            refreshValues();
            
            sendCommand( 04 , 0 , top );

          }
          
        } else if (mode==MODE_LED) {

          if (green<255) {
            green++;
            refreshValues();
            
           sendCommand( 02 , 0 , green );

          }
        }
        break;
     
   case btnLEFT:
        if (mode==MODE_MOTOR) {
          
          if (top>0) {
            top--;
            refreshValues();
            
            sendCommand( 04 , 0 , top );
          }
          
        } else if (mode==MODE_LED) {

          if (green>0) {
            green--;
            refreshValues();
            
            sendCommand( 02 , 0 , green );
          }
        }
        break;
     
   case btnUP:
        if (mode==MODE_MOTOR) {
          
          if (duty<MAX_DUTY) {
            duty++;
            refreshValues();
            
            sendCommand( 03 , 0 , duty );
          }
          
        } else if (mode==MODE_LED) {

          if (red<255) {
            red++;
            refreshValues();
            
            sendCommand( 01 , 0 , red );
          }
        }
        break;
     
   case btnDOWN:
        if (mode==MODE_MOTOR) {
          
          if (duty>0) {
            duty--;
            refreshValues();
            
            sendCommand( 03 , 0 , duty );
          }
          
        } else if (mode==MODE_LED) {

          if (red>0) {
            red--;
            refreshValues();
            
            sendCommand( 01 , 0 , red );
          }
        }
        break;
     
   case btnSELECT:
        if (mode==MODE_MOTOR) {
          mode=MODE_LED;
        } else {
          mode=MODE_MOTOR;
        }
        refresh();
        delay(500);     // Debounce
        
        break;
   
 }

        
}
