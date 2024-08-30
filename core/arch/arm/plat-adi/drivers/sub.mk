
srcs-y += rcu.c sec.c

srcs-$(CFG_ADI_SHARC_LOADER) += sharc.c
srcs-$(CFG_ADI_TRU) += tru.c

global-incdirs-y += .
