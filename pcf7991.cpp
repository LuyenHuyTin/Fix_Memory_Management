#include "ble_common.h"
#include "eeprom/pcf7991.h"

using namespace EsyPro;
using namespace EEPROM;
using namespace PCF7991;

static const char LOG_DGB_NAME[] = "at93cxx";
static uint16_t curAddr = 0;
static pcf7991 pcf7991Device;

static PCF7991::SetupCommand pcf7991SetupCmd;
static PCF7991::ReadCommand pcf7991ReadCmd;
static PCF7991::WriteCommand pcf7991WriteCmd;

Command *PCF7991::GetSpecificCmd(EsyPro::CommunicationCmd_t commCmdType)
{
    Command *cmd = NULL;

    switch (commCmdType & 0x0F)
    {
    case CMD_BASIC_MEM_SETUP_REQ:
        cmd = &pcf7991SetupCmd;
        NRF_LOG_INFO("[%s]: INFO: Setup Request", LOG_DGB_NAME);
        break;

    case CMD_BASIC_MEM_READ_DATA_REQ:
        cmd = &pcf7991ReadCmd;
        NRF_LOG_INFO("[%s]: INFO: Read Request", LOG_DGB_NAME);
        break;

    case CMD_BASIC_MEM_WRITE_DATA_REQ:
        cmd = &pcf7991WriteCmd;
        NRF_LOG_INFO("[%s]: INFO: Write Request", LOG_DGB_NAME);
        break;
    }

    return cmd;
}

// uint8_t serialToByte()
// {
//   uint8_t retval = 0;

//   while (Serial.available() < 2)
//   {

//   }

//   for(int i = 0; i<2; i++)
//   {
//     retval<<=4;
//     byte raw = Serial.read();
//     if(raw >= '0' && raw <= '9')
//       retval |= raw-'0';
//     else if(raw >= 'A' && raw <= 'F')
//       retval |= raw-'A'+10;
//     else if(raw >= 'a' && raw <= 'f')
//       retval |= raw-'a'+10;

//   }
//   return retval;
// }
// uint8_t readPCF7991Reg(uint8_t addr) 
static bool ReadAT93cxx(At93cxx_t *device, uint8_t *data, uint8_t dataLen)
{
    uint8_t readval = 0;
    writePCF7991Reg(data,8);
    nrf_delay_us(500);
    readval = readPCF991Response();
    return readval;
}

uint8_t readPCF991Response() {
    uint8_t _receive = 0;

    for (int i = 0; i < 8; i++) {
        nrf_gpio_pin_set(SCK_pin);
        nrf_delay_us(50);

        uint8_t tmp = nrf_gpio_pin_read(din_pin);
        if (tmp == 0) {
            _receive &= ~(1 << (7 - i));
        } else {
            _receive |= (1 << (7 - i));
        }
        nrf_gpio_pin_clear(SCK_pin);
        nrf_delay_us(50);
    }
    return _receive;
}

unsigned int fir_filter(unsigned int pulse_fil_in, unsigned int current_pulse)
{
    unsigned int pulse_fil_out;
    if (((int)pulse_fil_in - (int)current_pulse) > 3)
    {
        pulse_fil_out = pulse_fil_in - 1;
    }
    else if (((int)current_pulse - (int)pulse_fil_in) > 3)
    {
        pulse_fil_out = pulse_fil_in + 1;
    }
    else
    {
        pulse_fil_out = pulse_fil_in;
    }
    return pulse_fil_out;
}

void timer_event_handler(nrf_timer_event_t event_type, void *p_context)
{
    // do need to do something
}

void initTimer()
{
    uint32_t err_code;
    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;

    timer_cfg.mode = NRF_TIMER_MODE_TIMER; // Use TIMER mode for timing intervals
    timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_16;
    timer_cfg.frequency = NRF_TIMER_FREQ_250kHz;

    err_code = nrf_drv_timer_init(&TIMER_TEST, &timer_cfg, timer_event_handler); // Assuming no handler needed
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_enable(&TIMER_TEST);
}

