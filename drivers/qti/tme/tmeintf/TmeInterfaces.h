/*===========================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/

#ifndef TME_INTERFACES_H_INCLUDED
#define TME_INTERFACES_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "IxErrno.h"
#include "TmeInterfacesDefs.h"

/*
 * TmeForwardRequest() - forward a pre-encoded (CBOR/QBOR) request to
 * TME and return the raw response.
 *
 * The request buffer is forwarded as-is; no serialisation is performed here.
 * A hard-coded communication timeout is applied internally — callers do not
 * supply or influence the timeout value.
 *
 * @param [in]  reqBuf       Pointer to the pre-encoded request buffer.
 * @param [in]  reqSize      Size of the request buffer in bytes.
 * @param [out] rspBuf       Pointer to the response buffer.
 * @param [in]  rspBufSize   Size of the response buffer in bytes.
 *
 * @return E_SUCCESS on success, error code otherwise.
 */
int TmeForwardRequest(void *reqBuf,
                      size_t      reqSize,
                      void       *rspBuf,
                      size_t     *rspBufSize);

/*
 * TmePassthroughCmd() - asynchronously forward a pre-encoded (CBOR/QBOR)
 * request from to TME.
 *
 * Returns as soon as the request has been handed to the transport; does not
 * wait for TME to finish processing it. The TME CPU processes one request
 * at a time and does not support queuing, so only one request -- submitted
 * via this function or TmeForwardRequest() -- may be outstanding at a time.
 *
 * Use TmePassthroughAwait() with the returned handle to poll for the
 * response.
 *
 * @param [in]  reqBuf    Pointer to the pre-encoded request buffer.
 * @param [in]  reqSize   Size of the request buffer in bytes.
 * @param [out] handle    Opaque handle identifying this request, to be
 *                        passed to TmePassthroughAwait().
 *
 * @return E_SUCCESS and *handle set if the request was submitted.
 *         E_AGAIN if TME is currently processing another request; the
 *         caller should retry later.
 *         Other error code on failure to submit the request.
 */
int TmePassthroughCmd(void *reqBuf, size_t reqSize, uint32_t *handle);

/*
 * TmePassthroughAwait() - poll, without blocking, for the response to a
 * request previously submitted via TmePassthroughCmd().
 *
 * @param [in]     handle      Handle returned by TmePassthroughCmd().
 * @param [out]    rspBuf      Pointer to the response buffer.
 * @param [in/out] rspBufSize  On input: capacity of rspBuf in bytes.
 *                             On output: actual response size in bytes.
 *
 * @return E_SUCCESS and response copied into rspBuf if TME has finished
 *         processing the request.
 *         E_IN_PROGRESS if TME is still processing the request; call again
 *         later.
 *         Other error code if handle is invalid/stale.
 */
int TmePassthroughAwait(uint32_t handle, void *rspBuf, size_t *rspBufSize);

/*
 * TmeSHADigest() - compute a SHA digest over a message via TME.
 *
 * @param [in]  inHashAlgorithm  Hash algorithm to use (TME_HA_SHA256/384/512).
 * @param [in]  inMsg            Pointer to the input message.
 * @param [in]  inMsgLen         Length of the input message in bytes.
 * @param [out] outDigest        Buffer to receive the computed digest.
 * @param [out] outDigestLen     On input: capacity of outDigest in bytes.
 *                               On output: actual digest length written.
 * @param [out] errorInfo        Extended error information from TME.
 *
 * @return 0 if successful, non-zero value otherwise.
 */
int TmeSHADigest(TMEHashAlgID_t        inHashAlgorithm,
                 const uint8_t        *inMsg,
                 size_t                inMsgLen,
                 uint8_t              *outDigest,
                 size_t               *outDigestLen,
                 TmeExtendedErrorInfo *errorInfo);

