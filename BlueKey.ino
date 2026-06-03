#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include <BleKeyboard.h>

BleKeyboard bleKeyboard("VaultKey");

WebServer server(80);
Preferences prefs;

// ================= OLED =================

Adafruit_SH1106G display =
Adafruit_SH1106G(128, 64, &Wire);

// ================= BUTTONS =================

#define BTN_UP     18
#define BTN_OK     19
#define BTN_BACK   5

// ================= BUZZER =================

#define BUZZER_PIN 23

// ================= LIMITS =================

#define MAX_ITEMS 10

// ================= STORAGE =================

String names[MAX_ITEMS];
String usernames[MAX_ITEMS];
String passwords[MAX_ITEMS];

int itemCount = 0;
int menuIndex = 0;

// ================= STATES =================

enum ScreenState {

  MAIN_MENU,
  ITEM_VIEW
};

ScreenState currentScreen = MAIN_MENU;

// ================= SEND MODE =================

// 0 USERNAME
// 1 PASSWORD
// 2 AUTO LOGIN
// 3 TOTP

int sendMode = 0;

// ================= BUTTON STATES =================

bool lastUp = HIGH;
bool lastOk = HIGH;
bool lastBack = HIGH;

// ================= ENCRYPTION =================

String SECRET_KEY = "VAULTKEY_2026";

// =====================================================

void beep(int duration) {

  tone(BUZZER_PIN, 2000);

  delay(duration);

  noTone(BUZZER_PIN);
}

// =====================================================

String encrypt(String text) {

  String encrypted = "";

  for (int i = 0; i < text.length(); i++) {

    char c =
      text[i] ^
      SECRET_KEY[i % SECRET_KEY.length()];

    encrypted += String((int)c) + "-";
  }

  return encrypted;
}

// =====================================================

String decrypt(String encrypted) {

  String result = "";

  int start = 0;
  int index = encrypted.indexOf('-');

  int i = 0;

  while (index != -1) {

    int val =
      encrypted.substring(start, index).toInt();

    char c =
      val ^
      SECRET_KEY[i % SECRET_KEY.length()];

    result += c;

    start = index + 1;

    index = encrypted.indexOf('-', start);

    i++;
  }

  return result;
}

// =====================================================

String generateOTP() {

  long val =
    (millis() / 1000) * 7919;

  val = abs(val);

  val = val % 900000;

  val += 100000;

  return String(val);
}

// =====================================================

void loadPasswords() {

  prefs.begin("vault", true);

  itemCount = prefs.getInt("count", 0);

  if (itemCount > MAX_ITEMS)
    itemCount = MAX_ITEMS;

  for (int i = 0; i < itemCount; i++) {

    names[i] =
      prefs.getString(
        ("name" + String(i)).c_str(),
        ""
      );

    String encUser =
      prefs.getString(
        ("user" + String(i)).c_str(),
        ""
      );

    String encPass =
      prefs.getString(
        ("pass" + String(i)).c_str(),
        ""
      );

    usernames[i] = decrypt(encUser);
    passwords[i] = decrypt(encPass);
  }

  prefs.end();
}

// =====================================================

void savePassword(
  String name,
  String user,
  String pass
) {

  if (itemCount >= MAX_ITEMS)
    return;

  prefs.begin("vault", false);

  prefs.putString(
    ("name" + String(itemCount)).c_str(),
    name
  );

  prefs.putString(
    ("user" + String(itemCount)).c_str(),
    encrypt(user)
  );

  prefs.putString(
    ("pass" + String(itemCount)).c_str(),
    encrypt(pass)
  );

  itemCount++;

  prefs.putInt("count", itemCount);

  prefs.end();

  loadPasswords();
}

// =====================================================

void bootScreen() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(30, 18);
  display.println("VaultKey");

  display.setCursor(10, 36);
  display.println("Secure Password");

  display.setCursor(32, 50);
  display.println("Manager");

  display.display();

  beep(100);

  delay(2000);
}

// =====================================================

void drawMenu() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(30, 0);
  display.println("VaultKey");

  display.drawLine(
    0,
    10,
    128,
    10,
    SH110X_WHITE
  );

  for (int i = 0; i < itemCount; i++) {

    if (i == menuIndex) {

      display.fillRect(
        0,
        14 + (i * 10),
        128,
        10,
        SH110X_WHITE
      );

      display.setTextColor(SH110X_BLACK);

    } else {

      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(
      5,
      16 + (i * 10)
    );

    display.println(names[i]);
  }

  display.display();
}

// =====================================================

void showSelected() {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 0);
  display.println("ACCOUNT");

  display.drawLine(
    0,
    10,
    128,
    10,
    SH110X_WHITE
  );

  display.setCursor(0, 20);
  display.print("Name:");

  display.setCursor(45, 20);
  display.println(names[menuIndex]);

  display.setCursor(0, 36);

  if (sendMode == 0) {

    display.println("> USERNAME");

  } else if (sendMode == 1) {

    display.println("> PASSWORD");

  } else if (sendMode == 2) {

    display.println("> AUTO LOGIN");

  } else {

    display.println("> TOTP CODE");
  }

  display.setCursor(0, 54);

  display.println("UP=CHANGE OK=SEND");

  display.display();

  currentScreen = ITEM_VIEW;
}

// =====================================================

