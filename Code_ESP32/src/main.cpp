#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <PubSubClient.h>

// --- CẤU HÌNH BLYNK (Từ code đầy đủ) ---
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""
#include <BlynkSimpleEsp32.h>
char auth[] = BLYNK_AUTH_TOKEN; // <-- ĐÃ THÊM (Cần cho Blynk.config)

// --- CẤU HÌNH WIFI VÀ MQTT (Đã gộp và thống nhất) ---
const char *ssid = "";
const char *pass = "";

const char *mqtt_server = "raspberrypi.local";        // <-- IP CỦA PI (hoặc raspberrypi.local)
const char *mqtt_topic_open = "cua/lenh_mo";          // <-- KÊNH MỞ CỬA
const char *mqtt_topic_warning = "canh_bao/nguoi_la"; // <-- KÊNH CẢNH BÁO

// --- CẤU HÌNH PHẦN CỨNG (Từ code đầy đủ) ---
#define PIN_SG90 4
#define SS_PIN 5
#define RST_PIN 0

// --- BIẾN TOÀN CỤC: MQTT (MỚI - Từ code test) ---
WiFiClient espClient;
PubSubClient client(espClient);

// Biến quản lý MQTT (Không chặn, chống lỗi)
unsigned long lastHeartbeatTime = 0;
const long heartbeatInterval = 20000; // 20 giây
unsigned long lastHardReconnectTime = 0;
const long hardReconnectInterval = 50000; // 50 giây
unsigned long lastReconnectAttempt = 0;
const long reconnectInterval = 5000; // 5 giây

// --- BIẾN TOÀN CỤC: CỬA (LOGIC MỚI: KHÔNG CHẶN) ---
Servo sg90;
bool isDoorOpening = false;
unsigned long doorOpenStartTime = 0;
const long doorOpenDuration = 3000; // Cửa mở trong 3 giây

// --- BIẾN TOÀN CỤC: BLYNK & WIFI (Từ code đầy đủ) ---
bool isWiFiConnected = false;
// (Xóa v0StartTime và v0IsOn vì logic openDoor mới không cần)
bool v1State = true;
bool isSystemLocked = false;
// (Thêm biến mới cho WiFi non-blocking)
unsigned long lastWiFiReconnectAttempt = 0;
const long wifiReconnectInterval = 10000; // Thử kết nối WiFi mỗi 10 giây

// --- BIẾN TOÀN CỤC: KEYPAD & LCD (Từ code đầy đủ) ---
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
// byte rowPins[ROWS] = {13, 12, 14, 27};
// byte colPins[COLS] = {26, 25, 33, 32};
byte rowPins[ROWS] = {27, 14, 12, 13};
byte colPins[COLS] = {26, 25, 33, 32};

int addr = 0;
char password[6] = "12345";
char pass_def[6] = "12345";
char mode_changePass[6] = "*101#";
char data_input[6];
char new_pass1[6];
char new_pass2[6];

unsigned char in_num = 0, error_pass = 0, isMode = 0;
byte index_t = 0; // Dùng làm "cờ trạng thái" (state flag)

LiquidCrystal_I2C lcd(0x27, 16, 2);
Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// === BIẾN MỚI: DÙNG CHO HIỆU ỨNG 200MS (KHÔNG CHẶN) ===
bool isHidingKey = false;       // Cờ báo đang trong 200ms
unsigned long keyPressTime = 0; // Thời điểm bấm phím
int keyToHideCol = 0;           // Vị trí (cột) của phím cần che

// --- BIẾN TOÀN CỤC: RFID (Từ code đầy đủ) ---
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
byte nuidPICC[4] = {0x05, 0xF6, 0x75, 0x06};

