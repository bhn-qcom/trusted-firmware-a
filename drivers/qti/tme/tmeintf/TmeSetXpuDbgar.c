/*===========================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/

/*
 * Programs a set of XPU DBGAR (debug access region) addresses via TME.
 */

#include <stddef.h>
#include <stdint.h>

#include "IxErrno.h"
#include "TmeInterfaces.h"
#include "TmeInterfacesDefs.h"
#include "TmeMessage.h"
#include "TmeMessagesTags.h"
#include "tmecom_interfaces.h"

int TmeSetXpuDbgar(uint32_t *dbgars, size_t count)
{
  int                 ret         = E_FAILURE;
  tmeSetXpuDbgarRsp_t rsp         = {0};
  size_t              responseLen = sizeof(rsp);

  CHECK_BAIL((dbgars != NULL) && (count > 0U));

  CHECK_BAIL(E_SUCCESS == TransceiveMessage(TME_MSG_CBOR_TAG_SET_XPU_DBG_AR,
                                            dbgars,
                                            count * sizeof(*dbgars),
                                            &rsp,
                                            sizeof(rsp),
                                            &responseLen));

  CHECK_BAIL(responseLen == sizeof(rsp));

  ret = (E_SUCCESS == rsp.status) ? E_SUCCESS : E_FAILURE;

bail:
  return ret;
}
