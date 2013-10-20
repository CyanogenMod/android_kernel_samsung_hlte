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

# MSM8974
   zreladdr-$(CONFIG_ARCH_MSM8974)	:= 0x00008000
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v1-cdp.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v1-fluid.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v1-liquid.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v1-mtp.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v1-rumi.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v1-sim.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v2-cdp.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v2-fluid.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v2-liquid.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= msm8974-v2-mtp.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= apq8074-v2-cdp.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= apq8074-v2-liquid.dtb
#        dtb-$(CONFIG_ARCH_MSM8974)	+= apq8074-v2-dragonboard.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r00.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r01.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r02.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r03.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r04.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r05.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r06.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r07.dtb
         dtb-$(CONFIG_SEC_J_PROJECT)	+= msm8974-sec-ks01-r11.dtb

         dtb-$(CONFIG_SEC_JACTIVE_PROJECT)	+= msm8974-sec-jactive-r00.dtb

ifeq ($(CONFIG_SEC_H_PROJECT),y)
ifeq ($(CONFIG_SEC_LOCALE_JPN),y)
	dtb-y	+= msm8974-sec-hltejpn-r03.dtb
	dtb-y	+= msm8974-sec-hltejpn-r04.dtb
	dtb-y	+= msm8974-sec-hltejpn-r05.dtb
	dtb-y	+= msm8974-sec-hltejpn-r06.dtb
	dtb-y	+= msm8974-sec-hltejpn-r07.dtb
	dtb-y	+= msm8974-sec-hltejpn-r08.dtb
else
	ifeq ($(CONFIG_SEC_LOCALE_KOR),y)
		dtb-y	+= msm8974-sec-hltekor-r02.dtb
		dtb-y	+= msm8974-sec-hltekor-r03.dtb
		dtb-y	+= msm8974-sec-hltekor-r04.dtb
		dtb-y	+= msm8974-sec-hltekor-r05.dtb
		dtb-y	+= msm8974-sec-hltekor-r06.dtb
		dtb-y	+= msm8974-sec-hltekor-r07.dtb
	else
		ifeq ($(CONFIG_MACH_H3GDUOS),y)
			dtb-y	+= msm8974-sec-hlte-r02.dtb
			dtb-y	+= msm8974-sec-h3gchnduos-r03.dtb
			dtb-y	+= msm8974-sec-h3gchnduos-r04.dtb
			dtb-y	+= msm8974-sec-h3gchnduos-r05.dtb
			dtb-y	+= msm8974-sec-h3gchnduos-r06.dtb
			dtb-y	+= msm8974-sec-h3gchnduos-r07.dtb
			dtb-y	+= msm8974-sec-h3gchnduos-r08.dtb
		else
			dtb-y	+= msm8974-sec-hlte-r02.dtb
			dtb-y	+= msm8974-sec-hlte-r03.dtb
			dtb-y	+= msm8974-sec-hlte-r04.dtb
			dtb-y	+= msm8974-sec-hlte-r05.dtb
			dtb-y	+= msm8974-sec-hlte-r06.dtb
			dtb-y	+= msm8974-sec-hlte-r07.dtb
		endif
	endif
endif
endif
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-flte-r04.dtb
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-flte-r05.dtb
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-flte-r06.dtb
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-flte-r07.dtb
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-flte-r08.dtb

    # flte kor device tree
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-fltekor-r05.dtb
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-fltekor-r06.dtb
	dtb-$(CONFIG_SEC_F_PROJECT)	+= msm8974-sec-fltekor-r07.dtb

         dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-vienna-r00.dtb
         dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-vienna-r01.dtb
         dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-vienna-r02.dtb
         dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-vienna-r06.dtb
         dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-vienna-r08.dtb
         dtb-$(CONFIG_SEC_VIENNA_PROJECT)	+= msm8974-sec-vienna-r10.dtb
         dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03-r00.dtb
         dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03-r05.dtb
         dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03-r07.dtb
         dtb-$(CONFIG_SEC_LT03_PROJECT)	+= msm8974-sec-lt03kor-r02.dtb
         dtb-$(CONFIG_SEC_MONTBLANC_PROJECT)	+= msm8974-sec-montblanc-r00.dtb
         dtb-$(CONFIG_SEC_MONTBLANC_PROJECT)	+= msm8974-sec-montblanc-r01.dtb
         dtb-$(CONFIG_SEC_MONTBLANC_PROJECT)	+= msm8974-sec-montblanc-r02.dtb
         dtb-$(CONFIG_SEC_MONTBLANC_PROJECT)	+= msm8974-sec-montblanc-r03.dtb

		 dtb-$(CONFIG_SEC_MELIUSCA_PROJECT)	+= msm8974-sec-meliusca-r11.dtb
	  dtb-$(CONFIG_SEC_JS_PROJECT)	+= msm8974-sec-js01ltejpn-r04.dtb
	  dtb-$(CONFIG_SEC_JS_PROJECT)	+= msm8974-sec-js01ltejpn-r05.dtb
	  dtb-$(CONFIG_SEC_JS_PROJECT)	+= msm8974-sec-js01ltejpn-r06.dtb
	  dtb-$(CONFIG_SEC_JS_PROJECT)	+= msm8974-sec-js01ltejpn-r07.dtb
	  dtb-$(CONFIG_SEC_JS_PROJECT)	+= msm8974-sec-js01ltejpn-r08.dtb

	dtb-$(CONFIG_SEC_JVE_PROJECT)	+= msm8974-sec-jvelte-r08.dtb
	dtb-$(CONFIG_SEC_JVE_PROJECT)	+= msm8974-sec-jvelte-r09.dtb
	  
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
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-sim.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v1-qrd.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8226-v2-qrd.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-cdp.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-mtp.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= msm8926-qrd.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-xpm.dtb
        dtb-$(CONFIG_ARCH_MSM8226)	+= apq8026-mtp.dtb

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
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-rumi.dtb
        dtb-$(CONFIG_ARCH_MSM8610)	+= msm8610-sim.dtb

# MSMSAMARIUM
   zreladdr-$(CONFIG_ARCH_MSMSAMARIUM)	:= 0x00008000
	dtb-$(CONFIG_ARCH_MSMSAMARIUM)	+= msmsamarium-sim.dtb
	dtb-$(CONFIG_ARCH_MSMSAMARIUM)	+= msmsamarium-rumi.dtb
