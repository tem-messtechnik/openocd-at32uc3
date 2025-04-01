#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "target.h"
#include "jtag/jtag.h"
#include "avr32_jtag.h"
#include "avr32_flash.h"
#include "../helper/time_support.h"
#include "avr32_mem.h"
#include "avr32_uc3.h"
#include <sys/param.h>


uint32_t getRegister(struct avr32_jtag *jtag_info,
    uint32_t addr)
{
    uint32_t buf;
    memset(&buf, 0 , sizeof(buf));

    avr32_jtag_mwa_read(jtag_info, SLAVE_HSB_UNCACHED ,addr, &buf);
    
    return buf;
}

uint32_t setRegister(struct avr32_jtag *jtag_info,
    uint32_t addr, uint32_t value)
{
    return avr32_jtag_mwa_write(jtag_info, SLAVE_HSB_UNCACHED, addr, value);
}

int writeCommand(struct avr32_jtag *jtag_info, uint32_t command)
{
    return avr32_jtag_mwa_write(jtag_info, SLAVE_HSB_UNCACHED, FCMD, command);
}

int waitFlashReady(struct avr32_jtag *jtag_info)
{
    int64_t end = timeval_ms();
    int64_t start = timeval_ms();
    
    LOG_DEBUG("%s: waiting for flash", __func__ );

    while (end-start < 1000){
        uint32_t fsrReg = getRegister(jtag_info, FSR);
        LOG_DEBUG("%s: read fsr register: %x", __func__, fsrReg );
        // If LOCKE bit in FSR set
        if((fsrReg & FSR_LOCKE_MASK) >> FSR_LOCKE_OFFSET )
            return ERROR_JTAG_DEVICE_ERROR;
        // If PROGE bit in FSR set
        if((fsrReg & FSR_PROGE_MASK) >> FSR_PROGE_OFFSET )
           return ERROR_COMMAND_SYNTAX_ERROR;
        // Read FRDY bit in FSR
        if ((fsrReg & FSR_FRDY_MASK) >> FSR_FRDY_OFFSET){
            LOG_DEBUG("%s: ready to continue.", __func__ );
            return ERROR_OK; // FLASH ready for next operation
        }
        end = timeval_ms();
    }
    LOG_DEBUG("%s: timeout reached! (1s)",__func__);
    return ERROR_TIMEOUT_REACHED;
}

int clearPageBuffer(struct avr32_jtag *jtag_info)
{
    LOG_DEBUG("%s: start cleaning page buffer.", __func__);

    uint32_t command = WRITE_PROTECT_KEY | CMD_CLEAR_PAGE_BUFFER;
    waitFlashReady(jtag_info);
    writeCommand(jtag_info, command);
    waitFlashReady(jtag_info);
    LOG_DEBUG("%s: done cleaning page buffer.", __func__);

    return ERROR_OK;
}

uint32_t getInternalFlashSize(struct avr32_jtag *jtag_info)
{
    uint32_t prReg = getRegister(jtag_info, PR);
    unsigned int pr = (prReg & PR_FSZ_MASK) >> PR_FSZ_OFFSET;
    unsigned int size = 0;
    switch (pr)
    {
    case 0:
        size = 4 * 1024;
        break;
    case 1:
        size = 8 * 1024;
        break;
    case 2:
        size = 16 * 1024;
        break;
    case 3:
        size = 32 * 1024;
        break;
    case 4:
        size = 48 * 1024;
        break;
    case 5:
        size = 64 * 1024;
        break;
    case 6:
        size = 96 * 1024;
        break;
    case 7:
        size = 128 * 1024;
        break;
    case 8:
        size = 192 * 1024;
        break;
    case 9:
        size = 256 * 1024;
        break;
    case 10:
        size = 384 * 1024;
        break;
    case 11:
        size = 512 * 1024;
        break;
    case 12:
        size = 768 * 1024;
        break;
    case 13:
        size = 1024 * 1024;
        break;
    case 14:
        size = 2024 * 1024;
        break;
    default:
        LOG_DEBUG("%s: unknown flash size. pr register value: %x: %x", __func__, PR, prReg);
        return 0;
    }
    LOG_DEBUG("%s: pr register value: %x: %x", __func__, PR, prReg);
    LOG_DEBUG("%s: flash size is %d", __func__, size);

    return size;
}

int unlockRegion(struct avr32_jtag *jtag_info, uint32_t offset, uint32_t size)
{
    if (offset >= USER_PAGE_OFFSET && offset < USER_PAGE_OFFSET + BYTES_PER_PAGE)
        return ERROR_OK; // the user page doesn't need unlocking
    if (offset >= mDeviceSize || offset + size > mDeviceSize)
        return ERROR_FAIL;
    int lastpagetounlock = ((offset + size) / BYTES_PER_PAGE);
    // compute start offset of page to write to
    uint32_t page = offset & ~(BYTES_PER_PAGE - 1);
    int pagenr = ((offset) / BYTES_PER_PAGE);
    while (pagenr <= lastpagetounlock)
    {
        uint32_t command = WRITE_PROTECT_KEY | CMD_UNLOCK_REGION;
        // include the correct page number in the command
        command |= ((pagenr << FCMD_PAGEN_OFFSET) & FCMD_PAGEN_MASK);
        // Unlocking page: pagenr
        waitFlashReady(jtag_info);
        writeCommand(jtag_info, command); // execute unlock page command
        waitFlashReady(jtag_info);

        page += BYTES_PER_PAGE;
        offset = page;
        pagenr = ((offset) / BYTES_PER_PAGE);
    }
    return ERROR_OK;

}

