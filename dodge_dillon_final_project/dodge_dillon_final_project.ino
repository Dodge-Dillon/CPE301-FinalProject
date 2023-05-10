#include "Arduino.h"
#include <LiquidCrystal.h>
#include <dht.h>
#include <Stepper.h>
#include "uRTCLib.h"

#define RDA 0x80
#define TBE 0x20  

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
 
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

// Define Port A Register Pointers
volatile unsigned char* port_a = (unsigned char*) 0x22; 
volatile unsigned char* ddr_a  = (unsigned char*) 0x21; 
volatile unsigned char* pin_a  = (unsigned char*) 0x20;

// Define Port B Register Pointers
volatile unsigned char* port_b = (unsigned char*) 0x25; 
volatile unsigned char* ddr_b  = (unsigned char*) 0x24; 
volatile unsigned char* pin_b  = (unsigned char*) 0x23;

// Define Port C Register Pointers
volatile unsigned char* port_c = (unsigned char*) 0x28; 
volatile unsigned char* ddr_c  = (unsigned char*) 0x27; 
volatile unsigned char* pin_c  = (unsigned char*) 0x26;

// Define Port H Register Pointers
volatile unsigned char* port_h = (unsigned char*) 0x102; 
volatile unsigned char* ddr_h  = (unsigned char*) 0x101; 
volatile unsigned char* pin_h  = (unsigned char*) 0x100;

#define DISABLED 100
#define IDLE 200
#define RUNNING 300
#define ERROR 400
#define VENT 500

const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

dht DHT;
#define DHT11_PIN 8

#define BUTTON 13 // PB7 -- pin 13
int btn_press = 0;
int btn_state = 1;
int sys_state = IDLE;

#define FAN 30 // PC7 -- pin 30
#define DIRA 29 // PA7 -- pin 29
#define DIRB 28 // PA6 -- pin 28

#define RED_LED 9 // PH6 -- pin 9
#define YELLOW_LED 10 // PB4 -- pin 10
#define BLUE_LED 11 // PB5 -- pin 11
#define GREEN_LED 12 //PB6 -- pin 12

#define WATER_SENSOR 15
#define WATER_LOW 200
int w_level = 0;

#define STEPS 32
#define OUTPUT_STEPS 32*64
Stepper stepper(STEPS, 22, 23, 24, 25);
int stepsToTake;
int current = 0;
int previous = 0;

uRTCLib rtc(0x68);
char daysOfTheWeek[7][12] = {
  "Sunday", 
  "Monday", 
  "Tuesday", 
  "Wednesday", 
  "Thursday", 
  "Friday", 
  "Saturday"
};

void setup() 
{
  // setup the UART
  U0init(9600);
  // setup the ADC
  adc_init();

  set_pin(BUTTON);

  set_pin(FAN);
  set_pin(DIRA);
  set_pin(DIRB);
  write_low(FAN);
  write_high(DIRA);
  write_low(DIRB);

  set_pin(RED_LED);
  set_pin(YELLOW_LED);
  set_pin(BLUE_LED);
  set_pin(GREEN_LED);
  write_low(RED_LED);
  write_low(YELLOW_LED);
  write_low(BLUE_LED);
  write_low(GREEN_LED);

  URTCLIB_WIRE.begin();
  // sec, minute, hour, week, day, month, year
  //rtc.set(10,51,15,3,9,5,23);

  stepper.setSpeed(500);
  stepper.step(0);

  lcd.begin(16, 2);
}

void loop() 
{
  rtc.refresh();
  btn_press = digitalRead(BUTTON);

  if(btn_press == HIGH)
  {
    delay(500);
    if(btn_state == HIGH)
    {
      btn_state = LOW;
    }
    else
    {
      btn_state = HIGH;
    }
  }

  int chk = DHT.read11(DHT11_PIN);

  w_level = (int)adc_read(WATER_SENSOR);

  if(w_level > WATER_LOW)
  {
    current = map((int)adc_read(0),0,1023,0,255);

    if(current > (previous + 175))
    {
      stepsToTake = OUTPUT_STEPS / 2;
      stepper.step(stepsToTake);
      display_time(VENT);
      previous = current;
    }
    else if(current < (previous - 175))
    {
      stepsToTake = - OUTPUT_STEPS / 2;
      stepper.step(stepsToTake);
      display_time(VENT);
      previous = current;
    }
  }

  if(btn_state == LOW)
  {
    disabled();
  }
  else if(w_level < WATER_LOW)
  {
    error();
  }
  else if(DHT.temperature > 25)
  {
    running();
  }
  else
  {
    idle();
  }
}

void idle()
{
  if(sys_state != IDLE)
  {
    sys_state = IDLE;
    display_time(IDLE);
  }

  write_low(RED_LED);
  write_low(YELLOW_LED);
  write_low(BLUE_LED);
  write_high(GREEN_LED);

  write_low(FAN);

  lcd.setCursor(0,0); 
  lcd.print("Temp: ");
  lcd.print(DHT.temperature);
  lcd.print((char)223);
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Humidity: ");
  lcd.print(DHT.humidity);
  lcd.print("%");
  delay(1000);
}

