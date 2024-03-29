/*
 * HetaudaRobot3.c
 *
 * Created: 9/21/2016 9:28:22 PM
 * Author: Er. Saroj Shakya
 */ 

#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/interrupt.h>

#define LCD_DATA_PORT PORTB
#define LCD_DATA_DDR DDRB
#define LCD_RS 2
#define LCD_EN 3

#define MOTOR_PORT PORTD
#define MOTOR_DDR DDRD

#define SENSOR_PIN PINA
#define SENSOR_DDR DDRA
#define SENSOR_PORT PORTA

#define US_BUZZER_PORT PORTC
#define US_BUZZER_PIN PINC
#define US_BUZZER_DDR DDRC

#define US_PORT PORTC
#define  US_PIN   PINC
#define US_DDR    DDRC
#define US_POS_0 PC0
#define US_POS_1 PC1
#define US_ERROR 0xFFFF
#define US_NO_OBSTACLE 0xFFFE

#define ROUND1 3

#define MAX_GRID_X 5
#define MAX_GRID_Y 7

#define GRID_SENSOR_MASK 0xE0
#define BLOCK_SENSOR_MASK 0x07

#define TURN_90_DELAY 370
#define TURN_180_DELAY 680
#define BLOCK_GRIP_DELAY 2000
#define BLOCK_RELEASE_DELAY 1000
#define BLOCK_LIFT_DELAY 1000
#define BLOCK_DOWN_DELAY 500



#define LEFT 0
#define RIGHT 1
#define FORWARD 2
#define BACKWARD 3
#define LEFT_BACK 4
#define RIGHT_BACK 5

#define TRUE 1
#define FALSE 0

#define UNVISITED  0
#define VISITED 1
#define BLOCK 2
#define DESTINATION 3

#define BLOCK_SEARCH_MODE 0
#define SOLVE_MODE 1
#define INTERMEDIATE_MODE 2

#define INFINITE 20

char dir ='N';
unsigned char X=0,Y=0;
unsigned char dest_coun=0;
unsigned char block_no=0, block_no_1=0;

unsigned char coord_count = 1, count = 0;

unsigned char currentMode = BLOCK_SEARCH_MODE;
unsigned char prev_count = FALSE;
unsigned char prev_block_detect = FALSE;
unsigned char block_caught = FALSE;
unsigned char mode = 0;
typedef struct  STRUCTURE
{
	 unsigned char status;
	 char priority;
	 unsigned char X,Y;
	 unsigned char distance, visit;
	 unsigned int prevX,prevY;	
} STRUCTURE;

unsigned char Qfront=0, Qrear=0;

STRUCTURE QUEUE[15];
STRUCTURE block[ROUND1];
STRUCTURE destination[ROUND1];

STRUCTURE details1[MAX_GRID_X][MAX_GRID_Y];
STRUCTURE details2[MAX_GRID_X][MAX_GRID_Y];

unsigned char blockCounter = 0;
unsigned char destinationCounter = 3;
unsigned char loop_count = 0;

void DETECT_NEXT_NODE();
int main();

void LCD_CMND(unsigned char cmnd) 
{
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (cmnd & 0xF0);
	LCD_DATA_PORT &= ~(1<<LCD_RS); 
	LCD_DATA_PORT |= 1<<LCD_EN; 
	_delay_us(100);
	LCD_DATA_PORT &= ~(1<<LCD_EN); 
	_delay_us(300);
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (cmnd << 4); 
	LCD_DATA_PORT |= 1<<LCD_EN; 
	_delay_us(300);
	LCD_DATA_PORT &= ~(1<<LCD_EN); 
}
void LCD_DATA(unsigned char data) 
{
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (data & 0xF0);
	LCD_DATA_PORT |= 1<<LCD_RS; 
	LCD_DATA_PORT |= 1<<LCD_EN; 
	_delay_us(200);
	LCD_DATA_PORT &= ~(1<<LCD_EN); 
	_delay_us(200);
	LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (data << 4);
	LCD_DATA_PORT |= 1<<LCD_EN; 
	_delay_us(200);
	LCD_DATA_PORT &= ~(1<<LCD_EN); 
}
void LCD_INITIALIZE(void)
{
	LCD_DATA_DDR = 0xFC;
	LCD_DATA_PORT &= ~(1<<LCD_EN);
	_delay_ms(200);
	LCD_CMND(0x33);
	_delay_ms(20);
	LCD_CMND(0x32);
	_delay_ms(20);
	LCD_CMND(0x28);
	_delay_ms(20);
	LCD_CMND(0x0C);
	_delay_ms(20);
	LCD_CMND(0x01);
	_delay_ms(20);
}
void LCD_CLEAR(void)
{
	LCD_CMND(0x01);
	_delay_ms(2);
}
void LCD_PRINT(char * str)
{
	unsigned char i=0;
	while(str[i] != 0)
	{
		LCD_DATA(str[i]);
		i++;
		_delay_us(10);
	}
}
void LCD_SET_CURSER(unsigned char y, unsigned char x)
{ 
	if(y==1)
	LCD_CMND(0x7F+x);
	else if(y==2)
	LCD_CMND(0xBF+x);
}
void LCD_NUM(unsigned char num)
{
	LCD_DATA(num/10000 + 0x30);
	num = num%10000;
	LCD_DATA(num/1000 + 0x30);
	num = num%1000;
	LCD_DATA(num/100 + 0x30);
	num = num%100;
	LCD_DATA(num/10 + 0x30);
	LCD_DATA(num%10 + 0x30);
}

