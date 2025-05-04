#ifdef USE_RS485
#ifdef USE_EP_CO

#define XSNS_126 126
#define XRS485_25 25

struct EPCOt
{
    bool valid = false;
    float co_value = 0.0;
    char name[3] = "CO";
}EPCO;

#define EPCO_ADDRESS_ID 0x25
#define EPCO_ADDRESS_CHECK 0x0100
#define EPCO_ADDRESS_CO_CONCENTRATION 0x0006
#define EPCO_FUNCTION_CODE 0x03
#define EPCO_TIMEOUT 150

bool EPCOisConnected()
{
    if(!RS485.active) return false;

    RS485.Rs485Modbus -> Send(EPCO_ADDRESS_ID, 0x03 , (0x01 << 8 ) | 0x00 , 0x01);

    delay(200);
    
    //uint32_t start_time = millis();
    //uint32_t wait_until = millis() + EPCO_TIMEOUT;

    /* while(!TimeReached(wait_until))
    {
        delay(1);
        if(RS485.Rs485Modbus -> ReceiveReady()) break;
    }
    if(TimeReached(wait_until) && !RS485.Rs485Modbus -> ReceiveReady()) return false;
 */

    uint8_t buffer[8];
    uint8_t error = RS485.Rs485Modbus -> ReceiveBuffer(buffer, 8);

    if(error)
    {
        AddLog(LOG_LEVEL_INFO, PSTR("EPCO has error %d"), error);
        return false;
    }
    else
    {
        uint16_t check_EPCO = (buffer[3] << 8) | buffer[4];
        //AddLog(LOG_LEVEL_INFO,PSTR("Address of EPCO: 0x%02x"), check_EPCO );
        if(check_EPCO == EPCO_ADDRESS_ID) return true;
    }
    return false;
}

void EPCOInit(void)
{
    if(!RS485.active) return;
    EPCO.valid = EPCOisConnected();
    if(EPCO.valid) Rs485SetActiveFound(EPCO_ADDRESS_ID, EPCO.name);
    AddLog(LOG_LEVEL_INFO, PSTR(EPCO.valid ? "EPCO is connected" : "EPCO is not detected"));
}

void EPCOReadData(void)
{
    if(!EPCO.valid) return;

    //if(RS485.sensor_id != EPCO_ADDRESS_ID) return;
    
    if(isWaitingResponse(EPCO_ADDRESS_ID)) return;

    if((RS485.requestSent[EPCO_ADDRESS_ID] == 0) && RS485.lastRequestTime == 0)
    {
        RS485.Rs485Modbus->Send(EPCO_ADDRESS_ID, EPCO_FUNCTION_CODE, EPCO_ADDRESS_CO_CONCENTRATION, 1);
        RS485.requestSent[EPCO_ADDRESS_ID] = 1;
        RS485.lastRequestTime = millis();
    }

    if ((RS485.requestSent[EPCO_ADDRESS_ID] == 1) && millis() - RS485.lastRequestTime >= 200)
    {
        uint8_t buffer[8];
        uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

        if (error)
        {
            AddLog(LOG_LEVEL_INFO, PSTR("Modbus EPCO error: %d"), error);
        }
        else
        {
            uint16_t co_valueRaw = (buffer[3] << 8) | buffer[4];
            EPCO.co_value = co_valueRaw/100.0;
            //AddLog(LOG_LEVEL_INFO, PSTR("Value of CO: %.1f"), EPCO.co_value);
        }
        RS485.requestSent[EPCO_ADDRESS_ID] = 0;
        RS485.lastRequestTime = 0;
        //advanceSensorID();
    }
}

const char HTTP_SNS_EPCO[] PROGMEM = "{s} CO concentration {m} %.1fppm";
#define D_JSON_EPCO "EPCO"

void EPCOShow(bool json)
{
    if(json)
    {
        ResponseAppend_P(PSTR(",\"%s\":{"), EPCO.name);
        ResponseAppend_P(PSTR("\"" D_JSON_EPCO "\":%.1f"), EPCO.co_value);
        ResponseJsonEnd();
    }
#ifdef USE_WEBSERVER
    else
    {
        WSContentSend_PD(HTTP_SNS_EPCO, EPCO.co_value);
    }
#endif
}

bool Xsns126(uint32_t function)
{
    if (!Rs485Enabled(XRS485_25))
        return false;
    bool result = false;
    if (FUNC_INIT == function)
    {
        EPCOInit();
        //delay(200);
    }
    else if (EPCO.valid)
    {
        switch (function)
        {
        case FUNC_EVERY_250_MSECOND:
            EPCOReadData();
            break;
        case FUNC_JSON_APPEND:
            EPCOShow(1);
            break;
#ifdef USE_WEBSERVER
        case FUNC_WEB_SENSOR:
            EPCOShow(0);
            break;
#endif
        }
    }
    return result;
}

#endif
#endif