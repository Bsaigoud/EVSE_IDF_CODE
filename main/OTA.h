#ifndef OTA_H
#define OTA_H

#define MASTER 1
#define SLAVE 2
typedef struct
{
    bool GetImageDesc;
    bool UpdateRequired;
    bool UpdateStarted;
    bool DownloadFail;
    bool InstallationFail;
    bool UpdateSuccess;

} OtaStatus_t;

void UpdateFirmware(void);
void downloadSlaveBin(void);
void updateOtaPartition(void);
void updatePartitionDirect(void);
bool getOtaVersion(void);
bool getSlaveFirmwareVersion(void);
#endif