void processManchester()
{
    NRF_LOG_INFO("Enter process ");
    int bitcount = 0;
    int bytecount = 0;
    int mybytes[10] = {0};
    int lead = 0;
    int errorCnt = 0;
    int state = 1;
    int start = 0;
    int pulsetime_fil = 0;

    for (start = 0; start < 10; start++)
    {
        if (isrtimes_ptr[start] < 55)
            break;
    }
    start += 3;
    NRF_LOG_INFO("Start: %d", start);
    /* Aadapt filtered pulse time during first pulses */
    int pulsetime_accum = 0;
    for (uint8_t i = start + 1; i < start + 4; i++)
    {
        pulsetime_accum += isrtimes_ptr[i];
    }
    pulsetime_fil = pulsetime_accum / 4;

    if (((isrtimes_ptr[start]) & 1) == 0)
    {
        start--;
        pulsetime_fil = fir_filter(pulsetime_fil, isrtimes_ptr[start]);
    }

    for (int i = start; i < isrCnt; i++)
    {
        // int pulsetime_thresh = pulsetime_fil + (pulsetime_fil/2);
        int pulsetime_thresh = 55;
        int travelTime = isrtimes_ptr[i];
        if (((travelTime & 1) == 1)) // high
        {
            if (travelTime > pulsetime_thresh)
            {
                if (state)
                {
                    state = 1;
                    if (lead < 4)
                    {
                        lead++;
                        // Serial.print("X");
                    }
                    else
                    {
                        mybytes[bytecount] |= (1 << (7 - bitcount++));
                        // Serial.print("1");
                    }
                }
                else
                {
                    NRF_LOG_INFO("X");
                    if (bytecount < 1)
                        errorCnt++;
                }
            }
            else
            {
                pulsetime_fil = fir_filter(pulsetime_fil, travelTime);
                if (state)
                {
                    state = 0;
                    if (lead < 4)
                    {
                        lead++;
                    }
                    else
                    {
                        mybytes[bytecount] |= (1 << (7 - bitcount++));
                        // Serial.print("1");
                    }
                }
                else
                {
                    state++;
                }
            }
        }
        else
        {
            if (travelTime > pulsetime_thresh)
            {
                if (state)
                {
                    state = 1;
                    bitcount++;
                    // Serial.print("0");
                }
                else
                {
                    NRF_LOG_INFO("X");
                    if (bytecount < 1)
                    {
                        errorCnt++;
                    }
                }
            }
            else
            {
                pulsetime_fil = fir_filter(pulsetime_fil, travelTime);
                if (state)
                {
                    state = 0;
                    bitcount++;
                    // Serial.print("0");
                }
                else
                {
                    state++;
                }
            }
        }

        if (bitcount > 7)
        {
            bitcount = 0;
            bytecount++;
        }
        if (travelTime > 80)
        {
            if (bitcount > 0)
            {
                bytecount++;
            }
            break;
        }
    }

    char hash[20];
    std::string result = "";
    NRF_LOG_INFO("\nRESP:");
    if (errorCnt || bytecount == 0)
    {
        NRF_LOG_INFO("NORESP\n");
    }
    else
    {
        for (int s = 0; s < bytecount && s < 20; s++)
        {
            // sprintf(hash,"%02X", mybytes[s]);
            // NRF_LOG_INFO("%c", hash);
            // result += mybytes[s];
            NRF_LOG_INFO("0x%x", mybytes[s]);
        }
    }
    NRF_LOG_INFO("\n");
}

void pin_ISR(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    uint32_t travelTime = nrf_drv_timer_capture(&TIMER_TEST, NRF_TIMER_CC_CHANNEL0);
    // NRF_LOG_INFO("TRAVEL TIME: %d", travelTime);
    nrf_drv_timer_clear(&TIMER_TEST);
    if (nrf_gpio_pin_read(din_pin))
    {
        travelTime &= ~1;
    }
    else
    {
        travelTime |= 1;
    }

    /* handle case overflow
    if(TIFR1&(1 << TOV1)) {
        //Timer1_Overflow = 0;
        travelTime |= 65534;
        TIFR1 = (1<<TOV1);
    }

    */

    isrtimes_ptr[isrCnt] = travelTime;

    if (isrCnt < 400)
        isrCnt++;
}

