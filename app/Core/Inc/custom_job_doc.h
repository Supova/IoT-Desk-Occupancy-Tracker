/*
 * custom_job_doc.h
 *
 *  Created on: Apr 15, 2025
 *      Author: Shreyas Acharya, BHARATI SOFTWARE
 *  Modified by Supova: added file_checksum field for CRC32 verification
 */

#ifndef INC_CUSTOM_JOB_DOC_H_
#define INC_CUSTOM_JOB_DOC_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
	const char *image_ref;  // "streamID"
	uint32_t file_size;     // files[0].fileSize
	uint32_t file_checksum; // expected CRC32 from job document

	size_t image_ref_len;
	uint32_t file_id;
} custom_job_doc_fields_t;

#endif /* INC_CUSTOM_JOB_DOC_H_ */
