/*
PWM_Reader2
By Gabriel Staples
http://electricrcaircraftguy.blogspot.com/
28 Nov. 2013
-modified 30 Dec. 2013 to interact w/Hayden's car

-This code will read in a single PWM signal from an RC receiver.  I am using my custom Timer2_Counter code to get a precision of up to 0.5us.


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

//set up variables
const int input_pin = 8; // Pin 2 is the interrupt 0 pin, see here: http://arduino.cc/en/Reference/attachInterrupt
const int led = 3;
//set up throttle constants (found by measuring them from the throttle channel of the Rx previously)
const int PWM_center = 1490; //us
const int PWM_full_forward = 1005; //us; full throttle
const int PWM_full_reverse = 1865; //us; full reverse throttle
//choose delay times
int long_delay_t = 250; //ms; delay time for no throttle
int short_delay_t = 25; //ms; delay time for full throttle

//volatile variables for use in the ISR (Interrupt Service Routine)
volatile boolean pin_state_new = LOW; //initialize
volatile boolean pin_state_old = LOW; //initialize
volatile unsigned long t_start = 0; //us
volatile unsigned long t_start_old = 0; //us
volatile unsigned long t_end = 0; //us
volatile unsigned int PWM = 0; //us; the PWM signal high pulse time


void setup() {
  //configure Timer2
  setup_T2();
  
  //set up input pin
  pinMode(input_pin,INPUT);
//  attachInterrupt(0,interrupt_triggered,CHANGE);

  //turn on PCINT0_vect Pin Change Interrupts (for pins D8 to D13); see datasheet pg. 73-75.
  //1st, set flags on the proper Pin Change Mask Register
  PCMSK0 = 0b00000001; //here I am setting Bit0 to a 1, to mark pin D8's pin change register as on; for pin mapping see here: http://arduino.cc/en/Hacking/PinMapping168
  //2nd, set flags in the Pin Change Interrupt Control Register
  PCICR |= 0b00000001; //here I am turning on the pin change vector 0 interrupt, for PCINT0_vect, by setting the right-most bit to a 1
  
  Serial.begin(115200);
  //set up LED as output
  pinMode(led, OUTPUT);
}


void loop() {
  int delay_t;
  
  noInterrupts(); //turn off interrupts for reading multibyte variable
  unsigned int PWM_val = PWM;
  interrupts();
  
  if (PWM_val <= 1490 - 10){ //for forward throttle settings...
    delay_t = map(PWM_val, PWM_full_forward, PWM_center, short_delay_t, long_delay_t);
    delay_t = constrain(delay_t, short_delay_t, long_delay_t);
  }
  else if (PWM_val >= 1490 + 10){ //for reverse throttle settings...
    delay_t = map(PWM_val, PWM_full_reverse, PWM_center, short_delay_t, long_delay_t);
    delay_t = constrain(delay_t, short_delay_t, long_delay_t);
  }
  else{ //PWM is between 1490-9 and 1490+9 (ie: it is near zero throttle)
    delay_t = long_delay_t;
  }
  
  //print out values for debugging purposes
  Serial.print("PWM_val: ");
  Serial.print(PWM_val);
  Serial.print(", delay_t(ms): ");
  Serial.println(delay_t);
  
  //do blink sequence according to throttle setting
  digitalWrite(led, HIGH);
  delay(delay_t);
  digitalWrite(led, LOW);
  delay(delay_t);
  
}


ISR(PCINT0_vect) { //Interrupt Service Routine: Pin Change Interrupt: PCINT0 means that one of pins D8 to D13 has changed
  unsigned long time_now = get_T2_count(); //do first since T2_count WILL increment even in interrupts.
  pin_state_new = digitalRead(input_pin);
  if (pin_state_old != pin_state_new){
    //if the pin state actualy changed, & it was not just noise lasting < ~2us
    pin_state_old = pin_state_new; //update the state
    if (pin_state_new == HIGH){ //if the new state is high
      t_start = time_now;
      t_start_old = t_start; //us; update
    }
    else{
      t_end = time_now;
      //calculate the microsecond time being read in
      PWM = t_end - t_start; //us
      PWM /= 2; //convert from Timer2 counts to us;
    }
  } 
}
  