void openDoor()
{
    if (v1State == false || isSystemLocked == true)
    {
        Serial.println("!!! He thong DANG TAT/KHOA. Lenh mo cua bi TU CHOI.");
        if (v1State == false)
            Serial.println("   -> Ly do: Keypad da bi tat (V1=0).");
        if (isSystemLocked == true)
            Serial.println("   -> Ly do: He thong bi khoa (sai 3 lan).");
        return; // Thoát ngay, không làm gì cả
    }

    // Chỉ mở nếu cửa đang đóng
    if (isDoorOpening == false)
    {
        Serial.println(">>> Nhan lenh mo cua! Dang mo...");
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("---OPENDOOR---");

        sg90.write(180);
        isDoorOpening = true;
        doorOpenStartTime = millis(); // Bắt đầu đếm 3 giây

        if (isWiFiConnected)
        {
            Blynk.logEvent("infor_log", "Door opened");
            Blynk.virtualWrite(V0, 1); // Cập nhật nút Blynk
        }
    }
    // Reset lại các cờ trạng thái
    error_pass = 0;
    index_t = 0;
    isHidingKey = false; // <-- SỬA LỖI DẤU * CÒN SÓT LẠI (1)
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Nhan duoc tin nhan [");
    Serial.print(topic);
    Serial.print("] ");
    String message = "";
    for (int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println(message);

    // Xử lý lệnh MỞ CỬA
    if (String(topic) == mqtt_topic_open && message == "OPEN")
    {
        openDoor();
    }

    // === THAY ĐỔI 2: XỬ LÝ CẢNH BÁO NGƯỜI LẠ ===
    if (String(topic) == mqtt_topic_warning && message == "UNKNOWN_DETECTED")
    {
        Serial.println("!!! CANH BAO: Phat hien nguoi la!");

        // Gửi thông báo qua Blynk (nếu đang kết nối)
        if (isWiFiConnected && Blynk.connected())
        {
            Blynk.logEvent("door_warning", "Phat hien nguoi la!");
            // (Bạn có thể đổi tên sự kiện "door_warning" trong Blynk nếu muốn)
        }
    }
}

// HÀM KẾT NỐI LẠI (KHÔNG CHẶN)
void reconnect()
{
    Serial.print("Dang thu ket noi MQTT Broker...");
    if (client.connect("ESP32_Door_Client"))
    { // Dùng Client ID khác
        Serial.println("Da ket noi!");

        // Đăng ký kênh MỞ CỬA
        client.subscribe(mqtt_topic_open, 1); // Đăng ký QoS 1
        Serial.println("Da dang ky kenh 'cua/lenh_mo' (QoS 1)");

        // === THAY ĐỔI 3: ĐĂNG KÝ KÊNH CẢNH BÁO ===
        client.subscribe(mqtt_topic_warning, 1);
        Serial.println("Da dang ky kenh 'canh_bao/nguoi_la' (QoS 1)");
    }
    else
    {
        Serial.print("Loi, rc=");
        Serial.print(client.state());
        Serial.println(" Se thu lai sau 5 giay");
    }
}

// --- Các hàm EEPROM (Giữ nguyên) ---
void insertData(char data1[], char data2[])
{
    for (unsigned char i = 0; i < 5; i++)
    {
        data1[i] = data2[i];
    }
    data1[5] = '\0';
}
void writeEeprom(char data[])
{
    for (unsigned char i = 0; i < 5; i++)
    {
        EEPROM.write(i, data[i]);
    }
    EEPROM.commit();
}
void readEeprom()
{
    for (unsigned char i = 0; i < 5; i++)
    {
        password[i] = EEPROM.read(i);
    }
    password[5] = '\0';
}

// --- Các hàm Keypad (Giữ nguyên) ---
void clear_data_input()
{
    for (int i = 0; i < 6; i++)
    {
        data_input[i] = '\0';
    }
    in_num = 0;
}
unsigned char isBufferdata(char data[])
{
    for (unsigned char i = 0; i < 5; i++)
    {
        if (data[i] == '\0')
        {
            return 0;
        }
    }
    return 1;
}
bool compareData(char data1[], char data2[])
{
    for (unsigned char i = 0; i < 5; i++)
    {
        if (data1[i] != data2[i])
        {
            return false;
        }
    }
    return true;
}

// HÀM GETDATA (ĐÃ VIẾT LẠI: Sửa lỗi phím 'D')
void getData()
{
    char key = keypad.getKey();
    if (!key)
    {
        return; // Không có phím nào, thoát
    }

    // ƯU TIÊN 1: Phím 'D' (Delete) phải luôn hoạt động
    if (key == 'D' && in_num > 0)
    {
        in_num--;
        data_input[in_num] = '\0';
        lcd.setCursor(5 + in_num, 1);
        lcd.print(" ");
        isHidingKey = false; // Hủy bỏ bất kỳ hành động che phím nào
    }
    // ƯU TIÊN 2: Chỉ nhận phím mới (0-9, A, B, C, *, #)
    // NẾU chúng ta không đang trong 200ms chờ che phím
    else if (isHidingKey == false)
    {
        if (key != 'D' && in_num < 5)
        {
            data_input[in_num] = key;
            lcd.setCursor(5 + in_num, 1);
            lcd.print(key); // 1. Chỉ in phím

            // 2. Đặt cờ và timer để che phím sau 200ms
            isHidingKey = true;
            keyPressTime = millis();
            keyToHideCol = 5 + in_num;

            in_num++;
        }
    }
    // (Nếu isHidingKey == true và phím không phải 'D', phím bấm sẽ bị bỏ qua)

    // Kiểm tra buffer đầy (vẫn giữ nguyên)
    if (in_num == 5)
    {
        data_input[5] = '\0';
        in_num = 0; // Tự reset in_num
    }
}

// HÀM CHECKPASS (ĐÃ SỬA: Bỏ delay(1000))
unsigned long wrongPassTime = 0;
bool isWrongPass = false;

void checkPass()
{
    if (!v1State || isSystemLocked)
    {
        return;
    }

    // Xóa "WRONG PASSWORD" sau 1 giây
    if (isWrongPass && (millis() - wrongPassTime > 1000))
    {
        isWrongPass = false;
        lcd.clear();
    }

    getData(); // Luôn lấy data

    if (isBufferdata(data_input))
    {                        // Nếu data đã đủ 5 ký tự
        isHidingKey = false; // <-- SỬA LỖI DẤU * CÒN SÓT LẠI
        if (compareData(data_input, password))
        {
            lcd.clear();
            clear_data_input();
            index_t = 2; // Cờ: Chuyển sang trạng thái mở cửa
        }
        else if (compareData(data_input, mode_changePass))
        {
            lcd.clear();
            clear_data_input();
            index_t = 1; // Cờ: Chuyển sang trạng thái đổi pass
        }
        else
        {
            if (error_pass == 2)
            {
                clear_data_input();
                lcd.clear();
                index_t = 3; // Cờ: Chuyển sang trạng thái Lỗi
            }
            else
            {
                lcd.clear();
                lcd.setCursor(1, 1);
                lcd.print("WRONG PASSWORD");
                clear_data_input();
                error_pass++;
                isWrongPass = true;       // Đặt cờ
                wrongPassTime = millis(); // Bắt đầu đếm 1 giây
            }
        }
    }
}

// HÀM CHECK RFID (Giữ nguyên)
void checkRFID()
{
    if (!rfid.PICC_IsNewCardPresent())
        return;
    if (!rfid.PICC_ReadCardSerial())
        return;

    if (rfid.uid.uidByte[0] == nuidPICC[0] &&
        rfid.uid.uidByte[1] == nuidPICC[1] &&
        rfid.uid.uidByte[2] == nuidPICC[2] &&
        rfid.uid.uidByte[3] == nuidPICC[3])
    {
        openDoor(); // Đúng -> gọi hàm mở cửa (không-chặn)
        if (isWiFiConnected)
        {
            Blynk.virtualWrite(V0, 0); // <-- NÓ ĐÂY (SET V0 VỀ 0)
        }
    }
}

// HÀM ERROR (ĐÃ SỬA: KHÔNG CHẶN)
// Hàm này sẽ được gọi lặp đi lặp lại bởi loop() khi index_t = 3
void error()
{
    // Chỉ chạy 1 lần lúc vào
    if (isSystemLocked == false)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" WRONG 3 TIMES ");
        lcd.setCursor(0, 1);
        lcd.print("   LOCK DOOR   ");
        if (isWiFiConnected)
        {
            Blynk.logEvent("door_warning", "Wrong Password Warning");
            Blynk.virtualWrite(V1, 0); // Tắt keypad trên app
        }
        isSystemLocked = true;
        v1State = false; // Tắt keypad vật lý
    }

    // Chờ V1 (từ app Blynk) được bật lại
    if (v1State == true)
    {
        // (Hàm BLYNK_WRITE(V1) sẽ xử lý việc reset)
    }
}

