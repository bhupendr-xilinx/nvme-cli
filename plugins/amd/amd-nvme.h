/* SPDX-License-Identifier: GPL-2.0-or-later */
#undef CMD_INC_FILE
#define CMD_INC_FILE plugins/amd/amd-nvme

#if !defined(AMD_NVME) || defined(CMD_HEADER_MULTI_READ)
#define AMD_NVME

#include "cmd.h"

PLUGIN(NAME("amd", "AMD vendor specific extensions", NVME_VERSION),
	COMMAND_LIST(
		ENTRY("config-ae", "Submit a NVMe-MI Configuration set command for Async event, return results", config_ae)
		ENTRY("config-get", "Submit a NVMe-MI Configuration get command, return results", config_get)
		ENTRY("ctrl-primitive", "Submit a NVMe-MI Control primitive command, return results", ctrl_primitive)
		ENTRY("config-hs-change", "Submit a NVMe-MI Configuration set command for health Status change, return results", set_health_status_change)
		ENTRY("nvm-ss-hs-poll", "Submit a NVMe-MI NVM Subsystem health status poll command, return results", nvm_ss_hlth_stat_poll)
		ENTRY("ctrlr-hs-poll", "Submit a NVMe-MI Controller health status poll command, return results", ctrlr_hlth_stat_poll)
		ENTRY("reset", "Submit a NVMe-MI Reset command, return results", reset)
		ENTRY("shutdown", "Submit a NVMe-MI Shutdown command, return results", amd_shutdown)
		ENTRY("read-nvme-mi-data-structure", "Submit a NVMe-MI Read Data Structure command, return results", read_nvme_mi_data_structure)
	)
);

#endif

#include "define_cmd.h"
