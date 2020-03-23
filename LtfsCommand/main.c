/*
 *   File:   main.c
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
#include "ltfsreg.h"
#include "fusesvc.h"
#include "util.h"
#include "getopt.h"

#define DEFAULT_LOG_DIR    "C:\\ProgramData\\Hewlett-Packard\\LTFS"
#define DEFAULT_WORK_DIR   "C:\\tmp\\LTFS"

typedef enum {
	ListDrives,
	MapDrive,
	UnmapDrive,
	Start,
	Stop,
	Load,
	LoadOnly,
	Mount,
	Eject
} Operation;

static int ListTapeDrives();
static int StartLtfsService();
static int StopLtfsService();
static int MapTapeDrive(CHAR driveLetter, LPCSTR tapeDrive, BYTE tapeIndex, LPCSTR logDir, LPCSTR workDir, BOOL showOffline);
static int UnmapTapeDrive(CHAR driveLetter);
static int LoadTapeDrive(CHAR driveLetter, BOOL mount);
static int EjectTapeDrive(CHAR driveLetter);
static int MountTapeDrive(CHAR driveLetter);

int main(int argc, char *argv[])
{
	int opt = 0;
	Operation operation;
	BOOL showOffline = TRUE;
	BOOL driveLetterArgFound = FALSE;
	BOOL tapeDriveArgFound = FALSE;
	BYTE tapeIndex;
	CHAR driveName[8];
	CHAR driveLetter;
	LPCSTR logDir = DEFAULT_LOG_DIR;
	LPCSTR workDir = DEFAULT_WORK_DIR;
	
	if (!IsElevated())
	{
		fprintf(stderr, "This process requires elevation.\r\n");
		return EXIT_FAILURE;
	}

	while ((opt = getopt(argc, argv, "o:d:t:l:w:n")) != -1)
	{
		switch (opt)
		{
		case 'o':
			{
				if (!_stricmp(optarg, "list"))
					operation = ListDrives;
				else if (!_stricmp(optarg, "map"))
					operation = MapDrive;
				else if (!_stricmp(optarg, "unmap"))
					operation = UnmapDrive;
				else if (!_stricmp(optarg, "start"))
					operation = Start;
				else if (!_stricmp(optarg, "stop"))
					operation = Stop;
				else if (!_stricmp(optarg, "load"))
					operation = Load;
				else if (!_stricmp(optarg, "loadonly"))
					operation = LoadOnly;
				else if (!_stricmp(optarg, "mount"))
					operation = Mount;
				else if (!_stricmp(optarg, "eject"))
					operation = Eject;
				else
				{
					fprintf(stderr, "Invalid operation.\r\n");
					return EXIT_FAILURE;
				}
				break;
			}
		case 'd':
			{
				if (strlen(optarg) != 2 || optarg[1] != ':')
				{
					fprintf(stderr, "Invalid format for drive letter argument.\r\n");
					return EXIT_FAILURE;
				}

				driveLetter = toupper(optarg[0]);

				if (optarg[0] < 'D' || optarg[1] > 'Z')
				{
					fprintf(stderr, "Invalid drive letter.\r\n");
					return EXIT_FAILURE;
				}

				driveLetterArgFound = TRUE;
				break;
			}
		case 'n':
			{
				showOffline = FALSE;
				break;
			}
		case 'l':
			{
				logDir = optarg;
				break;
			}
		case 'w':
			{
				workDir = optarg;
				break;
			}
		case 't':
			{
				strcpy_s(driveName, sizeof(driveName), optarg);
				_strupr_s(driveName, sizeof(driveName));

				if (strlen(driveName) != 5 || strncmp(driveName, "TAPE", 4) != 0)
				{
					fprintf(stderr, "Invalid format for tape drive argument.\r\n");
					return EXIT_FAILURE;
				}

				tapeIndex = driveName[4];

				if (tapeIndex < '0' || tapeIndex > '9')
				{
					fprintf(stderr, "Invalid tape drive index\r\n");
					return EXIT_FAILURE;
				}

				tapeIndex -= '0';

				tapeDriveArgFound = TRUE;
				break;
			}
		default: /* '?' */
			{
				fprintf(stderr, "Usage: %s\r\n", argv[0]);
				return EXIT_FAILURE;
			}
		}
	}

	if (operation == MapDrive ||
		operation == UnmapDrive ||
		operation == Load ||
		operation == LoadOnly ||
		operation == Mount ||
		operation == Eject)
	{
		if (!driveLetterArgFound)
		{
			fprintf(stderr, "Drive letter not specified.\r\n");
			return EXIT_FAILURE;
		}
	}

	if (operation == MapDrive)
	{
		if (!tapeDriveArgFound)
		{
			fprintf(stderr, "Tape drive not specified.\r\n");
			return EXIT_FAILURE;
		}
	}

	switch (operation)
	{
	case ListDrives:
		return ListTapeDrives();

	case Start:
		return StartLtfsService();

	case Stop:
		return StopLtfsService();

	case MapDrive:
		return MapTapeDrive(driveLetter, driveName, tapeIndex, logDir, workDir, showOffline);

	case UnmapDrive:
		return UnmapTapeDrive(driveLetter);

	case Load:
		return LoadTapeDrive(driveLetter, TRUE);

	case LoadOnly:
		return LoadTapeDrive(driveLetter, FALSE);

	case Mount:
		return MountTapeDrive(driveLetter);

	case Eject:
		return EjectTapeDrive(driveLetter);
	}
}

