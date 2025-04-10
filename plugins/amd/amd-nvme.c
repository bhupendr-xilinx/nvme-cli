// SPDX-License-Identifier: GPL-2.0-or-later
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h> 
#include <ctype.h> 
#include <sys/stat.h>
#include <sys/time.h>

#include "nvme.h"
#include "libnvme.h"
#include "plugin.h"
#include "linux/types.h"
#include "nvme-print.h"

#define CREATE_CMD
#include "amd-nvme.h"

enum nvme_mi_command {
	READ_NVME_MI_DATA_STRUCTURE,
	NVM_SUBSYSTEM_HEALTH_STATUS_POLL,
	CONTROLLER_HEALTH_STATUS_POLL,
	CONFIGURATION_SET,
	CONFIGURATION_GET,
	VPD_READ,
	VPD_WRITE,
	RESET,
	SES_RECEIVE,
	SES_SEND,
	MGMT_EP_BUFFER_READ,
	MGMT_EP_BUFFER_WRITE,
	SHUTDOWN,
};

typedef struct ae_supp_list_hdr_s {
	uint8_t num_ae_supp_ds;
	uint8_t ae_supp_list_ver_num;
	uint16_t ae_supp_total_len;
	uint8_t ae_supp_list_hdr_len;
} __attribute__((packed)) ae_supp_list_hdr_t;

typedef struct ae_supp_ds_s {
	uint8_t ae_supp_len;
	/* AE Supported Info */
	uint16_t ae_supp_id: 8;
	uint16_t rsvd: 7;
	uint16_t ae_supp_enable: 1;
} __attribute__((packed)) ae_supp_ds_t;

typedef struct ae_occ_uniq_id_s {
	uint8_t ae_occ_id;
	uint32_t ae_occ_scope_id_info;
	/* AE Occurrence Scope Info (AESSI) */
	uint8_t ae_occ_scope: 4;
	uint8_t rsvd: 4;
} __attribute__((packed)) ae_occ_uniq_id_t;

typedef struct ae_occ_list_hdr_s {
	uint8_t num_ae_occ_ds;
	uint8_t ae_occ_list_ver_num;
	/* AE Occurrence List Length Info (AEOLLI)*/
	uint32_t ae_occ_list_total_len: 23;
	uint32_t ae_occ_list_overflow: 1;
	uint32_t ae_occ_list_hdr_len: 8;
	/* AEM Transmission Info (AEMTI) */
	uint8_t aem_retry_count: 3;
	uint8_t aem_generation_number: 5;
} __attribute__((packed)) ae_occ_list_hdr_t;

typedef struct ae_occ_hdr_s {
	uint8_t ae_occ_hdr_len;
	uint8_t ae_occ_spec_info_len;
	uint8_t ae_occ_ven_spec_info_len;
	ae_occ_uniq_id_t ae_occ_uniq_id;
} __attribute__((packed)) ae_occ_hdr_t;

typedef struct ae_enable_list_hdr_s {
	uint8_t num_ae_enable_ds;
	uint8_t ae_enable_list_ver_num;
	uint16_t ae_enable_total_len;
	uint8_t ae_enable_list_hdr_len;
} __attribute__((packed)) ae_enable_list_hdr_t;

typedef struct ae_enable_ds_s {
	uint8_t ae_enable_len;
	/* AE Enable Info (AEEI) */
	uint8_t ae_enable_id;
	uint8_t rsvd: 7;
	uint8_t ae_enable: 1;
} __attribute__((packed)) ae_enable_ds_t;

typedef struct nvme_aem_hdr_s {
    struct nvme_mi_mi_resp_hdr mi_hdr;
    ae_occ_list_hdr_t *occ_list_hdr;
    ae_occ_hdr_t *occ_hdr;
} __attribute__((packed)) nvme_aem_hdr_t;

