#include <Servo.h>  //this enables the library for the servo motor
#define IN0 2       /* D2 */
#define IN1 3       /* D3 */
#define IN2 4       /* D4 */
#define IN3 5       /* D5 */

#define OUT0 6 /* D6 */
#define OUT1 7 /* D7 */
#define OUT2 8 /* D8 */
#define OUT3 9 /* D9 */

#undef F
#define F(x) x

int grip;
int drop;
int16_t position = 149;
static const uint8_t g_Major = 0;
static const uint8_t g_Minor = 3;
static const char g_info[] = F("info");
static const char g_goto[] = F("goto");
static const char g_ADC7[] = F("ADC7");
static const char g_low[] = F("low");
static const char g_high[] = F("high");
static const char g_cursor[] = F(">>");

static const uint32_t flashInterval = 500; /* milliseconds */

struct pin_definition {
  const uint8_t pin;
  const char* name;
};

struct analog_pin_definition {
  const uint8_t pin;
  const char* name;
  uint16_t period;
  unsigned long lastPublish;
};

static Servo g_servoMotor;

struct pin_definition input_pins[] = { { IN0, F("IN0") }, { IN1, F("IN1") }, { IN2, F("IN2") }, { IN3, F("IN3") } };
struct pin_definition output_pins[] = { { OUT0, F("OUT0") }, { OUT1, F("OUT1") }, { OUT2, F("OUT2") }, { OUT3, F("OUT3") } };
struct analog_pin_definition analog_pins[] = { { 7, F("ADC7"), 0, 0 } };

void Help() {
  Serial.print(F("Levelshifter board "));
  Serial.print(g_Major);
  Serial.print(F("."));
  Serial.print(g_Minor);
  Serial.println(F("---------------------------------------"));
  Serial.println(F("'?' this screen"));
  Serial.println(F("'info' display the current settings of in and out"));
}

void Info() {

  // First time initialization, we report the current state of the output pins
  for (uint8_t index = 0; index < sizeof(output_pins) / sizeof(struct pin_definition); index++) {
    Serial.print(F("OUTPUT: "));
    Serial.print(output_pins[index].name);
    Serial.print(F("="));
    Serial.println(digitalRead(output_pins[index].pin) ? g_high : g_low);
  }

  // First time initialization, we report and record the current state for the input pins
  for (uint8_t index = 0; index < sizeof(input_pins) / sizeof(struct pin_definition); index++) {
    Serial.print("INPUT:  ");
    Serial.print(input_pins[index].name);
    Serial.print('=');

    Serial.println(digitalRead(input_pins[index].pin) == 0 ? g_low : g_high);
  }
}

void Goto(int16_t position) {
  Serial.print("Moving to: ");
  Serial.println(position);
  g_servoMotor.write(position);
}

uint16_t UnderPressure(const uint8_t pin) {
  uint16_t pressure = analogRead(pin);
  Serial.println(pressure);
  return pressure;
}

void UserEvaluation(const uint8_t length, const char buffer[]) {

  if ((length == 1) && (buffer[0] == '?')) {
    Help();
  } else if (strncmp(g_info, buffer, (sizeof(g_info) / sizeof(char) - 1)) == 0) {
    Info();
  } else if (strncmp(g_goto, buffer, (sizeof(g_goto) / sizeof(char) - 1)) == 0) {
    if ((buffer[sizeof(g_goto) / sizeof(char) - 1] == '=') && (length >= (sizeof(g_goto) / sizeof(char) + 1))) {
      int degrees = atoi(&(buffer[sizeof(g_goto) / sizeof(char)]));
      Goto(degrees);
    }
  } else {
    bool handled = false;

    for (uint8_t index = 0; ((handled == false) && (index < sizeof(output_pins) / sizeof(struct pin_definition))); index++) {
      const uint8_t keyLength = strlen(output_pins[index].name);
      if (strncmp(output_pins[index].name, buffer, keyLength) == 0) {
        handled = true;
        if ((buffer[keyLength] != '=') || (length != (keyLength + 2))) {
          Serial.println(F("Syntax should be '<pin name>=0' or '<pin name>=1'"));
        } else {
          const char value = toupper(buffer[keyLength + 1]);
          if ((value == 'L') || (value == '0')) {
            digitalWrite(output_pins[index].pin, 0);
          } else if ((value == 'H') || (value == '1')) {
            digitalWrite(output_pins[index].pin, 1);
          } else {
            Serial.println(F("Unrecognized value, must be '0', '1', 'l', 'h', 'L' or 'H'"));
          }
        }
      }
    }

    for (uint8_t index = 0; ((handled == false) && (index < sizeof(analog_pins) / sizeof(struct analog_pin_definition))); index++) {
      const uint8_t keyLength = strlen(analog_pins[index].name);
      if (strncmp(analog_pins[index].name, buffer, keyLength) == 0) {
        handled = true;
        if ((buffer[keyLength] != '=') || (length != (keyLength + 2))) {
          Serial.println(F("Syntax should be '<pin name>=<duration in 100ms>'"));
        } else {
          const uint16_t value = atoi(&(buffer[keyLength + 1]));
          analog_pins[index].period = value * 100;
          analog_pins[index].lastPublish = 0;
          if (value == 0) {
            Serial.print(F("Disabled the monitoring of: "));
            Serial.println(analog_pins[index].name);
          } else {
            Serial.print(F("Enabled monitoring of ["));
            Serial.print(analog_pins[index].name);
            Serial.print(F("] to a value of: "));
            Serial.print(analog_pins[index].period);
            Serial.println(F(" ms"));
          }
        }
      }
    }

    if (handled == false) {
      Serial.print(F("Unrecognized verb: "));
      Serial.println(buffer);
    }
  }
  Serial.print(F(">>"));
}

