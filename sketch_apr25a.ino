#include <SoftwareSerial.h>

SoftwareSerial sim800(7, 8);
#define BUZZER_PIN 12

const char fixedNumber[] = "+8801613302520";

const char* allowedCarrierPrefixes[] = {
  "+88016",  // adjust for your home carrier
  "+88017"
};
const byte allowedCarrierPrefixCount = sizeof(allowedCarrierPrefixes) / sizeof(allowedCarrierPrefixes[0]);

enum CallState {
  IDLE,
  DIALING,
  RINGING,
  CONNECTED
};

CallState currentState = IDLE;
String gsmLine;
String incomingNumber = "";
unsigned long ringStartTime;
bool waitingReject = false;
const unsigned long REJECT_DELAY = 4000;

// Function prototypes
void buzzerOff();
void sendAT(const String &cmd);
bool isAllowedCarrier(const String &number);
String parseClipNumber(const String &clipLine);
void resetCall();
void readSerialCommand();
void dialNumber(const char *number);

void setup() {
  Serial.begin(115200);
  sim800.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("=== GSM SYSTEM ===");
  Serial.println("Commands:");
  Serial.println("call        -> dial fixed number");
  delay(2000);
  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CLIP=1");
}

void loop() {
  readSIM800();
  readSerialCommand();
  handleReject();
}

void readSIM800() {
  while (sim800.available()) {
    char c = sim800.read();
    if (c == '\n' || c == '\r') {
      if (gsmLine.length()) {
        gsmLine.trim();
        handleEvent(gsmLine);
        gsmLine = "";
      }
    } else {
      gsmLine += c;
    }
  }
}

void readSerialCommand() {
  if (!Serial.available()) {
    return;
  }

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd == "call") {
    dialNumber(fixedNumber);
  }
}

void dialNumber(const char *number) {
  Serial.print("Dialing: ");
  Serial.println(number);
  sim800.print("ATD");
  sim800.print(number);
  sim800.println(";");
  currentState = DIALING;
}

void handleEvent(const String &line) {
  Serial.print("SIM800L: ");
  Serial.println(line);

  if (line.indexOf("RING") >= 0) {
    if (currentState != RINGING) {
      currentState = RINGING;
      Serial.println("STATE: INCOMING CALL");
      ringStartTime = millis();
      waitingReject = true;  // default to reject, override if allowed carrier
    }
  }

  if (line.indexOf("+CLIP:") >= 0) {
    incomingNumber = parseClipNumber(line);
    Serial.print("Caller: ");
    Serial.println(incomingNumber);
    if (incomingNumber.length() > 0 && isAllowedCarrier(incomingNumber)) {
      waitingReject = false;  // don't reject if same carrier
    }
  }

  if (line.indexOf("CONNECT") >= 0) {
    currentState = CONNECTED;
    waitingReject = false;
    Serial.println("STATE: CALL CONNECTED");
  }

  if (line.indexOf("BUSY") >= 0) {
    Serial.println("STATE: USER BUSY");
    digitalWrite(BUZZER_PIN, LOW);
    resetCall();
  }

  if (line.indexOf("NO ANSWER") >= 0) {
    Serial.println("STATE: NOT ANSWERED");
    resetCall();
  }

  if (line.indexOf("NO CARRIER") >= 0) {
    if (currentState == CONNECTED) {
      Serial.println("STATE: CALL ENDED (AFTER CONNECT)");
    } else {
      Serial.println("STATE: CALL FAILED / REJECTED");
    }
    buzzerOff();
    resetCall();
  }
}

void handleReject() {
  if (currentState != RINGING || !waitingReject) {
    return;
  }

  if (millis() - ringStartTime >= REJECT_DELAY) {
    Serial.println("Rejecting incoming call...");
    sim800.println("ATH");
    buzzerOff();
    waitingReject = false;
  }
}

void resetCall() {
  currentState = IDLE;
  waitingReject = false;
  incomingNumber = "";
}

bool isAllowedCarrier(const String &number) {
  for (byte i = 0; i < allowedCarrierPrefixCount; i++) {
    if (number.startsWith(allowedCarrierPrefixes[i])) {
      return true;
    }
  }
  return false;
}

String parseClipNumber(const String &clipLine) {
  int firstQuote = clipLine.indexOf('"');
  int secondQuote = clipLine.indexOf('"', firstQuote + 1);
  if (firstQuote >= 0 && secondQuote > firstQuote) {
    return clipLine.substring(firstQuote + 1, secondQuote);
  }
  String result = "";
  for (unsigned int i = 0; i < clipLine.length(); i++) {
    char c = clipLine.charAt(i);
    if (c == '+' || isDigit(c)) {
      result += c;
    }
  }
  return result;
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, LOW);
}

void sendAT(const String &cmd) {
  sim800.println(cmd);
  delay(800);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}
