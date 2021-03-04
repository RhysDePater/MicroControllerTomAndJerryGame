#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <graphics.h>
#include <macros.h>
#include "lcd_model.h"
#include "lcd.h"
#include "usb_serial.h"
#include "cab202_adc.h"

//-----------------------------------
//Variables
//-----------------------------------

//presets for calculating time through timer1
#define FREQUENCY 8000000.0
#define PRESCALE 8

//screen dimensions
#define STATUS_BAR_HEIGHT 8
#define WIDTH 84
#define HEIGHT 48

int lives;
#define walls 4
int level;
int score;
int seed;

//wall positions
int wallX1[walls] = {18, 25, 45, 58};
int wallX2[walls] = {13, 25, 60, 72};
int wallY1[walls] = {15, 35, 10, 25};
int wallY2[walls] = {25, 45, 10, 30};

//wall start positions so that they can get reset back to the start in a new level
int wallSX1[walls] = {18, 25, 45, 58};
int wallSX2[walls] = {13, 25, 60, 72};
int wallSY1[walls] = {15, 35, 10, 25};
int wallSY2[walls] = {25, 45, 10, 30};

//overflow for wall movement timer
volatile int wallOverflow;

//tom postions and start positions
double tomX, tomStartX;
double tomY, tomStartY;

//Jerry positions
double jerryX, jerryStartX;
double jerryY, jerryStartY;

//Tom movement variables
double tomDir, tomSpeed, tomDX, tomDY;

bool pause;
bool gameOver;
bool startScreen;

//time
char buffer[81];
double gameTime;
int seconds, minutes;
volatile int overflow_counter;

//varible to store serial input
char inputChar;

//initilise functions so they can be used before it is called
void draw_double(uint8_t x, uint8_t y, double value, colour_t colour);
void draw_int(uint8_t x, uint8_t y, int value, colour_t colour);
void draw_time(uint8_t x, uint8_t y, int value, colour_t colour);
void drawAll();
void setup();

#ifndef M_PI
#define M_PI 3.14159265358979323
#endif

//debounce values
uint8_t mask = 0b00000001;
volatile uint32_t rightBState, leftBState, rightState, leftState, downState, upState, centerState = 0;
volatile uint32_t rightBPress, leftBPress, rightPress, leftPress, downPress, upPress, centerPress;

//cheese variables
struct cheese
{
    double x;
    double y;
};

struct cheese cheeses[5] = {{1, 1}};

bool maxCheese;
volatile int cheeseOverflow;
int cheeseConsumed;

//trap variables
struct trap
{
    double x;
    double y;
};

struct trap traps[5] = {{1, 1}};

bool maxTraps;
volatile int trapOverflow;

//door variables;
struct door
{
    double x;
    double y;
};

struct door exitDoor = {-1, -1};
bool doorSpawned;

//fireworks Variables
struct firework
{
    double x;
    double y;
};
struct firework fireworks[20] = {{-1, -1}};
bool fireworkShootable;

//potion Variables
struct potion
{
    double x;
    double y;
};
struct potion jerryPotion = {-1, -1};
volatile int potionOverflow;
volatile int superOverFlow;
bool superJerry;
int superSize;

//pwn led 0 and 1 control variables
volatile uint8_t lightB;
volatile uint8_t overflow_counter1;

//-----------------------------------
//functions
//-----------------------------------

//function for setting up timer1 which is used for the time within the game
void setup_timer1(void)
{

    // Timer 1 in normal mode (WGM12. WGM13),
    // with pre-scaler 8 ==> 	(CS12,CS11,CS10).
    // Timer overflow on. (TOIE1)

    CLEAR_BIT(TCCR1B, WGM12);
    CLEAR_BIT(TCCR1B, WGM13);
    CLEAR_BIT(TCCR1B, CS12); //0 see table 14-6
    SET_BIT(TCCR1B, CS11);   //1
    CLEAR_BIT(TCCR1B, CS10); //0

    //enabling the timer overflow interrupt
    SET_BIT(TIMSK1, TOIE1);
}

//overflow for timer 1
ISR(TIMER1_OVF_vect)
{
    //increment game time variables, which are user for recording time and when actions should take
    //place
    if (!startScreen)
    {
        if (!pause)
        {
            overflow_counter++;
            cheeseOverflow++;
            trapOverflow++;
            wallOverflow++;
            if (level == 2)
            {
                potionOverflow++;
                if (superJerry)
                {
                    superOverFlow++;
                }
            }
        }
    }

    //debounce Right Button
    rightBState = ((rightBState << 1) & mask) | BIT_IS_SET(PINF, 5);

    if (rightBState == mask)
    {
        rightBPress = 1;
    }

    else if (rightBState == 0)
    {
        rightBPress = 0;
    }

    //debounce left Button
    leftBState = ((leftBState << 1) & mask) | BIT_IS_SET(PINF, 6);

    if (leftBState == mask)
    {
        leftBPress = 1;
    }

    else if (leftBState == 0)
    {
        leftBPress = 0;
    }

    //debounce right toggle
    rightState = ((rightState << 1) & mask) | BIT_IS_SET(PIND, 0);

    if (rightState == mask)
    {
        rightPress = 1;
    }

    else if (rightState == 0)
    {
        rightPress = 0;
    }

    //debounce left toggle
    leftState = ((leftState << 1) & mask) | BIT_IS_SET(PINB, 1);

    if (leftState == mask)
    {
        leftPress = 1;
    }

    else if (leftState == 0)
    {
        leftPress = 0;
    }

    //debounce up toggle
    upState = ((upState << 1) & mask) | BIT_IS_SET(PIND, 1);

    if (upState == mask)
    {
        upPress = 1;
    }

    else if (upState == 0)
    {
        upPress = 0;
    }

    //debounce down toggle
    downState = ((downState << 1) & mask) | BIT_IS_SET(PINB, 7);

    if (downState == mask)
    {
        downPress = 1;
    }

    else if (downState == 0)
    {
        downPress = 0;
    }

    //debounce down toggle
    centerState = ((centerState << 1) & mask) | BIT_IS_SET(PINB, 0);

    if (centerState == mask)
    {
        centerPress = 1;
    }

    else if (centerState == 0)
    {
        centerPress = 0;
    }
}

//function for check if anything is within the 5x5 bounds of a specified x and y coorodinate and return true or false
bool Collision(int x, int y)
{
    //get the bank and y pixle positions within the bank
    uint8_t bank = y >> 3;
    uint8_t pixel = y & 7;
    bool collided = false;

    //loop through the 5x5 area
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            //get the bank and pixle position of the current y coord being passsed through j
            bank = (y + j) >> 3;
            pixel = (y + j) & 7;
            //check that x and y position on screen and see if it is set
            if (screen_buffer[bank * LCD_X + (x + i)] & (1 << pixel))
            {
                //if there is an on pixel set collided to true
                collided = true;
            }
        }
    }
    return collided;
}

