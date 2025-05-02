#include "my_user_config.h"
#ifdef USE_LORA_UART
#define XDRV_74 74
#include "LoRa_E22.h"
#include "HardwareSerial.h"
#include "ArduinoJson.h"

struct LoraSerial_t
{
    bool active = false;
    byte tx = 0;
    byte rx = 0;
    LoRa_E22 *LoraSerial = nullptr;
}LoraSerial;

HardwareSerial *mySerial = nullptr;

class Device_info
{
public:
    String device_name;
    uint8_t channel;  // kênh truyền
    uint8_t addrHigh; // Địa chỉ cao
    uint8_t addrLow;  // Địa chỉ thấp
    float longitude;  // Kinh độ
    float latitude;   // Vĩ độ
    Device_info()
    {
        this->device_name = "";
    }
    Device_info(String device_name)
    {
        this->device_name = device_name;
    }
    Device_info(String device_name, uint8_t addr_high,uint8_t addr_low, float longitude, float latitude)
    {
        this->device_name = device_name;
        this->addrHigh = addr_high;
        this->addrLow = addr_low;
        this->longitude = longitude;
        this->latitude = latitude;
    }
    void setLoraName(String device_name)
    {
        this->device_name = device_name;
    }
};
/* ********************************Biến toàn cục***********************************/
StaticJsonDocument<256> Lora_mapping;
JsonObject Lora_mapping_ojb = Lora_mapping.to<JsonObject>();
/************************************************************************************* */
Device_info device_info("Device A",0x12,0x34, 106.76940000, 10.90682000);
void LoraSerialInit(void)
{
    LoraSerial.active = false;
    if(PinUsed(GPIO_LORA_TX) && PinUsed(GPIO_LORA_RX))
    {
        if(LoraSerial.active)
        {
            AddLog(LOG_LEVEL_ERROR, "LoraSerial: Lora Serial can be configured only on 1 time");
        }
        if(TasmotaGlobal.LoraSerial_enabled)
        {
            AddLog(LOG_LEVEL_ERROR, "LoraSerial: Lora Serial failed because RX/TX already configured");
        }
        else
        {
            LoraSerial.rx = Pin(GPIO_LORA_RX);
            LoraSerial.tx = Pin(GPIO_LORA_TX);
            LoraSerial.active = true;
        }
    }

    if(LoraSerial.active)
    {
        mySerial = new HardwareSerial(1); // Use UART2
        mySerial->begin(9600, SERIAL_8N1, LoraSerial.tx, LoraSerial.rx);
        String mac_str = TasmotaGlobal.mqtt_client; // 		DVES_18E8AC
        // mac_str = WiFi.macAddress(); // "AB:CD:EF:GH:JK:LM"
        int first = mac_str.lastIndexOf("_");             // At _18....
        String mac_device = mac_str.substring(first + 1); //"18E8AC"
        device_info.setLoraName(mac_device);
        //Serial1.begin(9600, SERIAL_8N1, LoraSerial.tx, LoraSerial.rx);
        LoraSerial.LoraSerial = new LoRa_E22(LoraSerial.tx, LoraSerial.rx, mySerial, UART_BPS_RATE_9600, SERIAL_8N1);
        ResponseStatus rs;
        bool check = LoraSerial.LoraSerial->begin();

        if (check)
        {
            //LoraSerial.active = true;
            TasmotaGlobal.LoraSerial_enabled = true;
            AddLog(LOG_LEVEL_INFO, "LoraSerial: Init OK");
        }
        else
        {
            delete mySerial;
            mySerial = nullptr;
            delete LoraSerial.LoraSerial;
            LoraSerial.LoraSerial = nullptr;
            LoraSerial.active = false;
            AddLog(LOG_LEVEL_ERROR, PSTR("LoraSerial: Init failed %s"), rs.getResponseDescription().c_str());
        }
    }
}

#define D_CMND_SEND_LORA_SERIAL "SendLora"
#define D_CMND_SEND_LORA_TELEMETRY "SendLoraTelemetry"
#define D_CMND_SEND_Lora_ATTRIBUTE "SendLoraAttribute"
#define D_CMND_PRINT_MAPPING_Lora "PrintMappingLora"
const char kLoraSerialCommands[] PROGMEM = "|"
    D_CMND_SEND_LORA_SERIAL "|"
    D_CMND_SEND_LORA_TELEMETRY"|"
    D_CMND_SEND_Lora_ATTRIBUTE"|"
    D_CMND_PRINT_MAPPING_Lora;

void (* const LoraSerialCommand[])(void) PROGMEM = {
    &CmndSendLora,
    &CmndSendLoraTelemetry,
    &CmndSendLoraAttribute,
    &CmndPrintMapLora
};