void PORT_INITIALIZE()
{
	SENSOR_DDR = 0b00010000;
	SENSOR_PORT = 0b11101111;
	MOTOR_DDR = 0xFF;
	MOTOR_PORT = 0x00;
}

void SET_INITIAL_INFO()
{
	unsigned int i,j;
	for(i=0;i<MAX_GRID_X;i++)
	{
		for(j=0;j<MAX_GRID_Y;j++)
		{
			details1[i][j].X=i;
			details1[i][j].Y=j;
			details1[i][j].status = UNVISITED;
			details1[i][j].priority = 0;
		}
	}
}

void DEFINE_DESTINATION()
{
	unsigned i;
	for (i=0;i<destinationCounter;i++)
	{
		destination[i].X = 2;
		destination[i].Y = i+2;
	}
}

void SOUND_BUZZER()
{
	US_BUZZER_DDR = 0b00001100;
	PORTC |= 0b00000100;
	_delay_ms(400);
	PORTC &= 0b11111011;
}

void PWM_SET()
{
	TCCR1A = 0xA1;
	TCCR1B = 0x01;
}
/*
void PWM_RESET()
{
	//TCCR1A = 0x00;
	//TCCR1B = 0x00;
	//OCR1A = 255;
	//OCR1B = 255;
}*/

/*
uint16_t GET_PULSE_WIDTH()
{
	uint32_t i,result;


	//Wait for the rising edge
	for(i=0;i<600000;i++)
	{
		if(!(US_PIN & (1<<US_POS_0))) continue; 
		else break;
	}

	if(i==600000) return 0xffff; //Indicates time out

	//High Edge Found

	//Setup Timer1
	TCCR1A=0X00;
	TCCR1B=(1<<CS11); //Prescaler = Fcpu/8
	TCNT1=0x00;       //Init counter
	//TCCR0 = 0x04; //Fcpu/256
	//TCNT0=0x00;
	//Now wait for the falling edge
	for(i=0;i<600000;i++)
	{
		if(US_PIN & (1<<US_POS_0))
		{
			if(TCNT1 > 60000) break; else continue;
		}
		else
		break;
	}

	if(i==600000) return 0xffff; //Indicates time out

	//Falling edge found

	result=TCNT1;
	//result=TCNT0;
	
	//Stop Timer
	TCCR1B=0x00;
	PWM_SET();
	//TCCR0 = 0x00;
	if(result > 60000) return 0xfffe; //No obstacle
	else return (result>>1);
}

void Wait()
{
	uint8_t i;
	for(i=0;i<10;i++)
	_delay_loop_2(0);
}

int BLOCK_DISTANCE()
{
	uint16_t r;
	int d = 100;

	Wait();
	//Set Ultra Sonic Port as out
	US_DDR|=(1<<US_POS_1);

	_delay_us(10);

	//Give the US pin a 15us High Pulse
	US_PORT |= (1<<US_POS_1);   //High

	_delay_us(15);

	US_PORT &= (~(1<<US_POS_1));//Low

	_delay_us(20);
	//Now make the pin input
	US_DDR &= (~(1<<US_POS_0));

	//Measure the width of pulse
	r = GET_PULSE_WIDTH();
	PWM_SET();
	
	//Handle Errors
	if(r==US_ERROR)
	{
		d = 500;
		//LCD_CLEAR();
		//LCD_PRINT("Error!");
	}
	else  if(r==US_NO_OBSTACLE)
	{
		d = 100;
		//LCD_CLEAR();
		//LCD_PRINT("Clear!");
	}
	if (r != US_ERROR && r != US_ERROR) 
	{
		d=(r/58.0); //Convert to cm
	}
	return d;
}
*/
void BOT_STOP()
{
	PWM_SET();
	OCR1A = 0;
	OCR1B = 0;
	MOTOR_PORT = 0x00;
}
//unsigned char speed = 200;
void BOT_MOVE(unsigned char direction)
{
	BOT_STOP();
	PWM_SET();
	switch(direction)
	{
		case LEFT:
			OCR1A = 0;
			OCR1B = 150;
			//MOTOR_PORT |= 0b00010000; //PIN4=OCR1B
			break;
		case RIGHT:
			OCR1A = 150;
			OCR1B = 0;
			//MOTOR_PORT |= 0b00100000; //PIN5=OCR1A
			break;
		case FORWARD:
			OCR1A = 170;
			OCR1B = 170;
			//MOTOR_PORT |= 0b00110000;
			break;
		case BACKWARD:
			OCR1A = 0;
			OCR1B = 0;
			MOTOR_PORT = 0b11000000;
			break;
		case LEFT_BACK:
			OCR1A = 0;
			OCR1B = 255;
			MOTOR_PORT |= 0b10000000; //PIN4=OCR1B
			break;
		case RIGHT_BACK:
			OCR1A = 255;
			OCR1B = 0;
			MOTOR_PORT |= 0b01000000; //PIN5=OCR1A
			break;	
	}	
}