void sendSelectedData() {

  display.clearDisplay();

  display.setCursor(18, 18);

  display.println("Processing...");

  display.display();

  delay(300);

  if (!bleKeyboard.isConnected()) {

    display.clearDisplay();

    display.setCursor(5, 28);

    display.println("Bluetooth OFF");

    display.display();

    beep(500);

    delay(1500);

    showSelected();

    return;
  }

  // ================= USERNAME =================

  if (sendMode == 0) {

    String safeUser =
      usernames[menuIndex];

    safeUser.trim();

    for (int i = 0; i < safeUser.length(); i++) {

      bleKeyboard.write(safeUser[i]);

      delay(15);
    }

    bleKeyboard.releaseAll();
  }

  // ================= PASSWORD =================

  else if (sendMode == 1) {

    String safePass =
      passwords[menuIndex];

    safePass.trim();

    for (int i = 0; i < safePass.length(); i++) {

      bleKeyboard.write(safePass[i]);

      delay(15);
    }

    bleKeyboard.releaseAll();
  }

  // ================= AUTO LOGIN =================

  else if (sendMode == 2) {

    String safeUser =
      usernames[menuIndex];

    String safePass =
      passwords[menuIndex];

    safeUser.trim();
    safePass.trim();

    // USERNAME

    for (int i = 0; i < safeUser.length(); i++) {

      bleKeyboard.write(safeUser[i]);

      delay(15);
    }

    delay(700);

    // TAB

    bleKeyboard.write(KEY_TAB);

    delay(700);

    // PASSWORD

    for (int i = 0; i < safePass.length(); i++) {

      bleKeyboard.write(safePass[i]);

      delay(15);
    }

    delay(700);

    // ENTER

    bleKeyboard.write(KEY_RETURN);

    delay(300);

    bleKeyboard.releaseAll();
  }

  // ================= TOTP =================

  else {

    String otp =
      generateOTP();

    display.clearDisplay();

    display.setCursor(24, 10);

    display.println("TOTP CODE");

    display.setTextSize(2);

    display.setCursor(15, 36);

    display.println(otp);

    display.display();

    beep(200);

    delay(5000);

    display.setTextSize(1);

    showSelected();

    return;
  }

  display.clearDisplay();

  display.setCursor(40, 28);

  display.println("DONE");

  display.display();

  beep(150);

  delay(1000);

  showSelected();
}

// =====================================================

void setupServer() {

  server.on("/", []() {

    String page = R"rawliteral(

<html>

<head>

<title>VaultKey</title>

<style>

body{
background:#111;
color:white;
font-family:Arial;
padding:20px;
}

input{
width:100%;
padding:10px;
margin-top:10px;
margin-bottom:15px;
}

button{
padding:12px;
width:100%;
background:#00aa55;
color:white;
border:none;
font-size:18px;
margin-top:10px;
}

</style>

</head>

<body>

<h2>VaultKey</h2>

<form action="/save">

<label>Account Name</label>
<input name="name">

<label>Username / Email</label>
<input name="user">

<label>Password</label>
<input name="pass">

<button type="submit">
SAVE PASSWORD
</button>

</form>

<form action="/clear">

<button type="submit">
CLEAR ALL PASSWORDS
</button>

</form>

</body>

</html>

)rawliteral";

    server.send(
      200,
      "text/html",
      page
    );
  });

  // =====================================================

  server.on("/save", []() {

    String name =
      server.arg("name");

    String user =
      server.arg("user");

    String pass =
      server.arg("pass");

    if (
      name.length() > 0 &&
      user.length() > 0 &&
      pass.length() > 0
    ) {

      savePassword(
        name,
        user,
        pass
      );

      server.send(
        200,
        "text/html",
        "<h2>Password Saved</h2>"
      );

      beep(150);

    } else {

      server.send(
        200,
        "text/html",
        "<h2>Missing Data</h2>"
      );
    }
  });

  // =====================================================

  server.on("/clear", []() {

    prefs.begin("vault", false);

    prefs.clear();

    prefs.end();

    itemCount = 0;

    server.send(
      200,
      "text/html",
      "<h2>All Passwords Deleted</h2>"
    );

    beep(400);
  });

  server.begin();
}

// =====================================================

void setup() {

  pinMode(
    BTN_UP,
    INPUT_PULLUP
  );

  pinMode(
    BTN_OK,
    INPUT_PULLUP
  );

  pinMode(
    BTN_BACK,
    INPUT_PULLUP
  );

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  Serial.begin(115200);

  Wire.begin(21, 22);

  display.begin(0x3C, true);

  display.clearDisplay();

  bleKeyboard.begin();

  WiFi.softAP(
    "VaultKey",
    "12345678"
  );

  setupServer();

  loadPasswords();

  bootScreen();

  drawMenu();
}

// =====================================================

void loop() {

  server.handleClient();

  bool upState =
    digitalRead(BTN_UP);

  bool okState =
    digitalRead(BTN_OK);

  bool backState =
    digitalRead(BTN_BACK);

  // ================= UP =================

  if (
    upState == LOW &&
    lastUp == HIGH
  ) {

    if (
      currentScreen ==
      MAIN_MENU
    ) {

      menuIndex++;

      if (
        menuIndex >= itemCount
      )
        menuIndex = 0;

      drawMenu();

    } else if (
      currentScreen ==
      ITEM_VIEW
    ) {

      sendMode++;

      if (sendMode > 3)
        sendMode = 0;

      showSelected();
    }

    beep(40);

    delay(180);
  }

  // ================= OK =================

  if (
    okState == LOW &&
    lastOk == HIGH
  ) {

    if (
      currentScreen ==
      MAIN_MENU
    ) {

      showSelected();

    } else if (
      currentScreen ==
      ITEM_VIEW
    ) {

      sendSelectedData();
    }

    beep(40);

    delay(180);
  }

  // ================= BACK =================

  if (
    backState == LOW &&
    lastBack == HIGH
  ) {

    if (
      currentScreen ==
      ITEM_VIEW
    ) {

      currentScreen =
        MAIN_MENU;

      drawMenu();
    }

    beep(40);

    delay(180);
  }

  lastUp = upState;
  lastOk = okState;
  lastBack = backState;
}