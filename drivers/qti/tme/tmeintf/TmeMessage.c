/*===========================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/

#include <stdint.h>
#include <stddef.h>
#include <stringl/stringl.h>

#include "IxErrno.h"
#include "tzbsp_err_fatal.h"
#include "bl31qtilib_cb_interface.h"
#include "tzbsp_log.h"
#include "tmecomTFA.h"
#include "tmecom_interfaces.h"
#include "TmeMessage.h"
#include "TmeMessagesTags.h"
#include "qcbor.h"

/*
 * In the TFA build, request/response message buffers are statically reserved
 * within the driver (mirroring tmecom.c's static allocation).
 */
static tmecomMsgReq_t gTmecomMsgReq = { 0 };
static tmecomMsgRsp_t gTmecomMsgRsp = { 0 };

static void *tmeMsgAllocReq(size_t size) {
  if (size <= sizeof(gTmecomMsgReq)) return (void *)&gTmecomMsgReq;
  return NULL;
}
static void tmeMsgFreeReq(void *pMem) { (void)pMem; }
static void *tmeMsgAllocRsp(size_t size) {
  if (size <= sizeof(gTmecomMsgRsp)) return (void *)&gTmecomMsgRsp;
  return NULL;
}
static void tmeMsgFreeRsp(void *pMem) { (void)pMem; }

size_t GetEncodedNumberSize(uint32_t number)
{
  size_t encodedSize = sizeof(uint8_t);

  /* Unreachable for uint32_t argument. Mirrors original implementation. */
  if (number > 0xffffffff) {
    encodedSize += sizeof(uint64_t);
  } else if (number > 0xffff) {
    encodedSize += sizeof(uint32_t);
  } else if (number > 0xff) {
    encodedSize += sizeof(uint16_t);
  } else if (number >= 24) {
    encodedSize += sizeof(uint8_t);
  }
  return encodedSize;
}

size_t GetMaxRequestPayload(void)
{
  return MAX_CBOR_REQ_LENGTH -
         GetEncodedNumberSize(TME_MSG_CBOR_TAG_MAX) -
         GetEncodedNumberSize(MAX_CBOR_REQ_LENGTH);
}

size_t GetMaxResponsePayload(void)
{
  return MAX_CBOR_RSP_LENGTH -
         GetEncodedNumberSize(TME_MSG_CBOR_TAG_MAX) -
         GetEncodedNumberSize(MAX_CBOR_RSP_LENGTH);
}

int EncodeMessage(uint32_t tag, const UsefulBufC messageBuf, UsefulBuf *encodedBuf)
{
  int                ret          = E_FAILURE;
  size_t             encodedLen   = 0;
  QCBOREncodeContext encodeContext = {0};

  QCBOREncode_Init(&encodeContext, *encodedBuf);

  QCBOREncode_AddTag(&encodeContext, tag);
  QCBOREncode_AddBytes(&encodeContext, messageBuf);

  ret = QCBOREncode_FinishGetSize(&encodeContext, &encodedLen);

  if (ret != QCBOR_SUCCESS)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ENCODE_MESSAGE_QCBOR_ERROR, tag);
    return ret;
  }

  if (encodedLen <= encodedBuf->len)
  {
    encodedBuf->len = encodedLen;
  }
  else
  {
    encodedBuf->len = 0;
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ENCODE_MESSAGE_LENGTH_ERROR, tag, encodedLen, encodedBuf->len);
    return E_NO_MEMORY;
  }

  return E_SUCCESS;
}