void BLOCK_LIFT(unsigned char direction)
{
	BOT_STOP();
	if(direction == FORWARD)
		MOTOR_PORT |= 0b00000001;
	else if (direction == BACKWARD)
		MOTOR_PORT |= 0b00000010;
}

void BLOCK_HOLD(unsigned char direction)
{
	BOT_STOP();
	if(direction == FORWARD)
		MOTOR_PORT |= 0b00000100;
	else if (direction == BACKWARD)
		MOTOR_PORT |= 0b00001000;
}

void MOVE_BACK_IF_BLOCK()
{
	BOT_MOVE(BACKWARD);
	_delay_ms(100);
	BOT_STOP();
	_delay_ms(100);
	//PWM_SET();
	while((SENSOR_PIN & GRID_SENSOR_MASK) != 0b11100000)
	{
		//OCR1A = 180;
		//OCR1B = 180;
		BOT_MOVE(BACKWARD);
	}
	BOT_STOP();
	_delay_ms(100);
	BOT_MOVE(FORWARD);
	_delay_ms(210);
	BOT_STOP();		
	if(dir == 'N')
	Y--;
	else if(dir=='E')
	X++;
	else if(dir=='W')
	X--;
	else if(dir=='S')
	Y++	;
	prev_count = TRUE;
	if(currentMode == BLOCK_SEARCH_MODE && blockCounter < 3)	DETECT_NEXT_NODE();
	if(blockCounter == 3 && currentMode == BLOCK_SEARCH_MODE)
	{
		mode = 1;
		currentMode = SOLVE_MODE;
		LCD_CLEAR();
		LCD_PRINT("NOW, ROBOT IS");
		LCD_SET_CURSER(2,2);
		LCD_PRINT("at ");
		LCD_DATA('(');
		LCD_DATA(X+0x30);
		LCD_DATA(',');
		LCD_DATA(Y+0x30);
		LCD_DATA(')');
		_delay_ms(2000);
		main();
	}
}

void BLOCK_RECORD()
{
	prev_block_detect = TRUE;
	if(currentMode == BLOCK_SEARCH_MODE)
	{
		block[block_no].X = X;
		block[block_no].Y = Y;
		details1[X][Y].status = BLOCK;
		details2[X][Y].status = BLOCK;
		details1[X][Y].priority = -1;
		LCD_CLEAR();
		LCD_PRINT("BLOCK");
		LCD_DATA(block_no+0x31);
		LCD_PRINT(" @ ");
		LCD_DATA('(');
		LCD_DATA(block[block_no].X+0x30);
		LCD_DATA(',');
		LCD_DATA(block[block_no].Y+0x30);
		LCD_DATA(')');
		coord_count++;
		blockCounter++;
		MOVE_BACK_IF_BLOCK();
	}
}

void STOP_N_UPDATE_LOC()
{
	BOT_STOP();
	if (dir == 'N')	Y++;
	else if (dir == 'E')	X--;
	else if (dir == 'W')	X++;
	else if (dir == 'S')	Y--	;
	count++;
	SOUND_BUZZER();
}