int unlockEntireFlash(struct avr32_jtag *jtag_info)
{
    return unlockRegion(jtag_info, 0, mDeviceSize);
}

int eraseSequence(struct avr32_jtag *jtag_info)
{
    waitFlashReady(jtag_info);
    uint32_t command = WRITE_PROTECT_KEY | CMD_ERASE_ALL;
    writeCommand(jtag_info, command);
    waitFlashReady(jtag_info);
    return ERROR_OK;
}

int programUserPage(struct avr32_jtag *jtag_info, uint32_t offset, uint32_t* dataBuffer, uint32_t dataSize)
{

    uint8_t bufferPacket[BYTES_PER_PAGE];
    if (offset >= BYTES_PER_PAGE || offset + dataSize > BYTES_PER_PAGE){
        LOG_ERROR("%s: Tried to program past user page boundary", __func__);
        return ERROR_FAIL;
    }
            // Packet bufferPacket(BYTES_PER_PAGE) define a buffer packet
            // to manipulate the data
            // If the packet to be written is smaller than the user page we fill the
            // remaining space with existing data
    if (offset > 0 || dataSize < BYTES_PER_PAGE)
        avr32_jtag_read_memory8(jtag_info, mBaseAddress + USER_PAGE_OFFSET, BYTES_PER_PAGE, bufferPacket);
    // Must clear the page buffer before writing to it.
    clearPageBuffer(jtag_info);
    int bytesLeftInPacket = dataSize;
    int i = 0; // data packet index
    // Fill buffer packet
    while (bytesLeftInPacket > 0)
    {
        bufferPacket[offset]=dataBuffer[i];
        offset++;
        i++;
        bytesLeftInPacket--;
    }
    // Write page buffer
    avr32_jtag_write_memory8(jtag_info, mBaseAddress + USER_PAGE_OFFSET, BYTES_PER_PAGE, bufferPacket);
    uint32_t command = WRITE_PROTECT_KEY | CMD_WRITE_USER_PAGE;
    waitFlashReady(jtag_info);
    writeCommand(jtag_info, command); // execute user page write command
    waitFlashReady(jtag_info);
    return ERROR_OK;
}

int programSequence(struct avr32_jtag *jtag_info, uint32_t offset, uint32_t* dataBuffer, uint32_t dataSize)
{
    if (offset >= USER_PAGE_OFFSET && offset < USER_PAGE_OFFSET + BYTES_PER_PAGE)
        programUserPage(jtag_info, offset - USER_PAGE_OFFSET, dataBuffer, dataSize);
    
    if (offset >= mDeviceSize || offset + dataSize > mDeviceSize){
        LOG_ERROR("%s: Region to be programmed lies outside flash address space.",__func__);
        return ERROR_FAIL;
    }
     // compute start offset of page to write to
    uint32_t page = offset & ~(BYTES_PER_PAGE - 1);
    unsigned int bytesLeft = dataSize;
    uint32_t bufferPacket[WORDS_PER_PAGE]; // we write one page at a time
    // Loop until all bytes in data has been written
    while (bytesLeft > 0)
    {
        memset(bufferPacket, 0xff, BYTES_PER_PAGE);
        // Must clear the page buffer before writing to it.
        clearPageBuffer(jtag_info);
        /* Keeps track of how many bytes to write to the bufferPacket.
         * If the start offset is not aligned on a page boundary, we will not fill
         * the bufferPacket completely. This is also the case when the number of
         * bytes left to write is less than the size of a page. If the bufferPacket
         * is not filled completely we first read the current flash content into
         * the packet. This way we will always preserve existing flash data
         * adjacent to the new data we wish to write.
         */
        int bytesLeftInPacket = MIN((page + BYTES_PER_PAGE - offset), bytesLeft);
        int bufferOffset = offset % BYTES_PER_PAGE;
        if (bufferOffset != 0 || bytesLeftInPacket != (BYTES_PER_PAGE))
        {
            avr32_jtag_read_memory32(jtag_info, mBaseAddress + page, dataSize, bufferPacket);
            
        }
        for (int i = 0; i < bytesLeftInPacket/4; ++i)
        {
            bufferPacket[bufferOffset++]=dataBuffer[i];
        }
        LOG_DEBUG("%s: current bufferPacket that will be written into page %x: ", __func__, page);
        for (int i = 0; i< WORDS_PER_PAGE; i++)
            LOG_DEBUG("%s: \t %x", __func__, bufferPacket[i]);

        
        LOG_DEBUG("%s: start write into flash. Content: %x ... Address: %x, remaining bytes: %d", __func__, *bufferPacket, mBaseAddress+page, bytesLeft);
        int retval = avr32_jtag_write_memory32(jtag_info, mBaseAddress + page, WORDS_PER_PAGE, bufferPacket);
        if (retval != ERROR_OK){
            LOG_ERROR("%s: memory write failed!", __func__);
            return ERROR_FAIL;
        }
        int pagenr = ((offset) / BYTES_PER_PAGE);
        uint32_t command = WRITE_PROTECT_KEY | CMD_WRITE_PAGE;
        // include the correct page number in the command
        command |= pagenr << FCMD_PAGEN_OFFSET;
        waitFlashReady(jtag_info);
        LOG_DEBUG("%s: sending write command: %x", __func__, command);
        writeCommand(jtag_info, command); // execute page write command
        LOG_DEBUG("%s: command sent", __func__);
        waitFlashReady(jtag_info);

        page += BYTES_PER_PAGE;
        offset = page;
        bytesLeft -= bytesLeftInPacket;
    }
    LOG_DEBUG("%s: program sequence is done! But did it work?", __func__);
    return ERROR_OK;
}