void running()
{
  if(sys_state != RUNNING)
  {
    sys_state = RUNNING;
    display_time(RUNNING);
  }
  
  write_low(RED_LED);
  write_low(YELLOW_LED);
  write_high(BLUE_LED);
  write_low(GREEN_LED);

  write_high(FAN);

  lcd.setCursor(0,0); 
  lcd.print("Temp: ");
  lcd.print(DHT.temperature);
  lcd.print((char)223);
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Humidity: ");
  lcd.print(DHT.humidity);
  lcd.print("%");
  delay(1000);
}

void disabled()
{
  if(sys_state != DISABLED)
  {
    sys_state = DISABLED;
    display_time(DISABLED);
  }
  
  write_low(RED_LED);
  write_high(YELLOW_LED);
  write_low(BLUE_LED);
  write_low(GREEN_LED);

  write_low(FAN);

  lcd.clear();
}

void error()
{
  if(sys_state != ERROR)
  {
    sys_state = ERROR;
    display_time(ERROR);
  }
  
  write_high(RED_LED);
  write_low(YELLOW_LED);
  write_low(BLUE_LED);
  write_low(GREEN_LED);

  write_low(FAN);

  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("ERROR:");
  lcd.setCursor(0,1);
  lcd.print("Water Level Low");
  delay(1000);
}

void display_time(int state)
{
  switch(state)
  {
    case DISABLED:
      U0putchar('D');
      U0putchar('i');
      U0putchar('s');
      U0putchar('a');
      U0putchar('b');
      U0putchar('l');
      U0putchar('e');
      U0putchar('d');
      U0putchar(':');
      U0putchar(' ');
      break;
    case IDLE:
      U0putchar('I');
      U0putchar('d');
      U0putchar('l');
      U0putchar('e');
      U0putchar(':');
      U0putchar(' ');
      break;
    case RUNNING:
      U0putchar('R');
      U0putchar('u');
      U0putchar('n');
      U0putchar('n');
      U0putchar('i');
      U0putchar('n');
      U0putchar('g');
      U0putchar(':');
      U0putchar(' ');
      break;
    case ERROR:
      U0putchar('E');
      U0putchar('r');
      U0putchar('r');
      U0putchar('o');
      U0putchar('r');
      U0putchar(':');
      U0putchar(' ');
      break;
    case VENT:
      U0putchar('V');
      U0putchar('e');
      U0putchar('n');
      U0putchar('t');
      U0putchar(':');
      U0putchar(' ');
      break;
    default:
      U0putchar('D');
      U0putchar('a');
      U0putchar('t');
      U0putchar('e');
      U0putchar(' ');
      U0putchar('&');
      U0putchar(' ');
      U0putchar('T');
      U0putchar('i');
      U0putchar('m');
      U0putchar('e');
      U0putchar(':');
      U0putchar(' ');
      break;
  }

  print_2_digits(rtc.month());
  U0putchar('/');
  print_2_digits(rtc.day());
  U0putchar('/');
  print_2_digits(rtc.year());

  U0putchar(' ');

  print_2_digits(rtc.hour());
  U0putchar(':');
  print_2_digits(rtc.minute());
  U0putchar(':');
  print_2_digits(rtc.second());
  U0putchar('\n');
}

void print_2_digits(int number)
{
  unsigned char c[2];
  c[0] = number % 10;           // isolate the 10ths place
  c[1] = (number / 10) % 10;    // isolate the 100ths place

  // display the number with each individual digit as a char
  U0putchar(c[1] + '0');
  U0putchar(c[0] + '0');
}

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}

unsigned char U0getchar()
{
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

void set_pin(unsigned char pin)
{
  switch(pin)
  {
    case 9:
      *ddr_h |= 0x01 << 6;
      break;
    case 10:
      *ddr_b |= 0x01 << 4;
      break;
    case 11:
      *ddr_b |= 0x01 << 5;
      break;
    case 12:
      *ddr_b |= 0x01 << 6;
      break;
    case 13:
      *ddr_b &= 0xEF;
      break;
    case 28:
      *ddr_a |= 0x01 << 6;
      break;
    case 29:
      *ddr_a |= 0x01 << 7;
      break;
    case 30:
      *ddr_c |= 0x01 << 7;
      break;
  }
}

void write_high(unsigned char pin)
{
  switch(pin)
  {
    case 9:
      *port_h |= 0x01 << 6;
      break;
    case 10:
      *port_b |= 0x01 << 4;
      break;
    case 11:
      *port_b |= 0x01 << 5;
      break;
    case 12:
      *port_b |= 0x01 << 6;
      break;
    case 28:
      *port_a |= 0x01 << 6;
      break;
    case 29:
      *port_a |= 0x01 << 7;
      break;
    case 30:
      *port_c |= 0x01 << 7;
      break;
  }
}

void write_low(unsigned char pin)
{
  switch(pin)
  {
    case 9:
      *port_h &= ~(0x01 << 6);
      break;
    case 10:
      *port_b &= ~(0x01 << 4);
      break;
    case 11:
      *port_b &= ~(0x01 << 5);
      break;
    case 12:
      *port_b &= ~(0x01 << 6);
      break;
    case 28:
      *port_a &= ~(0x01 << 6);
      break;
    case 29:
      *port_a &= ~(0x01 << 7);
      break;
    case 30:
      *port_c &= ~(0x01 << 7);
      break;
  }
}


