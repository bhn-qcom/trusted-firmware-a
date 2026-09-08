/*===========================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/

/*
 * Generic TME access-control (AC) module invocation over TMECOM.
 *
 * Uses static scratch buffers for the CBOR request/response rather than the
 * stack or a heap allocator: TF-A has no heap, and BL31 on this platform has
 * only a 4KB per-core stack (PLATFORM_STACK_SIZE), nowhere near enough for
 * ~1.8KB+2KB scratch buffers.  Static reuse is safe here because, like every
 * other tmeintf call, only one TME request may be outstanding at a time -
 * see TmeMessage.c's own gTmecomMsgReq/gTmecomMsgRsp for the same pattern.
 *
 * The wire format is TransceiveMessage()'s usual outer tag(bstr(...))
 * envelope wrapped around an inner CBOR array [requestId, bstr(inBuffer),
 * outSize]; TME replies with an inner array [status, bstr(output)].  This is
 * a message-specific inner encoding, layered on top of - not a replacement
 * for - the outer envelope every tmeintf call goes through.
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
#include "qcbor.h"
#include "qcbor_spiffy_decode.h"

static uint8_t sInvokeAcReqBuf[MAX_CBOR_REQ_LENGTH];
static uint8_t sInvokeAcRspBuf[MAX_CBOR_RSP_LENGTH];

int TmeInvokeAC(uint32_t requestId,
               uint8_t *inBuffer,
               size_t   inSize,
               uint8_t *outBuffer,
               size_t   outSize)
{
  int    ret         = E_FAILURE;
  size_t requestLen  = GetMaxRequestPayload();
  size_t responseLen = GetMaxResponsePayload();

  do
  {
    if ((outBuffer == NULL) || (outSize == 0U))
    {
      ret = E_INVALID_ARG;
      break;
    }

    QCBOREncodeContext ECtx = {0};
    const UsefulBufC   inData = {inBuffer, inSize};
    int                cborRet;

    QCBOREncode_Init(&ECtx, (UsefulBuf){sInvokeAcReqBuf, requestLen});

    QCBOREncode_OpenArray(&ECtx);
    QCBOREncode_AddUInt64(&ECtx, requestId);
    QCBOREncode_AddBytes(&ECtx, inData);
    /* Tell TME how big our response buffer is, so it can size its reply. */
    QCBOREncode_AddUInt64(&ECtx, outSize);
    QCBOREncode_CloseArray(&ECtx);

    cborRet = QCBOREncode_FinishGetSize(&ECtx, &requestLen);
    if (cborRet != QCBOR_SUCCESS)
    {
      break;
    }

    if (E_SUCCESS != TransceiveMessage(TME_MSG_CBOR_TAG_INVOKE_AC,
                                       sInvokeAcReqBuf,
                                       requestLen,
                                       sInvokeAcRspBuf,
                                       responseLen,
                                       &responseLen))
    {
      break;
    }

    QCBORDecodeContext DCtx;
    QCBORItem          arrayItem = {0};
    UsefulBufC         output    = {0};
    uint64_t           status    = 0;

    QCBORDecode_Init(&DCtx, (UsefulBufC){sInvokeAcRspBuf, responseLen},
                     QCBOR_DECODE_MODE_NORMAL);

    QCBORDecode_EnterArray(&DCtx, &arrayItem);
    if (arrayItem.val.uCount != 2U)
    {
      break;
    }

    QCBORDecode_GetUInt64(&DCtx, &status);
    QCBORDecode_GetByteString(&DCtx, &output);
    QCBORDecode_ExitArray(&DCtx);

    if (QCBOR_SUCCESS != QCBORDecode_GetError(&DCtx))
    {
      break;
    }

    if (E_SUCCESS != status)
    {
      break;
    }

    /*
     * Check the SOURCE length against outBuffer's capacity BEFORE copying -
     * see TmeMessage.c's DecodeMessage() for why comparing memscpy()'s
     * return value (min(dst,src)) against outSize instead would always be
     * true and silently accept a truncated copy.
     */
    if (output.len > outSize)
    {
      ret = E_NO_MEMORY;
      break;
    }

    memscpy(outBuffer, outSize, output.ptr, output.len);
    ret = E_SUCCESS;
  } while (0);

  return ret;
}