void CmndSendLoraTelemetry(void)
{
    if (XdrvMailbox.data_len == 0)
    {
        AddLog(LOG_LEVEL_INFO, PSTR("No data to transmit"));
        ResponseCmndDone();
        return;
    }

    StaticJsonDocument<256> doc;
    JsonArray arr = doc.createNestedArray(String(device_info.device_name));
    JsonObject data = arr.createNestedObject();

    StaticJsonDocument<128> mailboxDoc;
    DeserializationError error = deserializeJson(mailboxDoc, XdrvMailbox.data);
    if (error)
    {
        AddLog(LOG_LEVEL_ERROR, PSTR("JSON parse error"));
        ResponseCmndDone();
        return;
    }
    for (JsonPair p : mailboxDoc.as<JsonObject>())
    {
        data[p.key()] = p.value();
    }
    String json_string;
    serializeJson(doc, json_string);
    int len = json_string.length();
    ResponseStatus rs = LoraSerial.LoraSerial->sendMessage(json_string.c_str(), len);
    AddLog(LOG_LEVEL_INFO, PSTR("LoRa Send: %s, with len is %d"), rs.getResponseDescription().c_str(), len);
    ResponseCmndDone();
}
void CmndPrintMapLora(void)
{
    AddLog(LOG_LEVEL_INFO, PSTR("===== Lora MAPPING ====="));

    for (JsonPair p : Lora_mapping_ojb)
    {
        const char *key = p.key().c_str();                // "V1", "V2", ...
        const char *value = p.value().as<const char *>(); // "DHT11-Temperature", ...
        AddLog(LOG_LEVEL_INFO, PSTR("%s => %s"), key, value);
    }
    AddLog(LOG_LEVEL_INFO, PSTR("========================="));
}
void CmndSendLoraAttribute(void)
{
    StaticJsonDocument<256> doc;
    JsonObject deviceA = doc.createNestedObject(device_info.device_name);
    deviceA["addrHigh"] = device_info.addrHigh;
    deviceA["addrLow"] = device_info.addrLow;
    deviceA["longitude"] = device_info.longitude;
    deviceA["latitude"] = device_info.latitude;
    String json_string;
    serializeJson(doc, json_string);
    AddLog(LOG_LEVEL_INFO, PSTR("Lora Send: %s"), json_string.c_str());
    LoraSerial.LoraSerial->sendFixedMessage(device_info.addrHigh,device_info.addrLow,device_info.channel,json_string);
    ResponseCmndDone();
}
void CmndSendLora(void)
{
    if (XdrvMailbox.data_len == 0)
    {
        AddLog(LOG_LEVEL_INFO, PSTR("Nothing to transmit"));
        ResponseCmndDone();
        return;
    }
    char *tran = XdrvMailbox.data;
    AddLog(LOG_LEVEL_INFO, PSTR("Transmit data: %s"), tran);
    ResponseStatus rs = LoraSerial.LoraSerial->sendMessage(tran);
    AddLog(LOG_LEVEL_INFO, rs.getResponseDescription().c_str());
    ResponseCmndDone();
}

void LoraSerial_COLLECT_DATA()
{
    ResponseClear();
    XsnsCall(FUNC_JSON_APPEND);
    const char *raw = ResponseData();
    //  Bắt lỗi dấu phẩy ở đầu
    String fixed = raw;
    if (fixed.startsWith(","))
    {
        fixed = fixed.substring(1); // Bỏ dấu phẩy đầu
    }
    fixed = "{" + fixed + "}"; // Bọc thành JSON hoàn chỉnh

    AddLog(LOG_LEVEL_INFO, PSTR("Sensor JSON fixed: %s"), fixed.c_str());

    // Parse JSON
    StaticJsonDocument<256> input_doc;
    DeserializationError err = deserializeJson(input_doc, fixed);
    if (err)
    {
        AddLog(LOG_LEVEL_ERROR, PSTR("Sensor JSON parse error: %s"), err.c_str());
        return;
    }

    // Gói lại JSON kiểu {"Device":[{...}]}
    StaticJsonDocument<512> out_doc;
    JsonArray arr = out_doc.createNestedArray(String(device_info.device_name));
    JsonObject data = arr.createNestedObject();
    int v_count = 1; // Đếm số biến V
    for (JsonPair p : input_doc.as<JsonObject>())
    {
        const char *sensorName = p.key().c_str(); // "DHT11"
        JsonObject inner = p.value().as<JsonObject>();
        for (JsonPair q : inner)
        {
            String sensor_sub = String(sensorName) + "-" + q.key().c_str();
            // Tạo key dạng V1, V2, ...
            String v_key = "V" + String(v_count);
            Lora_mapping_ojb[v_key] = sensor_sub;
            data[v_key] = q.value();
            v_count++;
        }
    }
    String final_payload;
    serializeJson(out_doc, final_payload);
    LoraSerial.LoraSerial->sendFixedMessage(device_info.addrHigh,device_info.addrLow,device_info.channel,final_payload);
}

void LoraSerialProcessing()
{
    if(!LoraSerial.active) return;

    if(LoraSerial.LoraSerial -> available() > 1)
    {
        ResponseContainer rc = LoraSerial.LoraSerial -> receiveMessage();
        if(rc.status.code == 1)
        {
            AddLog(LOG_LEVEL_INFO, PSTR("Receive Mess: "));
            AddLog(LOG_LEVEL_INFO, rc.data.c_str());
        }
    }
}
bool Xdrv74(uint32_t function)
{
    bool result = false;
    if(FUNC_PRE_INIT == function)
    {
        LoraSerialInit();
    }
    else if(LoraSerial.active)
    {
        switch(function)
        {
            case FUNC_ACTIVE:
                result = true;
                break;
            case FUNC_EVERY_250_MSECOND:
                LoraSerialProcessing();
                break;
            case FUNC_COMMAND:
                result = DecodeCommand(kLoraSerialCommands,LoraSerialCommand);
                break;
            case FUNC_AFTER_TELEPERIOD:
                LoraSerial_COLLECT_DATA();
        }
    }
    return result;
}
#endif // USE_LORA_UART
