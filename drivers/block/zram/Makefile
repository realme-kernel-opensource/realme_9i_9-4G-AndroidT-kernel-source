# SPDX-License-Identifier: GPL-2.0-only
zram-y	:=	zcomp.o zram_drv.o
zram-$(CONFIG_ZRAM_DEDUP)	+=	zram_dedup.o

obj-$(CONFIG_ZRAM)	+=	zram.o
zram-$(CONFIG_HYBRIDSWAP) += hybridswap/hybridswap_main.o
zram-$(CONFIG_HYBRIDSWAP_SWAPD) += hybridswap/hybridswap_swapd.o
zram-$(CONFIG_HYBRIDSWAP_ASYNC_COMPRESS) += hybridswap/hybridswap_akcompress.o
zram-$(CONFIG_HYBRIDSWAP_CORE) += hybridswap/hybridswap_area.o hybridswap/hybridswap_core.o hybridswap/hybridswap_ctrl.o hybridswap/hybridswap_list.o hybridswap/hybridswap_lru_rmap.o hybridswap/hybridswap_manager.o hybridswap/hybridswap_perf.o hybridswap/hybridswap_schedule.o hybridswap/hybridswap_stats.o