int DecodeMessage(uint32_t tag, const UsefulBufC encodedBuf, UsefulBuf *messageBuf)
{
  int                ret           = E_FAILURE;
  size_t             decodedLen    = 0;
  QCBORDecodeContext decodeContext = {0};
  QCBORItem          item          = {0};

  QCBORDecode_Init(&decodeContext, encodedBuf, QCBOR_DECODE_MODE_NORMAL);

  ret = QCBORDecode_GetNext(&decodeContext, &item);
  if (ret != QCBOR_SUCCESS)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_QCBOR_ERROR, tag);
    return ret;
  }

  /* v1.2 QCBOR no longer exposes a single item.uTag field; fetch the
   * outermost tag (index 0) of the decoded item via the accessor. */
  uint64_t itemTag = QCBORDecode_GetNthTag(&decodeContext, &item, 0);

  /* Either tag matches the message, or TME responded with an error tag. */
  if ((itemTag != tag) && (itemTag != TME_MSG_CBOR_TAG_ERROR))
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_TAG_ERROR, itemTag, tag);
    return E_FAILURE;
  }

  /*
   * Generic errors are sent as integers.  A valid bstr message is handled
   * below.  For example, the message may have failed some integrity check
   * due to corruption in flight.
   */
  if ((item.uDataType == QCBOR_TYPE_UINT64) || (item.uDataType == QCBOR_TYPE_INT64))
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_DATA_ERROR, item.val.uint64);
    return E_FAILURE;
  }

  /* A valid, handled message has a bstr-formatted response. */
  if (item.uDataType != QCBOR_TYPE_BYTE_STRING)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_TYPE_ERROR, item.uDataType);
    return E_FAILURE;
  }

  /*
   * Check the SOURCE length against the caller's capacity BEFORE copying.
   *
   * memscpy() copies min(dst_size, src_size) and returns that count, so
   * testing its return value against messageBuf->len can never fail - the
   * old `decodedLen <= messageBuf->len` check was always true and silently
   * truncated an over-long response while still reporting E_SUCCESS.  Per
   * memscpy()'s contract (see drivers/qti/tme/tme_tfa_glue.c), truncation is
   * detected by comparing against the SOURCE size.
   */
  if (item.val.string.len > messageBuf->len)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_BUF_COPY_ERROR, tag);
    messageBuf->len = 0;
    return E_NO_MEMORY;
  }

  decodedLen = memscpy(messageBuf->ptr,
                       messageBuf->len,
                       item.val.string.ptr,
                       item.val.string.len);

  messageBuf->len = decodedLen;

  return E_SUCCESS;
}

static int TransceiveMessageInternal(uint32_t  tag,
                                     void     *clientPtr,
                                     void     *reqBuf,
                                     size_t    reqBufLen,
                                     void     *respBuf,
                                     size_t    respBufCapacity,
                                     size_t   *respBufLen,
                                     uint32_t  timeoutMSec)
{
  int ret = 0;

  tmecomMsgReq_t *tmecomMsgReq      = NULL;
  size_t          tmecomMsgReqLen    = 0;
  UsefulBufC      requestBuf         = {reqBuf, reqBufLen};
  UsefulBuf       encodedRequestBuf  = {0};
  size_t          encodedRequestLen  = 0;

  tmecomMsgRsp_t *tmecomMsgRsp       = NULL;
  size_t          tmecomMsgRspLen    = 0;
  UsefulBuf       responseBuf        = {respBuf, respBufCapacity};
  UsefulBuf       encodedResponseBuf = {0};
  size_t          encodedResponseLen = 0;

  if (clientPtr == NULL)
  {
    ret = E_INVALID_ARG;
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_CLIENT_INVALID, tag);
    goto exit;
  }

  /* Calculate the CBOR-encoded request size and allocate a framed buffer. */
  encodedRequestLen = GetEncodedNumberSize(tag) +
                      GetEncodedNumberSize(reqBufLen) +
                      reqBufLen;

  if (encodedRequestLen > MAX_CBOR_REQ_LENGTH)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ALLOCATE_FAILED, tag, encodedRequestLen);
    ret = E_DATA_TOO_LARGE;
    goto exit;
  }

  tmecomMsgReqLen = sizeof(tmecomMsgHdr) + encodedRequestLen;
  tmecomMsgReq    = tmeMsgAllocReq(tmecomMsgReqLen);

  if (tmecomMsgReq == NULL)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ALLOCATE_FAILED, tag, encodedRequestLen);
    ret = E_NO_MEMORY;
    goto exit;
  }

  encodedRequestBuf.ptr = tmecomMsgReq->encReqBuf;
  encodedRequestBuf.len = encodedRequestLen;

  /* Calculate the CBOR-encoded response size and allocate a framed buffer. */
  encodedResponseLen = GetEncodedNumberSize(tag) +
                       GetEncodedNumberSize(respBufCapacity) +
                       respBufCapacity;

  if (encodedResponseLen > MAX_CBOR_RSP_LENGTH)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ALLOCATE_FAILED, tag, encodedResponseLen);
    ret = E_DATA_TOO_LARGE;
    goto exit;
  }

  tmecomMsgRspLen = sizeof(tmecomMsgHdr) + encodedResponseLen;
  tmecomMsgRsp    = tmeMsgAllocRsp(tmecomMsgRspLen);

  if (tmecomMsgRsp == NULL)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ALLOCATE_FAILED, tag, encodedResponseLen);
    ret = E_NO_MEMORY;
    goto exit;
  }

  encodedResponseBuf.ptr = tmecomMsgRsp->encRspBuf;
  encodedResponseBuf.len = encodedResponseLen;

  /* CBOR-encode the raw request struct. */
  ret = EncodeMessage(tag, requestBuf, &encodedRequestBuf);
  if (ret != QCBOR_SUCCESS)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_ENCODE_MESSAGE_ERROR, tag, ret);
    goto exit;
  }

  /* Send the framed request synchronously to TME. */
  ret = tmecomClientSendMessageSync(clientPtr,
                                    tmecomMsgReq,
                                    tmecomMsgReqLen,
                                    tmecomMsgRsp,
                                    &tmecomMsgRspLen,
                                    timeoutMSec);
  if (ret != 0)
  {
    /* A transport failure communicating with TME is non-recoverable. */
    bl31qtilib_cb_error_fatal(TME_ERR_FATAL_TMECOM_PROTOCOL_FAILURE);
    /*
     * error_fatal() is not expected to return, but do not rely on that:
     * falling through with a failed transport would underflow the
     * tmecomMsgRspLen - sizeof(tmecomMsgHdr) subtraction below (rsp len is
     * 0 or short on failure) and hand DecodeMessage a ~2^64-byte buffer.
     * Bailing here also preserves the transport's error code, which
     * DecodeMessage would otherwise overwrite.
     */
    goto exit;
  }

  /*
   * Strip the tmecomMsgHdr from the returned length before CBOR decode.
   * tmecomMsgRspLen is an in/out parameter, so re-validate it rather than
   * trusting the transport not to have enlarged it past the buffer.
   */
  if ((tmecomMsgRspLen < sizeof(tmecomMsgHdr)) ||
      (tmecomMsgRspLen > (sizeof(tmecomMsgHdr) + encodedResponseLen)))
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_ERROR, tag, tmecomMsgRspLen);
    ret = E_FAILURE;
    goto exit;
  }

  encodedResponseBuf.len = tmecomMsgRspLen - sizeof(tmecomMsgHdr);

  /* CBOR-decode the response. */
  ret = DecodeMessage(tag, UsefulBuf_Const(encodedResponseBuf), &responseBuf);

  if (ret != QCBOR_SUCCESS)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_DECODE_MESSAGE_ERROR, tag, ret);
    goto exit;
  }

  *respBufLen = responseBuf.len;