char DETECT_BLOCK()
{
	if((SENSOR_PIN & BLOCK_SENSOR_MASK) == 0b00000110 && prev_block_detect == FALSE && block_caught == FALSE)
	{
		BOT_STOP();
		STOP_N_UPDATE_LOC();
		block_no = 0;
		BLOCK_RECORD();
		return TRUE;
	}
	else if ((SENSOR_PIN & BLOCK_SENSOR_MASK) == 0b00000100 && prev_block_detect == FALSE && block_caught == FALSE)
	{
		BOT_STOP();
		STOP_N_UPDATE_LOC();
		block_no = 1;
		BLOCK_RECORD();
		return TRUE;
	}
	else if ((SENSOR_PIN & BLOCK_SENSOR_MASK) == 0b00000000 && prev_block_detect == FALSE && block_caught == FALSE)
	{
		BOT_STOP();
		STOP_N_UPDATE_LOC();
		block_no = 2;
		BLOCK_RECORD();
		return TRUE;
	}
	else //if(((SENSOR_PIN & 0x08) != 0x00 && block_caught == FALSE))
	{
		prev_block_detect =  FALSE;
		return FALSE;
	}
}

void FOLLOW_LINE_FORWARD()
{	
	if(DETECT_BLOCK() == TRUE)
	{
		BOT_STOP();
		SOUND_BUZZER();
	}
	
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11100000 && prev_count==FALSE)
	{
		prev_count= TRUE;
		count++;
		
		if(dir == 'N')
		Y++;
		else if(dir=='E')
		X--;
		else if(dir=='W')
		X++;
		else if(dir=='S')
		Y--;
		if(currentMode == BLOCK_SEARCH_MODE)
		{
			details1[X][Y].status = VISITED;
			details1[X][Y].priority = 0;
			coord_count++;
			DETECT_NEXT_NODE();
		}
		//if(BLOCK_DISTANCE() > 13 )
		//{
			//speed = 100;
		//}
		//_delay_ms(100);
	}
	
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11100000 && prev_count==TRUE)
	{
		BOT_MOVE(FORWARD);
	}
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b01000000)
	{
		BOT_MOVE(FORWARD);
		prev_count = FALSE;
	}
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11000000)
	{
		BOT_MOVE(LEFT);
		//prev_count = FALSE;
	}
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b10000000)
	{
		BOT_MOVE(LEFT);
		//prev_count = FALSE;
	}
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b01100000)
	{
		BOT_MOVE(RIGHT);
		//prev_count = FALSE;
	}
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b00100000)
	{
		BOT_MOVE(RIGHT);
		//prev_count = FALSE;
	}
	else if ((SENSOR_PIN & GRID_SENSOR_MASK) == 0b00000000)
	{
		BOT_STOP();
	}
}

