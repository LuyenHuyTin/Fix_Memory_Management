#include "main.h"

const uint8_t DEVICE_VERSION[3] = {1, 2, 0};
const nrf_drv_timer_t TIMER_TEST = NRF_DRV_TIMER_INSTANCE(4);
unsigned int isrtimes[400];
unsigned int *isrtimes_ptr = isrtimes;
volatile int bitsCnt=0;
volatile int isrCnt=0;
volatile int capturedone=0;
unsigned long starttime=0;
int rfoffset = 2;
unsigned char AbicPhaseMeas;
int debug = 0;
int decodemode = 0;
int delay_1 = 20;
int delay_0 = 14;
int delay_p = 5;
int hysteresis =1;
bool gpiote_initialized = false;

namespace PCF7991 {
    static union{
        struct {
            unsigned char filter_l:1;
            unsigned char filter_h:1;
            unsigned char gain:2;
            unsigned char page_nr:2;
            unsigned char SetPageCmd:2;

        };
        unsigned char byteval;
    }AbicConf_Page0;

    static union{
        struct {
            unsigned char txdis:1;
            unsigned char hysteresis:1;
            unsigned char pd:1;
            unsigned char pd_mode:1;
            unsigned char page_nr:2;    
            unsigned char SetPageCmd:2;
        };
        unsigned char byteval;
    }AbicConf_Page1;

    static union{
        struct {
            unsigned char freeze:2;
            unsigned char acqamp:1;
            unsigned char threset:1;
            unsigned char page_nr:2;    
            unsigned char SetPageCmd:2;
        };
        unsigned char byteval;
    }AbicConf_Page2;

    struct pcf7991 {
        uint16_t size;
    };

    EsyPro::Command *GetSpecificCmd(EsyPro::CommunicationCmd_t commCmdType);
    
    class SetupCommand : public EsyPro::Command {
    public:
        void Execute(EsyPro::CommPacket_t *commResPacket,
                     const EsyPro::CommPacket_t *commReqPacket,
                     EsyPro::CommunicationType_t commType) override;
    };

    class ReadCommand : public EsyPro::Command {
    public:
        void Execute(EsyPro::CommPacket_t *commResPacket,
                     const EsyPro::CommPacket_t *commReqPacket,
                     EsyPro::CommunicationType_t commType) override;
    };

    class WriteCommand : public EsyPro::Command {
    public:
        void Execute(EsyPro::CommPacket_t *commResPacket,
                     const EsyPro::CommPacket_t *commReqPacket,
                     EsyPro::CommunicationType_t commType) override;
    };
}