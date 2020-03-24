/*
 *   File:   util.c
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
#include "util.h"

BOOL PollFileSystem(CHAR driveLetter)
{
	char path[64];

	_snprintf_s(path, _countof(path), _TRUNCATE, "\\\\.\\%c:", driveLetter);

	HANDLE handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(handle);
		return TRUE;
	}

	return FALSE;
}

BOOL IsElevated()
{
	BOOL result = FALSE;
	HANDLE token = NULL;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		TOKEN_ELEVATION elevation;
		DWORD size = sizeof(TOKEN_ELEVATION);

		if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
		{
			result = elevation.TokenIsElevated;
		}

		CloseHandle(token);
	}

	return result;
}
