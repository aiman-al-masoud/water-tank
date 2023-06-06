#include <SoftwareSerial.h>

/* ports */
int WRITE_INNER_PUMP;
int WRITE_OUTER_PUMP;
int READ_ECHO = 7;
int WRITE_TRIGGER = 8;
int RX = 2;
int TX = 3;

/* variables */
float THRESH = 0.5;
float TANK_HEIGHT;
float currLevel;
float setPoint;
float error;
long duration;
long distance;
SoftwareSerial HM10(2, 3);

void setup()
{
    Serial.begin(9600);
    HM10.begin(9600);
    pinMode(WRITE_INNER_PUMP, OUTPUT);
    pinMode(WRITE_OUTER_PUMP, OUTPUT);
    pinMode(READ_ECHO, INPUT);
    pinMode(WRITE_TRIGGER, OUTPUT);
}

void loop()
{

    /* read set point from bluetooth */

    /* ****************************** */

    /* read current water level */

    digitalWrite(WRITE_TRIGGER, LOW);
    delayMicroseconds(2);
    digitalWrite(WRITE_TRIGGER, HIGH);
    delayMicroseconds(10);
    digitalWrite(WRITE_TRIGGER, LOW);
    duration = pulseIn(READ_ECHO, HIGH);
    distance = duration / 58.2;
    currLevel = TANK_HEIGHT - distance;

    /* ****************************** */

    /* recompute error and control pumps */
    error = currLevel - setPoint;

    if (abs(error) < THRESH)
    {
        digitalWrite(WRITE_INNER_PUMP, LOW);
        digitalWrite(WRITE_OUTER_PUMP, LOW);
    }
    else if (error > 0)
    {
        digitalWrite(WRITE_INNER_PUMP, HIGH);
        digitalWrite(WRITE_OUTER_PUMP, LOW);
    }
    else
    {
        digitalWrite(WRITE_INNER_PUMP, LOW);
        digitalWrite(WRITE_OUTER_PUMP, HIGH);
    }
    /* ****************************** */

    delay(50);
}