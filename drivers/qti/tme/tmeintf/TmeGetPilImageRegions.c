/*===========================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/

/*
 * PIL (Peripheral Image Loader) image region query over TMECOM.
 *
 * Copies only the entries TME actually reported (response.regionListCount,
 * bounds-checked against the caller's capacity below) into regionList, so a
 * caller-supplied buffer smaller than TMECOM_PIL_IMAGES_MAX_REGIONS cannot be
 * overflowed.
 */

#include <stddef.h>
#include <stdint.h>
#include <stringl/stringl.h>

#include "IxErrno.h"
#include "TmeInterfaces.h"
#include "TmeInterfacesDefs.h"
#include "TmeMessage.h"
#include "TmeMessagesTags.h"
#include "tmecom_interfaces.h"

int TmeGetPilImageRegions(uint32_t       *const swIdCount,
                         uint32_t       *const swIds,
                         uint32_t       *const regionListCount,
                         tmePilRegion_t *const regionList)
{
  int                        ret         = E_FAILURE;
  tmeGetPilImageRegionsReq_t request     = {0};
  tmeGetPilImageRegionsRsp_t response    = {0};
  size_t                     responseLen = 0;
  uint32_t                   regionListCapacity;

  do
  {
    if ((regionListCount == NULL) || (regionList == NULL) ||
        (swIds == NULL) || (swIdCount == NULL))
    {
      ret = E_INVALID_ARG;
      break;
    }

    if ((*swIdCount > TMECOM_PIL_IMAGES_MAX_SWIDS) ||
        (*regionListCount > TMECOM_PIL_IMAGES_MAX_REGIONS))
    {
      ret = E_OUT_OF_RANGE;
      break;
    }

    if ((*swIdCount == 0U) || (*regionListCount == 0U))
    {
      ret = E_INVALID_ARG;
      break;
    }

    regionListCapacity = *regionListCount;

    request.swIdCount = *swIdCount;
    memscpy(request.swIds, sizeof(uint32_t) * (*swIdCount),
           swIds, sizeof(uint32_t) * (*swIdCount));

    if (E_SUCCESS != TransceiveMessage(TME_MSG_CBOR_TAG_GET_PIL_REGIONS,
                                       &request,
                                       sizeof(request),
                                       &response,
                                       sizeof(response),
                                       &responseLen))
    {
      break;
    }

    if ((E_SUCCESS != response.status) || (responseLen != sizeof(response)))
    {
      break;
    }

    if (regionListCapacity < response.regionListCount)
    {
      ret = E_INVALID_ARG;
      break;
    }

    *regionListCount = response.regionListCount;
    memscpy(regionList, regionListCapacity * sizeof(tmePilRegion_t),
           response.regionList,
           response.regionListCount * sizeof(tmePilRegion_t));

    ret = E_SUCCESS;
  } while (0);

  return ret;
}