/*
 * TmeFuseRead() - read one QFPROM row via TME.
 *
 * @param [in]  addrType         Fuse address space (TME_QFPROM_ADDR_SPACE_*).
 * @param [in]  fuseAddr         SoC address of the QFPROM row to read.
 * @param [out] fuseData         Receives the row contents, low word first.
 *                               Must point to space for at least
 *                               TME_QFPROM_FUSE_DATA_WORDS uint32_t values -
 *                               a QFPROM row is always read two words at a
 *                               time, regardless of the width of interest.
 * @param [out] qfpromApiStatus  Status reported by TME's qfprom driver;
 *                               TME_QFPROM_NO_ERR on a clean read.  Written
 *                               only when the call returns E_SUCCESS.
 *
 * @return E_SUCCESS if the exchange completed and the response was the
 *         expected size, error code otherwise.
 *
 * NOTE: E_SUCCESS only means the request/response exchange itself succeeded.
 * The caller MUST also check @p qfpromApiStatus - TME reports a rejected or
 * failed fuse read there, not in the return value.
 */
int TmeFuseRead(TmeQfpromAddrSpace_t addrType,
                uint32_t             fuseAddr,
                uint32_t *const      fuseData,
                uint32_t *const      qfpromApiStatus);

/*
 * TmeFuseWriteMultiple() - blow up to TME_MAX_FUSE_WRITE_REQ QFPROM rows in a
 * single request via TME.
 *
 * ###########################################################################
 * # DESTRUCTIVE AND IRREVERSIBLE.  QFPROM fuses are one-time-programmable:   #
 * # any bit set in fuseArray[].data[] is blown permanently on real silicon    #
 * # and can never be cleared.  Blowing the wrong row can brick the part or    #
 * # lock it out of secure boot.                                              #
 * #                                                                         #
 * # An all-zero data[] blows nothing, which is what makes it safe to use for  #
 * # exercising this path without altering chip state.                        #
 * ###########################################################################
 *
 * @param [in]  fuseArray        Rows to write.  Not modified.
 * @param [in]  fuseArrayLen     Number of entries in fuseArray; must be in
 *                               1..TME_MAX_FUSE_WRITE_REQ.
 * @param [out] qfpromApiStatus  Status reported by TME for the write;
 *                               TME_QFPROM_NO_ERR on success.  Always written
 *                               once the arguments validate - set to
 *                               TME_QFPROM_STATUS_UNSET before the exchange.
 *
 * @return E_SUCCESS only if the exchange completed, the response was the
 *         expected size, AND TME reported TME_QFPROM_NO_ERR.
 *         E_BAD_ADDRESS / E_NO_DATA / E_DATA_TOO_LARGE on bad arguments,
 *         other error code on a failed exchange or a rejected write.
 *
 * NOTE: unlike TmeFuseRead(), a nonzero status is folded into the return value
 * here, so E_SUCCESS does mean the write itself was accepted.
 */
int TmeFuseWriteMultiple(TMEFuse_t      *fuseArray,
                         size_t          fuseArrayLen,
                         uint32_t *const qfpromApiStatus);

/*
 * TmeWriteConfigRegister() - write a QFPROM configuration register via TME.
 *
 * @param [in]  registerId  Register to write (QFPROM_BIST_CTRL,
 *                           QFPROM_WRITE_DISABLE_STICKY_BIT0/1).
 * @param [in]  value       Value to write into the register.
 *
 * @return E_SUCCESS if the exchange completed, the response was the expected
 *         size, AND TME reported a zero status.  Error code otherwise -
 *         including when TME rejects registerId itself.
 */
int TmeWriteConfigRegister(tmeConfigRegisterId_e registerId, uint32_t value);

/*
 * TmeSetXpuDbgar() - program a set of XPU DBGAR addresses via TME.
 *
 * @param [in] dbgars  Array of XPU DBGAR addresses.  Not modified.
 * @param [in] count   Number of entries in dbgars; must be nonzero.
 *
 * @return E_SUCCESS if the exchange completed, the response was the expected
 *         size, AND TME reported a zero status.  Error code otherwise.
 */
int TmeSetXpuDbgar(uint32_t *dbgars, size_t count);

/*
 * TmeInvokeAC() - invoke an access-control (AC) module in TME.
 *
 * AC modules write and interpret the content of inBuffer/outBuffer
 * themselves; this call only moves the bytes to and from TME.
 *
 * @param [in]  requestId  Request id identifying the AC module in TME.
 * @param [in]  inBuffer   Data to send to TME.  Not modified.
 * @param [in]  inSize     Size of inBuffer in bytes.
 * @param [out] outBuffer  Receives data from TME.  Must not be NULL.
 * @param [in]  outSize    Capacity of outBuffer in bytes; must be nonzero.
 *
 * @return E_SUCCESS if the exchange completed, TME reported a zero status,
 *         AND the response fit within outSize.  Error code otherwise -
 *         including E_INVALID_ARG for a NULL outBuffer or zero outSize.
 */
