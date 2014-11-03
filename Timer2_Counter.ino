/*
By Gabriel Staples
http://electricrcaircraftguy.blogspot.com/
7 Dec. 2013

/*
===================================================================================================
  LICENSE & DISCLAIMER
  Copyright (C) 2014 Gabriel Staples.  All right reserved.
  
  ------------------------------------------------------------------------------------------------
  License: GNU General Public License Version 3 (GPLv3) - https://www.gnu.org/licenses/gpl.html
  ------------------------------------------------------------------------------------------------
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/
===================================================================================================
*/

-This code uses Timer2 to create a more precise timer than micros().  Micros() updates only every 4us.  However, I want something that will update every 0.5us, and that's 
what this provides. 
-The downside is that it changes the behavior of PWM output (using analogWrite) on Pins 3 & 11.
-To use this code, simply copy this file into your directory where you are already working on a specific Arduino file.  Next time you open up your main file, this code
will open up as an additional tab in the Arduino IDE.  
-I have tested this code ONLY on Arduinos using the Atmega328 microcontroller; more specifically, the Arduino Nano.

Functions:
setup_T2();  //This function MUST be called before any of the other Timer2 functions will work.  This function will generally only be called one time in your setup() loop.
             //"setup_T2()" prepares Timer2 and speeds it up to provide greater precision than micros() can give.
get_T2_count();  //gets the counter from Timer 2. Returns the Timer2 counter value as an unsigned long.  Each count represents a time interval of 0.5us.
                 //note that the time returned WILL update even in Interrupt Service Routines (ISRs), so if you call this function in an ISR, and you want the time to be
                 //as close to possible as a certain event that occured which called the ISR you are in, make sure to call "get_T2_count()" first thing when you enter
                 //the ISR.
get_T2_micros(); //returns the Timer2 microsecond time, with a precision of 0.5us, as a float.
reset_T2(); //resets the Timer2 counters back to 0.  Very useful if you want to count up from a specific moment in time
revert_T2_to_normal(); //this function might also be called "unsetup_T2".  It simply returns Timer2 to its normal state that Arduino had it in prior to calling "setup_T2"
unsetup_T2(); //the exact same as "revert_T2_to_normal()"


*/

//Set up Global Variables
//volatile (used in ISRs)
volatile unsigned long T2_overflow_count = 0; //initialize Timer2 overflow counter
volatile unsigned long T2_total_count = 0; //initialize Timer2 total counter
//normal
byte tccr2a_save; //initialize; will be used to backup default settings
byte tccr2b_save; //initialize; will be used to backup default settings

//Interrupt Service Routine (ISR) for when Timer2's counter overflows
ISR(TIMER2_OVF_vect) //Timer2's counter has overflowed 
{
  T2_overflow_count++; //increment the timer2 overflow counter
}


//reset Timer2's counters
void reset_T2()
{
  T2_overflow_count = 0; //reset overflow counter
  T2_total_count = 0; //reset total counter
  TCNT2 = 0; //reset Timer2 counter
  TIFR2 |= 0b00000001; //reset Timer2 overflow flag; see datasheet pg. 160; this prevents an immediate execution of Timer2's overflow ISR
}
  

//get total count for Timer2
unsigned long get_T2_count()
{
  noInterrupts(); //prepare for critical section of code
  uint8_t tcnt2_save = TCNT2; //grab the counter value
  boolean flag_save = bitRead(TIFR2,0); //grab the timer2 overflow flag value
  if (flag_save) { //if the overflow flag is set
    tcnt2_save = TCNT2; //update variable since the overflow flag could have just tripped between previously saving the TCNT2 value and reading bit 0 of TIFR2.  
                        //If this is the case, TCNT2 might have just changed from 255 to 0, and so we need to grab the new value of TCNT2 to prevent an error of up 
                        //to 127.5us in any time obtained using the T2 counter (ex: T2_micros). (Note: 255 counts / 2 counts/us = 127.5us)
                        //Note: this line of code DID in fact fix the error just described, in which I periodically saw an error of ~127.5us in some values read in
                        //by some PWM read code I wrote.
    T2_overflow_count++; //force the overflow count to increment
    TIFR2 |= 0b00000001; //reset Timer2 overflow flag since we just manually incremented above; see datasheet pg. 160; this prevents execution of Timer2's overflow ISR
  }
  T2_total_count = T2_overflow_count*256 + tcnt2_save; //get total Timer2 count
  interrupts(); //allow interrupts again
  return T2_total_count;
}


//get the time in microseconds, as determined by Timer2; the precision will be 0.5 microseconds instead of the 4 microsecond precision of micros()
float get_T2_micros()
{
  float T2_micros = get_T2_count()/2; 
  return T2_micros;
}


//Configure Timer2
void setup_T2()
{
  //increase the speed of timer2; see below link, as well as the datasheet pg 158-159.
  TCCR2B = TCCR2B & 0b11111000 | 0x02; //Timer2 is now faster than default; see here for more info: http://playground.arduino.cc/Main/TimerPWMCheatsheet
  //Note: don't forget that when you speed up Timer2 like this you are also affecting any PWM output (using analogWrite) on Pins 3 & 11.  
  //Refer to the link just above, as well as to this source here:  http://www.oreilly.de/catalog/arduinockbkger/Arduino_Kochbuch_englKap_18.pdf

  //Enable Timer2 overflow interrupt; see datasheet pg. 159-160
  TIMSK2 |= 0b00000001; //enable Timer2 overflow interrupt. (by making the right-most bit in TIMSK2 a 1)
  //TIMSK2 &= 0b11111110; //use this code to DISABLE the Timer2 overflow interrupt, if you ever wish to do so later.
  
  //set timer2 to "normal" operation mode.  See datasheet pg. 147, 155, & 157-158 (incl. Table 18-8).  
  //-This is important so that the timer2 counter, TCNT2, counts only UP and not also down.
  //-To do this we must make WGM22, WGM21, & WGM20, within TCCR2A & TCCR2B, all have values of 0.
  tccr2a_save = TCCR2A; //first, backup some values
  tccr2b_save = TCCR2B; //backup some more values
  TCCR2A &= 0b11111100; //set WGM21 & WGM20 to 0 (see datasheet pg. 155).
  TCCR2B &= 0b11110111; //set WGM22 to 0 (see pg. 158).
}  


//undo configuration changes for Timer2
void revert_T2_to_normal()
{
  TCCR2B = TCCR2B & 0b11111000 | 0x03; //set Timer2 speed back to default
  TIMSK2 &= 0b11111110; //use this code to DISABLE the Timer2 overflow interrupt
  TCCR2A = tccr2a_save; //restore default settings
  TCCR2B = tccr2b_save; //restore default settings
}


//same as revert_T2_to_normal()
void unsetup_T2()
{
  revert_T2_to_normal();
}


