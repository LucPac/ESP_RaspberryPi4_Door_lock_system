# Code test RC522

```c
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN   22
#define SS_PIN    5
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
    Serial.begin(9600);

    SPI.begin();
    mfrc522.PCD_Init();

    delay(4);

    mfrc522.PCD_DumpVersionToSerial();
    Serial.println(F("DA SAN SANG DOC THE RFID! (DA CAU HINH CHO ESP32)"));
    Serial.println(F("Hay dua the hoac tag vao gan dau doc..."));
    Serial.println();
}

void loop() {
    if (!mfrc522.PICC_IsNewCardPresent())
        return;

    if (!mfrc522.PICC_ReadCardSerial())
        return;

    Serial.print(F("Phat hien the! Ma UID:"));

    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) {
        uidString += " 0";
        } else {
        uidString += " ";
        }
        uidString += String(mfrc522.uid.uidByte[i], HEX);
    }

    uidString.toUpperCase();
    Serial.println(uidString);
    Serial.println();

    mfrc522.PICC_HaltA();

    mfrc522.PCD_StopCrypto1();

    delay(1000);
}
```

# Code test Keypad 4x4

```c
#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

void setup(){
  Serial.begin(9600);

  Serial.println("DA SAN SANG TEST MA TRAN PHIM 4x4 (ESP32)");
  Serial.println("Hay nhan mot phim bat ky...");
}

void loop(){
  char key = keypad.getKey();

  if (key){
    Serial.print("Phim da nhan: ");
    Serial.println(key);
  }
}
```

# Code test Servo SG90

```c
#include <ESP32Servo.h>

#define SERVO_PIN 15

Servo myServo;

void setup() {
    Serial.begin(9600);

    myServo.attach(SERVO_PIN);

    Serial.println("DA SAN SANG TEST SERVO SG90 (ESP32)");
    Serial.println("Servo se quay tu 0 -> 180 -> 0 lien tuc.");

    myServo.write(0);
    delay(1000);
}

void loop() {
    int angle;
    Serial.println("Quay tu 0 -> 180");
    for (angle = 0; angle <= 180; angle += 1)
    {
        myServo.write(angle);
        delay(15);
    }

    delay(1000);

    Serial.println("Quay tu 180 -> 0");
    for (angle = 180; angle >= 0; angle -= 1)
    {
        myServo.write(angle);
        delay(15);
    }

    delay(1000);
}
```

# Cdoe test LCD1602 I2C

```c
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

String textLine1 = "NGUYEN TAN DAT ";
String textLine2 = "NGUYEN THAI DUONG ";

String padding = "                ";

String scrollLine1 = padding + textLine1;

String scrollLine2 = textLine2 + padding;

int scrollIndex1 = 0;
int scrollIndex2 = 0;

void setup() {
    Serial.begin(9600);
    Wire.begin(21, 22);

    lcd.init();
    lcd.backlight();

    Serial.println("DA SAN SANG TEST LCD I2C 16x2 (ESP32)");
    lcd.setCursor(0, 0);
    lcd.print("Dang Khoi Dong...");
    delay(1000);
}

void loop() {
    lcd.setCursor(0, 0);
    lcd.print(padding); // Ghi đè 16 khoảng trắng để xóa

    // 2. Lấy 16 ký tự từ chuỗi cuộn
    //    substring(vi_tri_bat_dau, vi_tri_ket_thuc)
    String sub1 = scrollLine1.substring(scrollIndex1, scrollIndex1 + 16);

    // 3. Hiển thị chuỗi con lên LCD
    lcd.setCursor(0, 0);
    lcd.print(sub1);

    // 4. Di chuyển index cho lần lặp sau
    scrollIndex1++;

    // 5. Nếu cuộn hết chuỗi, quay lại từ đầu
    //    (scrollLine1.length() - 16) là vị trí bắt đầu cuối cùng
    if (scrollIndex1 > (scrollLine1.length() - 16)) {
        scrollIndex1 = 0;
    }

    // --- XỬ LÝ DÒNG 2 (Cuộn từ Trái sang Phải) ---

    // 1. Xóa dòng 2
    lcd.setCursor(0, 1);
    lcd.print(padding);

    // 2. Lấy 16 ký tự.
    //    Vì cuộn từ trái sang phải, chúng ta lấy chuỗi con TỪ CUỐI
    int len2 = scrollLine2.length();
    String sub2 = scrollLine2.substring(len2 - 16 - scrollIndex2, len2 - scrollIndex2);

    // 3. Hiển thị chuỗi con
    lcd.setCursor(0, 1);
    lcd.print(sub2);

    // 4. Di chuyển index
    scrollIndex2++;

    // 5. Nếu cuộn hết, quay lại từ đầu
    if (scrollIndex2 > (len2 - 16)) {
        scrollIndex2 = 0;
    }

    // --- Tốc độ cuộn ---
    delay(300); // Chờ 300ms. Giảm số này để cuộn nhanh hơn.
}
```