//function for checking if there is a pixel on at any of the pixel where a Jerry pixel should be (J)
bool JerryCollision(int x, int y)
{
    //initilise the variables for storing the y bank and pixel
    uint8_t bank = y >> 3;
    uint8_t pixel = y & 7;

    bool collided = false;
    //loop through for the top 5 pixel pixels at the specified x and y positions
    //this is checking the line across the top of Jerry;s J
    for (int j = 0; j < 5 + superSize; j++)
    {
        bank = (y) >> 3;
        pixel = (y)&7;
        if (screen_buffer[bank * LCD_X + (x + j)] & (1 << pixel))
        {
            collided = true;
            break;
        }
    }
    //if there hasn't been a collision yet
    if (!collided)
    {
        //loop through 4 pixel across the y direction at position x+2, this would be the center of Jerry and Supersize
        //is for when the potion has been obtained
        for (int i = 1; i < 4 + superSize; i++)
        {
            bank = (y + i) >> 3;
            pixel = (y + i) & 7;
            if (screen_buffer[bank * LCD_X + (x + 2)] & (1 << pixel))
            {
                collided = true;
                break;
            }
        }
    }
    //if there hasn't been a collision yet
    if (!collided)
    {
        //loop though three pixels (or 4 in supermode) at the bottom of the j, this is x + h, y+4+superSize
        for (int h = 0; h < 3 + superSize; h++)
        {
            bank = (y + 4 + superSize) >> 3;
            pixel = (y + 4 + superSize) & 7;
            if (screen_buffer[bank * LCD_X + (x + h)] & (1 << pixel))
            {
                collided = true;
                break;
            }
        }
    }
    //return the value for collsion
    return collided;
}

//this function is for checking if an pixels in the T for tom is colliding with another pixel.
//it takes an x and y position which should be Toms x and y
bool TomCollision(int x, int y)
{
    //first the y bank and pixel varibales are initilised
    uint8_t bank = y >> 3;
    uint8_t pixel = y & 7;

    bool collided = false;
    //loop for the top 5 pixels of Toms T
    for (int j = 0; j < 5; j++)
    {
        bank = (y) >> 3;
        pixel = (y)&7;
        if (screen_buffer[bank * LCD_X + (x + j)] & (1 << pixel))
        {
            collided = true;
            break;
        }
    }
    //if there has been no collision
    if (!collided)
    {
        //loop through for the 5 pixels down the y axis of Toms T, has x+2
        for (int i = 1; i < 5; i++)
        {
            bank = (y + i) >> 3;
            pixel = (y + i) & 7;
            if (screen_buffer[bank * LCD_X + (x + 2)] & (1 << pixel))
            {
                collided = true;
                break;
            }
        }
    }
    //return the value for collison
    return collided;
}

//save the start values for walls to the backup arrays
void setWall()
{
    for (int i = 0; i < 4; i++)
    {
        wallX1[i] = wallSX1[i];
        wallX2[i] = wallSX2[i];
        wallY1[i] = wallSY1[i];
        wallY2[i] = wallSY2[i];
    }
}

//this function takes a pointer for an x and y variable and assigns them a random screen position that has no pixel
//on within a 5x5 arae
void randomScreenPosition(int *X, int *Y)
{
    //initilise a varible to keep track whether a suitable location has been found
    bool spawnOK = false;

    //while no suitable location has been found, repeat
    while (!spawnOK)
    {
        //assign a random value within the screen bounds -5 since they are 5 pixels big
        *X = round(rand() % (WIDTH - 5));
        *Y = round(rand() % (HEIGHT - 5));
        //is the value below the status bar
        if (*Y > STATUS_BAR_HEIGHT)
        {
            //if there is no pixel on with a 5x5 area of the x and y
            if (!Collision(*X, *Y))
            {
                //break out of the loop
                spawnOK = true;
            }
        }
    }
}

//this function draws jerry at his current x and y coordinate
void drawJerry()
{
    draw_line(jerryX, jerryY, jerryX + 4 + superSize, jerryY, FG_COLOUR);
    draw_line(jerryX + 2 + superSize, jerryY, jerryX + 2 + superSize, jerryY + 4 + superSize, FG_COLOUR);
    draw_line(jerryX + 2 + superSize, jerryY + 4 + superSize, jerryX, jerryY + 4 + superSize, FG_COLOUR);
}

//this function draws Tom at his current x and y position
void drawTom()
{
    draw_line(tomX, tomY, tomX + 4, tomY, FG_COLOUR);
    draw_line(tomX + 2, tomY, tomX + 2, tomY + 4, FG_COLOUR);
}

//this function draws a cheese at a specified x and y coordinate
void drawCheese(int x, int y)
{
    draw_line(x, y, x + 4, y, FG_COLOUR);
    draw_line(x, y, x, y + 4, FG_COLOUR);
    draw_line(x, y + 4, x + 4, y + 4, FG_COLOUR);
    draw_line(x + 4, y, x + 4, y + 1, FG_COLOUR);
    draw_line(x + 4, y + 3, x + 4, y + 4, FG_COLOUR);
}

//this function sets every cheeses x and y value to -1 which means they will be ignored and not draw or interacted with
void clearCheese()
{
    for (int i = 0; i < 5; i++)
    {
        cheeses[i].x = -1;
        cheeses[i].y = -1;
    }
}

//this function sets every traps x and y value to -1 which means they will be ignored and not draw or interacted with
void clearTraps()
{
    for (int i = 0; i < 5; i++)
    {
        traps[i].x = -1;
        traps[i].y = -1;
    }
}

//this function sets the door x and y to -1 meaning it isn't drawn
void clearDoor()
{
    exitDoor.x = -1;
    exitDoor.y = -1;
}

//this function sets the potion x and y to -1 meaning it isn't drawn
void clearPotion()
{
    jerryPotion.x = -1;
    jerryPotion.y = -1;
}

//this function sets every firworks x and y value to -1 which means they will be ignored and not draw or interacted with
void clearFireworks()
{
    for (int i = 0; i < 20; i++)
    {
        fireworks[i].x = -1;
        fireworks[i].y = -1;
    }
}

//this function is for drawing the cheesing on screen
void drawCheeses()
{
    //loop through for all 5 cheese
    for (int i = 0; i < 5; i++)
    {
        //if the x and y for the current cheese is NOT -1
        if (cheeses[i].x != -1 && cheeses[i].y != -1)
        {
            //draw the specified cheese at the x and y stored for that cheese
            drawCheese(cheeses[i].x, cheeses[i].y);
        }
    }
}

//drawTrao draws a trap at the specified x and y position
void drawTrap(int x, int y)
{
    draw_line(x, y + 1, x + 4, y + 1, FG_COLOUR);
    draw_line(x + 2, y, x + 2, y + 4, FG_COLOUR);
    draw_line(x + 2, y + 4, x + 4, y + 4, FG_COLOUR);
}

//drawTraps draw the traps on screen using the drawTrap function and provideing it with positions
void drawTraps()
{
    //loop for all 5 traps
    for (int i = 0; i < 5; i++)
    {
        //if the x and y for the specified trap is NOT -1
        if (traps[i].x != -1 && traps[i].y != -1)
        {
            //draw the specified trap at the stored x and y position
            drawTrap(traps[i].x, traps[i].y);
        }
    }
}

//this function is for drawing the fireworks on screen
void drawFireworks()
{
    //loop through for all 20 fireworks
    for (int i = 0; i < 20; i++)
    {
        //if the specified firework x and y position is NOT -1
        if (fireworks[i].x != -1 && fireworks[i].y != -1)
        {
            //draw the specified firework at the stored x and y value
            draw_pixel(fireworks[i].x, fireworks[i].y, FG_COLOUR);
        }
    }
}

