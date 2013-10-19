/*
** =========================================================================
** File:
**     tspdrv_TSP5000.h
**
** Description:
**     Constants and type definitions for the TouchSense Kernel Module.
**
** Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/

#ifndef _TSPDRV_H
#define _TSPDRV_H
#define VIBE_DEBUG
#include <mach/msm_iomap.h>
#include <linux/mfd/pm8xxx/pm8921.h>

/* Constants */
#define MODULE_NAME                         "tspdrv"
#define TSPDRV                              "/dev/"MODULE_NAME
#define TSPDRV_MAGIC_NUMBER                 0x494D4D52
#define TSPDRV_IOCTL_GROUP                  0x52
/*
** Obsolete IOCTL command
** #define TSPDRV_IDENTIFY_CALLER           _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 2)
*/
#define TSPDRV_STOP_KERNEL_TIMER            _IO(TSPDRV_IOCTL_GROUP, 1) /* obsolete, may be removed in future */
#define TSPDRV_SET_MAGIC_NUMBER             _IO(TSPDRV_IOCTL_GROUP, 2)
#define TSPDRV_ENABLE_AMP                   _IO(TSPDRV_IOCTL_GROUP, 3)
#define TSPDRV_DISABLE_AMP                  _IO(TSPDRV_IOCTL_GROUP, 4)
#define TSPDRV_GET_NUM_ACTUATORS            _IO(TSPDRV_IOCTL_GROUP, 5)
#define TSPDRV_SET_DEVICE_PARAMETER         _IO(TSPDRV_IOCTL_GROUP, 6)
#define TSPDRV_SET_DBG_LEVEL                _IO(TSPDRV_IOCTL_GROUP, 7)
#define TSPDRV_GET_DBG_LEVEL                _IO(TSPDRV_IOCTL_GROUP, 8)

#define MAX_DEBUG_BUFFER_LENGTH             1024
#define VIBE_KP_CFG_UPDATE_RATE_MS          95


#define VIBE_MAX_DEVICE_NAME_LENGTH	64
#define SPI_HEADER_SIZE	3   /* DO NOT CHANGE - SPI buffer header size */
 /* DO NOT CHANGE - maximum number of samples */
#define VIBE_OUTPUT_SAMPLE_SIZE	50

/* Type definitions */
#ifdef __KERNEL__

#define DBL_TEMP                        0
#define DBL_FATAL                       0
#define DBL_ERROR                       1
#define DBL_WARNING                     2
#define DBL_INFO                        3
#define DBL_VERBOSE                     4
#define DBL_OVERKILL                    5


typedef struct
{
    int32_t nDeviceIndex;
    int32_t nDeviceParamID;
    int32_t nDeviceParamValue;
} device_parameter;

struct samples_buffer {
	u_int8_t nactuator_index;  /* 1st byte is actuator index */
	u_int8_t nbit_depth;       /* 2nd byte is bit depth */
	u_int8_t nbuffer_size;     /* 3rd byte is data size */
	u_int8_t data_buffer[VIBE_OUTPUT_SAMPLE_SIZE];
};

struct actuator_samples_buffer {
	int8_t nindex_playing_buffer;
	u_int8_t nindex_output_value;
	/* Use 2 buffers to receive samples from user mode */
	struct samples_buffer actuator_samples[2];
};

#endif

/* Error and Return value codes */
#define VIBE_S_SUCCESS		0	/* Success */
#define VIBE_E_FAIL		 -4	/* Generic error */

#if defined(VIBE_RECORD) && defined(VIBE_DEBUG)
	void _RecorderInit(void);
	void _RecorderTerminate(void);
	void _RecorderReset(int nActuator);
	void _Record(int actuatorIndex, const char *format, ...);
#endif
#define VIBRATION_ON            1
#define VIBRATION_OFF           0


/* Kernel Debug Macros */
#ifdef __KERNEL__
	#ifdef VIBE_DEBUG
		#define DbgOut(_x_, ...) printk(_x_, ##__VA_ARGS__)
	#else   /* VIBE_DEBUG */
		#define DbgOut(_x_)
	#endif  /* VIBE_DEBUG */

	#if defined(VIBE_RECORD) && defined(VIBE_DEBUG)
		#define DbgRecorderInit(_x_) _RecorderInit _x_
		#define DbgRecorderTerminate(_x_) _RecorderTerminate _x_
		#define DbgRecorderReset(_x_) _RecorderReset _x_
		#define DbgRecord(_x_) _Record _x_
	#else /* defined(VIBE_RECORD) && defined(VIBE_DEBUG) */
		#define DbgRecorderInit(_x_)
		#define DbgRecorderTerminate(_x_)
		#define DbgRecorderReset(_x_)
		#define DbgRecord(_x_)
	#endif /* defined(VIBE_RECORD) && defined(VIBE_DEBUG) */
#endif  /* __KERNEL__ */

#if defined(CONFIG_MOTOR_DRV_MAX77693)
extern void max77693_vibtonz_en(bool en);
#endif

#endif  /* _TSPDRV_H */