// HÀM CHANGEPASS (ĐÃ SỬA: KHÔNG CHẶN)
// Đây là logic máy trạng thái (state machine)
int changePassState = 0;
unsigned long stateTimer = 0; // Timer chung cho các trạng thái

void changePass()
{
    switch (changePassState)
    {
    case 0: // Trạng thái 0: Chờ nhập pass cũ
        lcd.setCursor(0, 0);
        lcd.print("CURRENT PASSWORD");
        getData();
        if (isBufferdata(data_input))
        {
            isHidingKey = false; // <-- SỬA LỖI DẤU * (THÊM VÀO 1)
            if (compareData(data_input, password))
            {
                clear_data_input();
                changePassState = 1; // Sang trạng thái 1
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("-- CHANGE PASS --");
                stateTimer = millis(); // Bắt đầu đếm 3s
            }
            else
            {
                lcd.clear();
                lcd.print("WRONG");
                stateTimer = millis();
                changePassState = 10; // Trạng thái thoát
            }
        }
        break;

    case 1: // Trạng thái 1: Đợi 3s
        if (millis() - stateTimer > 3000)
        {
            changePassState = 2; // Sang trạng thái 2
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("--- NEW PASS ---");
        }
        break;

    case 2: // Trạng thái 2: Chờ nhập pass mới lần 1
        getData();
        if (isBufferdata(data_input))
        {
            isHidingKey = false; // <-- SỬA LỖI DẤU * (THÊM VÀO 2)
            insertData(new_pass1, data_input);
            clear_data_input();
            changePassState = 3; // Sang trạng thái 3
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("---- AGAIN ----");
        }
        break;

    case 3: // Trạng thái 3: Chờ nhập pass mới lần 2
        getData();
        if (isBufferdata(data_input))
        {
            isHidingKey = false; // <-- SỬA LỖI DẤU * (THÊM VÀO 3)
            insertData(new_pass2, data_input);
            clear_data_input();
            changePassState = 4;   // Sang trạng thái 4 (kiểm tra)
            stateTimer = millis(); // Bắt đầu đếm 1s
        }
        break;

    case 4: // Trạng thái 4: Kiểm tra và lưu
        if (compareData(new_pass1, new_pass2))
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("--- SUCCESS ---");
            writeEeprom(new_pass2);
            insertData(password, new_pass2);
        }
        else
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("-- MISMATCHED --");
        }
        changePassState = 10;  // Sang trạng thái thoát
        stateTimer = millis(); // Bắt đầu đếm 1s
        break;

    case 10: // Trạng thái 10: Chờ 1 giây
        if (millis() - stateTimer > 1000)
        {
            lcd.clear();
            index_t = 0;         // Thoát
            changePassState = 0; // Reset máy trạng thái
        }
        break;
    }
}

