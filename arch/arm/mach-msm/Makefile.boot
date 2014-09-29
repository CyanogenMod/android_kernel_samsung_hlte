# MSM7x01A
   zreladdr-$(CONFIG_ARCH_MSM7X01A)	:= 0x10008000
params_phys-$(CONFIG_ARCH_MSM7X01A)	:= 0x10000100
initrd_phys-$(CONFIG_ARCH_MSM7X01A)	:= 0x10800000

# MSM7x25
   zreladdr-$(CONFIG_ARCH_MSM7X25)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X25)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X25)	:= 0x0A000000

# MSM7x27
   zreladdr-$(CONFIG_ARCH_MSM7X27)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X27)	:= 0x0A000000

# MSM7x27A
   zreladdr-$(CONFIG_ARCH_MSM7X27A)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X27A)	:= 0x00200100

# MSM8625
   zreladdr-$(CONFIG_ARCH_MSM8625)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM8625)	:= 0x00200100

# MSM7x30
   zreladdr-$(CONFIG_ARCH_MSM7X30)	:= 0x00208000
params_phys-$(CONFIG_ARCH_MSM7X30)	:= 0x00200100
initrd_phys-$(CONFIG_ARCH_MSM7X30)	:= 0x01200000

ifeq ($(CONFIG_MSM_SOC_REV_A),y)
# QSD8x50
   zreladdr-$(CONFIG_ARCH_QSD8X50)	:= 0x20008000
params_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x20000100
initrd_phys-$(CONFIG_ARCH_QSD8X50)	:= 0x24000000
endif

# MSM8x60
   zreladdr-$(CONFIG_ARCH_MSM8X60)	:= 0x40208000

# MSM8960
   zreladdr-$(CONFIG_ARCH_MSM8960)	:= 0x80208000

# MSM8930
   zreladdr-$(CONFIG_ARCH_MSM8930)	:= 0x80208000

# APQ8064
   zreladdr-$(CONFIG_ARCH_APQ8064)	:= 0x80208000

# MSM8974 & 8974PRO
   zreladdr-$(CONFIG_ARCH_MSM8974)	:= 0x00008000
	dtb-$(CONFIG_SEC_MONDRIAN_PROJECT)	+= msm8974-sec-mondrianlte-r00.dtb
	dtb-$(CONFIG_SEC_MONDRIAN_PROJECT)	+= apq8074-sec-mondrianwifi-r00.dtb
	dtb-$(CONFIG_SEC_MONDRIAN_PROJECT)	+= msm8974-sec-mondrianlte-r07.dtb
	dtb-$(CONFIG_SEC_MONDRIAN_PROJECT)	+= apq8074-sec-mondrianwifi-r07.dtb
	dtb-$(CONFIG_SEC_MONDRIAN_PROJECT)	+= msm8974-sec-mondrianlte-r08.dtb
	dtb-$(CONFIG_SEC_MONDRIAN_PROJECT)	+= apq8074-sec-mondrianwifi-r08.dtb
	dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-viennalte-r00.dtb
	dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-viennalte-r10.dtb
	dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-viennalte-r12.dtb
ifeq ($(CONFIG_SEC_K_PROJECT),y)
    ifeq ($(CONFIG_MACH_KLTE_KOR),y)
        # dtbs for KOR
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r00.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r01.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r02.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r03.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r04.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r05.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kkor-r06.dtb
    else ifeq ($(CONFIG_MACH_K3GDUOS_CTC),y)
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kctc-r00.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kctc-r01.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kctc-r02.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kctc-r03.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kctc-r04.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kctc-r05.dtb
    else ifeq ($(CONFIG_MACH_KLTE_JPN),y)
			# dtbs for JPN
			dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kjpn-r03.dtb
			dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kjpn-r04.dtb
			dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kjpn-r05.dtb    
			dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-kjpn-r06.dtb
	else
        # default dtbs
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r00.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r01.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r02.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r03.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r04.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r05.dtb
        dtb-$(CONFIG_SEC_K_PROJECT)	+= msm8974pro-ac-sec-k-r06.dtb
    endif        
endif        
	dtb-$(CONFIG_SEC_N2_PROJECT)	+= msm8974-sec-n2-r00.dtb
ifeq ($(CONFIG_SEC_H_PROJECT),y)
ifeq ($(CONFIG_SEC_LOCALE_KOR),y)
	dtb-y += msm8974-sec-hltekor-r04.dtb
	dtb-y += msm8974-sec-hltekor-r05.dtb
	dtb-y += msm8974-sec-hltekor-r06.dtb
	dtb-y += msm8974-sec-hltekor-r07.dtb
else
	dtb-y += msm8974-sec-hlte-r03.dtb
	dtb-y += msm8974-sec-hlte-r04.dtb
	dtb-y += msm8974-sec-hlte-r05.dtb
	dtb-y += msm8974-sec-hlte-r06.dtb
	dtb-y += msm8974-sec-hlte-r07.dtb
	dtb-y += msm8974-sec-hlte-r09.dtb
endif
endif
	dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03-r00.dtb
	dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03-r05.dtb
	dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03-r07.dtb
	dtb-$(CONFIG_SEC_PICASSO_PROJECT)	+= msm8974-sec-picasso-r11.dtb
	dtb-$(CONFIG_SEC_PICASSO_PROJECT)	+= msm8974-sec-picasso-r12.dtb
	dtb-$(CONFIG_SEC_PICASSO_PROJECT)	+= msm8974-sec-picasso-r14.dtb
	dtb-$(CONFIG_SEC_V2_PROJECT)	+= msm8974-sec-v2lte-r00.dtb
	dtb-$(CONFIG_SEC_V2_PROJECT)	+= msm8974-sec-v2lte-r01.dtb
	dtb-$(CONFIG_SEC_V2_PROJECT)	+= msm8974-sec-v2lte-r02.dtb
