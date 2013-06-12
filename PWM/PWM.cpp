/*---------------------------------------------------------------------*\
 * 
 *	Names:   Reed Slaby, Kevin Stauffer
 *	
 *	Created: SS2013
 *
 *	Purpose: This program is designed to regulate and control a flyback
 *			 converter.  The output voltage should be held at a user-
 *			 defined value while the input voltage varies randomly.
 *
 *	Input:   PIND2 - Voltage Up
 *			 PIND3 - Voltage Down
 *
 *	Output:  PIND5 - PWM
 *			 PORTB - LCD Data Bus
 *			 PIND0 - LCD Register Select
 *			 PIND1 - LCD Read/Write
 *			 PIND7 - LCD Enable
 *
\*---------------------------------------------------------------------*/

//Definitions
#define F_CPU 8000000

// Define various Gains for PID. initial hit and miss....
#define P_GAIN 0.2  //initial 0.8
#define I_GAIN 0.000 //initial 0.005
#define D_GAIN 0.000  //initial 0.01
#define _dt_ 0.5
#define mult 1000
#define maxvoltage 30 //set this based on the voltage divider setup used (max measurable voltage)

//Includes
#include <avr/interrupt.h>
#include <avr/io.h>
#include <tgmath.h>
#include <util/delay.h>

//Function Prototypes
void LCDChar(char text, unsigned char cursorPosition);
void LCDCommand(unsigned char command);
void LCDData(unsigned char data);
void LCDInit();
void LCDString(char * text, unsigned char cursorPosition);
void LCDVoltage(int number, unsigned char cursorStartPos);

//Global Variables
volatile uint8_t theLowADC;
volatile uint16_t theTenBitResults;
int set_voltage = 150, previous_error = 0;  //set_voltage is an integer value (ex. 150 = 1.50 volts)
int error, feedback_voltage, output;
int D_error = 0, I_error = 0;

//Constants
const int charOffset = 0x30;

int main(void)
{

	//Disable Interrupts
	cli();
	//Set data direction
	DDRD = 0xA3;
	DDRB = 0xFF;
	DDRA |= (1 << 1);
	// set PORTA initial values
	PORTA = 0x00;
	//Set counter options
	TCCR1A = 0xA2;
	TCCR1B = 0x19;
	//Set TOP = ICR1 for 24.5kHz
	ICR1 = 0x090;
	//Arbitrarily Set OCR1A (Duty Cycle)
	OCR1A = 0x05;
	//Enable INT0 and INT1
	GICR |= 1<<INT0 | 1<<INT1;
	//Set Falling Edge Trigger for Interrupts
	MCUCR |= 1<<ISC01 | 1<<ISC00 | 1<<ISC11 | 1<<ISC10;
	//Set ADC prescaler to division of 16, so at a clk f of 8Mhz,
	//ADC speed is 500kHz. See table 85 in datasheet for prescaler
	//selection options.
	ADCSRA |= 1<<ADPS2;
	//Set voltage reference as AVCC, should be 5 V? binary= 01000000  See Page 208 or214
	ADMUX = 0x40;
	//Enable Start Conversion
	ADCSRA |= 1<<ADSC;
	//Enable ADC interrupt
	ADCSRA |= 1<<ADIE;
	//Enable the ADC
	ADCSRA |= 1<<ADEN;
	//Re-enable Interrupts
	sei();
	
	LCDInit();
	LCDString("  ADC Voltage:  ",0x80);
	LCDString("     00.00V     ",0xC0);
	//setup PID
	previous_error = set_voltage - feedback_voltage; //calculate the previous difference between set and ADC input voltage
	
	
	while(1)
	{
		//_delay_ms(4000*_dt_); //delay within the loop
		
		
		//Calculate Proportional Error
		error = set_voltage - feedback_voltage;
		
		//Calculate Integral Error by summing up small small errors
		I_error += (error)*_dt_;
		
		//Calculate Differential Error, by dividing error by time interval
		D_error = (error - previous_error)/_dt_;

		//Get output by summing up respective errors multiplied with their respective Gains
		output = (P_GAIN * error) + (I_GAIN * I_error) + (D_GAIN * D_error);
		
		//set OCR1A Duty cycle to output
		if(output > 0x3)
			{
				output = 0x3;
			}
		OCR1A = output; //duty cycle is the output (new feedback adjusted value)/steps in the adc
		
		//Update Previous error
		previous_error = error;
		_delay_ms(255);
		feedback_voltage = theTenBitResults;
		//LCDVoltage(feedback_voltage,0xC5);
		LCDVoltage(feedback_voltage,0xC5);  //the duty cycle
	}	
}