void InputEvaluation(uint32_t& state) {
  // First time initialization, we report and record the current state
  for (uint8_t index = 0; index < sizeof(input_pins) / sizeof(struct pin_definition); index++) {
    bool value = !(digitalRead(input_pins[index].pin) == 0);

    // Check if we have a different state than we recorded..
    if (value ^ ((state & (1 << index)) != 0)) {
      // Yes, so time to notify
      Serial.println();
      Serial.print(F("INPUT:  "));
      Serial.print(input_pins[index].name);
      Serial.print(F("="));
      Serial.println(value ? g_high : g_low);
      Serial.print(F(">>"));

      // and record!!
      state ^= (1 << index);
    }
  }
}

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
static unsigned long g_previousFlashMillis = 0;

// This is the buffer to remember what the user is entering through the
// serial console
static char g_buffer[16];
static uint8_t g_length;

static uint32_t g_lastState = 0;

void setup() {
  Serial.begin(115200);
  Serial.flush();
  Goto(position);
  // grip digital pin LED_BUILTIN as an output, just for DEBUG purpose.
  pinMode(LED_BUILTIN, OUTPUT);

  // First time initialization, we report the current state of the output pins
  for (uint8_t index = 0; index < sizeof(output_pins) / sizeof(struct pin_definition); index++) {
    pinMode(output_pins[index].pin, OUTPUT);
  }

  // First time initialization, we report and record the current state for the input pins
  for (uint8_t index = 0; index < sizeof(input_pins) / sizeof(struct pin_definition); index++) {
    pinMode(input_pins[index].pin, INPUT);
    if (digitalRead(input_pins[index].pin) != 0) {
      g_lastState |= (1 << index);
    }
  }

  Help();
  Info();
  Serial.print(g_cursor);

  g_servoMotor.attach(11);
  delay(3000);
}

void loop() {
  unsigned long currentMillis = millis();
  Serial.println(grip);
  uint16_t pressure;

  grip = digitalRead(IN0);
  if (grip == 1) {
      while (position > 60) {
            pressure = UnderPressure(7);
        if (pressure < 300) {
          position--;
          delay(50);
          Goto(position); //Try to grab the object untill the pressure is enough
        } else if ((position == 60) && (pressure < 300)){
          digitalWrite(OUT3, HIGH);
          delay(100);
          digitalWrite(OUT3, LOW); //If nothing is grabbed, send a signal to the robot
        }
      }
    }
  
  if (pressure>300){
    digitalWrite(OUT0,HIGH);
    grip = 0;
    delay(100);
    digitalWrite(OUT0, LOW);
  }

   drop = digitalRead(IN2);
  if (drop == 1){
    position = 150;
    Goto(position);
    digitalWrite(OUT2, HIGH);
    drop = 0;
    delay(1000);
    digitalWrite(OUT2, LOW);
  }

    // check to see if it's time to blink the LED; that is, if the difference
    // between the current time and last time you blinked the LED is bigger than
    // the interval at which you want to blink the LED.
    if (static_cast<uint32_t>(currentMillis - g_previousFlashMillis) >= flashInterval) {
      // save the last time you blinked the LED
      g_previousFlashMillis = currentMillis;

      // set the LED with the ledState of the variable:
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }

    while (Serial.available() > 0) {
      char character = Serial.read();
      if (::isprint(character)) {
        if (g_length < sizeof(g_buffer)) {
          g_buffer[g_length++] = character;
          Serial.print(character);
        } else {
          Serial.print(F("*"));
        }
      } else {
        g_buffer[g_length] = '\0';
        Serial.println();
        UserEvaluation(g_length, g_buffer);
        g_length = 0;
        Serial.print(g_cursor);
      }
    }

    InputEvaluation(g_lastState);

    for (uint8_t index = 0; (index < sizeof(analog_pins) / sizeof(struct analog_pin_definition)); index++) {
      if ((analog_pins[index].period != 0) && (analog_pins[index].lastPublish < currentMillis)) {
        analog_pins[index].lastPublish = currentMillis + analog_pins[index].period;
        UnderPressure(analog_pins[index].pin);
      }
    }
  }
