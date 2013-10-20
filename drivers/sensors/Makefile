#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS)		+= sensors_core.o

# combo chipset
obj-$(CONFIG_SENSORS_BMI055)	+= bma255.o bmg160.o
# magnetic_sensor
obj-$(CONFIG_SENSORS_AK8963C)	+= ak8963c.o
# optical_sensor
obj-$(CONFIG_SENSORS_CM3323)	+= cm3323.o
obj-$(CONFIG_SENSORS_CM36653)	+= cm36653.o
# grip_sensor
obj-$(CONFIG_SENSORS_SX9500)	+= sx9500.o
# 6-axis_sensor
obj-$(CONFIG_INPUT_MPU6500)     += mpu6500/
obj-$(CONFIG_SENSORS_YAS532)     += yas532/