void FOLLOW_LINE_BACKWARD()
{
	if (((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11100000 && prev_count==FALSE))
	{
		prev_count= TRUE;
		count++;
	}
	else if (((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11100000 && prev_count==TRUE))
	{
		BOT_MOVE(BACKWARD);
	}
	else
	{
		BOT_MOVE(BACKWARD);
	}
}

void TURN_90(unsigned char direction)
{
	BOT_STOP();
	_delay_ms(100);
	if(direction == LEFT)
	{
		BOT_MOVE(FORWARD);
		_delay_ms(210);
		BOT_STOP();
		_delay_ms(100);
		BOT_MOVE(LEFT_BACK);
		_delay_ms(TURN_90_DELAY);
		//if((SENSOR_PIN & GRID_SENSOR_MASK) == 0b111000000) BOT_MOVE(LEFT_BACK);
		//_delay_ms(75);
		BOT_STOP();
		switch(dir)
		{
			case 'N':
				dir = 'W';
				break;
			case 'E':
				dir = 'N';
				break;
			case  'W':
				dir ='S';
				break;
			case 'S':
				dir = 'E';
				break;
		}
	}
	
	else if(direction == RIGHT)
	{
		BOT_MOVE(FORWARD);
		_delay_ms(210);
		BOT_STOP();
		_delay_ms(100);
		BOT_MOVE(RIGHT_BACK);
		_delay_ms(TURN_90_DELAY);
		//if((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11100000) BOT_MOVE(RIGHT_BACK);
		//_delay_ms(75);
		BOT_STOP();		
		switch(dir)
		{
			case 'N':
				dir = 'E';
				break;
			case 'E':
				dir = 'S';
				break;
			case  'W':
				dir ='N';
				break;
			case 'S':
				dir = 'W';
				break;
		}
	}
}

void TURN_180()
{
	BOT_STOP();
	_delay_ms(100);
	BOT_MOVE(FORWARD);
	_delay_ms(210);
	BOT_STOP();
	_delay_ms(100);
	BOT_MOVE(LEFT_BACK);
	_delay_ms(TURN_180_DELAY);
	BOT_STOP();
	if((SENSOR_PIN & GRID_SENSOR_MASK) != 0b11100000) BOT_MOVE(LEFT_BACK);
	_delay_ms(100);
	BOT_STOP();
	switch(dir)
	{
		case 'N':
			dir = 'S';
			break;
		case 'E':
			dir = 'W';
			break;
		case  'W':
			dir ='E';
			break;
		case 'S':
			dir = 'N';
			break;
	}
	loop_count++;
}

void FOLLOW_ONE_NODE(unsigned char direction)
{
	count = 0;
	if(direction == FORWARD)
	{
		while (count == 0)
		{
			FOLLOW_LINE_FORWARD();
		}
		
	}
	else if(direction == BACKWARD)
	{
		while(count == 0 )
		{
			FOLLOW_LINE_BACKWARD();
		}
	}
}

void MOVE_NEXT_NODE(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2)
{	
	if(x2==x1 && y2==y1)
	{
		BOT_STOP();
	}
	if(y2>y1)
	{
		if(dir == 'N')
		{
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'E' )
		{
			TURN_90(LEFT);
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'W')
		{
			TURN_90(RIGHT);
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'S')
		{
			TURN_180();
			FOLLOW_ONE_NODE(FORWARD);
		}
		
	}
	else if(y2<y1)
	{
		if(dir == 'N')
		{
			TURN_180();
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'E' )
		{
			TURN_90(RIGHT);
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'W')
		{
			TURN_90(LEFT);
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'S')
		{
			FOLLOW_ONE_NODE(FORWARD);
		}
	}
	else if(x2>x1)
	{
		if(dir == 'N')
		{
			TURN_90(LEFT);
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'E' )
		{
			TURN_180();
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'W')
		{
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'S')
		{
			TURN_90(RIGHT);
			FOLLOW_ONE_NODE(FORWARD);
		}
	}
	else if(x2<x1)
	{
		if(dir == 'N')
		{
			TURN_90(RIGHT);
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'E' )
		{
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'W')
		{
			TURN_180();
			FOLLOW_ONE_NODE(FORWARD);
		}
		else if(dir == 'S')
		{
			TURN_90(LEFT);
			FOLLOW_ONE_NODE(FORWARD);
		}
	}
}
void DETECT_NEXT_NODE()
{
	unsigned char dir_count = 0;
	STRUCTURE nextNode;
	nextNode.X = X;
	nextNode.Y = Y;
	nextNode.priority = 0;

	if((X-1) >= 0 && (X-1) < MAX_GRID_X)
	{
		if(details1[X-1][Y].status == UNVISITED)
		{
			details1[X-1][Y].priority++;
		}
	}

	if((Y-1) >= 0 && (Y-1) < MAX_GRID_Y )
	{
		if(details1[X][Y-1].status == UNVISITED)
		{
			details1[X][Y-1].priority++;
		}
	}

	if((X+1) > 0 && (X+1) < MAX_GRID_X)
	{
		if(details1[X+1][Y].status == UNVISITED)
		{
			details1[X+1][Y].priority++;
		}
	}
	
	if((Y+1) > 0 && (Y+1) < MAX_GRID_Y )
	{
		if(details1[X][Y+1].status == UNVISITED)
		{
			details1[X][Y+1].priority++;
		}
	}

	if(((X+1)<MAX_GRID_X) && (details1[X+1][Y].priority > nextNode.priority) && (details1[X+1][Y].status == UNVISITED))
	{
		nextNode.X = X+1;
		nextNode.Y = Y;
		dir_count = 1;
		loop_count = 1;
	}
	
	if(((Y-1)>=0) && (details1[X][Y-1].priority > nextNode.priority) && (details1[X][Y-1].status == UNVISITED))
	{
		nextNode.X = X;
		nextNode.Y = Y-1;
		dir_count = 2;
		loop_count = 1;
	}
	
	if(((X-1)>=0) && (details1[X-1][Y].priority > nextNode.priority) && (details1[X-1][Y].status == UNVISITED))
	{
		nextNode.X = X-1;
		nextNode.Y = Y;
		dir_count = 3;
		loop_count = 1;
	}
	
	if(((Y+1)<MAX_GRID_Y) && (details1[X][Y+1].priority > nextNode.priority) && (details1[X][Y+1].status == UNVISITED))
	{
		nextNode.X = X;
		nextNode.Y = Y+1;
		dir_count = 4;
		loop_count = 1;
	}
	
	if (dir_count == 0)
	{
		loop_count++;
		if(((Y+1)<MAX_GRID_Y) && (details1[X][Y+1].status == VISITED))
		{
			nextNode.X = X;
			nextNode.Y = Y+1;
		}
		if(((X-1)>=0) && (details1[X-1][Y].status == VISITED))
		{
			nextNode.X = X-1;
			nextNode.Y = Y;
		}
		if(((Y-1)>=0) && (details1[X][Y-1].status == VISITED))
		{
			nextNode.X = X;
			nextNode.Y = Y-1;
		}
		if(((X+1)<MAX_GRID_X) && (details1[X+1][Y].status == VISITED))
		{
			nextNode.X = X+1;
			nextNode.Y = Y;
		}
		details1[X][Y].status = BLOCK;
	}
	loop_count--;
	if (loop_count > 4 && currentMode == BLOCK_SEARCH_MODE)
	{
		loop_count = 5;
		if(((X+1)<MAX_GRID_X) && (details1[X+1][Y].status == VISITED))
		{
			nextNode.X = X+1;
			nextNode.Y = Y;
		}
		
		if(((Y-1)>=0) && (details1[X][Y-1].status == VISITED))
		{
			nextNode.X = X;
			nextNode.Y = Y-1;
		}
		
		if(((X-1)>=0) && (details1[X-1][Y].status == VISITED))
		{
			nextNode.X = X-1;
			nextNode.Y = Y;
		}
		
		if(((Y+1)<MAX_GRID_Y) && (details1[X][Y+1].status == VISITED))
		{
			nextNode.X = X;
			nextNode.Y = Y+1;
		}
	}
	
	LCD_CLEAR();
	LCD_SET_CURSER(2,1);
	LCD_DATA('(');
	LCD_DATA(X+0x30);
	LCD_DATA(',');
	LCD_DATA(Y+0x30);
	LCD_DATA(')');
	LCD_PRINT(" --> ");
	LCD_PRINT("(");
	LCD_DATA(nextNode.X+0x30);
	LCD_DATA(',');
	LCD_DATA(nextNode.Y+0x30);
	LCD_DATA(')');
	LCD_SET_CURSER(1,7);
	LCD_NUM(coord_count);
	if(currentMode == BLOCK_SEARCH_MODE)
	{
		MOVE_NEXT_NODE(X,Y,nextNode.X,nextNode.Y);
	}
}

void ENQUEUE(STRUCTURE q)
{
	QUEUE[Qfront] = q;
	Qfront++;
}

STRUCTURE DEQUEUE()
{
	Qrear++;
	return (QUEUE[Qrear-1]);
}

unsigned char DIGKSTRA(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2, unsigned char MOVE)
{
	unsigned char i,j;
	STRUCTURE dqued, pathQUEUE[12];
	unsigned char tempX = 0, tempY = 0;
	unsigned char tempDistance = 0;
	for(i=0;i<MAX_GRID_X;i++)
	{
		for(j=0;j<MAX_GRID_Y;j++)
		{
			details2[i][j].distance = INFINITE;
			details2[i][j].visit = FALSE;
			details2[i][j].X = i;
			details2[i][j].Y = j;
		}
	}	
	details2[x1][y1].distance = 0;
	details2[x1][y1].visit = TRUE;
	Qfront=0; Qrear=0;
	
	ENQUEUE(details2[x1][y1]);
	while(Qrear != 34)
	{
		dqued = DEQUEUE();
		tempX = dqued.X;
		tempY = dqued.Y;
		if((tempY+1 < MAX_GRID_Y) && details2[tempX][tempY+1].visit == FALSE && details2[tempX][tempY+1].status != DESTINATION)
		{
			if(block_caught == TRUE && details1[tempX][tempY+1].status == BLOCK)
			{
				;
			}
			else
			{
			ENQUEUE(details2[tempX][tempY+1]);
			tempDistance = details2[tempX][tempY].distance+1;
			details2[tempX][tempY+1].visit = TRUE;
			if(tempDistance < details2[tempX][tempY+1].distance)
			{
				details2[tempX][tempY+1].distance =  tempDistance;
				details2[tempX][tempY+1].prevX= tempX;
				details2[tempX][tempY+1].prevY= tempY;
			}
			}
		}
		
		if((tempX-1 >= 0) && details2[tempX-1][tempY].visit == FALSE && details2[tempX-1][tempY].status != DESTINATION)
		{
			if(block_caught == TRUE && details1[tempX-1][tempY].status == BLOCK)
			{
				;
			}
			else
			{
			ENQUEUE(details2[tempX-1][tempY]);
			tempDistance = details2[tempX][tempY].distance+1;
			details2[tempX-1][tempY].visit = TRUE;
			if(tempDistance < details2[tempX-1][tempY].distance)
			{
				details2[tempX-1][tempY].distance =  tempDistance;
				details2[tempX-1][tempY].prevX= tempX;
				details2[tempX-1][tempY].prevY= tempY;
			}
			}
		}
	
		if((tempY-1 >= 0) && details2[tempX][tempY-1].visit == FALSE && details2[tempX][tempY-1].status != DESTINATION)
		{
			if(block_caught == TRUE  && details1[tempX][tempY-1].status == BLOCK)
			{
				;
			}
			else
			{
			ENQUEUE(details2[tempX][tempY-1]);
			tempDistance = details2[tempX][tempY].distance+1;
			details2[tempX][tempY-1].visit = TRUE;
			if(tempDistance < details2[tempX][tempY-1].distance)
			{
				details2[tempX][tempY-1].distance =  tempDistance;
				details2[tempX][tempY-1].prevX= tempX;
				details2[tempX][tempY-1].prevY= tempY;
			}
			}
		}
		
		if((tempX+1 < MAX_GRID_X) && details2[tempX+1][tempY].visit == FALSE && details2[tempX+1][tempY].status != DESTINATION)
		{
			if(block_caught == TRUE  && details1[tempX+1][tempY].status == BLOCK)
			{
				;
			}
			else
			{
			ENQUEUE(details2[tempX+1][tempY]);
			tempDistance = details2[tempX][tempY].distance+1;
			details2[tempX+1][tempY].visit = TRUE;
			if(tempDistance < details2[tempX+1][tempY].distance)
			{
				details2[tempX+1][tempY].distance =  tempDistance;
				details2[tempX+1][tempY].prevX= tempX;
				details2[tempX+1][tempY].prevY= tempY;
			}
			}
		}
	}

	pathQUEUE[0].X = x2;
	pathQUEUE[0].Y = y2;
	tempX = pathQUEUE[0].X;
	tempY = pathQUEUE[0].Y;
	j = details2[x2][y2].distance;
	
	if(j>0 && j<INFINITE)
	{
		for(i=1;i<=j;i++)
		{
			pathQUEUE[i].X = details2[tempX][tempY].prevX;
			pathQUEUE[i].Y = details2[tempX][tempY].prevY;
			tempX = pathQUEUE[i].X;
			tempY = pathQUEUE[i].Y;
		}
		
		if(MOVE==TRUE)
		{
			for(i=j;i>0;i--)
			{
				//LCD_CLEAR();
				LCD_SET_CURSER(2,1);
				LCD_DATA('(');
				LCD_DATA(pathQUEUE[i].X + 0x30);
				LCD_DATA(',');
				LCD_DATA(pathQUEUE[i].Y + 0x30);
				LCD_DATA(')');
				LCD_PRINT(" --> ");
				LCD_PRINT("(");
				LCD_DATA(pathQUEUE[i-1].X + 0x30);
				LCD_DATA(',');
				LCD_DATA(pathQUEUE[i-1].Y + 0x30);
				LCD_DATA(')');				
				MOVE_NEXT_NODE(pathQUEUE[i].X,pathQUEUE[i].Y,pathQUEUE[i-1].X,pathQUEUE[i-1].Y);
			}
		}
		return j;	
	}
	return INFINITE;
}

void REMOVE_DESTINATION_OR_BLOCK_FROM_ARRAY(unsigned char index, unsigned char arrayType)
{
	if(arrayType == BLOCK)
	{
		block[index] = block[blockCounter-1];
		blockCounter--;
	}
	else if(arrayType == DESTINATION)
	{
		destination[index] = destination[destinationCounter-1];
		destinationCounter--;
	}
}

void MOVE_TO_NEAREST_BLOCK()
{
	unsigned char i, j=0;
	unsigned char distance,tempd;
	block_caught = FALSE;
	distance = DIGKSTRA(X,Y,block[0].X,block[0].Y,FALSE);
	for(i=1;i<blockCounter;i++)
	{
		tempd = DIGKSTRA(X,Y,block[i].X,block[i].Y,FALSE);
		if(tempd < distance)
		{
			distance = tempd;
			j=i;
		}
	}	
	LCD_CLEAR();
	LCD_PRINT("Aproach to block");
	BOT_MOVE(FORWARD);
	_delay_ms(100);
	BOT_STOP();
	DIGKSTRA(X,Y,block[j].X,block[j].Y,TRUE);
	block_no_1 = j;
	REMOVE_DESTINATION_OR_BLOCK_FROM_ARRAY(j,BLOCK);
	BLOCK_HOLD(FORWARD); //grip the block
	LCD_SET_CURSER(1,1);
	LCD_PRINT("    Gripping    ");
	_delay_ms(BLOCK_GRIP_DELAY);
	BLOCK_LIFT(FORWARD);  //lift the block
	_delay_ms(BLOCK_LIFT_DELAY);
	block_caught = TRUE;
	FOLLOW_ONE_NODE(FORWARD);
	if(dir == 'N')
		Y--;
	else if(dir=='E')
		X++;
	else if(dir=='W')
		X--;
	else if(dir=='S')
		Y++;
}

void MOVE_TO_NEAREST_DESTINATION()
{
	LCD_SET_CURSER(1,1);
	LCD_PRINT("   Arranging   ");
	dest_coun = block_no_1;
	DIGKSTRA(X,Y,destination[dest_coun].X,destination[dest_coun].Y,TRUE);
	details2[destination[dest_coun].X][destination[dest_coun].Y].status = DESTINATION;
	REMOVE_DESTINATION_OR_BLOCK_FROM_ARRAY(dest_coun,DESTINATION);
	BOT_STOP();
	BOT_MOVE(BACKWARD); //move little back to place block
	_delay_ms(200);
	LCD_CLEAR();
	LCD_PRINT("   Placing @");
	LCD_SET_CURSER(2,1);
	LCD_PRINT("     (");
	LCD_DATA(X+0x30);
	LCD_DATA(',');
	LCD_DATA(Y+0x30);
	LCD_DATA(')');
	BLOCK_LIFT(BACKWARD); //lift down the block
	_delay_ms(BLOCK_DOWN_DELAY);
	BLOCK_HOLD(BACKWARD); //release the block
	_delay_ms(BLOCK_RELEASE_DELAY);
	BOT_STOP();
	block_caught = FALSE;
	MOVE_BACK_IF_BLOCK();
	LCD_CLEAR();
}

int main(void)
{
	unsigned char i=0;
	while(1)
	{
		if(mode == 0)
		{
			LCD_INITIALIZE();
			PORT_INITIALIZE();
			SET_INITIAL_INFO();
			//TCCR1A = 0xA1; TCCR1B = 0x01;
			//OCR1A = 150; OCR1B = 150;
			LCD_SET_CURSER(1,1);
			LCD_PRINT("Welcome Hetauda!");
			LCD_SET_CURSER(2,1);
			LCD_PRINT("Please Wait...");
			_delay_ms(1000);	
			LCD_CLEAR();
			LCD_PRINT("  CAN INFOTECH");
			LCD_SET_CURSER(2,1);
			LCD_PRINT("      2016");
			_delay_ms(1000);
			/*while(1)
			{
				LCD_CLEAR();
				LCD_NUM(BLOCK_DISTANCE());
				_delay_ms(100);
			}*/
			while(1)
			{
				if((SENSOR_PIN & GRID_SENSOR_MASK) == 0b11100000)
				{
					SOUND_BUZZER();
					LCD_CLEAR();
					LCD_PRINT("BOT @ Start Node");
					_delay_ms(1000);
					currentMode = INTERMEDIATE_MODE;
					FOLLOW_ONE_NODE(FORWARD);
			
			//------BLOCK SEARCH MODE STARTS HERE-------
					currentMode = BLOCK_SEARCH_MODE;
					X=0;Y=0;
					details1[0][0].status = VISITED;
					LCD_CLEAR();
					LCD_PRINT("BLOCK SEARCH MOD");
					_delay_ms(1000);
					BOT_MOVE(FORWARD);
					_delay_ms(100);
					DETECT_NEXT_NODE();
				}
			}
				
			//------BLOCK SEARCH MODE ENDS HERE-------
	
		}
			//------BLOCK SOLVE MODE STARTS HERE--------
			LCD_CLEAR();
			LCD_PRINT("  SOLVE MODE");
			_delay_ms(1000);
			currentMode = SOLVE_MODE;
			DEFINE_DESTINATION();
			for (i=0;i<ROUND1;i++)
			{
				MOVE_TO_NEAREST_BLOCK();
				MOVE_TO_NEAREST_DESTINATION();
			}
			//------BLOCK SOLVE MODE ENDS HERE--------
			DIGKSTRA(X,Y,0,0,TRUE);
			BOT_STOP();
			SOUND_BUZZER();
			_delay_ms(100);
			SOUND_BUZZER();
			_delay_ms(100);
			SOUND_BUZZER();
			LCD_CLEAR();
			LCD_PRINT("TASK COMPLETED!!");
			while(1)
			{
				;
			}
	}
}
