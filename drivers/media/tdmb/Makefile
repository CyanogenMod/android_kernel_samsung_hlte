#
# Makefile for TDMB Device Drivers.
#

ccflags-y += -Idrivers/media/tdmb
ccflags-y += -v

obj-$(CONFIG_TDMB) += tdmb.o tdmb_data.o


# FC8050
ccflags-$(CONFIG_TDMB_FC8050) += -Idrivers/media/tdmb/fc8050
obj-$(CONFIG_TDMB_FC8050) += tdmb_port_fc8050.o
obj-$(CONFIG_TDMB_FC8050) += fc8050/
# FC8080
ccflags-$(CONFIG_TDMB_FC8080) += -Idrivers/media/tdmb/fc8080
obj-$(CONFIG_TDMB_FC8080) += tdmb_port_fc8080.o
obj-$(CONFIG_TDMB_FC8080) += fc8080/
# MTV319
ccflags-$(CONFIG_TDMB_MTV319) += -Idrivers/media/tdmb/mtv319
obj-$(CONFIG_TDMB_MTV319) += tdmb_port_mtv319.o
obj-$(CONFIG_TDMB_MTV319) += mtv319/
# TCC3170
ccflags-$(CONFIG_TDMB_TCC3170) += -Idrivers/media/tdmb/tcc3170
ccflags-$(CONFIG_TDMB_TCC3170) += -Idrivers/media/tdmb/tcc3170/inc
ccflags-$(CONFIG_TDMB_TCC3170) += -Idrivers/media/tdmb/tcc3170/inc/tcpal
ccflags-$(CONFIG_TDMB_TCC3170) += -Idrivers/media/tdmb/tcc3170/inc/tcbd_diagnosis
ccflags-$(CONFIG_TDMB_TCC3170) += -Idrivers/media/tdmb/tcc3170/inc/tcbd_stream_parser
ccflags-$(CONFIG_TDMB_TCC3170) += -Idrivers/media/tdmb/tcc3170/inc/tcc_fic_decoder
obj-$(CONFIG_TDMB_TCC3170) += tdmb_port_tcc3170.o
obj-$(CONFIG_TDMB_TCC3170) += tcc3170/
# Test
obj-$(CONFIG_TDMB_SIMUL) += tdmb_port_Simul.o

# i/f
obj-$(CONFIG_TDMB_SPI) += tdmb_spi.o
obj-$(CONFIG_TDMB_EBI) += tdmb_ebi.o