//This interrupt takes an input from a button and increases the duty
//cycle.
//Params: in
ISR(INT0_vect){
	//raise set point voltage if less than the max...
	if(set_voltage < maxvoltage)
	{
		set_voltage++;  //update integer variable of set voltage
	}
	
	
	/////////////////////// This loop was to test, changing the duty cycle... for the actual project, we need to adjust the set voltage
	// if(OCR1A <= (ICR1 - 1))
	//OCR1A += 0x01;
	//////////////////////
}

//This interrupt takes an input from a button and decreases the duty
//cycle.
//Params: in
ISR(INT1_vect){  
	//lower set point voltage
	if(set_voltage > 0)
	{
		set_voltage--;  //update integer variable of set voltage
	}
	////////////////////// This loop was to test, changing the duty cycle... for the actual project, we need to adjust the setvoltage
	//if(OCR1A > 0x01)
	//OCR1A -= 0x01;
	////////////////////////
}

//This interrupt reads the ADC port and converts it to a usable value.
//Params: in
ISR(ADC_vect)
{
	//Assign the variable theLowADC as the value in the register ADCL
	theLowADC = ADCL; 
	//Assign theTenBitResults as the value in ADCH shifted 8 left. 
	theTenBitResults = ADCH<<8;  
	theTenBitResults += theLowADC;
	//Start ADC conversion
	ADCSRA |=1<<ADSC;  
}

//This function sends a Character to the specified cursor position on 
//the LCD Display.
//Params: in, in
void LCDChar(char text, unsigned char cursorPosition)
{
	LCDCommand(cursorPosition);
	_delay_ms(1);
	LCDData(text);
}

//This function sends a Register command to the LCD Display.
//Params: in
void LCDCommand(unsigned char command)
{
	PORTB = command;
	PORTD &= ~0x03;
	PORTD |= 0x80;
	_delay_ms(1);
	PORTD &= ~0x80;
	_delay_ms(1);
}

//This function sends a character to the display.
//Params: in
void LCDData(unsigned char data)
{
	PORTB = data;
	PORTD &= ~0x02;
	PORTD |= 0x81;	
	_delay_ms(1);
	PORTD &= ~0x81;
}

//This function initializes the LCD Display.
//Params: none
void LCDInit()
{
	_delay_ms(32);
	//Function Set
	LCDCommand(0x3C);
	//Display On
	LCDCommand(0x0F);
	//Clear Display
	LCDCommand(0x01);
	_delay_ms(1);
	//Set Entry Mode
	LCDCommand(0x06);
}

//This function sends a string value to the specified cursor position on 
//the LCD display.
//Params: ref, in
void LCDString(char* text, unsigned char cursorPosition)
{
	LCDCommand(cursorPosition);
	while(*text)
	{
		LCDData(*text);
		text++;
	}
}

//This function converts a float Voltage to characters and sends them to 
//the specified cursor position on the LCD Display.
//Params: in, in
void LCDVoltage(int number, unsigned char cursorStartPos)
{
	int index;
	int digit;
	int arr[4];
	int tempNumber;
	number = number*3;
	for (int i = 1; i <= 4; i++) 
	{
		 index = 4 - i;
		 digit = tempNumber / pow(10, index);
		 arr[i-1] = digit;
		 tempNumber = tempNumber - pow(10, index) * digit;
	}
	char hundredth = arr[3] + charOffset;
	char tenth = arr[2] + charOffset;
	char ones = arr[1] + charOffset;
	char tens = arr[0] + charOffset;
	
	LCDChar(tens,cursorStartPos);
	LCDChar(ones,cursorStartPos + 1);
	LCDChar(tenth,cursorStartPos + 3);
	LCDChar(hundredth,cursorStartPos + 4);
}