exit:
  if (tmecomMsgReq != NULL) tmeMsgFreeReq(tmecomMsgReq);
  if (tmecomMsgRsp != NULL) tmeMsgFreeRsp(tmecomMsgRsp);

  return ret;
}

int TransceiveMessage(uint32_t tag,
                      void    *reqBuf,
                      size_t   reqBufLen,
                      void    *respBuf,
                      size_t   respBufLen,
                      size_t  *respLen)
{
  tmecomClient *client = NULL;
  int           ret;

  ret = tmecomInterfaceInit(&client);

  if (ret == E_SUCCESS)
  {
    ret = TransceiveMessageInternal(tag,
                                    client,
                                    reqBuf,
                                    reqBufLen,
                                    respBuf,
                                    respBufLen,
                                    respLen,
                                    TMECOM_RESPONSE_TIMEOUT_MS);
  }

  if (E_SUCCESS != ret)
  {
    TFA_LOG_ERR(TZBSP_TME_MESSAGE_TRANSCEIVE_FAIL, tag);
  }

  return ret;
}

uint32_t UpdatedExtendedErrorInfo(TmeExtendedErrorInfo *errorInfo,
                                  TmeExtendedErrorInfo  result)
{
  uint32_t ret = E_INVALID_ARG;

  if (errorInfo)
  {
    errorInfo->tmeErrorStatus    = result.tmeErrorStatus;
    errorInfo->seqErrorStatus    = result.seqErrorStatus;
    errorInfo->seqKPErrorStatus0 = result.seqKPErrorStatus0;
    errorInfo->seqKPErrorStatus1 = result.seqKPErrorStatus1;
    errorInfo->seqRspStatus      = result.seqRspStatus;

    uint32_t isFailure = errorInfo->tmeErrorStatus  ||
                         errorInfo->seqErrorStatus  ||
                         errorInfo->seqKPErrorStatus0 ||
                         errorInfo->seqKPErrorStatus1;

    ret = isFailure ? E_FAILURE : E_SUCCESS;
  }

  return ret;
}
