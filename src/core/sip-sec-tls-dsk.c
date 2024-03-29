/**
 * @file sip-sec-tls-dsk.c
 *
 * pidgin-sipe
 *
 * Copyright (C) 2011 SIPE Project <http://sipe.sourceforge.net/>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * Specification references:
 *
 *   - [MS-SIPAE]:    http://msdn.microsoft.com/en-us/library/cc431510.aspx
 *   - [MS-OCAUTHWS]: http://msdn.microsoft.com/en-us/library/ff595592.aspx
 *   - MS Tech-Ed Europe 2010 "UNC310: Microsoft Lync 2010 Technology Explained"
 *     http://ecn.channel9.msdn.com/o9/te/Europe/2010/pptx/unc310.pptx
 */

#include <string.h>

#include <glib.h>

#include "sipe-common.h"
#include "sip-sec.h"
#include "sip-sec-mech.h"
#include "sip-sec-tls-dsk.h"
#include "sipe-backend.h"
#include "sipe-digest.h"
#include "sipe-tls.h"

/* Security context for TLS-DSK */
typedef struct _context_tls_dsk {
	struct sip_sec_context common;
	struct sipe_tls_state *state;
	enum sipe_tls_digest_algorithm algorithm;
	guchar *client_key;
	guchar *server_key;
	gsize key_length;
} *context_tls_dsk;

/* sip-sec-mech.h API implementation for TLS-DSK */

static sip_uint32
sip_sec_acquire_cred__tls_dsk(SipSecContext context,
			      SIPE_UNUSED_PARAMETER const char *domain,
			      SIPE_UNUSED_PARAMETER const char *username,
			      const char *password)
{
	context_tls_dsk ctx = (context_tls_dsk)context;

	ctx->state = sipe_tls_start((gpointer) password);
	if (ctx->state) {
		/* Authentication not yet completed */
		ctx->common.is_ready = FALSE;

		return SIP_SEC_E_OK;
	} else {
		return SIP_SEC_E_INTERNAL_ERROR;
	}
}

static sip_uint32
sip_sec_init_sec_context__tls_dsk(SipSecContext context,
				  SipSecBuffer in_buff,
				  SipSecBuffer *out_buff,
				  SIPE_UNUSED_PARAMETER const char *service_name)
{
	context_tls_dsk ctx = (context_tls_dsk) context;
	struct sipe_tls_state *state = ctx->state;

	state->in_buffer = in_buff.value;
	state->in_length = in_buff.length;

	if (sipe_tls_next(state)) {
		if ((state->algorithm != SIPE_TLS_DIGEST_ALGORITHM_NONE) &&
		    state->client_key && state->server_key) {
			/* Authentication is completed */
			ctx->common.is_ready = TRUE;

			/* copy key pair */
			ctx->algorithm  = state->algorithm;
			ctx->key_length = state->key_length;
			ctx->client_key = g_memdup(state->client_key,
						   state->key_length);
			ctx->server_key = g_memdup(state->server_key,
						   state->key_length);

			SIPE_DEBUG_INFO("sip_sec_init_sec_context__tls_dsk: handshake completed, algorithm %d, key length %" G_GSIZE_FORMAT,
					ctx->algorithm, ctx->key_length);

			sipe_tls_free(state);
			ctx->state = NULL;
		} else {
			out_buff->value  = state->out_buffer;
			out_buff->length = state->out_length;
			/* we take ownership of the buffer */
			state->out_buffer = NULL;
		}
	} else {
		sipe_tls_free(state);
		ctx->state = NULL;
	}

	return((ctx->common.is_ready || ctx->state) ? SIP_SEC_E_OK : SIP_SEC_E_INTERNAL_ERROR);
}

static sip_uint32
sip_sec_make_signature__tls_dsk(SipSecContext context,
				const char *message,
				SipSecBuffer *signature)
{
	context_tls_dsk ctx = (context_tls_dsk) context;
	sip_uint32 result = SIP_SEC_E_INTERNAL_ERROR;

	switch (ctx->algorithm) {
	case SIPE_TLS_DIGEST_ALGORITHM_MD5:
		signature->length = SIPE_DIGEST_HMAC_MD5_LENGTH;
		signature->value  = g_malloc0(signature->length);
		sipe_digest_hmac_md5(ctx->client_key, ctx->key_length,
				     (guchar *) message, strlen(message),
				     signature->value);
		result = SIP_SEC_E_OK;
		break;

	case SIPE_TLS_DIGEST_ALGORITHM_SHA1:
		signature->length = SIPE_DIGEST_HMAC_SHA1_LENGTH;
		signature->value  = g_malloc0(signature->length);
		sipe_digest_hmac_sha1(ctx->client_key, ctx->key_length,
				      (guchar *) message, strlen(message),
				      signature->value);
		result = SIP_SEC_E_OK;
		break;

	default:
		/* this should not happen */
		break;
	}

	return(result);
}

static sip_uint32
sip_sec_verify_signature__tls_dsk(SipSecContext context,
				  const char *message,
				  SipSecBuffer signature)
{
	context_tls_dsk ctx = (context_tls_dsk) context;
	SipSecBuffer mac    = { 0, NULL };
	sip_uint32 result   = SIP_SEC_E_INTERNAL_ERROR;

	switch (ctx->algorithm) {
	case SIPE_TLS_DIGEST_ALGORITHM_MD5:
		mac.length = SIPE_DIGEST_HMAC_MD5_LENGTH;
		mac.value  = g_malloc0(mac.length);
		sipe_digest_hmac_md5(ctx->server_key, ctx->key_length,
				     (guchar *) message, strlen(message),
				     mac.value);
		break;

	case SIPE_TLS_DIGEST_ALGORITHM_SHA1:
		mac.length = SIPE_DIGEST_HMAC_SHA1_LENGTH;
		mac.value  = g_malloc0(mac.length);
		sipe_digest_hmac_sha1(ctx->server_key, ctx->key_length,
				      (guchar *) message, strlen(message),
				      mac.value);
		break;

	default:
		/* this should not happen */
		break;
	}

	if (mac.value) {
		result = memcmp(signature.value, mac.value, mac.length) ?
			SIP_SEC_E_INTERNAL_ERROR : SIP_SEC_E_OK;
		g_free(mac.value);
	}

	return(result);
}

static void
sip_sec_destroy_sec_context__tls_dsk(SipSecContext context)
{
	context_tls_dsk ctx = (context_tls_dsk) context;

	sipe_tls_free(ctx->state);
	g_free(ctx->client_key);
	g_free(ctx->server_key);
	g_free(ctx);
}

SipSecContext
sip_sec_create_context__tls_dsk(SIPE_UNUSED_PARAMETER guint type)
{
	context_tls_dsk context = g_malloc0(sizeof(struct _context_tls_dsk));
	if (!context) return(NULL);

	context->common.acquire_cred_func     = sip_sec_acquire_cred__tls_dsk;
	context->common.init_context_func     = sip_sec_init_sec_context__tls_dsk;
	context->common.destroy_context_func  = sip_sec_destroy_sec_context__tls_dsk;
	context->common.make_signature_func   = sip_sec_make_signature__tls_dsk;
	context->common.verify_signature_func = sip_sec_verify_signature__tls_dsk;

	return((SipSecContext) context);
}

/*
  Local Variables:
  mode: c
  c-file-style: "bsd"
  indent-tabs-mode: t
  tab-width: 8
  End:
*/