//this function is for drawing the door in screen
void drawDoor()
{
    //save the door x and y position to an easier to write variable
    int x = exitDoor.x;
    int y = exitDoor.y;

    //if the door position is NOT -1 -1
    if (x != -1 && y != -1)
    {
        //draw the door
        draw_line(x, y, x + 2, y, FG_COLOUR);
        draw_line(x, y, x, y + 4, FG_COLOUR);
        draw_line(x, y + 4, x + 2, y + 4, FG_COLOUR);
        draw_pixel(x + 3, y + 1, FG_COLOUR);
        draw_pixel(x + 4, y + 2, FG_COLOUR);
        draw_pixel(x + 3, y + 3, FG_COLOUR);
    }
}

//function for drawing the potion on the screen
void drawPotion()
{
    //save the potion x and y values to easier to write variables
    int x = jerryPotion.x;
    int y = jerryPotion.y;

    //if the potion x and Y values are NOT -1 -1
    if (x != -1 && y != -1)
    {
        //draw the potion on screen
        draw_line(x, y, x + 4, y, FG_COLOUR);
        draw_line(x, y, x, y + 4, FG_COLOUR);
        draw_line(x + 4, y, x + 4, y + 2, FG_COLOUR);
        draw_line(x, y + 2, x + 4, y + 2, FG_COLOUR);
    }
}

//setupTom calculates Toms direction and speed
void setupTom()
{
    //calculate Toms random direction
    tomDir = rand() * M_PI * 2 / RAND_MAX;
    //calculate Toms speed with a max value of 0.4
    tomSpeed = ((double)rand() / (double)(RAND_MAX)*0.4);

    //if the speed is less then 0.3 set it to 0.3
    if (tomSpeed < 0.3)
    {
        tomSpeed = 0.3;
    }

    //Combine toms speed and direction and assign it to a DX and DY variable
    tomDX = tomSpeed * cos(tomDir);
    tomDY = tomSpeed * sin(tomDir);
}

//this function sets up the serial connection
void setupSerial()
{
    //if in level 2
    if (level == 2)
    {
        //if serial has not been configured yet
        if (!usb_configured())
        {
            //initilise the serial connection
            usb_init();

            while (!usb_configured())
            {
                // Block until USB is ready.
            }
        }
    }
}

//this function sets up the switched on the teensy
void setupInput()
{
    //joysticks
    CLEAR_BIT(DDRD, 1); //up
    CLEAR_BIT(DDRB, 1); //left
    CLEAR_BIT(DDRB, 7); //down
    CLEAR_BIT(DDRD, 0); //right
    CLEAR_BIT(DDRB, 0); //center

    CLEAR_BIT(DDRF, 5); //Right switch
    CLEAR_BIT(DDRF, 6); //Left switch

    //led 1 and 2
    SET_BIT(DDRB, 2);
    SET_BIT(DDRB, 3);
}

//the setupVars function assigns all the variables there intial values
void setupVars(void)
{
    tomX = LCD_X - 5;
    tomY = LCD_Y - 9;
    tomStartX = tomX;
    tomStartY = tomY;

    jerryX = 0;
    jerryY = STATUS_BAR_HEIGHT + 1;
    jerryStartX = jerryX;
    jerryStartY = jerryY;

    lives = 5;
    level = 1;
    score = 0;

    gameTime = 0;
    overflow_counter = 0;
    cheeseOverflow = 0;
    trapOverflow = 0;
    potionOverflow = 0;
    superOverFlow = 0;
    wallOverflow = 0;
    overflow_counter1 = 0;
    seed = 1;
    cheeseConsumed = 0;
    superSize = 0;
    lightB = 0;

    pause = false;
    maxCheese = false;
    maxTraps = false;
    gameOver = false;
    doorSpawned = false;
    fireworkShootable = false;
    superJerry = false;
}

//this function brightLoop is for gradually increasing the brightness of the leds
//when Jerry is super mode
void brightLoop()
{
    //if jerry is in super mode
    if (superJerry)
    {
        //increase the brightness by 8, since the variable is a uint8_t it overlaps back to 0 once
        //254 is reached which is the mac brightness
        lightB += 8;
    }
    else
    {
        //if not is supermode set the brightness to 0
        lightB = 0;
    }
}

//this function sets up the timer used for led PWM
void setupTimer()
{
    // Timer 0 in normal mode (WGM02),
    // with pre-scaler 1024 ==> ~30Hz overflow 	(CS02,CS01,CS00).
    // Timer overflow on. (TOIE0)
    CLEAR_BIT(TCCR0B, WGM02);
    //prescaler 1

    CLEAR_BIT(TCCR0B, CS02); //0
    CLEAR_BIT(TCCR0B, CS01); //0
    SET_BIT(TCCR0B, CS00);   //1

    //enabling the timer overflow interrupt
    SET_BIT(TIMSK0, TOIE0);
}

//this function draws the status bar on screen
void drawStatus()
{
    //lvl
    draw_string(0, 0, "Lv:", FG_COLOUR);
    draw_int(14, 0, level, FG_COLOUR);
    //lives
    draw_string(20, 0, "L:", FG_COLOUR);
    draw_int(29, 0, lives, FG_COLOUR);
    //score
    draw_string(35, 0, "S:", FG_COLOUR);
    draw_int(44, 0, score, FG_COLOUR);
    //time
    draw_time(60, 0, minutes, FG_COLOUR);
    draw_char(70, 0, ':', FG_COLOUR);
    draw_time(74, 0, gameTime, FG_COLOUR);
    //line
    draw_line(0, STATUS_BAR_HEIGHT, 84, STATUS_BAR_HEIGHT, FG_COLOUR);
}

//this function is for drawing the walls in the screen
void drawWalls()
{
    //loop through for every wall
    for (int i = 0; i < walls; i++)
    {
        //draw the wall on the screen
        draw_line(wallX1[i], wallY1[i], wallX2[i], wallY2[i], FG_COLOUR);
    }
}

//this timerOverflow is for the pwm of LED 1 and 2
ISR(TIMER0_OVF_vect)
{
    //increase the value for seed which is used for genereating random numbers
    seed++;

    //if Jerry is in supermode
    if (superJerry)
    {
        //if the counter is less then the light brightness value
        //turn the leds on
        if (overflow_counter1 < lightB)
        {
            SET_BIT(PORTB, 2);
            SET_BIT(PORTB, 3);
        }
        //if it is higher then turn the leds off
        else
        {
            CLEAR_BIT(PORTB, 2);
            CLEAR_BIT(PORTB, 3);
        }
        //increase the counter
        overflow_counter1++;
    }
    //if not in super mode, turn the leds off
    else
    {
        CLEAR_BIT(PORTB, 2);
        CLEAR_BIT(PORTB, 3);
    }
}

//this function is for calculating the game time
void timer()
{
    //claculate the time
    gameTime = (overflow_counter * 65536.0 + TCNT1) * PRESCALE / FREQUENCY;
    //set seconds to time
    seconds = gameTime;
    //if seconds is equal to 60
    if (seconds == 60)
    {
        //restart the timer and increment the minutes
        overflow_counter = 0;
        minutes++;
    }
}