// HÀM KẾT NỐI WIFI (ĐÃ SỬA: Dùng logic của code MQTT test)
// Hàm này chỉ chạy 1 LẦN trong setup()
void setupWiFi()
{
    delay(10);
    Serial.print("\nDang ket noi WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);

    int retries = 0;
    // delay() trong setup() là OK, vì nó không làm ảnh hưởng đến loop()
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    { // Thử 10 giây
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        isWiFiConnected = true;
        Serial.println("\nWiFi Da ket noi!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        Blynk.config(auth); // Chỉ config, không connect
    }
    else
    {
        isWiFiConnected = false;
        Serial.println("\nKhong the ket noi WiFi. Chay o che do Offline.");
        // Không restart, cho phép chạy offline
    }
}

// === HÀM MỚI: KIỂM TRA VÀ KẾT NỐI LẠI WIFI (KHÔNG CHẶN) ===
// Hàm này sẽ chạy trong loop()
void checkAndReconnectWiFi()
{
    // Chỉ kiểm tra nếu đang mất kết nối
    if (WiFi.status() != WL_CONNECTED)
    {
        isWiFiConnected = false;
        // Đã đến lúc thử kết nối lại chưa? (mỗi 10 giây)
        if (millis() - lastWiFiReconnectAttempt > wifiReconnectInterval)
        {
            lastWiFiReconnectAttempt = millis();
            Serial.println("!!! Mat ket noi WiFi! Dang thu ket noi lai...");
            WiFi.reconnect();
        }
    }
    else
    {
        isWiFiConnected = true;
    }
}

BLYNK_WRITE(V0)
{ // Nút Mở/Đóng Cửa
    if (isWiFiConnected)
    {
        int pinValue = param.asInt();
        if (pinValue == 1)
        {
            openDoor(); // Gọi hàm mở cửa không-chặn
        }
        // Không cần xử lý (pinValue == 0)
        // vì cửa sẽ tự đóng sau 3 giây (trong loop)
    }
}

BLYNK_WRITE(V1)
{ // Nút Bật/Tắt Keypad
    if (isWiFiConnected)
    {
        int pinValue = param.asInt();
        v1State = (pinValue == 1); // Cập nhật trạng thái

        if (v1State == false)
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("     LOCKED    ");
        }

        // Mở khóa hệ thống nếu đang bị khóa
        if (v1State == true && isSystemLocked == true)
        {
            isSystemLocked = false;
            error_pass = 0;
            index_t = 0; // Thoát trạng thái Lỗi
            lcd.clear();
        }
    }
}

void setup()
{
    Serial.begin(115200); // Tăng tốc độ Serial
    delay(10);            // delay() trong setup() là OK

    // Bỏ Serial2 và rcv_uart_esp_cam() vì đã dùng MQTT
    // Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

    SPI.begin();
    rfid.PCD_Init();

    EEPROM.begin(8);
    readEeprom();

    // Dùng chân PIN_SG90 (4)
    sg90.setPeriodHertz(50);
    sg90.attach(PIN_SG90, 500, 2400);
    sg90.write(0);
    Serial.println("Servo da san sang.");

    lcd.init();
    lcd.backlight();
    lcd.clear();

    // Khởi động WiFi (Chỉ chạy 1 lần)
    setupWiFi();

    // Khởi động MQTT (MỚI)
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    client.setKeepAlive(10); // Phải có (10 giây)
}

void loop()
{

    // --- PHẦN 1: KẾT NỐI (WIFI, BLYNK, MQTT) ---

    // 1. KIỂM TRA WIFI (LOGIC MỚI: KHÔNG CHẶN)
    checkAndReconnectWiFi();

    // 2. CHẠY BLYNK (Nếu có WiFi)
    if (isWiFiConnected)
    {
        if (!Blynk.connected())
        {
            Blynk.connect(); // Cố gắng kết nối Blynk nếu bị mất
        }
        Blynk.run();
    }

    // 3. KIỂM TRA MQTT (LOGIC MỚI: KHÔNG CHẶN)
    if (isWiFiConnected && !client.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > reconnectInterval)
        {
            lastReconnectAttempt = now;
            reconnect(); // Thử kết nối lại (chỉ một lần)
        }
    }

    // 4. LUÔN LẮNG NGHE MQTT (Nếu có WiFi)
    if (isWiFiConnected)
    {
        client.loop(); // <-- ĐÃ THÊM (Rất quan trọng)
    }

    // --- PHẦN 2: LOGIC CỬA (KHÔNG CHẶN) ---

    // 5. KIỂM TRA ĐÓNG CỬA TỰ ĐỘNG
    // (Chạy dù là MQTT, Blynk, Keypad hay RFID mở)
    if (isDoorOpening == true)
    {
        if (millis() - doorOpenStartTime > doorOpenDuration)
        {
            sg90.write(0);
            isDoorOpening = false;
            Serial.println("Da dong cua (tu dong).");
            lcd.clear();
            if (isWiFiConnected)
            {
                Blynk.virtualWrite(V0, 0); // Tắt nút Blynk
            }
        }
    }

    // 6. HEARTBEAT (Từ code MQTT)
    if (isWiFiConnected && (millis() - lastHeartbeatTime > heartbeatInterval))
    {
        lastHeartbeatTime = millis();
        if (client.connected())
        {
            Serial.println("Dang gui heartbeat 'esp32/status'...");
            client.publish("esp32/status", "online");
        }
    }

    // 7. "BÚA TẠ" (SLEDGEHAMMER) (Từ code MQTT)
    if (isWiFiConnected && (millis() - lastHardReconnectTime > hardReconnectInterval))
    {
        lastHardReconnectTime = millis();
        if (client.connected())
        {
            Serial.println("!!! [BUA TA] Dang EP ngat ket noi MQTT...");
            client.disconnect();
            lastReconnectAttempt = 0;
        }
        else
        {
            Serial.println("!!! [BUA TA] Khong can ep.");
        }
    }

    // --- PHẦN 3: LOGIC KEYPAD (KHÔNG CHẶN) ---

    // 8. KIỂM TRA CHE DẤU "*" SAU 200MS (LOGIC MỚI)
    if (isHidingKey == true)
    {
        if (millis() - keyPressTime > 200)
        { // Đã đủ 200ms
            lcd.setCursor(keyToHideCol, 1);
            lcd.print("*");
            isHidingKey = false; // Xong
        }
    }

    // --- PHẦN 4: LOGIC CHÍNH (MÁY TRẠNG THÁI) ---

    // Chỉ chạy logic chính nếu cửa đang đóng
    if (isDoorOpening == false)
    {

        switch (index_t)
        {
        case 0: // Trạng thái 0: Chờ
            if (v1State == true && isSystemLocked == false)
            {
                // Chỉ in nếu không đang báo lỗi VÀ không đang chờ che phím
                if (!isWrongPass && !isHidingKey)
                {
                    lcd.setCursor(1, 0);
                    lcd.print("ENTER PASSWORD");
                }
                checkRFID(); // Ưu tiên RFID
                checkPass(); // Sau đó check Keypad
            }
            else if (isSystemLocked == true)
            {
                index_t = 3; // Nếu bị khóa, nhảy sang trạng thái Lỗi
            }
            else if (v1State == false)
            {
                // (Đã bị BLYNK_WRITE(V1) tắt)
            }
            break;

        case 1: // Trạng thái 1: Đổi mật khẩu
            changePass();
            break;

        case 2: // Trạng thái 2: Mở cửa
            openDoor();
            // (Hàm openDoor() mới đã tự động reset index_t = 0 nên sẽ thoát ngay)
            break;

        case 3: // Trạng thái 3: Lỗi
            error();
            // (Hàm error() mới sẽ tự reset index_t = 0 khi được mở khóa)
            break;
        }
    }
}