/* Asynchronous event identifier */
typedef enum ae_id_e {
	AE_CONTROLLER_READY,
	AE_CONTROLLER_FATAL_STATUS,
	AE_SHUTDOWN_STATUS,
	AE_CONTROLLER_ENABLE,
	AE_NAMESPACE_ATTR_CHANGED,
	AE_FIRMWARE_ACTIVATED,
	AE_COMPOSITE_TEMPERATURE,
	AE_PERCENT_DRIVE_LIFE_USED,
	AE_AVAILABLE_SPARE,
	AE_SMART_WARNINGS,
	AE_TELEMETRY_CTRL_INIT_DATA_AVL,
	AE_PCIE_LINK_ACTIVE,
	AE_SANITIZE_FAILURE_MODE,
	/* Vendor defined async events */
	AE_INCOMPATIBLE_BACKEND_SSD = 0xC0,
	AE_BACKEND_SSD_ASYNC_EVENT,
	AE_BACKEND_SSD_IP_CONFIG_MISMATCH,
	AE_BACKEND_SSD_SURPRISE_REMOVAL,
	AE_BACKEND_SSD_SURPRISE_ADDITION,
	AE_BACKEND_SSD_CMD_TIME_OUT,
	AE_BACKEND_SSD_CMD_COMP_WITH_ERROR,
	AE_BACKEND_SSD_RESET_BY_INTERPOSER,
	AE_BACKEND_SSD_PANIC_RESET_ACTION_SUCCESS,
	AE_BACKEND_SSD_PANIC_RESET_ACTION_FAILURE,
	AE_BACKEND_SSD_POWER_CYCLE_REQUESTED,
	AE_BACKEND_SSD_FAILURE_DEC_BY_INTERPOSER,
	AE_BACKEND_SSD_INVALID_POWER_ON_IP_CONFIG,
	AE_EXTENT_MGMT_EXTENT_FILL_THRESHOLD_FOR_SSD,
	AE_EXTENT_MGMT_NO_FREE_EXTENTS_ON_SSD,
	AE_MIRR_NAMESPACES_RESYNC_STARTED,
	AE_MIRR_NAMESPACES_RESYNC_FINISHED,
	AE_MIRR_NAMESPACES_DEGRADED_SSD_FAILURE,
	AE_MIRR_NAMESPACES_RESILVER_STARTED,
	AE_MIRR_NAMESPACES_RESILVER_COMPLETED,
	AE_IO_CONTROLLER_DISABLED,
	AE_NAMESPACE_DISABLED,
	AE_INTERPOSER_INIT_NVM_SUBSYS_RESET,
} ae_id_t;

typedef enum ae_occ_scope_e {
	AE_OCC_SCOPE_NAMESPACE,
	AE_OCC_SCOPE_CONTROLLER,
	AE_OCC_SCOPE_NVM_SUBSYS,
	AE_OCC_SCOPE_MGMT_EP,
	AE_OCC_SCOPE_PORT,
	AE_OCC_SCOPE_EG
} ae_occ_scope_t;

/**
 * enum nvme_mi_resp_status_e - NVME-MI response status field.
 */
typedef enum nvme_mi_resp_status_e {
	STATUS_SUCCESS,
	STATUS_MORE_PROCESSING_REQUIRED,
	STATUS_INTERNAL_ERROR,
	STATUS_INVALID_COMMAND_OPCODE,
	STATUS_INVALID_PARAMETER,
	STATUS_INVALID_COMMAND_SIZE,
	STATUS_INVALID_COMMAND_INPUT_DATA_SIZE,
	STATUS_ACCESS_DENIED,
	STATUS_VPD_UPDATES_EXCEEDED = 0x20,
	STATUS_PCIE_INACCESSIBLE,
	STATUS_OPCODE_NOT_SUPPORTED = 0xE0,
	STATUS_FEATURE_NOT_SUPPORTED,
	STATUS_NOT_IMPLEMENTED,
	STATUS_NOT_SUPPORTED,
	STATUS_RESET_IN_PROGRESS,
	STATUS_INTERPOSER_NOT_CONFIGURED,
} nvme_mi_resp_status_t;

struct ae_config {
    char *enable_aeid_list;
	char *disable_aeid_list;
	__u8 aerd;
	__u8 aemd;
	__u8 numaee;
	__u8 aeelver;
	__u8 aeetl;
	__u8 aeelhl;
	__u8 csi;
};

struct get_config {
    __u8 cid;
	__u8 pid;
	__u8 csi;
};

int parse_mi_resp_status(uint8_t status, uint8_t *resp_buffer) {
	int rc = -1;
	switch (status) {
		case STATUS_SUCCESS:
			printf("Status: Success\n");
			break;
		case STATUS_MORE_PROCESSING_REQUIRED:
			printf("Status: More Processing Required\n");
			break;
		case STATUS_INTERNAL_ERROR:
			printf("Status: Internal Error\n");
			break;
		case STATUS_INVALID_COMMAND_OPCODE:
			printf("Status: Invalid Command Opcode\n");
			break;
		case STATUS_INVALID_PARAMETER:
			printf("Status: Invalid Parameter, parameter error locations are:\n");
			printf("Invalid Parameter Bit location(BITLOC): %02x\n", resp_buffer[0] & 0x07);
			printf("Invalid Parameter Byte location(BYTLOC): %04x\n", (resp_buffer[1] << 8) | resp_buffer[2]);
			break;
		case STATUS_INVALID_COMMAND_SIZE:
			printf("Status: Invalid Command Size\n");
			break;
		case STATUS_INVALID_COMMAND_INPUT_DATA_SIZE:
			printf("Status: Invalid Command Input Data Size\n");
			break;
		case STATUS_ACCESS_DENIED:
			printf("Status: Access Denied\n");
			break;
		case STATUS_VPD_UPDATES_EXCEEDED:
			printf("Status: VPD Updates Exceeded\n");
			break;
		case STATUS_PCIE_INACCESSIBLE:
			printf("Status: PCIe Inaccessible\n");
			break;
		case STATUS_OPCODE_NOT_SUPPORTED:
			printf("Status: Opcode Not Supported\n");
			break;
		case STATUS_FEATURE_NOT_SUPPORTED:
			printf("Status: Feature Not Supported\n");
			break;
		case STATUS_NOT_IMPLEMENTED:
			printf("Status: Not Implemented\n");
			break;
		case STATUS_NOT_SUPPORTED:
			printf("Status: Not Supported\n");
			rc = 1;
			break;
		case STATUS_RESET_IN_PROGRESS:
			printf("Status: Reset In Progress\n");
			break;
		case STATUS_INTERPOSER_NOT_CONFIGURED:
			printf("Status: Interposer Not Configured\n");
			break;
		default:
			printf("Status: Unknown\n");
			break;
	}
	return rc;
}

