/*===========================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/

/*
 * Retrieves the list of software image IDs signed by a given signing
 * authority, over TMECOM.
 */

#include <stddef.h>
#include <stdint.h>

#include "IxErrno.h"
#include "TmeInterfaces.h"
#include "TmeInterfacesDefs.h"
#include "TmeMessage.h"
#include "TmeMessagesTags.h"
#include "tmecom_interfaces.h"

int TmeGetSignedImageIds(tmeSoftwareRootCaIds signingAuthority,
                        uint32_t            *outputSwIds,
                        size_t               outputSwIdMax,
                        size_t              *outputSwIdCount)
{
  int                 ret         = E_FAILURE;
  size_t              responseLen = 0;
  tmeSignedSwIdsReq_t request     = {.signingAuthority = (uint32_t)signingAuthority};
  tmeSignedSwIdsRsp_t response    = {0};

  do
  {
    if ((outputSwIds == NULL) || (outputSwIdMax == 0U) || (outputSwIdCount == NULL))
    {
      ret = E_INVALID_ARG;
      break;
    }

    if (E_SUCCESS != TransceiveMessage(TME_MSG_CBOR_TAG_GET_SIGNED_IMAGE_IDS,
                                       &request,
                                       sizeof(request),
                                       &response,
                                       sizeof(response),
                                       &responseLen))
    {
      break;
    }

    if (responseLen != sizeof(response))
    {
      break;
    }

    if (E_SUCCESS != response.status)
    {
      break;
    }

    if (response.swIdCount > outputSwIdMax)
    {
      ret = E_DATA_TOO_LARGE;
      break;
    }

    for (size_t swId = 0U; swId < response.swIdCount; ++swId)
    {
      outputSwIds[swId] = response.swIds[swId];
    }

    *outputSwIdCount = response.swIdCount;
    ret              = E_SUCCESS;
  } while (0);

  return ret;
}