static int ListTapeDrives()
{
	PTAPE_DRIVE driveList;
	DWORD numDrivesFound;

	if (TapeGetDriveList(&driveList, &numDrivesFound))
	{
		printf("\r\nCurrently attached tape drives:\r\n\r\n");

		PTAPE_DRIVE drive = driveList;

		while (drive != NULL)
		{
			printf("TAPE%d: %s: %s %s\r\n", drive->DevIndex, drive->SerialNumber, drive->VendorId, drive->ProductId);
			drive = drive->Next;
		}

		TapeDestroyDriveList(driveList);

		return EXIT_SUCCESS;
	}

	fprintf(stderr, "\r\nNo tape drives found.\r\n");
	return EXIT_FAILURE;
}

static int StartLtfsService()
{
	BOOL success = FuseStartService();

	if (!success)
	{
		fprintf(stderr, "Failed to start service.\r\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int StopLtfsService()
{
	BOOL success = FuseStopService();

	if (!success)
	{
		fprintf(stderr, "Failed to stop service.\r\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int MapTapeDrive(CHAR driveLetter, LPCSTR tapeDrive, BYTE tapeIndex, LPCSTR logDir, LPCSTR workDir, BOOL showOffline)
{
	PTAPE_DRIVE driveList;
	DWORD numDrivesFound;
	BOOL success = FALSE;
	BOOL driveFound = FALSE;

	if (PollFileSystem(driveLetter))
	{
		fprintf(stderr, "Drive letter %c: already in use.\r\n", driveLetter);
		return EXIT_FAILURE;
	}

	if (TapeGetDriveList(&driveList, &numDrivesFound))
	{
		PTAPE_DRIVE drive = driveList;

		while (drive != NULL)
		{
			if (drive->DevIndex == tapeIndex)
			{
				driveFound = TRUE;

				if (LtfsRegGetMappingProperties(driveLetter, NULL, 0))
				{
					fprintf(stderr, "Mapping for %c: arleady exists.\r\n", driveLetter);
				}
				else
				{
					success = LtfsRegCreateMapping(driveLetter, tapeDrive, (BYTE)drive->DevIndex, drive->SerialNumber, logDir, workDir, showOffline);

					if (!success)
						fprintf(stderr, "Failed to create registry entries.\r\n");
				}

				break;
			}

			drive = drive->Next;
		}

		TapeDestroyDriveList(driveList);

		if (!driveFound)
		{
			fprintf(stderr, "Drive %s not found.\r\n", tapeDrive);
			return EXIT_FAILURE;
		}

		if (!success)
		{
			return EXIT_FAILURE;
		}

		success = StopLtfsService();

		if (!success)
		{
			fprintf(stderr, "Failed to stop LTFS service.\r\n");
			return EXIT_FAILURE;
		}

		success = StartLtfsService();

		if (!success)
		{
			fprintf(stderr, "Failed to start LTFS service.\r\n");
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	fprintf(stderr, "No tape drives found.\r\n");
	return EXIT_FAILURE;
}

static int UnmapTapeDrive(CHAR driveLetter)
{
	BYTE numMappings;
	BOOL success;
	
	success = LtfsRegGetMappingCount(&numMappings);

	if (!success)
	{
		fprintf(stderr, "Failed to get mappings from registry.\r\n");
		return EXIT_FAILURE;
	}

	if (!numMappings)
	{
		fprintf(stderr, "No drives currently mapped.\r\n");
		return EXIT_FAILURE;
	}

	success = LtfsRegRemoveMapping(driveLetter);

	if (!success)
	{
		fprintf(stderr, "Failed to remove mapping from registry.\r\n");
		return EXIT_FAILURE;
	}

	numMappings--;

	success = FuseStopService();
	if (!success)
	{
		fprintf(stderr, "Failed to stop LTFS service.\r\n");
		return EXIT_FAILURE;
	}

	if (numMappings > 0)
	{
		success = FuseStartService();

		if (!success)
		{
			fprintf(stderr, "Failed to start LTFS service.\r\n");
			return EXIT_FAILURE;
		}
	}

	return success;
}

static int LoadTapeDrive(CHAR driveLetter, BOOL mount)
{
	char devName[64];
	BOOL result = FALSE;
	
	result = LtfsRegGetMappingProperties(driveLetter, devName, _countof(devName));

	if (!result)
	{
		fprintf(stderr, "Mapping for %c: does not exist.\r\n", driveLetter);
		return EXIT_FAILURE;
	}

	result = TapeLoad(devName);

	if (!result)
	{
		return EXIT_FAILURE;
	}
	
	if (mount)
	{
		result = PollFileSystem(driveLetter);

		if (!result)
		{
			fprintf(stderr, "Cannot start file system. LTFS not running.\r\n");
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

static int MountTapeDrive(CHAR driveLetter)
{
	BOOL result = PollFileSystem(driveLetter);

	if (!result)
	{
		fprintf(stderr, "Cannot start file system. LTFS not running.\r\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int EjectTapeDrive(CHAR driveLetter)
{
	char devName[64];
	BOOL result = FALSE;

	result = LtfsRegGetMappingProperties(driveLetter, devName, _countof(devName));

	if (!result)
	{
		fprintf(stderr, "Mapping for %c: does not exist.\r\n", driveLetter);
		return EXIT_FAILURE;
	}

	result = TapeEject(devName);

	if (!result)
	{
		fprintf(stderr, "Failed to eject tape.\r\n");
		return EXIT_FAILURE;
	}

	// Not sure why LTFSConfigurator does this, but we'll do it too.
	result = PollFileSystem(driveLetter);

	if (!result)
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
