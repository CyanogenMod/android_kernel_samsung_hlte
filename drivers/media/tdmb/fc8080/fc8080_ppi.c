/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_ppi.c

	Description : EBI2LCD interface header file

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/
#include <linux/mutex.h>

#include "fci_types.h"
#include "fc8080_regs.h"
#include "fc8080_ppi.h"
#include "fci_oal.h"

#define BBM_BASE_ADDR   (0)

#define PPI_BMODE       0x00
#define PPI_WMODE       0x04
#define PPI_LMODE       0x08
#define PPI_RD_THRESH   0x30
#define PPI_RD_REG      0x20
#define PPI_READ        0x40
#define PPI_WRITE       0x00
#define PPI_AINC        0x80

static DEFINE_MUTEX(lock);

#define FC8080_PPI_REG  (*(volatile u8 *) (BBM_BASE_ADDR))

s32 fc8080_ppi_init(HANDLE handle, u16 param1, u16 param2)
{
	return BBM_OK;
}

s32 fc8080_ppi_byteread(HANDLE handle, u16 addr, u8 *data)
{
	u16 length = 1;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_READ);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = length >> 4;
	FC8080_PPI_REG = length;

	*data = FC8080_PPI_REG << 4;
	*data |= (FC8080_PPI_REG & 0x0f);
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_wordread(HANDLE handle, u16 addr, u16 *data)
{
	u16 length = 2;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_READ | PPI_AINC);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = length >> 4;
	FC8080_PPI_REG = length;

	*data = (FC8080_PPI_REG & 0x0f) << 4;
	*data |= FC8080_PPI_REG & 0x0f;
	*data |= (FC8080_PPI_REG & 0x0f) << 12;
	*data |= (FC8080_PPI_REG & 0x0f) << 8;
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_longread(HANDLE handle, u16 addr, u32 *data)
{
	u16 length = 4;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_READ | PPI_AINC);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = length >> 4;
	FC8080_PPI_REG = length;

	*data = (FC8080_PPI_REG & 0x0f) << 4;
	*data |= FC8080_PPI_REG & 0x0f;
	*data |= (FC8080_PPI_REG & 0x0f) << 12;
	*data |= (FC8080_PPI_REG & 0x0f) << 8;
	*data |= (FC8080_PPI_REG & 0x0f) << 20;
	*data |= (FC8080_PPI_REG & 0x0f) << 16;
	*data |= (FC8080_PPI_REG & 0x0f) << 28;
	*data |= (FC8080_PPI_REG & 0x0f) << 24;
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_bulkread(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 i, j;
	u8 command;
	u16 x, y;

	x = length / 255;
	y = length % 255;

	mutex_lock(&lock);
	for (i = 0; i < x; i++, addr += 255) {
		FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (addr & 0x0f);

		command = (u8) (PPI_READ | PPI_AINC);

		FC8080_PPI_REG = command >> 4;
		FC8080_PPI_REG = command;

		FC8080_PPI_REG = (u8) ((255 >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (255 & 0x0f);

		for (j = 0; j < 255; j++) {
			data[i * 255 + j] = (u8) ((FC8080_PPI_REG & 0x0f) << 4);
			data[i * 255 + j] |= (u8) (FC8080_PPI_REG & 0x0f);
		}
	}

	if (y) {
		FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (addr & 0x0f);

		command = (u8) (PPI_READ | PPI_AINC);

		FC8080_PPI_REG = command >> 4;
		FC8080_PPI_REG = command;

		FC8080_PPI_REG = (u8) ((y >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (y & 0x0f);

		for (j = 0; j < y; j++) {
			data[x * 255 + j] = (u8) ((FC8080_PPI_REG & 0x0f) << 4);
			data[x * 255 + j] |= (u8) (FC8080_PPI_REG & 0x0f);
		}
	}
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_bytewrite(HANDLE handle, u16 addr, u8 data)
{
	u16 length = 1;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_WRITE);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = length >> 4;
	FC8080_PPI_REG = length;

	FC8080_PPI_REG = data >> 4;
	FC8080_PPI_REG = data;
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_wordwrite(HANDLE handle, u16 addr, u16 data)
{
	u16 length = 2;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_WRITE | PPI_AINC);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = length >> 4;
	FC8080_PPI_REG = length;

	FC8080_PPI_REG = data >> 4;
	FC8080_PPI_REG = data;
	FC8080_PPI_REG = data >> 12;
	FC8080_PPI_REG = data >> 8;
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_longwrite(HANDLE handle, u16 addr, u32 data)
{
	u16 length = 4;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_WRITE | PPI_AINC);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = length >> 4;
	FC8080_PPI_REG = length;

	FC8080_PPI_REG = data >> 4;
	FC8080_PPI_REG = data;
	FC8080_PPI_REG = data >> 12;
	FC8080_PPI_REG = data >> 8;
	FC8080_PPI_REG = data >> 20;
	FC8080_PPI_REG = data >> 16;
	FC8080_PPI_REG = data >> 28;
	FC8080_PPI_REG = data >> 24;
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_bulkwrite(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 i, j;
	u8 command;
	u16 x, y;

	x = length / 255;
	y = length % 255;

	mutex_lock(&lock);
	for (i = 0; i < x; i++, addr += 255) {
		FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (addr & 0x0f);

		command = (u8) (PPI_WRITE | PPI_AINC);

		FC8080_PPI_REG = command >> 4;
		FC8080_PPI_REG = command;

		FC8080_PPI_REG = (u8) ((255 >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (255 & 0x0f);

		for (j = 0; j < 255; j++) {
			FC8080_PPI_REG = (u8) ((data[i * 255 + j] >> 4) & 0x0f);
			FC8080_PPI_REG = (u8) (data[i * 255 + j] & 0x0f);
		}
	}

	if (y) {
		FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
		FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (addr & 0x0f);

		command = (u8) (PPI_WRITE | PPI_AINC);

		FC8080_PPI_REG = command >> 4;
		FC8080_PPI_REG = command;

		FC8080_PPI_REG = (u8) ((y >> 4) & 0x0f);
		FC8080_PPI_REG = (u8) (y & 0x0f);

		for (j = 0; j < y; j++) {
			FC8080_PPI_REG = (u8) ((data[x * 255 + j] >> 4) & 0x0f);
			FC8080_PPI_REG = (u8) (data[x * 255 + j] & 0x0f);
		}
	}
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_dataread(HANDLE handle, u16 addr, u8 *data, u32 length)
{
	s32 i;
	u8 command;

	mutex_lock(&lock);
	FC8080_PPI_REG = (u8) ((addr >> 12) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 8) & 0x0f);
	FC8080_PPI_REG = (u8) ((addr >> 4) & 0x0f);
	FC8080_PPI_REG = (u8) (addr & 0x0f);

	command = (u8) (PPI_READ | PPI_RD_THRESH);

	FC8080_PPI_REG = command >> 4;
	FC8080_PPI_REG = command;

	FC8080_PPI_REG = 0;
	FC8080_PPI_REG = 0;

	for (i = 0; i < length; i++) {
		data[i] = (u8) ((FC8080_PPI_REG & 0x0f) << 4);
		data[i] |= (u8) (FC8080_PPI_REG & 0x0f);
	}
	mutex_unlock(&lock);

	return BBM_OK;
}

s32 fc8080_ppi_deinit(HANDLE handle)
{
	return BBM_OK;
}
