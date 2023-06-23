
/* ports */
int WRITE_INNER_PUMP = 7;
int WRITE_OUTER_PUMP = 2;
int READ_ECHO = 12;
int WRITE_TRIGGER = 13;

/* variables */
float THRESH = 0.5;
float TANK_HEIGHT = 6; // cm
float currLevel;
float setPoint = 0; // cm
float error;
long duration;
long distance;

void setup()
{
    Serial.begin(9600);
    pinMode(WRITE_INNER_PUMP, OUTPUT);
    pinMode(WRITE_OUTER_PUMP, OUTPUT);
    pinMode(READ_ECHO, INPUT);
    pinMode(WRITE_TRIGGER, OUTPUT);

    /* turn pumps off */
    digitalWrite(WRITE_INNER_PUMP, LOW);
    digitalWrite(WRITE_OUTER_PUMP, LOW);
}

void loop()
{

    /* read set point from bluetooth */

    if (Serial.available())
    {
        delay(10);
        const auto x = Serial.readString();
        Serial.println(x);
    }

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
        // Serial.println("all pumps off!");
        // digitalWrite(WRITE_INNER_PUMP, LOW);
        // digitalWrite(WRITE_OUTER_PUMP, LOW);
    }
    else if (error > 0)
    {
        // Serial.println("remove water!");
        // digitalWrite(WRITE_INNER_PUMP, HIGH);
        // digitalWrite(WRITE_OUTER_PUMP, LOW);
    }
    else
    {
        // Serial.println("add water!");
        // digitalWrite(WRITE_INNER_PUMP, LOW);
        // digitalWrite(WRITE_OUTER_PUMP, HIGH);
    }
    /* ****************************** */

    delay(50);
}
