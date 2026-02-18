// VEX 276-2151 PPM Receiver Reader for arduino
//
// PPM format:
//   - Sync: ~9ms LOW pulse
//   - Each channel: LOW pulse (840-1680us) + HIGH framing pulse (~500us)
//   - Midpoint/neutral: ~1250us
//   - 6 channels total (4 analog sticks + 2 button channels)
//   - Signal idles HIGH (open collector, pull-up via Arduino INPUT_PULLUP)
//
// Button channels (5 & 6):
//   - Short pulse (~840us) = top button pressed
//   - Long pulse (~1680us) = bottom button pressed
//   - Neither/both pressed = both OFF

const int PPM_PIN = 2;
const int NUM_CHANNELS = 6;
const unsigned long SYNC_THRESHOLD = 5000;  // >5ms LOW = sync pulse
const unsigned long SIGNAL_TIMEOUT = 100000; // 100ms without a frame = signal lost

// Receiver range
const int PPM_MIN = 840;
const int PPM_MID = 1250;
const int PPM_MAX = 1680;

// Deadzone for stick channels (mapped -100..+100)
const int DEADZONE = 8;

// Button thresholds
const int BUTTON_LOW_THRESHOLD  = 1050;  // Below this = top button pressed
const int BUTTON_HIGH_THRESHOLD = 1580;  // Above this = bottom button pressed

// Button state struct
struct ButtonPair {
  bool top;
  bool bottom;
};

ButtonPair buttons[2]; // buttons[0] = Ch5, buttons[1] = Ch6

volatile int channelValues[NUM_CHANNELS];
volatile int channelIndex = 0;
volatile bool frameReady = false;
volatile unsigned long lastFrameTime = 0;
volatile unsigned long lastTime = 0;
volatile bool inSync = false;

void ppmInterrupt() {
  unsigned long now = micros();
  unsigned long duration = now - lastTime;
  lastTime = now;

  int pinState = digitalRead(PPM_PIN);

  if (pinState == HIGH) {
    if (duration >= SYNC_THRESHOLD) {
      channelIndex = 0;
      inSync = true;
    } else if (inSync && channelIndex < NUM_CHANNELS) {
      channelValues[channelIndex] = constrain((int)duration, PPM_MIN - 100, PPM_MAX + 100);
      channelIndex++;
      if (channelIndex >= NUM_CHANNELS) {
        frameReady = true;
        lastFrameTime = now;
      }
    }
  }
}

void setSafeDefaults(int ch[], ButtonPair btn[]) {
  for (int i = 0; i < 4; i++) ch[i] = PPM_MID;
  btn[0] = {false, false};
  btn[1] = {false, false};
}

void updateButtonStates(int ch5, int ch6) {
  // Channel 5
  if (ch5 < BUTTON_LOW_THRESHOLD) {
    buttons[0] = {true, false};
  } else if (ch5 > BUTTON_HIGH_THRESHOLD) {
    buttons[0] = {false, true};
  } else {
    buttons[0] = {false, false};
  }

  // Channel 6
  if (ch6 < BUTTON_LOW_THRESHOLD) {
    buttons[1] = {true, false};
  } else if (ch6 > BUTTON_HIGH_THRESHOLD) {
    buttons[1] = {false, true};
  } else {
    buttons[1] = {false, false};
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PPM_PIN), ppmInterrupt, CHANGE);
  lastTime = micros();
  lastFrameTime = micros();
  Serial.println("VEX PPM Receiver Ready");
}

void loop() {
  // Check for signal loss
  unsigned long timeSinceLastFrame;
  noInterrupts();
  timeSinceLastFrame = micros() - lastFrameTime;
  interrupts();

  if (timeSinceLastFrame > SIGNAL_TIMEOUT) {
    int safeCh[NUM_CHANNELS];
    setSafeDefaults(safeCh, buttons);
    Serial.println("!!! SIGNAL LOST - outputs set to safe defaults !!!");
    Serial.println();
    delay(100);
    return;
  }

  if (frameReady) {
    frameReady = false;

    // Snapshot volatile array safely
    int ch[NUM_CHANNELS];
    noInterrupts();
    for (int i = 0; i < NUM_CHANNELS; i++) ch[i] = channelValues[i];
    interrupts();

    // Channels 1-4: analog sticks, mapped to -100..+100
    Serial.println("=== Channels 1-4 (Sticks) ===");
    for (int i = 0; i < 4; i++) {
      int mapped;
      if (ch[i] <= PPM_MID) {
        mapped = map(ch[i], PPM_MIN, PPM_MID, -100, 0);
      } else {
        mapped = map(ch[i], PPM_MID, PPM_MAX, 0, 100);
      }
      mapped = constrain(mapped, -100, 100);

      if (abs(mapped) <= DEADZONE) mapped = 0;

      Serial.print("  Ch"); Serial.print(i + 1);
      Serial.print(": "); Serial.print(ch[i]); Serial.print("us  =>  ");
      Serial.println(mapped);
    }

    // Update and print button states
    updateButtonStates(ch[4], ch[5]);

    Serial.println("=== Channels 5-6 (Buttons) ===");
    Serial.print("  Ch5 Top:    "); Serial.println(buttons[0].top    ? "ON" : "OFF");
    Serial.print("  Ch5 Bottom: "); Serial.println(buttons[0].bottom ? "ON" : "OFF");
    Serial.print("  Ch6 Top:    "); Serial.println(buttons[1].top    ? "ON" : "OFF");
    Serial.print("  Ch6 Bottom: "); Serial.println(buttons[1].bottom ? "ON" : "OFF");

    Serial.println();
    delay(100);
  }
}
