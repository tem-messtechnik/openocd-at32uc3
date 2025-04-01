#define HFLASH 0xFFFE0000
#define FCR HFLASH+0x0
#define FCMD HFLASH+0x4
#define FSR HFLASH+0x8
#define PR HFLASH+0xC
#define VR HFLASH+0x10
#define FGPFRHI HFLASH+0x14
#define FGPFRLO HFLASH+0x18

#define FSR_FRDY_MASK       0x00000001
#define FSR_FRDY_OFFSET     0
#define FSR_PROGE_MASK      0x00000008
#define FSR_PROGE_OFFSET    3
#define FSR_LOCKE_MASK      0x00000004
#define FSR_LOCKE_OFFSET    2
#define PR_FSZ_MASK         0x0000001F
#define PR_FSZ_OFFSET       0
#define FCMD_FCMD_MASK      0x0000001F
#define FCMD_FCMD_OFFSET    0
#define FCMD_PAGEN_MASK     0x00FFFF00
#define FCMD_PAGEN_OFFSET   8
#define FCMD_KEY_MASK       0xFF000000
#define FCMD_KEY_OFFSET     24
#define FGPFR_LOCK_MASK     0x0000FFFF
#define FGPFR_LOCK_OFFSET   0
#define WORDS_PER_PAGE      128
#define BYTES_PER_PAGE      WORDS_PER_PAGE*4
#define USER_PAGE_OFFSET    0x00800000
#define WRITE_PROTECT_KEY   0xA5000000
#define CMD_WRITE_PAGE          1
#define CMD_ERASE_PAGE          2
#define CMD_CLEAR_PAGE_BUFFER   3    
#define CMD_LOCK_REGION         4
#define CMD_UNLOCK_REGION       5
#define CMD_ERASE_ALL           6
#define CMD_WRITE_GP_FUSE_BIT   7
#define CMD_ERASE_GP_FUSE_BIT   8
#define CMD_SET_SECURITY_BIT    9
#define CMD_PROGRAM_GP_FUSE_BYTE 10
#define CMD_WRITE_USER_PAGE     13
#define CMD_ERASE_USER_PAGE     14

#define mDeviceSize 512*1024
#define mBaseAddress 0x80000000


uint32_t getRegister(struct avr32_jtag *jtag_info, uint32_t addr);
uint32_t setRegister(struct avr32_jtag *jtag_info,
    uint32_t addr, uint32_t value);

int writeCommand(struct avr32_jtag *jtag_info, uint32_t command);
int waitFlashReady(struct avr32_jtag *jtag_info);
int clearPageBuffer(struct avr32_jtag *jtag_info);
uint32_t getInternalFlashSize(struct avr32_jtag *jtag_info);
int unlockRegion(struct avr32_jtag *jtag_info, uint32_t offset, uint32_t size);
int unlockEntireFlash(struct avr32_jtag *jtag_info);
int eraseSequence(struct avr32_jtag *jtag_info);
int programUserPage(struct avr32_jtag *jtag_info, uint32_t offset, uint32_t* dataBuffer, uint32_t dataSize);
int programSequence(struct avr32_jtag *jtag_info, uint32_t offset, uint32_t* dataBuffer, uint32_t dataSize);









