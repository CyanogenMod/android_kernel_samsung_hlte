#
#  makefile for tspdrv kernel module
#

obj-$(CONFIG_VIBETONZ) += vibrator.o
obj-$(CONFIG_MOTOR_DRV_MAX77803)	+= max77803_haptic.o
obj-$(CONFIG_MOTOR_DRV_ISA1400)		+= isa1400_vibrator.o
obj-$(CONFIG_MOTOR_DRV_TSP5000)		+= tspdrv_TSP5000.o
vibrator-objs +=tspdrv.o

# Makefile for Vibrator controlled by PMIC Regulator.
obj-$(CONFIG_MSM_VIBRATOR) += msm_vibrator.o