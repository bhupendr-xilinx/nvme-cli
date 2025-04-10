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
		//ENTRY("enp", "Submit an NVMe ENP admin passthrough command, return results", enp_admin_passthru)
	)
);

#endif

#include "define_cmd.h"