int TmeInvokeAC(uint32_t requestId,
               uint8_t *inBuffer,
               size_t   inSize,
               uint8_t *outBuffer,
               size_t   outSize);

/*
 * TmeGetSignedImageIds() - retrieve the software image IDs signed by a given
 * signing authority.
 *
 * @param [in]  signingAuthority  CA whose signed image IDs to retrieve.
 * @param [out] outputSwIds       Receives the image IDs.
 * @param [in]  outputSwIdMax     Capacity of outputSwIds, in entries; must
 *                                be nonzero.
 * @param [out] outputSwIdCount   Receives the number of entries written to
 *                                outputSwIds.
 *
 * @return E_SUCCESS if the exchange completed, the response was the expected
 *         size, AND TME reported a zero status.  E_DATA_TOO_LARGE if TME's
 *         list does not fit in outputSwIdMax.  E_INVALID_ARG for a NULL
 *         outputSwIds/outputSwIdCount or a zero outputSwIdMax.
 */
int TmeGetSignedImageIds(tmeSoftwareRootCaIds signingAuthority,
                        uint32_t            *outputSwIds,
                        size_t               outputSwIdMax,
                        size_t              *outputSwIdCount);

/*
 * TmeGetPilImageRegions() - retrieve the PIL (Peripheral Image Loader)
 * memory regions TME has recorded for a set of software IDs.
 *
 * @param [in]     swIdCount        Number of entries in swIds; must be
 *                                  nonzero and at most TMECOM_PIL_IMAGES_MAX_SWIDS.
 * @param [in]     swIds            Software IDs to query.
 * @param [in,out] regionListCount  On input: capacity of regionList, in
 *                                  entries; must be nonzero and at most
 *                                  TMECOM_PIL_IMAGES_MAX_REGIONS.  On output:
 *                                  number of entries TME reported.
 * @param [out]    regionList       Receives the regions TME reported.
 *
 * @return E_SUCCESS if the exchange completed, the response was the expected
 *         size, AND TME reported a zero status.  E_OUT_OF_RANGE if swIdCount
 *         or the input regionListCount exceeds its maximum.  E_INVALID_ARG
 *         for a NULL pointer, a zero swIdCount/regionListCount, or an input
 *         regionListCount too small for what TME reported.
 */
int TmeGetPilImageRegions(uint32_t       *const swIdCount,
                         uint32_t       *const swIds,
                         uint32_t       *const regionListCount,
                         tmePilRegion_t *const regionList);

/*
 * TmeUpdateRollbackVersion() - tell TME to commit the recorded image versions
 * into the antirollback (ARB) fuses.
 *
 * ###########################################################################
 * # DESTRUCTIVE AND IRREVERSIBLE.  This blows antirollback fuses, which are #
 * # one-time-programmable: once TME has raised the stored ARB version for   #
 * # an image, that part will permanently refuse to boot any older-versioned #
 * # build of it.  There is no "undo" and no non-destructive dry run -       #
 * # unlike TmeFuseWriteMultiple(), there is no payload to zero out,         #
 * # because TME chooses the fuses itself from versions it recorded during   #
 * # authentication.                                                         #
 * #                                                                         #
 * # Do NOT call this from a boot-time probe or self-test.                   #
 * ###########################################################################
 *
 * Until this is called, TME caches version information instead of committing
 * it.  Afterwards it also updates the rollback version as part of signature
 * verification for images authenticated later (e.g. modem).  TME FW tracks
 * that this call was made, so it is a one-time transition per boot.
 *
 * The intended single call site is the point at which the current boot is
 * declared good: at TZ/BL31 cold boot when A/B OTA is disabled, or, when A/B
 * OTA is enabled, only after HLOS has signalled a successful boot.  Calling it
 * before that defeats the purpose of A/B rollback.
 *
 * @return E_SUCCESS if the exchange completed, the response was the expected
 *         size, AND TME reported a zero status.  Error code otherwise.
 */
int TmeUpdateRollbackVersion(void);

#endif /* TME_INTERFACES_H_INCLUDED */