void gpio_init_interrupt(void)
{
    if (!gpiote_initialized)
    {
        ret_code_t err_code;

        err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);

        nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
        in_config.pull = NRF_GPIO_PIN_PULLUP;

        err_code = nrf_drv_gpiote_in_init(din_pin, &in_config, pin_ISR);
        APP_ERROR_CHECK(err_code);

        nrf_drv_gpiote_in_event_enable(din_pin, true);
        gpiote_initialized = true;
    }
}

void writePCF7991Reg(uint8_t send, uint8_t bits)
{
    nrf_gpio_cfg_output(dout_pin);
    nrf_gpio_pin_clear(dout_pin);
    nrf_delay_us(50);
    nrf_gpio_pin_set(SCK_pin);
    nrf_delay_us(50);
    nrf_gpio_pin_set(dout_pin);
    nrf_delay_us(50);
    nrf_gpio_pin_clear(SCK_pin);
    for (uint8_t i = 0; i < bits; i++) // 10010011
    {
        nrf_delay_us(50);
        if ((send >> (7 - i)) & 0x01)
        {
            nrf_gpio_pin_set(dout_pin);
        }
        else
        {
            nrf_gpio_pin_clear(dout_pin);
        }
        nrf_delay_us(50);
        nrf_gpio_pin_set(SCK_pin);
        nrf_delay_us(50);
        nrf_gpio_pin_clear(SCK_pin);
    }
}

static bool WritePcf7991(pcf7991 *device, const uint8_t *data, uint8_t dataLen)
{
    int32_t bytes = dataLen / 8;
    uint8_t rembits = dataLen % 8;
    int32_t bytBits = 8;
    nrf_gpio_pin_clear(dout_pin);

    writePCF7991Reg(0x19, 8);
    nrf_gpio_pin_clear(dout_pin);

    nrf_delay_us(20); // 20
    nrf_gpio_pin_set(dout_pin);
    nrf_delay_us(20);
    nrf_gpio_pin_clear(dout_pin);

    for (int by = 0; by <= bytes; by++)
    {
        if (by == bytes)
            bytBits = rembits;
        else
            bytBits = 8;

        for (int i = 0; i < bytBits; i++)
        {
            if ((data[by] >> (7 - i)) & 0x01)
            {
                for (int i = 0; i < delay_1; i++)
                {
                    nrf_delay_us(10); // 180
                }
                nrf_gpio_pin_set(dout_pin);
                nrf_delay_us(10);
                nrf_gpio_pin_clear(dout_pin);
            }
            else
            {
                // Serial.print("0");
                for (int i = 0; i < delay_0; i++)
                {
                    nrf_delay_us(10); // 120
                }
                nrf_gpio_pin_set(dout_pin);
                nrf_delay_us(10);
                nrf_gpio_pin_clear(dout_pin);
            }
        }
    }
    // end of transmission
    if (decodemode == 0)
        nrf_delay_us(1200);
    else
        nrf_delay_us(400); // Why biphase needs shorter delay???
}

void readTagResp(void)
{
    writePCF7991Reg(0xe0, 3);
    initTimer();
    isrCnt = 0;
    // nrf_gpio_pin_toggle(TEST_PIN);

    gpio_init_interrupt();

    for (volatile int i = 0; i < 380; i++)
        for (volatile int k = 0; k < 450; k++)

            if (isrCnt < 400 && isrCnt > 3)
            {
                isrtimes_ptr[isrCnt - 1] = isrtimes_ptr[isrCnt - 2] + 201;
                isrCnt++;
            }

    nrf_drv_gpiote_in_event_disable(din_pin);
    // gpiote_initialized = false;
}

void communicateTag(uint8_t *tagcmd, unsigned int cmdLengt)
{
    isrtimes_ptr = isrtimes;
    writeToTag(tagcmd, cmdLengt);
    readTagResp();

    NRF_LOG_INFO("ISRcnt: 0x%x", isrCnt);

    if (debug)
    {
        char hash[5];
        for (int s = 0; s < isrCnt; s++)
        {
            NRF_LOG_INFO("%d, ", isrtimes[s]);
        }
    }
    if (decodemode == 0)
        processManchester();
    // else
    //     processManchester();
}

void tester()
{
    isrCnt = 0;
    gpio_init_interrupt();

    uint8_t phase = readPCF7991Reg(0x08);

    // STOP_TIMER;
    NRF_LOG_INFO("Measured phase: 0x%x", phase);
    NRF_LOG_INFO("ISRcnt: %x", isrCnt);
}

