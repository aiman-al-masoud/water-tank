
/* ports */
int WRITE_INNER_PUMP = 7;
int WRITE_OUTER_PUMP = 2;
int READ_ECHO = 12;
int WRITE_TRIGGER = 13;

/* variables */
float THRESH = 1;    // cm
float TANK_HEIGHT = 10; // cm
float setPoint = 0;    // cm

void setup()
{

     /* turn pumps off */
    digitalWrite(WRITE_INNER_PUMP, LOW);
    digitalWrite(WRITE_OUTER_PUMP, LOW);

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
        auto x = Serial.readString().toFloat();
        setPoint = x;
    }

    /* ****************************** */

    /* read current water level */
    auto currLevel = averageReadWaterLevel();

    /* ****************************** */

    /* recompute error and control pumps */
    auto error = currLevel - setPoint;
    Serial.println(error);

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

    delay(1000);
}

float readWaterLevel()
{
    digitalWrite(WRITE_TRIGGER, LOW);
    delayMicroseconds(2);
    digitalWrite(WRITE_TRIGGER, HIGH);
    delayMicroseconds(10);
    digitalWrite(WRITE_TRIGGER, LOW);
    auto duration = pulseIn(READ_ECHO, HIGH);
    auto distance = duration / 58.2;
    auto currLevel = TANK_HEIGHT - distance;
    return currLevel;
}

float averageReadWaterLevel()
{
    int cycle = 1000;
    int numberReadings = 20;
    float sum = 0;
    int delay =  50;

    for (int i = 0; i < numberReadings; i++)
    {
        sum += readWaterLevel();
        delay(delay);
    }

    return sum / numberReadings;
}