void parse_ae_occ_scope (uint32_t ae_occ_scope_id_info, uint8_t ae_occ_scope) {
	switch (ae_occ_scope) {
		case AE_OCC_SCOPE_NAMESPACE:
			printf("  AE Occurrence Scope: Namespace\n");
			printf("  AE Occurrence Namespace ID (AEONSID): %08x\n", ae_occ_scope_id_info);
			break;
		case AE_OCC_SCOPE_CONTROLLER:
			printf("  AE Occurrence Scope: Controller\n");
			printf("  AE Occurrence Controller ID (AEOCID): %04x\n", ae_occ_scope_id_info & 0xFFFF);
			break;
		case AE_OCC_SCOPE_NVM_SUBSYS:
			printf("  AE Occurrence Scope: NVM Subsystem\n");
			break;
		case AE_OCC_SCOPE_MGMT_EP:
			printf("  AE Occurrence Scope: Management Endpoint\n");
			printf("  AE Occurrence Management Endpoint ID (AEOMEID): %02x\n", ae_occ_scope_id_info & 0xFF);
			break;
		case AE_OCC_SCOPE_PORT:
			printf("  AE Occurrence Scope: Port\n");
			printf("  AE Occurrence Port ID (AEOPID): %02x\n", ae_occ_scope_id_info & 0xFFFF);
			printf("  AE Occurrence Port Type (AEOPT): %d\n", (ae_occ_scope_id_info >> 16) & 0x1);
			break;
		case AE_OCC_SCOPE_EG:
			printf("  AE Occurrence Scope: Event Group\n");
			printf("  AE Occurrence Endurance Group ID (AEOEGID): %04x\n", ae_occ_scope_id_info & 0xFFFF);
			break;
		default:
			printf("  AE Occurrence Scope: Unknown\n");
			break;
	}
}

void get_ae_occ_ven_spec_info(uint8_t *ae_occ_ven_spec_info) {
	printf("  AE Occurrence Specific Info: ");
	for (int i = 6; i >= 0; i--) {
    	printf("%02x", ae_occ_ven_spec_info[i]);
	}
	printf("\n");
}