//this is the function for spawning cheese on the screen (giving them a position)
void spawnCheese()
{
    int cheeseX;
    int cheeseY;
    bool spawnCheese = false;
    int cheeseTime = 0;

    //if there are still cheese to spawn calculate the time
    if (!maxCheese)
    {
        cheeseTime = (cheeseOverflow * 65536.0 + TCNT1) * PRESCALE / FREQUENCY;
    }
    //if the max cheese have been spawned, set the overflow to 0 (time to 0)
    else
    {
        cheeseOverflow = 0;
    }

    if (cheeseTime >= 2)
    {
        cheeseOverflow = 0;
        //if there are still cheese to spawn
        if (maxCheese == false)
        {
            //loop for every cheese
            for (int i = 0; i < 5; i++)
            {
                //if the specifed cheese x and y is equal to -1 -1, meaning it is not on screen
                if (cheeses[i].x == -1 && cheeses[i].y == -1)
                {
                    //find a randome position for the cheese
                    randomScreenPosition(&cheeseX, &cheeseY);
                    //set the position and break out of the loop
                    cheeses[i].x = cheeseX;
                    cheeses[i].y = cheeseY;
                    //indicate the a cheese was spawned
                    spawnCheese = true;
                    break;
                }
            }
            //if no cheese were spawned then indicate that max cheese have been spawned
            if (spawnCheese == false)
            {
                maxCheese = true;
            }
        }
    }
}

//this function is for spawning the potion on screen (giving it a postion)
void spawnPotion()
{
    //calculate the potion time
    int potionTime = (potionOverflow * 65536.0 + TCNT1) * PRESCALE / FREQUENCY;

    //if the time is over or equal to 5
    if (potionTime >= 5)
    {
        //set the time to zero and check if there is no potion on screen currently
        potionOverflow = 0;
        if (jerryPotion.x == -1 && jerryPotion.y == -1)
        {
            //place the potion at the tom x and y posiion
            jerryPotion.x = round(tomX);
            jerryPotion.y = round(tomY);
        }
    }
}

//this function is for spawning the traps on the screen by providing them with a position
void spawnTrap()
{
    bool spawnTrap = false;
    int trapTime = 0;

    //if there are still traps to spawn calculate it's time
    if (!maxTraps)
    {
        trapTime = (trapOverflow * 65536.0 + TCNT1) * PRESCALE / FREQUENCY;
    }
    //if max traps have been reached set time to 0
    else
    {
        trapOverflow = 0;
    }

    //if the time is over or equal to 3
    if (trapTime >= 3)
    {
        trapOverflow = 0;
        //if there are still traps to spawn
        if (maxTraps == false)
        {
            //loop for all 5 traps and check if any are at -1 -1, meaning they can be spawned
            for (int i = 0; i < 5; i++)
            {
                if (traps[i].x == -1 && traps[i].y == -1)
                {
                    //set the position and break out of the loop
                    traps[i].x = round(tomX);
                    traps[i].y = round(tomY);
                    //indicate a trap has been spawned
                    spawnTrap = true;
                    break;
                }
            }
            //if no traps were spawned indicate the max number of traps has been reached
            if (spawnTrap == false)
            {
                maxTraps = true;
            }
        }
    }
}

//this function is for spawning the door onto the screen
void spawnDoor(void)
{
    int doorX, doorY;
    //if the game is no paused
    if (!pause)
    {
        //if a door hasn't already been spawned
        if (!doorSpawned)
        {
            //if the score is equal to or larger then 5
            if (score >= 5)
            {
                //calculate a random position for the door and set the position
                randomScreenPosition(&doorX, &doorY);
                exitDoor.x = doorX;
                exitDoor.y = doorY;
                //indicate the door is spawned
                doorSpawned = true;
            }
        }
    }
}

//this function is for spawning fireworks onto the screen
void spawnFirework(void)
{
    //initilise a previous state for button pressing
    static uint8_t prevState = 0;
    //if center press isn't equal to prevstate meaning it is pressed (1) or f is pressed through serial
    if (centerPress != prevState || inputChar == 'f')
    {
        prevState = centerPress;
        //if the button is pressed or f is pressed
        if (prevState == 1 || inputChar == 'f')
        {
            //if the user is able to shoot fireworks
            if (fireworkShootable)
            {
                //loop for all 20 fireworks and check if it is at -1 -1 meaning it can be drawn
                for (int i = 0; i < 20; i++)
                {
                    if (fireworks[i].x == -1 && fireworks[i].y == -1)
                    {
                        //spawn the firework at jerry's x and y
                        fireworks[i].x = jerryX;
                        fireworks[i].y = jerryY;
                        break;
                    }
                }
            }
        }
    }
}

//this function if for checking fireworks collision and receives pointers to an x and y
//value for a firework
void fireworkCollision(double *x, double *y)
{
    //clear the screen and only draw the walls
    clear_screen();
    drawWalls();

    //check is the firework is colliding with a pixel meaning it
    //has collided with a wall
    uint8_t bank = (int)*y >> 3;
    uint8_t pixel = (int)*y & 7;

    if (screen_buffer[bank * LCD_X + ((int)*x)] & (1 << pixel))
    {
        //remove the firework from the screen
        *x = -1;
        *y = -1;
    }
    //draw everything back on screen
    drawAll();
    show_screen();

    //if the pixel is within the bounds of toms position
    if (*x >= tomX)
    {
        if (*x <= tomX + 4)
        {
            if (*y >= tomY)
            {
                if (*y <= tomY + 4)
                {
                    //remove the firework and set tom back to his start position
                    *x = -1;
                    *y = -1;
                    tomX = tomStartX;
                    tomY = tomStartY;
                }
            }
        }
    }
}

//this function is for moving the fireworks position on the screen
void moveFirework(void)
{
    //firework speed
    double step = 0.7;

    //loop for all 20 fireworks
    for (int i = 0; i < 20; i++)
    {
        //if the firework is on screen
        if (fireworks[i].x != -1 && fireworks[i].y != -1)
        {
            //if the firwork x position is the same as tom don't change it
            if (fireworks[i].x == round(tomX + 2))
            {
                //if the firework y position is bigger then toms y, lower the firework y
                if (fireworks[i].y > round(tomY + 2))
                {
                    fireworks[i].y -= 1 * step;
                }
                //if not increase the y value
                else
                {
                    fireworks[i].y += 1 * step;
                }
            }
            //if the firework y is the same as toms y, don't change the y
            else if (fireworks[i].y == round(tomY + 2))
            {
                //if the fireworks x is larger then toms x, decrease the x value
                if (fireworks[i].x > round(tomX + 2))
                {
                    fireworks[i].x -= 1 * step;
                }
                //if not increase the x value
                else
                {
                    fireworks[i].x += 1 * step;
                }
            }
            else
            {
                //if the firework y valeu is larger then Toms decrease it, else increase it
                if (fireworks[i].y > round(tomY + 2))
                {
                    fireworks[i].y -= 1 * step;
                    //if the fireworks x value is bigger then tim decrease it else increase it
                    if (fireworks[i].x > round(tomX + 2))
                    {
                        fireworks[i].x -= 1 * step;
                    }
                    else
                    {
                        fireworks[i].x += 1 * step;
                    }
                }
                else
                {
                    fireworks[i].y += 1 * step;
                    //if the fireworks x value is bigger then tim decrease it else increase it
                    if (fireworks[i].x > round(tomX + 2))
                    {
                        fireworks[i].x -= 1 * step;
                    }
                    else
                    {
                        fireworks[i].x += 1 * step;
                    }
                }
            }
            //check for this fireworks collision
            fireworkCollision(&fireworks[i].x, &fireworks[i].y);
        }
    }
}

