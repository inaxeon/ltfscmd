/*
 *   File:   tape.c
 *   Author: Matthew Millman (inaxeon@hotmail.com)
 *
 *   Command line LTFS Configurator for Windows
 *
 *   This is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   You should have received a copy of the GNU General Public License
 *   along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"
#include "tape.h"

#define TC_MP_MEDIUM_PARTITION         0x11
#define TC_MP_MEDIUM_PARTITION_SIZE    28

static BOOL ScsiIoControl(HANDLE hFile, DWORD deviceNumber, PVOID cdb, UCHAR cdbLength, PVOID dataBuffer, USHORT bufferLength, BYTE dataIn, ULONG timeoutValue);

BOOL TapeGetDriveList(PTAPE_DRIVE *driveList, PDWORD numDrivesFound)
{
    HDEVINFO devInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_TAPE, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    SP_DEVICE_INTERFACE_DATA devData;
    PTAPE_DRIVE listHead = NULL;
    PTAPE_DRIVE listLast = NULL;
    DWORD devIndex = 0;
    DWORD devsFound = 0;
    BOOL lastRet = FALSE;

    do
    {
        devData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        lastRet = SetupDiEnumDeviceInterfaces(devInfo, NULL, &GUID_DEVINTERFACE_TAPE, devIndex, &devData);

        if (lastRet == TRUE)
        {
            DWORD dwRequiredSize = 0;
            SetupDiGetDeviceInterfaceDetail(devInfo, &devData, NULL, 0, &dwRequiredSize, NULL);
            if (dwRequiredSize > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                PSP_DEVICE_INTERFACE_DETAIL_DATA devDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LMEM_FIXED, dwRequiredSize);
                devDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                if (SetupDiGetDeviceInterfaceDetail(devInfo, &devData, devDetail, dwRequiredSize, &dwRequiredSize, NULL) == TRUE)
                {
                    HANDLE handle = CreateFile(devDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                    if (handle != INVALID_HANDLE_VALUE)
                    {
                        BOOL result = FALSE;
                        BYTE dataBuffer[1024];
                        BYTE cdb[6];
                        STORAGE_DEVICE_NUMBER devNum;

                        PTAPE_DRIVE driveData = (PTAPE_DRIVE)LocalAlloc(LMEM_FIXED, sizeof(TAPE_DRIVE));
                        driveData->Next = NULL;

                        DWORD lpBytesReturned;
                        result = DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &devNum, sizeof(STORAGE_DEVICE_NUMBER), &lpBytesReturned, NULL);

                        driveData->DevIndex = devNum.DeviceNumber;

                        if (result)
                        {
                            memset(dataBuffer, 0, sizeof(dataBuffer));
                            memset(cdb, 0, sizeof(cdb));

                            ((PCDB)(cdb))->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
                            ((PCDB)(cdb))->CDB6INQUIRY.IReserved = 4;

                            result = ScsiIoControl(handle, devNum.DeviceNumber, cdb, sizeof(cdb), dataBuffer, sizeof(dataBuffer), SCSI_IOCTL_DATA_IN, 10);

                            if (result)
                            {
                                PINQUIRYDATA inquiryResult = (PINQUIRYDATA)dataBuffer;
                                strncpy_s((char *)driveData->VendorId, sizeof(driveData->VendorId), (char *)inquiryResult->VendorId, MEMBER_SIZE(INQUIRYDATA, VendorId));
                                strncpy_s((char *)driveData->ProductId, sizeof(driveData->ProductId), (char *)inquiryResult->ProductId, MEMBER_SIZE(INQUIRYDATA, ProductId));
                            }
                        }

                        if (result)
                        {
                            memset(dataBuffer, 0, sizeof(dataBuffer));
                            memset(cdb, 0, sizeof(cdb));

                            ((PCDB)(cdb))->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
                            ((PCDB)(cdb))->CDB6INQUIRY.IReserved = 4;
                            ((PCDB)(cdb))->CDB6INQUIRY.PageCode = 0x80;
                            ((PCDB)(cdb))->CDB6INQUIRY.Reserved1 = 1;

                            BOOL result = ScsiIoControl(handle, devNum.DeviceNumber, cdb, sizeof(cdb), dataBuffer, sizeof(dataBuffer), SCSI_IOCTL_DATA_IN, 10);

                            if (result)
                            {
                                PVPD_SERIAL_NUMBER_PAGE inquiryResult = (PVPD_SERIAL_NUMBER_PAGE)dataBuffer;
                                strncpy_s((char *)driveData->SerialNumber, sizeof(driveData->SerialNumber), (char *)inquiryResult->SerialNumber, inquiryResult->PageLength);
                            }
                        }

                        if (result)
                        {
                            memset(dataBuffer, 0, sizeof(dataBuffer));
                            memset(cdb, 0, sizeof(cdb));

                            ((PCDB)(cdb))->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
                            ((PCDB)(cdb))->MODE_SENSE.PageCode = TC_MP_MEDIUM_PARTITION;
                            ((PCDB)(cdb))->MODE_SENSE.AllocationLength = 255;

                            BOOL result = ScsiIoControl(handle, devNum.DeviceNumber, cdb, sizeof(cdb), dataBuffer, sizeof(dataBuffer), SCSI_IOCTL_DATA_IN, 10);

                            // Fuck knows. LTFSConfigurator.exe performs this operation (and others), which it appears may be able to tell us whether or not the 
                            // drive is compatible with LTFS. I have yet to figure out how to parse this data to perform this test, so we're not doing it at present.
                        }

                        if (result)
                        {
                            if (listLast)
                                listLast->Next = driveData;

                            if (listHead == NULL)
                                listHead = driveData;

                            listLast = driveData;
                            devsFound++;
                        }
                        else
                        {
                            LocalFree(driveData);
                        }
                    }

                    CloseHandle(handle);
                }

                LocalFree(devDetail);
            }
        }

        devIndex++;

    } while (lastRet == TRUE);

    SetupDiDestroyDeviceInfoList(devInfo);

    *driveList = listHead;
    *numDrivesFound = devsFound;

    return devIndex > 0;
}

void TapeDestroyDriveList(PTAPE_DRIVE driveList)
{
    PTAPE_DRIVE drive = driveList;

    while (drive != NULL)
    {
        PTAPE_DRIVE toFree = drive;
        drive = drive->Next;
        LocalFree(toFree);
    }
}

BOOL TapeLoad(LPCSTR tapeDrive)
{
    CHAR drivePath[64];
    HANDLE handle;
    BOOL result = FALSE;
    BYTE cdb[6];

    _snprintf_s(drivePath, _countof(drivePath), _TRUNCATE, "\\\\.\\%s", tapeDrive);

    handle = CreateFile(drivePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE)
        return FALSE;

    memset(cdb, 0, sizeof(cdb));

    ((PCDB)(cdb))->START_STOP.OperationCode = SCSIOP_LOAD_UNLOAD;
    ((PCDB)(cdb))->START_STOP.Start = 1;

    result = ScsiIoControl(handle, 0, cdb, sizeof(cdb), NULL, 0, SCSI_IOCTL_DATA_UNSPECIFIED, 300);
    
    CloseHandle(handle);

    return result;
}

BOOL TapeEject(LPCSTR tapeDrive)
{
    DWORD bytesReturned;
    CHAR drivePath[64];
    HANDLE handle;
    BOOL result = FALSE;

    _snprintf_s(drivePath, _countof(drivePath), _TRUNCATE, "\\\\.\\%s", tapeDrive);

    handle = CreateFile(drivePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE)
        return FALSE;

    result = DeviceIoControl(handle, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);

    if (result)
    {
        result = DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
    }

    if (result)
    {
        result = DeviceIoControl(handle, IOCTL_DISK_EJECT_MEDIA, NULL, 0, NULL, 0, &bytesReturned, NULL);
    }

    CloseHandle(handle);

    return result;
}

#define SENSE_INFO_LEN 64

static BOOL ScsiIoControl(HANDLE hFile, DWORD deviceNumber, PVOID cdb, UCHAR cdbLength, PVOID dataBuffer, USHORT bufferLength, BYTE dataIn, ULONG timeoutValue)
{
    DWORD bytesReturned;
    BYTE scsiBuffer[sizeof(SCSI_PASS_THROUGH_DIRECT) + SENSE_INFO_LEN];

    PSCSI_PASS_THROUGH_DIRECT scsiDirect = (PSCSI_PASS_THROUGH_DIRECT)scsiBuffer;
    memset(scsiDirect, 0, sizeof(scsiBuffer));

    scsiDirect->Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    scsiDirect->CdbLength = cdbLength;
    scsiDirect->DataBuffer = dataBuffer;
    scsiDirect->SenseInfoLength = SENSE_INFO_LEN;
    scsiDirect->SenseInfoOffset = sizeof(SCSI_PASS_THROUGH_DIRECT);
    scsiDirect->DataTransferLength = bufferLength;
    scsiDirect->TimeOutValue = timeoutValue;
    scsiDirect->DataIn = dataIn;

    memcpy(scsiDirect->Cdb, cdb, cdbLength);

    return DeviceIoControl(hFile, IOCTL_SCSI_PASS_THROUGH_DIRECT, scsiDirect, sizeof(scsiBuffer), scsiDirect, sizeof(scsiBuffer), &bytesReturned, NULL);
}