void parse_ae_occ_ven_spec_info(uint8_t ae_occ_id, 
								uint32_t ae_occ_scope_id_info, 
								uint8_t ae_occ_scope, 
								uint8_t *ae_occ_ven_spec_info) {
	printf("Parsing AE Occurrence Vendor Specific Info:\n");
	parse_ae_occ_scope(ae_occ_scope_id_info, ae_occ_scope);
	printf("  AE Occurrence ID: %02x\n", ae_occ_id);
	switch (ae_occ_id) {
		case AE_CONTROLLER_READY:
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_CONTROLLER_FATAL_STATUS:
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_CONTROLLER_ENABLE:
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_INCOMPATIBLE_BACKEND_SSD:
			printf("  AE Occurrence ID: Incompatible Backend SSD\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_ASYNC_EVENT:
			printf("  AE Occurrence ID: Backend SSD Async Event\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_IP_CONFIG_MISMATCH:
			printf("  AE Occurrence ID: Backend SSD IP Config Mismatch\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_SURPRISE_REMOVAL:
			printf("  AE Occurrence ID: Backend SSD Surprise Removal\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_SURPRISE_ADDITION:
			printf("  AE Occurrence ID: Backend SSD Surprise Addition\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_CMD_TIME_OUT:
			printf("  AE Occurrence ID: Backend SSD Command Timeout\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_CMD_COMP_WITH_ERROR:
			printf("  AE Occurrence ID: Backend SSD Command Completion with Error\n");
			break;
		case AE_BACKEND_SSD_RESET_BY_INTERPOSER:
			printf("  AE Occurrence ID: Backend SSD Reset by Interposer\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_PANIC_RESET_ACTION_SUCCESS:
			printf("  AE Occurrence ID: Backend SSD Panic Reset Action Success\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_PANIC_RESET_ACTION_FAILURE:
			printf("  AE Occurrence ID: Backend SSD Panic Reset Action Failure\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_POWER_CYCLE_REQUESTED:
			printf("  AE Occurrence ID: Backend SSD Power Cycle Requested\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_FAILURE_DEC_BY_INTERPOSER:
			printf("  AE Occurrence ID: Backend SSD Failure Declared by Interposer\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_EXTENT_MGMT_EXTENT_FILL_THRESHOLD_FOR_SSD:
			printf("  AE Occurrence ID: Extent Management Extent Fill Threshold for SSD\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_EXTENT_MGMT_NO_FREE_EXTENTS_ON_SSD:
			printf("  AE Occurrence ID: Extent Management No Free Extents on SSD\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_RESYNC_STARTED:
			printf("  AE Occurrence ID: Mirror Namespaces Resync Started\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);	
			break;
		case AE_MIRR_NAMESPACES_RESYNC_FINISHED:
			printf("  AE Occurrence ID: Mirror Namespaces Resync Finished\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_DEGRADED_SSD_FAILURE:
			printf("  AE Occurrence ID: Mirror Namespaces Degraded SSD Failure\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_RESILVER_STARTED:
			printf("  AE Occurrence ID: Mirror Namespaces Resilver Started\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_RESILVER_COMPLETED:
			printf("  AE Occurrence ID: Mirror Namespaces Resilver Completed\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_IO_CONTROLLER_DISABLED:
			printf("  AE Occurrence ID: IO Controller Disabled\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_NAMESPACE_DISABLED:
			printf("  AE Occurrence ID: Namespace Disabled\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_INTERPOSER_INIT_NVM_SUBSYS_RESET:
			printf("  AE Occurrence ID: Interposer Init NVM Subsystem Reset\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		default:
			printf("  AE Occurrence ID: Unknown\n");
			break;
	}
	
}

void parse_ae_occ_spec_info(uint8_t ae_occ_id, 
							uint32_t ae_occ_scope_id_info, 
							uint8_t ae_occ_scope, 
							uint8_t ae_occ_spec_info) {
	printf("Parsing AE Occurrence Specific Info:\n");
	parse_ae_occ_scope(ae_occ_scope_id_info, ae_occ_scope);
	printf("  AE Occurrence ID: %02x\n", ae_occ_id);
	switch (ae_occ_id) {	
		case AE_SHUTDOWN_STATUS:
			printf("  Shutdown Status Value (SSV): %d\n", ae_occ_spec_info & 0x3);
			printf("  Shutdown Type Value (STV): %d\n", (ae_occ_spec_info >> 2) & 0x1);
			break;
		case AE_NAMESPACE_ATTR_CHANGED:
			break;
		case AE_FIRMWARE_ACTIVATED:
			printf("  Firmware Slot: %d\n", ae_occ_spec_info & 0x3);
			printf("  Next Firmware Slot Activated at Controller Reset: %d\n", (ae_occ_spec_info >> 4) & 0x3);
			break;
		case AE_COMPOSITE_TEMPERATURE:
			printf("  Composite Temperature Value (CTV): %d\n", ae_occ_spec_info);
			break;
		case AE_PERCENT_DRIVE_LIFE_USED:
			printf("  Max Drive Life Used Change: %d\n", ae_occ_spec_info);
			break;
		case AE_AVAILABLE_SPARE:
			printf("  Min Available Spare (MAS): %d\n", ae_occ_spec_info);
			break;
		case AE_SMART_WARNINGS:
			printf("  SMART Warnings Value (CWV): %d\n", ae_occ_spec_info);
			break;
		case AE_TELEMETRY_CTRL_INIT_DATA_AVL:
			printf("  AE Occurrence ID: Telemetry Controller Init Data Available (ID: %02x)\n", ae_occ_id);
			break;
		case AE_PCIE_LINK_ACTIVE:
			printf("  AE Occurrence ID: PCIe Link Active (ID: %02x)\n", ae_occ_id);
			break;
		case AE_SANITIZE_FAILURE_MODE:
			printf("  AE Occurrence ID: Sanitize Failure Mode (ID: %02x)\n", ae_occ_id);
			break;
		default:
			printf("  AE Occurrence ID: Unknown (ID: %02x)\n", ae_occ_id);
			break;
	}
	
}

static int nvme_amd_config_get(struct nvme_dev *dev, __u8 cid, __u8 pid, __u8 csi)
{
	struct nvme_mi_mi_req_hdr mi_req = {0};
	struct {
		struct nvme_mi_mi_resp_hdr mi_hdr;
		char buffer[4096];
	} resp = { 0 };

	size_t len = 4096;
	int rc = 0;

	mi_req.opcode = 4;
	mi_req.cdw0 = ((uint32_t)cid) | ((uint32_t)pid << 24);
	rc = nvme_mi_mi_xfer(dev->mi.ep, &mi_req, 0, &resp.mi_hdr, &len, csi);
	assert(rc == 0);

	printf("\nSTATUS:%d \n",resp.mi_hdr.status);
	if (cid == 4) {
		struct {
			ae_supp_list_hdr_t *hdr;
			ae_supp_ds_t *ds;
		}ae_resp = {0};

		printf("AEELVER:%d \n",resp.mi_hdr.nmresp[0]);
		ae_resp.hdr = (ae_supp_list_hdr_t *)resp.buffer;
		ae_resp.ds = (ae_supp_ds_t *)(resp.buffer + sizeof(ae_supp_list_hdr_t));
    	
		// Access the structure fields
    	printf("Number of AE Supported DS (NUMAES): %u\n", ae_resp.hdr->num_ae_supp_ds);
    	printf("AE Supported List Version Number (AESLVER): %u\n", ae_resp.hdr->ae_supp_list_ver_num);
    	printf("AE Supported Total Length (AESTL): %u\n", ae_resp.hdr->ae_supp_total_len);
    	printf("AE Supported List Header Length (AESLHL): %u\n", ae_resp.hdr->ae_supp_list_hdr_len);
		printf("********************************************************\n");
		printf("               AE Data Structures to follow             \n");
		printf("********************************************************\n");

		// Loop through each ae_supp_ds structure and print their values
    	for (int i = 0; i < ae_resp.hdr->num_ae_supp_ds; i++) {
        	// Access the structure fields
			printf("********************************************************\n");
        	printf("AE Supported ID (AESI): %u\n", ae_resp.ds->ae_supp_id);
        	printf("AE Supported Enable (AESE): %u\n", ae_resp.ds->ae_supp_enable);
        	printf("AE Supported Length (AESL): %u\n", ae_resp.ds->ae_supp_len);
        	printf("Reserved (RSVD): %u\n", ae_resp.ds->rsvd);
			printf("********************************************************\n");

        	// Move the pointer to the next ae_supp_ds structure
			ae_resp.ds++;
    	}
	} else if (cid == 3) {
		printf("MTU: %d for PORT ID: %d \n",resp.mi_hdr.nmresp[0], pid);
	} else {
		printf("SFREQ: %d for PORT ID: %d \n",resp.mi_hdr.nmresp[0], pid);
	}
	return 0;
}

static int nvme_amd_set_ae(struct nvme_dev *dev, __u8 *enable_list, __u8 *disable_list,
						   __u8 aerd, __u8 aemd, __u8 numaee, __u8 aeelver,
						   __u8 aeetl, __u8 aeelhl, __u8 csi)
{
	struct {
		struct nvme_mi_mi_resp_hdr resp_hdr;
		uint8_t resp_buf[4096];
	} resp = { 0 };

	struct {
		struct nvme_mi_mi_req_hdr req_hdr;
		ae_enable_list_hdr_t aeelh;
		ae_enable_ds_t ae_en_info[35];
	} req = { 0 };

	struct {
		ae_occ_list_hdr_t *occ_list_hdr;
    	ae_occ_hdr_t *occ_hdr;
	} ae_resp = {0};

	uint8_t cid = 4, msg_size = 5;
	size_t len = 4096;
	int rc = 0;
	uint8_t *ae_occ_spec_info;
	uint8_t *ae_occ_ven_spec_info;

	//Opcode for Configuration set
	req.req_hdr.opcode = 3;

	//store cid, retry delay and delay field
	req.req_hdr.cdw0 = ((uint32_t)cid) | ((uint32_t)aerd << 8) | ((uint32_t)aemd << 16);
	printf("cdw0 field value is: %06x\n",req.req_hdr.cdw0);

	//number of AE's to be enabled/disabled
	req.aeelh.num_ae_enable_ds = numaee;
	printf("NUMAEE:: 0x%x\n",req.aeelh.num_ae_enable_ds);
	//version number for testing purpose only
	if (aeelver != 0) {
		req.aeelh.ae_enable_list_ver_num = aeelver;
	}

	//If user defined total length is available then only store it (Only for Testing purpose)
	//otherwise it is based on number of ae's.
	if (aeetl != 0) {
		req.aeelh.ae_enable_total_len = aeetl;
	} else {
		req.aeelh.ae_enable_total_len = (req.aeelh.num_ae_enable_ds * sizeof(ae_enable_ds_t)) + sizeof(ae_enable_list_hdr_t);
	}

	printf(" ae_enable_total_len:: %d\n",req.aeelh.ae_enable_total_len);

	//If user defined header length is available then only store it (Only for Testing purpose)
	//otherwise it is fixed.
	if (aeelhl != 0) {
		req.aeelh.ae_enable_list_hdr_len = aeelhl;
	} else {
		req.aeelh.ae_enable_list_hdr_len = sizeof(ae_enable_list_hdr_t);
	}
	printf(" ae_enable_list_hdr_len:: %d\n",req.aeelh.ae_enable_list_hdr_len);

	// Store elements in enable_list
	int i = 0;
	while (enable_list[i] != 0 && i < sizeof(enable_list)) {
		req.ae_en_info[i].ae_enable_len = 3;
		req.ae_en_info[i].ae_enable_id = enable_list[i];
		req.ae_en_info[i].ae_enable = 1;
		i++;
	}
	// Store elements in disable_list
	i = 0;
	while (disable_list[i] != 0 && i < sizeof(disable_list)) {
		req.ae_en_info[i].ae_enable_len = 3;
		req.ae_en_info[i].ae_enable_id = disable_list[i];
		req.ae_en_info[i].ae_enable = 0;
		i++;
	}
	//Message size to be sent 
	msg_size = (msg_size + (numaee * sizeof(ae_enable_ds_t)));
	//Send the message for ACK size = 5 if numaee==0 else size = 5 + (numaee * 3)
	rc = nvme_mi_mi_xfer(dev->mi.ep, &req.req_hdr, msg_size, &resp.resp_hdr, &len, csi);
	assert(rc == 0);

	if (resp.resp_hdr.status) {
		int status = parse_mi_resp_status(resp.resp_hdr.status, resp.resp_buf);
		if (status != 1) return -1;
	}
	ae_resp.occ_list_hdr = (ae_occ_list_hdr_t *)resp.resp_buf;
	ae_resp.occ_hdr = (ae_occ_hdr_t *)(resp.resp_buf + sizeof(ae_occ_list_hdr_t));
	
	// Print the fields of ae_occ_list_hdr_t
	printf("ae_occ_list_hdr_t:\n");
	printf("num_ae_occ_ds   (NUMAEO): %02x\n", ae_resp.occ_list_hdr->num_ae_occ_ds);
	printf("ae_occ_list_ver_num (AELVER): %02x\n", ae_resp.occ_list_hdr->ae_occ_list_ver_num);
	printf("ae_occ_list_total_len (AEOLTL): %06x\n", ae_resp.occ_list_hdr->ae_occ_list_total_len);
	printf("ae_occ_list_overflow (AEOLO): %01x\n", ae_resp.occ_list_hdr->ae_occ_list_overflow);
	printf("ae_occ_list_hdr_len (AEOLHL): %02x\n", ae_resp.occ_list_hdr->ae_occ_list_hdr_len);
	printf("aem_retry_count (AEMRC): %01x\n", ae_resp.occ_list_hdr->aem_retry_count);
	printf("aem_generation_number (AEMGN): %01x\n", ae_resp.occ_list_hdr->aem_generation_number);
	for (int i = 1; i <= ae_resp.occ_list_hdr->num_ae_occ_ds; i++ ) {
		// Print the fields of ae_occ_hdr_t
	    printf("ae_occ_hdr_t:\n");
		printf("ae_occ_hdr_len (AELHLEN): %02x\n", ae_resp.occ_hdr->ae_occ_hdr_len);
		printf("ae_occ_spec_info_len (AEOSIL): %02x\n", ae_resp.occ_hdr->ae_occ_spec_info_len);
		printf("ae_occ_ven_spec_info_len (AEOVSIL): %02x\n", ae_resp.occ_hdr->ae_occ_ven_spec_info_len);
		// Print the fields of ae_occ_uniq_id
		printf("ae_occ_uniq_id_t:\n");
		printf("ae_occ_id (AEOI): %02x\n", ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_id);
		printf("ae_occ_scope_id_info (AEOCIDI): %08x\n", ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope_id_info);
		printf("ae_occ_scope (AESS): %01x\n", ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope);
		printf("rsvd: %02x\n", ae_resp.occ_hdr->ae_occ_uniq_id.rsvd);
		ae_occ_spec_info = (uint8_t*)(ae_resp.occ_hdr);
		ae_occ_spec_info = ae_occ_spec_info + 9;
				
		if (ae_resp.occ_hdr->ae_occ_spec_info_len && !(ae_resp.occ_hdr->ae_occ_ven_spec_info_len)) {
			parse_ae_occ_spec_info(ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_id, 
								   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope_id_info, 
								   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope, 
								   *(ae_occ_spec_info));
		} else if (!(ae_resp.occ_hdr->ae_occ_spec_info_len) && ae_resp.occ_hdr->ae_occ_ven_spec_info_len) {
			parse_ae_occ_ven_spec_info(ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_id, 
									   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope_id_info, 
									   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope, 
									   ae_occ_spec_info);
		} else if ((ae_resp.occ_hdr->ae_occ_spec_info_len) && (ae_resp.occ_hdr->ae_occ_ven_spec_info_len)) {
			   printf("check if occ_hdr->ae_occ_uniq_id.ae_occ_id is <C0-FF>, if yes then raise error\n");
		} else {printf("No AE occurance data structure present\n");}
		if (ae_resp.occ_hdr->ae_occ_spec_info_len){
			ae_occ_spec_info = ++ae_occ_spec_info;
		} else {
			ae_occ_spec_info += 7;
		}
		ae_resp.occ_hdr = ae_occ_spec_info;
	}
		
	return 0;
}

static int config_ae(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    const char *desc =(
			"Send a Configuration set NVMe-MI command to enable/disable Async Event.\n"
			"Return results.\n");

    const char *aerd = "Retry Delay (required)";
	const char *aemd = "Delay";
	const char *numaee = "Number of AE to be configured";
    const char *enable_aeid_list = "Comma seprated event ID List to be enabled";
	const char *disable_aeid_list = "Comma seprated event ID List to be disabled";
	const char *aeelver = "AE Event List Version";
	const char *aeetl = "AE Event List Total length";
	const char *aeelhl = "AE Event List Header length";
	const char *csi = "Command slot identifier";

	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	__u32 result;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	__u8 enable_list[34] = {0};
	__u8 disable_list[34] = {0};

	struct ae_config cfg = {
		.enable_aeid_list = "",
		.disable_aeid_list = "",
		.aerd = 0,
		.aemd = 0,
		.numaee = 0,
		.aeelver = 0,
		.aeetl = 0,
		.aeelhl = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		  OPT_UINT("aerd", 'r', &cfg.aerd, aerd),
		  OPT_UINT("aemd", 'd', &cfg.aemd, aemd),
		  OPT_UINT("numaee", 'n', &cfg.numaee, numaee),
		  OPT_LIST("enable_aeid_list", 'e', &cfg.enable_aeid_list, enable_aeid_list),
		  OPT_LIST("disable_aeid_list", 'D', &cfg.disable_aeid_list, disable_aeid_list),
		  OPT_UINT("aeelver", 'v', &cfg.aeelver, aeelver),
		  OPT_UINT("aeetl", 't', &cfg.aeetl, aeetl),
		  OPT_UINT("aeelhl", 'h', &cfg.aeelhl, aeelhl),
		  OPT_UINT("csi", 'c', &cfg.csi, csi),
		  OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);
	if (cfg.numaee != 0) {
		if (strlen(cfg.enable_aeid_list) > 0) {
			status = argconfig_parse_comma_sep_array_u8(cfg.enable_aeid_list,
									  enable_list,
									  sizeof(enable_list));
			if (status < 0) {
				nvme_show_error("%s: %s", __func__, nvme_strerror(errno));
				return status;
			}
		}

		if (strlen(cfg.disable_aeid_list) > 0) {
			status = argconfig_parse_comma_sep_array_u8(cfg.disable_aeid_list,
								  disable_list,
								  sizeof(disable_list));
			if (status < 0) {
				nvme_show_error("%s: %s", __func__, nvme_strerror(errno));
				return status;
			}
		}
		if (strlen(cfg.enable_aeid_list) == 0 && strlen(cfg.disable_aeid_list) == 0) {
			nvme_show_error("%s: Both enable and disable AEID lists are empty", __func__);
			return -EINVAL;
		}
	}
	err = nvme_amd_set_ae(dev, enable_list, disable_list,
						 cfg.aerd, cfg.aemd,
						 cfg.numaee, cfg.aeelver,
						 cfg.aeetl, cfg.aeelhl,
						 cfg.csi);

	gettimeofday(&end_time, NULL);
	
	return err;
}

static int config_get(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    const char *desc =(
			"Send a Configuration get NVMe-MI command to retrieve Async Event status.\n"
			"Return results."
		);

    const char *cid = "Configuration identifier (required)";
	const char *pid = "Port ID";
	
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	__u32 result;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	
	struct get_config cfg = {
		.cid = 0,
		.pid = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		  OPT_UINT("cid", 'c', &cfg.cid, cid),
		  OPT_UINT("pid", 'p', &cfg.pid, pid),
		  OPT_UINT("csi", 'c', &cfg.csi, csi),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);

	err = nvme_amd_config_get(dev, cfg.cid, cfg.pid, cfg.csi);
						 
	gettimeofday(&end_time, NULL);
	
	return err;
}

static int ctrl_primitive(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    const char *desc =(
			"Send a Control primitive NVMe-MI command[abort|pause|resume|get-state|replay].\n"
			"Return results."
		);

    const char *action = "Control primitive action to be taken (required)";
	
	
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	__u32 result;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	int rc = 0;
	uint8_t opcode = 0xff;
	uint16_t cpsr;
	
	struct config {
		char		*action;
	};

	struct config cfg = {
		.action = "",
	};

	OPT_ARGS(opts) = {
		  OPT_STR("action", 'a', &cfg.action, action),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);

	

	static const struct {
		const char *name;
		uint8_t opcode;
	} control_actions[] = {
		{ "pause", nvme_mi_control_opcode_pause },
		{ "resume", nvme_mi_control_opcode_resume },
		{ "abort", nvme_mi_control_opcode_abort },
		{ "get-state", nvme_mi_control_opcode_get_state },
		{ "replay", nvme_mi_control_opcode_replay },
	};

	static const char * const slot_state[] = {
		"Idle",
		"Receive",
		"Process",
		"Transmit",
	};

	static const char * const cpas_state[] = {
		"Command aborted after processing completed or no command to abort",
		"Command aborted before processing began",
		"Command processing partially completed",
		"Reserved",
	};

	for (int i = 0; i < 5; i++) {
		if (!strcmp(cfg.action, control_actions[i].name)) {
			opcode = control_actions[i].opcode;
			break;
		}
	}

	if (opcode == 0xff) {
		fprintf(stderr, "invalid action specified: %s\n", action);
		return -1;
	}

	rc = nvme_mi_control(dev->mi.ep, opcode, 0, &cpsr); /* cpsp reserved in example */
	if (rc) {
		warn("can't perform primitive control command");
		return -1;
	}

	printf("NVMe control primitive\n");
	switch (opcode) {
	case nvme_mi_control_opcode_pause:
		printf(" Pause : cspr is %#x\n", cpsr);
		printf("  Pause Flag Status Slot 0: %s\n", (cpsr & (1 << 0)) ? "Yes" : "No");
		printf("  Pause Flag Status Slot 1: %s\n", (cpsr & (1 << 1)) ? "Yes" : "No");
		break;
	case nvme_mi_control_opcode_resume:
		printf(" Resume : cspr is %#x\n", cpsr);
		break;
	case nvme_mi_control_opcode_abort:
		printf(" Abort : cspr is %#x\n", cpsr);
		printf("  Command Aborted Status: %s\n", cpas_state[cpsr & 0x3]);
		break;
	case nvme_mi_control_opcode_get_state:
		printf(" Get State : cspr is %#x\n", cpsr);
		printf("  Slot Command Servicing State: %s\n", slot_state[cpsr & 0x3]);
		printf("  Bad Message Integrity Check: %s\n", (cpsr & (1 << 4)) ? "Yes" : "No");
		printf("  Timeout Waiting for a Packet: %s\n", (cpsr & (1 << 5)) ? "Yes" : "No");
		printf("  Unsupported Transmission Unit: %s\n", (cpsr & (1 << 6)) ? "Yes" : "No");
		printf("  Bad Header Version: %s\n", (cpsr & (1 << 7)) ? "Yes" : "No");
		printf("  Unknown Destination ID: %s\n", (cpsr & (1 << 8)) ? "Yes" : "No");
		printf("  Incorrect Transmission Unit: %s\n", (cpsr & (1 << 9)) ? "Yes" : "No");
		printf("  Unexpected Middle or End of Packet: %s\n", (cpsr & (1 << 10)) ? "Yes" : "No");
		printf("  Out-of-Sequence Packet Sequence Number: %s\n", (cpsr & (1 << 11)) ? "Yes" : "No");
		printf("  Bad, Unexpected, or Expired Message Tag: %s\n", (cpsr & (1 << 12)) ? "Yes" : "No");
		printf("  Bad Packet or Other Physical Layer: %s\n", (cpsr & (1 << 13)) ? "Yes" : "No");
		printf("  NVM Subsystem Reset Occurred: %s\n", (cpsr & (1 << 14)) ? "Yes" : "No");
		printf("  Pause Flag: %s\n", (cpsr & (1 << 15)) ? "Yes" : "No");
		break;
	case nvme_mi_control_opcode_replay:
		printf(" Replay : cspr is %#x\n", cpsr);
		break;
	default:
		/* unreachable */
		break;
	}
	//err = nvme_amd_config_get(dev, cfg.cid, cfg.pid);
						 
	gettimeofday(&end_time, NULL);
	
	return err;
}