//this function is for checking if  the player is colliding with the cheese
void collisionCheese(int dirX, int dirY)
{
    bool cheeseCollided = false;
    //loop for every cheese
    for (int i = 0; i < 5; i++)
    {
        if (cheeseCollided == false)
        {
            //if the dirX is larger then -1, this determins which x row to check (only checking rows to increase efficiency)
            if (dirX > -1)
            {
                //loop for each pixle in this row
                for (int j = 0; j < 5 + superSize; j++)
                {
                    //if any of the pixels are within the bounds of a cheese
                    if (round(jerryX + dirX) >= round(cheeses[i].x))
                    {
                        if (round(jerryX + dirX) <= round(cheeses[i].x + 4))
                        {
                            if (round(jerryY + j) >= round(cheeses[i].y))
                            {
                                if (round(jerryY + j) <= round(cheeses[i].y + 4))
                                {
                                    //indicate that a cheese has been collided and break
                                    cheeseCollided = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            //if the dirY is larger then -1, this determins which Y row to check (only checking rows to increase efficiency)
            if (dirY > -1)
            {
                //this is the same as the x part but for y values
                for (int j = 0; j < 5 + superSize; j++)
                {
                    if (round(jerryX + j) >= round(cheeses[i].x))
                    {
                        if (round(jerryX + j) <= round(cheeses[i].x + 4))
                        {
                            if (round(jerryY + dirY) >= round(cheeses[i].y))
                            {
                                if (round(jerryY + dirY) <= round(cheeses[i].y + 4))
                                {
                                    cheeseCollided = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            //if a cheese has been collided
            if (cheeseCollided)
            {
                //clear the screen and draw only the cheese
                clear_screen();
                drawCheeses();
                //check is this there is a cheese directly colliding with the on pixels in Jerry's J
                if (JerryCollision(jerryX, jerryY))
                {
                    //remove the cheese and increase the score
                    cheeses[i].x = -1;
                    cheeses[i].y = -1;
                    score++;
                    cheeseConsumed++;
                    //indicate that cheese can be spawned again
                    maxCheese = false;
                    //check is score is three and if so allow the userto shoot fireworks
                    if (score == 3)
                    {
                        fireworkShootable = true;
                    }
                }
                drawAll();
                show_screen();
            }
        }
    }
}

void collisionTrap(int dirX, int dirY)
{
    bool trapCollided = false;
    //if not in super Jerry mode
    if (!superJerry)
    {
        //loop for every trap
        for (int i = 0; i < 5; i++)
        {
            if (trapCollided == false)
            {
                //if the dirX is larger then -1, this determins which x row to check (only checking rows to increase efficiency)
                if (dirX > -1)
                {
                    //loop for each pixle in this row
                    for (int j = 0; j < 5 + superSize; j++)
                    {
                        if (round(jerryX + dirX) >= round(traps[i].x))
                        {
                            if (round(jerryX + dirX) <= round(traps[i].x + 4))
                            {
                                if (round(jerryY + j) >= round(traps[i].y))
                                {
                                    if (round(jerryY + j) <= round(traps[i].y + 4))
                                    {
                                        //indicate the a trap has been collided and break out of the loop
                                        trapCollided = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                //if the dirY is larger then -1, this determins which Y row to check (only checking rows to increase efficiency)
                if (dirY > -1)
                {
                    //this is the same as x but for y
                    for (int j = 0; j < 5 + superSize; j++)
                    {
                        if (round(jerryX + j) >= round(traps[i].x))
                        {
                            if (round(jerryX + j) <= round(traps[i].x + 4))
                            {
                                if (round(jerryY + dirY) >= round(traps[i].y))
                                {
                                    if (round(jerryY + dirY) <= round(traps[i].y + 4))
                                    {
                                        trapCollided = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                //if a trap has been collided
                if (trapCollided)
                {
                    //clear everything of the screen and only draw the traps on screen
                    clear_screen();
                    drawTraps();
                    //check if the trap is directly colliding with one of Jerrys pixels
                    if (JerryCollision(jerryX, jerryY))
                    {
                        //remove the traps and remove a life
                        traps[i].x = -1;
                        traps[i].y = -1;
                        lives--;
                        //indicate more traps can be spawned
                        maxTraps = false;
                    }
                    drawAll();
                    show_screen();
                }
            }
        }
    }
}

//this function is to check is jerry and tom have collided
void collisionTomJerry(int dirX, int dirY)
{
    bool Collided = false;

    //if the dirX is larger then -1, this determins which x row to check (only checking rows to increase efficiency)
    if (dirX > -1)
    {
        //loop through every pixel within this row
        for (int j = 0; j < 5 + superSize; j++)
        {
            if (Collided == false)
            {
                //if the pixel is within toms bounds
                if (round(jerryX + dirX) >= round(tomX))
                {
                    if (round(jerryX + dirX) <= round(tomX + 4))
                    {
                        if (round(jerryY + j) >= round(tomY))
                        {
                            if (round(jerryY + j) <= round(tomY + 4))
                            {
                                //indicate that they have collided and break out of the loop
                                Collided = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    //if the dirY is larger then -1, this determins which Y row to check (only checking rows to increase efficiency)
    if (dirY > -1)
    {
        //this is the same as x but for y
        for (int j = 0; j < 5 + superSize; j++)
        {
            if (Collided == false)
            {
                if (round(jerryX + j) >= round(tomX))
                {
                    if (round(jerryX + j) <= round(tomX + 4))
                    {
                        if (round(jerryY + dirY) >= round(tomY))
                        {
                            if (round(jerryY + dirY) <= round(tomY + 4))
                            {
                                Collided = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    //if collision is true
    if (Collided)
    {
        //clear the screen and only draw tom
        clear_screen();
        drawTom();
        //if tom is directly colliding with one of Jerry's pixels
        if (JerryCollision(round(jerryX), round(jerryY)))
        {
            //if Jerry is not in super mode
            if (!superJerry)
            {
                //move them both back ot start positions and remove a life
                tomX = tomStartX;
                tomY = tomStartY;

                jerryX = jerryStartX;
                jerryY = jerryStartY;
                lives--;
            }
            //if in super Jerry mode
            else
            {
                //move tom back to the start and increase the score
                tomX = tomStartX;
                tomY = tomStartY;

                score++;
                if (score == 3)
                {
                    fireworkShootable = true;
                }
            }
        }
        drawAll();
        show_screen();
    }
}

//this function checks to see if the player has collided with the door
void collisionDoor(int dirX, int dirY)
{
    bool Collided = false;

    //if the dirX is larger then -1, this determins which x row to check (only checking rows to increase efficiency)
    if (dirX > -1)
    {
        //loop for every pixel in this row
        for (int j = 0; j < 5 + superSize; j++)
        {
            if (Collided == false)
            {
                //if this pixel is within the bounds of the door
                if (round(jerryX + dirX) >= round(exitDoor.x))
                {
                    if (round(jerryX + dirX) <= round(exitDoor.x + 4))
                    {
                        if (round(jerryY + j) >= round(exitDoor.y))
                        {
                            if (round(jerryY + j) <= round(exitDoor.y + 4))
                            {
                                //indicate there was a collision and break out of the loop
                                Collided = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    //if the dirY is larger then -1, this determins which y row to check (only checking rows to increase efficiency)
    if (dirY > -1)
    {
        //this is the same as x but for y
        for (int j = 0; j < 5 + superSize; j++)
        {
            if (Collided == false)
            {
                if (round(jerryX + j) >= round(exitDoor.x))
                {
                    if (round(jerryX + j) <= round(exitDoor.x + 4))
                    {
                        if (round(jerryY + dirY) >= round(exitDoor.y))
                        {
                            if (round(jerryY + dirY) <= round(exitDoor.y + 4))
                            {
                                Collided = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    //if these was a collision
    if (Collided)
    {
        //clear the screen except for the door
        clear_screen();
        drawDoor();
        //if the door is directly colliding with one of Jerry's pixels
        if (JerryCollision(jerryX, jerryY))
        {
            //if in level 1, start level 2, else end the game
            if (level == 1)
            {
                setup();
                level = 2;
            }
            else
            {
                gameOver = true;
            }
        }
        drawAll();
        show_screen();
    }
}

//this function checks is the player is colliding with the potion
void collisionPotion(int dirX, int dirY)
{
    bool Collided = false;
    //if the dirX is larger then -1, this determins which x row to check (only checking rows to increase efficiency)
    if (dirX > -1)
    {
        //if these is a potion on screen
        if (jerryPotion.x != -1 && jerryPotion.y != -1)
        {
            //loop through ever pixel in this row
            for (int j = 0; j < 5 + superSize; j++)
            {
                if (Collided == false)
                {
                    //if the pixel is within the bounds of the potion
                    if (round(jerryX + dirX) >= round(jerryPotion.x))
                    {
                        if (round(jerryX + dirX) <= round(jerryPotion.x + 4))
                        {
                            if (round(jerryY + j) >= round(jerryPotion.y))
                            {
                                if (round(jerryY + j) <= round(jerryPotion.y + 4))
                                {
                                    //indicate there was a collision and break out of the loop
                                    Collided = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    //if the dirY is larger then -1, this determins which y row to check (only checking rows to increase efficiency)
    if (dirY > -1)
    {
        //this is the same as x but for y
        if (jerryPotion.x != -1 && jerryPotion.y != -1)
        {
            for (int j = 0; j < 5 + superSize; j++)
            {
                if (Collided == false)
                {
                    if (round(jerryX + j) >= round(jerryPotion.x))
                    {
                        if (round(jerryX + j) <= round(jerryPotion.x + 4))
                        {
                            if (round(jerryY + dirY) >= round(jerryPotion.y))
                            {
                                if (round(jerryY + dirY) <= round(jerryPotion.y + 4))
                                {
                                    Collided = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    //if there was a collision
    if (Collided)
    {
        //clear everything but the potion off the screen
        clear_screen();
        drawPotion();
        //if the potion is directlt colliding with one of Jerry's pixel
        if (JerryCollision(jerryX, jerryY))
        {
            //if Jerry is already in super mode just reset his super time
            if (superJerry)
            {
                superOverFlow = 0;
            }
            //set Jerry to Super mode and increase his size by 1
            superJerry = true;
            superSize = 1;
            //clear the potion off screen
            clearPotion();
        }
        drawAll();
        show_screen();
    }
}

//this function is for timing how long the player has been in super mode
void superTimer()
{
    //calculate the super timer
    int superTime = (superOverFlow * 65536.0 + TCNT1) * PRESCALE / FREQUENCY;

    //if in super mode
    if (superJerry)
    {
        //if superTime if larger then or equal to 10
        if (superTime >= 10)
        {
            //exit super mode and set the timer to 0
            superJerry = false;
            superOverFlow = 0;
            superSize = 0;
        }
    }
}

//this function is for allowing the player to change level with the left button
void changeLevel()
{
    //if the left button is pressed or l is pressed through serial
    static uint8_t prevState = 0;
    if (leftBPress != prevState || inputChar == 'l')
    {
        prevState = leftBPress;
        if (prevState == 1 || inputChar == 'l')
        {
            //if a door has been spawned (level can be changed)
            if (doorSpawned)
            {
                //go to next level in level 1 or end game if in level 2
                if (level == 1)
                {
                    setup();
                    level = 2;
                }
                else
                {
                    gameOver = true;
                }
            }
        }
    }
}

//this function is for pausing the game
void pauseGame()
{
    //if not in start screen
    if (!startScreen)
    {
        //if right button is pressed or p is pressed through serial
        static uint8_t prevState = 0;
        if (rightBPress != prevState || inputChar == 'p')
        {
            prevState = rightBPress;
            if (prevState == 1 || inputChar == 'p')
            {
                //if not paused, pause the game
                if (!pause)
                {
                    pause = true;
                }
                //if paused, unpause the game
                else
                {
                    pause = false;
                }
            }
        }
    }
}

//this function checks if the user is out of lives
void outOFLives()
{
    if (lives == 0)
    {
        gameOver = true;
    }
}

//this fucntion gets the users input through serial
void getKeyboardInput()
{
    //if in level 2
    if (level == 2)
    {
        //save the users input to inputChar
        inputChar = usb_serial_getchar();
    }
}

//game setup, that calls the other set up functions
void setup()
{
    set_clock_speed(CPU_8MHz);
    lcd_init(LCD_DEFAULT_CONTRAST);
    lcd_clear();
    adc_init();
    setWall();
    clearCheese();
    clearTraps();
    clearFireworks();
    clearDoor();
    clearPotion();
    setupVars();
    setupTimer();
    setupInput();
    setup_timer1();
    setupTom();
    sei();
}

//this is the function for moving the walls on screen
void wallMove()
{
    //calculate the wall time
    double wallTime = (wallOverflow * 65536.0 + TCNT1) * PRESCALE / FREQUENCY;

    //gets the right potentiometer value and sets time to 0
    int rightADC = adc_read(1);
    double time = 0;

    bool reverse = false;

    //if the rightADC is in the top half
    if (rightADC > 512)
    {
        //calculate the timing for wall movement (1 second being the fastest)
        time = 512 / ((double)rightADC - 512);
    }
    //if the rightADC is the lower half
    if (rightADC <= 512)
    {
        //calculate the timing for wall movement (1 second being the fastest)
        time = (double)rightADC / (1024 - rightADC);
        time = time + 1;
        //indicate the wall movement is reversed
        reverse = true;
    }

    //if the calulate wallTime is higher then or equal to the time specified by the rightADC
    if (wallTime >= time)
    {
        //set WallTime back to 0
        wallOverflow = 0;
        //loop for every wall
        for (int i = 0; i < 4; i++)
        {
            //if the y values are equal
            if (wallY1[i] == wallY2[i])
            {
                //is not reverse
                if (!reverse)
                {
                    //move the wall up one pixel
                    wallY1[i]--;
                    wallY2[i]--;
                    //if the wall is above the status bar wrap is around the screen
                    if (wallY1[i] <= STATUS_BAR_HEIGHT)
                    {
                        wallY1[i] = HEIGHT - 1;
                        wallY2[i] = HEIGHT - 1;
                    }
                }
                //if reverse
                else
                {
                    //move the wall down one pixel
                    wallY1[i]++;
                    wallY2[i]++;
                    //if the wall moves out of the screen wrap it around
                    if (wallY1[i] >= HEIGHT)
                    {
                        wallY1[i] = STATUS_BAR_HEIGHT + 1;
                        wallY2[i] = STATUS_BAR_HEIGHT + 1;
                    }
                }
            }
            //the wall x values are the same
            if (wallX1[i] == wallX2[i])
            {
                //if not reverse
                if (!reverse)
                {
                    //move the wall right one pixel
                    wallX1[i]++;
                    wallX2[i]++;
                    //if the wall exits the screen wrap it to the other side
                    if (wallX1[i] >= WIDTH)
                    {
                        wallX1[i] = 0;
                        wallX2[i] = 0;
                    }
                }
                //if reverse
                else
                {
                    //move the wall one pixel left
                    wallX1[i]--;
                    wallX2[i]--;
                    //if outside screen wrap to the other side
                    if (wallX1[i] < 0)
                    {
                        wallX1[i] = WIDTH - 1;
                        wallX2[i] = WIDTH - 1;
                    }
                }
            }
            //if x1 is larger then x2
            if (wallX1[i] > wallX2[i])
            {
                //calculate the x and y difference
                int yDif = wallY2[i] - wallY1[i];
                int xDif = wallX1[i] - wallX2[i];

                //if not reverse
                if (!reverse)
                {
                    //move diagonaly down right
                    wallX1[i]++;
                    wallY1[i]++;
                    wallX2[i]++;
                    wallY2[i]++;
                    //if the lowest part of the wall exits the screen wrap the wall to the other side
                    if (wallY2[i] > HEIGHT - 1)
                    {
                        wallX1[i] = xDif;
                        wallY1[i] = STATUS_BAR_HEIGHT + 1;
                        wallY2[i] = STATUS_BAR_HEIGHT + 1 + yDif;
                        wallX2[i] = 0;
                    }
                }
                //if reverse
                else
                {
                    //move the wall diagonally left up
                    wallX1[i]--;
                    wallY1[i]--;
                    wallX2[i]--;
                    wallY2[i]--;
                    //if the heightest part of the wall exits the screen wrap it to the other side of the screen
                    if (wallY1[i] <= STATUS_BAR_HEIGHT)
                    {
                        wallX1[i] = WIDTH - 1;
                        wallY1[i] = HEIGHT - 1 - yDif;
                        wallY2[i] = HEIGHT - 1;
                        wallX2[i] = WIDTH - 1 - xDif;
                    }
                }
            }
            //if x1 if less then x2 and y2 is larger then y1
            if (wallX1[i] < wallX2[i] && wallY2[i] > wallY1[i])
            {
                //calculate the difference between the x and y values
                int yDif = wallY2[i] - wallY1[i];
                int xDif = wallX2[i] - wallX1[i];

                //if not reverse
                if (!reverse)
                {
                    //move the wall diagonally left down
                    wallX1[i]--;
                    wallY1[i]++;
                    wallX2[i]--;
                    wallY2[i]++;
                    //if the lowest part of the wall exits the screen, wrap the wall around to the other side of the screen
                    if (wallY2[i] > HEIGHT - 1)
                    {
                        wallX1[i] = WIDTH - 1 - xDif;
                        wallY1[i] = STATUS_BAR_HEIGHT + 1;
                        wallY2[i] = STATUS_BAR_HEIGHT + 1 + yDif;
                        wallX2[i] = WIDTH - 1;
                    }
                }
                //if reverse
                else
                {
                    //move the wall diagonally right up
                    wallX1[i]++;
                    wallY1[i]--;
                    wallX2[i]++;
                    wallY2[i]--;
                    //if either the highest part of the wall or the furthest right part of the wall exits the screen,
                    //wrap the wall around to the other side of the screen
                    if (wallY1[i] <= STATUS_BAR_HEIGHT || wallX2[i] >= WIDTH)
                    {
                        wallX1[i] = 0;
                        wallY1[i] = HEIGHT - 1 - yDif;
                        wallY2[i] = HEIGHT - 1;
                        wallX2[i] = xDif;
                    }
                }
            }
        }
    }
}

//this function checks if the wall has moved into Jerry
void wallTrapedJerry()
{
    //if not superJerry
    if (!superJerry)
    {
        //check if any of Jerry's pixel is colliding with another pixel (the wall)
        if (JerryCollision(jerryX, jerryY))
        {
            //move Jery back to the start
            jerryX = jerryStartX;
            jerryY = jerryStartY;
        }
    }
}

//this function checks if the wall has moved into Tom
void wallTrapedTom()
{
    //check is any of Toms pixels is collideing with a pixel (the wall)
    if (TomCollision(round(tomX), round(tomY)))
    {
        //move tom back to the start
        tomX = tomStartX;
        tomY = tomStartY;
    }
}

//this function is for moving Tom
void moveTom()
{
    //calculates Toms speed multipler from the left potentiometer
    int leftADC = adc_read(0);
    double Tspeed = (double)leftADC / 1023;

    //if the game isn't paused
    if (!pause)
    {
        //calculate Tom's new x and y position
        int newX = round(tomX + (tomDX * Tspeed));
        int newY = round(tomY + (tomDY * Tspeed));

        //flag for whether Tom has bounced
        bool bounced = false;

        //if tom is touching the left or right side of the screen
        if (newX == 0 || newX == 80)
        {
            //recalculate Toms speed and directions and indicate he has bounced
            setupTom();

            bounced = true;
        }

        //if tom try to move out the bottom of the screen or into the status bar
        if (newY == STATUS_BAR_HEIGHT + 1 || newY == 44)
        {
            //recalculate Toms speed and directions and indicate he has bounced
            setupTom();

            bounced = true;
        }

        //if one of toms pixels has collided with a wall
        if (TomCollision(round(newX), round(newY)))
        {
            //recalculate Toms speed and directions and indicate he has bounced
            setupTom();

            bounced = true;
        }

        //if tom didn't bounce
        if (!bounced)
        {
            //move tom to the new position
            tomX += (tomDX * Tspeed);
            tomY += (tomDY * Tspeed);
            //check is Tom Collided with Jerry
            collisionTomJerry(0, 0);
            collisionTomJerry(4, 4);

            seed++;
        }
    }
}

//this function is for check if Jerry has collided with anything
//it takes his x and y position as well has his x and y direction
void jerryCollision(double newX, double newY, int dirX, int dirY)
{
    //if Jerry has collided with a wall or Jerry is in super mode
    if (!JerryCollision(newX, newY) || superJerry)
    {
        //move Jerry
        jerryX = newX;
        jerryY = newY;

        //check is Jerry has collided with any of the game elements
        collisionCheese(dirX, dirY);
        collisionTrap(dirX, dirY);
        collisionTomJerry(dirX, dirY);
        if (doorSpawned)
        {
            collisionDoor(dirX, dirY);
        }
        if (level == 2)
        {
            collisionPotion(dirX, dirY);
        }
    }
}

//this function is for move Jerry
void moveJerry()
{
    static uint8_t prevState = 0;
    double newX = jerryX;
    double newY = jerryY;

    //get the value from the left potentiometer and calculate the speed multipler with it
    int leftADC = adc_read(0);
    double Jspeed = (double)leftADC / 1023;

    //if up is pressed or w is sent through serial and the Y value is larger then the status back height
    if ((upPress != prevState || inputChar == 'w') && jerryY > STATUS_BAR_HEIGHT + 1)
    {
        prevState = upPress;
        if (prevState == 1 || inputChar == 'w')
        {
            //calculate the new newY
            newY -= 1 * Jspeed;
            //change the seed for random calculations
            seed--;
            //check Jerry for any collisions
            jerryCollision(newX, newY, -1, 0);
        }
    }

    //if left is pressed or a is sent through serial and the x value is bigger then 0
    else if ((leftPress != prevState || inputChar == 'a') && jerryX > 0)
    {
        prevState = leftPress;
        if (prevState == 1 || inputChar == 'a')
        {
            //calculate the new newX
            newX -= 1 * Jspeed;
            //change the seed for random calculations
            seed--;
            //check Jerry for any collisions
            jerryCollision(newX, newY, 0, -1);
        }
    }

    //if down is pressed or s is sent through serial and the Y value is less then the bottom of then screen
    else if ((downPress != prevState || inputChar == 's') && jerryY < 48 - 5 - superSize)
    {
        prevState = downPress;
        if (prevState == 1 || inputChar == 's')
        {
            //calculate the new newY
            newY += 1 * Jspeed;
            //change the seed for random calculations
            seed++;
            //check Jerry for any collisions
            jerryCollision(newX, newY, -1, 4 + superSize);
        }
    }

    //if right is pressed or d is sent through serial and the x value is less then left of screen minus the size of Jerry
    else if ((rightPress != prevState || inputChar == 'd') && jerryX < 84 - CHAR_WIDTH - superSize)
    {
        prevState = rightPress;
        if (prevState == 1 || inputChar == 'd')
        {
            //calculate the new newX
            newX += 1 * Jspeed;
            //change the seed for random calculations
            seed++;
            //check Jerry for any collisions
            jerryCollision(newX, newY, 4 + superSize, -1);
        }
    }
}

//this function is for drawing everything on the screen
void drawAll(void)
{
    clear_screen();
    wallMove();
    drawWalls();
    wallTrapedJerry();
    wallTrapedTom();
    moveTom();
    moveJerry();
    drawCheeses();
    drawTraps();
    drawJerry();
    drawTom();
    drawFireworks();
    drawDoor();
    drawPotion();
    drawStatus();
    show_screen();
}

void usb_serial_send(char *message)
{
    // Cast to avoid "error: pointer targets in passing argument 1
    //	of 'usb_serial_write' differ in signedness"
    usb_serial_write((uint8_t *)message, strlen(message));
}

//this function is for send the user game info over serial
void printInfo()
{
    //if in level2
    if (level == 2)
    {
        //if the user presses i in serial
        if (inputChar == 'i')
        {
            //send game info to the user
            snprintf(buffer, sizeof(buffer), "Time is '%d' minutes '%d' secs\r\n", minutes, seconds);
            usb_serial_send(buffer);
            snprintf(buffer, sizeof(buffer), "In level '%d'\r\n", level);
            usb_serial_send(buffer);
            snprintf(buffer, sizeof(buffer), "Jerry has '%d' lives\r\n", lives);
            usb_serial_send(buffer);
            snprintf(buffer, sizeof(buffer), "The score is '%d'\r\n", score);
            usb_serial_send(buffer);
            int fireworkCount = 0;
            for (int i = 0; i < 20; i++)
            {
                if (fireworks[i].x != -1 && fireworks[i].y != -1)
                {
                    fireworkCount++;
                }
            }
            snprintf(buffer, sizeof(buffer), "There are '%d' fireworks on the screen\r\n", fireworkCount);
            usb_serial_send(buffer);
            int trapsCount = 0;
            for (int i = 0; i < 5; i++)
            {
                if (traps[i].x != -1 && traps[i].y != -1)
                {
                    trapsCount++;
                }
            }
            snprintf(buffer, sizeof(buffer), "There are '%d' traps on the screen\r\n", trapsCount);
            usb_serial_send(buffer);
            int cheeseCount = 0;
            for (int i = 0; i < 5; i++)
            {
                if (cheeses[i].x != -1 && cheeses[i].y != -1)
                {
                    cheeseCount++;
                }
            }
            snprintf(buffer, sizeof(buffer), "There are '%d' cheeses on the screen\r\n", cheeseCount);
            usb_serial_send(buffer);
            snprintf(buffer, sizeof(buffer), "'%d' amount of cheese has been consumed\r\n", cheeseConsumed);
            usb_serial_send(buffer);
            if (superJerry)
            {
                snprintf(buffer, sizeof(buffer), "Jerry Super Mode: True\r\n");
                usb_serial_send(buffer);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), "Jerry Super Mode: False\r\n");
                usb_serial_send(buffer);
            }
            if (pause)
            {
                snprintf(buffer, sizeof(buffer), "Game Paused: True\r\n");
                usb_serial_send(buffer);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), "Game Paused: False\r\n");
                usb_serial_send(buffer);
            }
        }
    }
}

//function for displaying the start screen
void StartScreen()
{
    startScreen = true;

    //if start screen is true
    while (startScreen)
    {
        //if the users presses the right button
        static uint8_t prevState = 0;
        if (rightBPress != prevState)
        {
            prevState = rightBPress;
            if (prevState == 1)
            {
                //exit start screen
                startScreen = false;
            }
        }
        //print the start screen to the screen
        clear_screen();
        draw_string(15, 10, "Rhys De Pater", FG_COLOUR);
        draw_string(15, 20, "n10478515", FG_COLOUR);
        draw_string(15, 30, "Escape Tom", FG_COLOUR);
        show_screen();
    }
    //add a delay so that the game doesn't auto pause
    _delay_ms(300);
}

//this function is for display game over
void GameOver()
{
    //while gameOver is true
    while (gameOver)
    {
        //if the user presses the right button
        static uint8_t prevState = 0;
        if (rightBPress != prevState)
        {
            prevState = rightBPress;
            if (prevState == 1)
            {
                //restart the game
                setup();
                StartScreen();
            }
        }
        //display gameover
        clear_screen();
        draw_string(20, 20, "Game Over!", FG_COLOUR);
        show_screen();

        //add a delay so that the game doesn't auto pause
        _delay_ms(100);
    }
}

void update(void)
{
    getKeyboardInput();
    pauseGame();
    spawnCheese();
    spawnTrap();
    spawnDoor();
    spawnFirework();
    spawnPotion();
    moveFirework();
    superTimer();
    changeLevel();
    outOFLives();
    setupSerial();
    printInfo();
    brightLoop();
    GameOver();
    srand(seed);
}

void process()
{
    drawAll();
    update();
    timer();
}

int main(void)
{
    setup();

    StartScreen();

    for (;;)
    {
        process();
        _delay_ms(30);
    }
}

// -------------------------------------------------
// Helper functions.
// -------------------------------------------------

void draw_double(uint8_t x, uint8_t y, double value, colour_t colour)
{
    snprintf(buffer, sizeof(buffer), "%f", value);
    draw_string(x, y, buffer, colour);
}
void draw_time(uint8_t x, uint8_t y, int value, colour_t colour)
{
    snprintf(buffer, sizeof(buffer), "%02d", value);
    draw_string(x, y, buffer, colour);
}
void draw_int(uint8_t x, uint8_t y, int value, colour_t colour)
{
    snprintf(buffer, sizeof(buffer), "%d", value);
    draw_string(x, y, buffer, colour);
}