static bool SetupPCF7991(pcf7991 *device)
{
    AbicConf_Page0.SetPageCmd = 1;
    AbicConf_Page1.SetPageCmd = 1;
    AbicConf_Page2.SetPageCmd = 1;

    AbicConf_Page0.gain = 2;
    AbicConf_Page0.filter_h = 1;
    AbicConf_Page0.filter_l = 1;
    AbicConf_Page1.hysteresis = 0;
    AbicConf_Page1.page_nr = 1;
    AbicConf_Page2.page_nr = 2;

    // nrf_gpio_cfg_output(CLKOUT);
    nrf_gpio_cfg_output(SCK_pin);
    nrf_gpio_cfg_output(dout_pin);
    nrf_gpio_cfg_input(din_pin, NRF_GPIO_PIN_PULLUP);

    nrf_gpio_pin_set(SCK_pin);
    nrf_gpio_pin_set(dout_pin);
    nrf_gpio_pin_set(din_pin);
    NRF_LOG_INFO("Init done");
    writePCF7991Reg(0x40, 8); // wake up
    AbicConf_Page1.txdis = 0;
    writePCF7991Reg(AbicConf_Page1.byteval, 8); // rf on
    uint8_t readval = 0;
    for (int i = 2; i < 9; i++)
    {
        readval = readPCF7991Reg(i);
        // NRF_LOG_INFO("address: 0x%x - value: 0x%x", i, readval);
    }
    writePCF7991Reg(0x70, 8);
    return true;
}

void SetupCommand::Execute(CommPacket_t *commResPacket,
                           const CommPacket_t *commReqPacket,
                           CommunicationType_t commType)
{
    bool ret;

    // at93cxxDevice.model = (EEPROMType_t)commReqPacket->buffer[3];
    // at93cxxDevice.org = (AT93cxxOrg_t)commReqPacket->buffer[2];

    ret = SetupAT93cxx(&pcf7991Device);
    // if (ret)
    // {
    //     commResPacket->bleUUID = CUSTOM_VALUE_CTR_RES_CHAR_UUID;
    //     commResPacket->cmd = CMD_BASIC_MEM_SETUP_RES;
    //     commResPacket->buffer[0] = 0x01;
    //     commResPacket->bufLen = 1;
    // }
    // else
    // {
    //     commResPacket->bleUUID = CUSTOM_VALUE_CTR_RES_CHAR_UUID;
    //     commResPacket->cmd = CMD_BASIC_MEM_SETUP_RES;
    //     commResPacket->buffer[0] = 0x00;
    //     commResPacket->bufLen = 1;
    // }

    this->SetCommandRepeatState(false);
}

void ReadCommand::Execute(EsyPro::CommPacket_t *commResPacket,
                          const EsyPro::CommPacket_t *commReqPacket,
                          EsyPro::CommunicationType_t commType)
{
    bool ret;

    ret = ReadAT93cxx(&at93cxxDevice, commResPacket->buffer, EEPROM_93CXX_BUF_SIZE);
    if (ret) {
        commResPacket->bleUUID = CUSTOM_VALUE_READ_CHAR_UUID;
        commResPacket->cmd = CMD_BASIC_MEM_READ_DATA_RES;
        commResPacket->bufLen = EEPROM_93CXX_BUF_SIZE;
    } else {
        commResPacket->cmd = CMD_IGNORE_RES;
    }

    this->SetCommandRepeatState(false);
}

void WriteCommand::Execute(EsyPro::CommPacket_t *commResPacket,
                           const EsyPro::CommPacket_t *commReqPacket,
                           EsyPro::CommunicationType_t commType)
{
    bool ret;
    ret = WritePcf7991(&pcf7991Device, commReqPacket->buffer, EEPROM_93CXX_BUF_SIZE);
    // if (ret)
    // {
    //     commResPacket->bleUUID = CUSTOM_VALUE_CTR_RES_CHAR_UUID;
    //     commResPacket->cmd = CMD_BASIC_MEM_WRITE_DATA_RES;
    //     commResPacket->bufLen = 0;
    // }
    // else
    // {
    //     commResPacket->cmd = CMD_IGNORE_RES;
    // }

    this->SetCommandRepeatState(false);
}