ifeq ($(CONFIG_SEC_F_PROJECT),y)
# flte kor device tree
	dtb-y	+= msm8974-sec-fltekor-r05.dtb
	dtb-y	+= msm8974-sec-fltekor-r06.dtb
	dtb-y	+= msm8974-sec-fltekor-r07.dtb
	dtb-y	+= msm8974-sec-fltekor-r09.dtb
	dtb-y	+= msm8974-sec-fltekor-r10.dtb
	dtb-y	+= msm8974-sec-fltekor-r11.dtb
	dtb-y	+= msm8974-sec-fltekor-r12.dtb
	dtb-y	+= msm8974-sec-fltekor-r13.dtb
	dtb-y	+= msm8974-sec-fltekor-r14.dtb
	dtb-y	+= msm8974-sec-fltekor-r16.dtb
endif
ifeq ($(CONFIG_SEC_KS01_PROJECT),y)
ifeq ($(CONFIG_SEC_LOCALE_KOR),y)
    # dtbs for KOR
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r00.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r01.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r02.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r03.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r04.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r05.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r06.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r07.dtb
    dtb-$(CONFIG_SEC_KS01_PROJECT)	+= msm8974-sec-ks01lte-r11.dtb
endif
endif
	dtb-$(CONFIG_SEC_KACTIVE_PROJECT)	+= msm8974pro-ac-sec-kactivelte-r00.dtb

# APQ8084
   zreladdr-$(CONFIG_ARCH_APQ8084)	:= 0x00008000
        dtb-$(CONFIG_ARCH_APQ8084)	+= apq8084-sim.dtb

# MSMKRYPTON
   zreladdr-$(CONFIG_ARCH_MSMKRYPTON)	:= 0x00008000
	dtb-$(CONFIG_ARCH_MSMKRYPTON)	+= msmkrypton-sim.dtb

# MSM9615
   zreladdr-$(CONFIG_ARCH_MSM9615)	:= 0x40808000

# MSM9625
   zreladdr-$(CONFIG_ARCH_MSM9625)	:= 0x00208000
        dtb-$(CONFIG_ARCH_MSM9625)	+= msm9625-v1-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM9625)	+= msm9625-v1-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM9625)	+= msm9625-v1-rumi.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2-cdp.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2.1-mtp.dtb
	dtb-$(CONFIG_ARCH_MSM9625)      += msm9625-v2.1-cdp.dtb

# MSM8226
   zreladdr-$(CONFIG_ARCH_MSM8226)	:= 0x00008000
#        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sim.dtb
#        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-fluid.dtb
#        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-720p-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-1080p-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-720p-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-1080p-mtp.dtb
#        dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v1-mtp.dtb
#        dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-720p-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-v2-1080p-mtp.dtb
#	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-mtp-r01.dtb
ifeq ($(CONFIG_MACH_MILLET3G_EUR),y)
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-millet3geur-r00.dtb
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-millet3geur-r01.dtb
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-millet3geur-r02.dtb
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-millet3geur-r03.dtb
else ifeq ($(CONFIG_MACH_MILLETLTE_OPEN),y)
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-sec-milletlte-r00.dtb
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-sec-milletlte-r01.dtb
else ifeq ($(CONFIG_MACH_MILLETWIFI_OPEN),y)
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-milletwifieur-r00.dtb
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-milletwifieur-r01.dtb
else ifeq ($(CONFIG_MACH_MATISSE3G_OPEN),y)
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-matisse3g-r00.dtb
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-matisse3g-r01.dtb
else ifeq ($(CONFIG_MACH_MATISSELTE_OPEN),y)
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-sec-matisselte-r00.dtb
else ifeq ($(CONFIG_MACH_BERLUTI3G_EUR),y)
	 dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sec-berluti3geur-r00.dtb
endif

# FSM9XXX
   zreladdr-$(CONFIG_ARCH_FSM9XXX)	:= 0x10008000
params_phys-$(CONFIG_ARCH_FSM9XXX)	:= 0x10000100
initrd_phys-$(CONFIG_ARCH_FSM9XXX)	:= 0x12000000

# FSM9900
   zreladdr-$(CONFIG_ARCH_FSM9900)	:= 0x00008000
        dtb-$(CONFIG_ARCH_FSM9900)	:= fsm9900-rumi.dtb
        dtb-$(CONFIG_ARCH_FSM9900)	:= fsm9900-sim.dtb

# MPQ8092
   zreladdr-$(CONFIG_ARCH_MPQ8092)	:= 0x00008000

# MSM8610
   zreladdr-$(CONFIG_ARCH_MSM8610)	:= 0x00008000
#   ifeq ($(CONFIG_SEC_HEAT_PROJECT),y)
#	ifeq ($(CONFIG_MACH_HEAT_DYN),y)
		dtb-$(CONFIG_SEC_HEAT_PROJECT)	+= msm8610-sec-heat-tfnvzw-r00.dtb
#	endif
#else
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-cdp.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-cdp.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-mtp.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-mtp.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-rumi.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-sim.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-qrd-skuaa.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v1-qrd-skuab.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-qrd-skuaa.dtb
#        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-v2-qrd-skuab.dtb
#endif
# MSMSAMARIUM
#   zreladdr-$(CONFIG_ARCH_MSMSAMARIUM)	:= 0x00008000
#	dtb-$(CONFIG_ARCH_MSMSAMARIUM)	+= msmsamarium-sim.dtb
#	dtb-$(CONFIG_ARCH_MSMSAMARIUM)	+= msmsamarium-rumi.dtb
