/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2011, Digium, Inc.
 *
 * David Vossel <dvossel@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Translate between signed linear and OPUS (Open Codec)
 *
 * \ingroup codecs
 *
 * \extref The OPUS library - http://www.opus-codec.org
 *
 */

#include "asterisk.h"
#include "speex/speex_resampler.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include <opus/opus.h>  /* TODO make build system aware of this header */

#include "asterisk/translate.h"
#include "asterisk/module.h"
#include "asterisk/utils.h"
#include "asterisk/linkedlists.h"

#define OUTBUF_SIZE   8096

#define MAX_ENC_RETURN_FRAMES 8

#define DEFAULT_TIME_PERIOD 20

static struct ast_translator *translators;
static int trans_size;

/* This is the list of signed linear formats we can
 * translate OPUS to and from in one step */
static int id_list[] = {
	AST_FORMAT_SLINEAR,
	AST_FORMAT_SLINEAR12,
	AST_FORMAT_SLINEAR16,
	AST_FORMAT_SLINEAR24,
	AST_FORMAT_SLINEAR32,
	AST_FORMAT_SLINEAR44,
	AST_FORMAT_SLINEAR48,
	AST_FORMAT_SLINEAR96,
};

struct opus_encoder_pvt {
	int init;
	OpusEncoder *enc;
	SpeexResamplerState *resamp;

	/* slin input buffer. Samples go into this buffer before being feed to encoder */
	int16_t slin_buf[OUTBUF_SIZE];
	/* Current number of samples in our slin input buffer */
	unsigned slin_samples;
	/* The number of slin samples to encode at a time */
	unsigned int frame_size;
	/* OPUS output sample rate */
	unsigned int sample_rate;

	/*! Number of current valid out frame buffers */
	unsigned int frame_offsets_num;
	/*! The current number of bytes stored in the frame offsets. */
	unsigned int frame_offsets_numbytes;
	/* Pointers to the beginning of each valid outframe */
	struct {
		unsigned char *buf;
		unsigned int len;
	} frame_offsets[MAX_ENC_RETURN_FRAMES];
};
struct opus_decoder_pvt {
	int init;
	OpusDecoder *dec;
	SpeexResamplerState *resamp;
	unsigned int frame_size;
	int16_t slin_buf[OUTBUF_SIZE];
	unsigned slin_samples;
};

static int opus_enc_set(struct ast_trans_pvt *pvt, struct ast_format *slin_src)
{
	struct opus_encoder_pvt *enc = pvt->pvt;
	int slin_rate = ast_format_rate(slin_src);
	int opus_rate = slin_rate;
	int error = 0;

	if (pvt->explicit_dst.id) {
		opus_rate = ast_format_rate(&pvt->explicit_dst);
	}

	if (slin_rate != opus_rate) {
		if (!(enc->resamp = speex_resampler_init(1, slin_rate, opus_rate, 5, &error))) {
			return -1;
		}
	}

	if (!(enc->enc = opus_encoder_create(slin_rate, 1, OPUS_APPLICATION_VOIP))) {
		ast_log(LOG_WARNING, "Failed to create OPUS encoder\n");
		speex_resampler_destroy(enc->resamp);
		return -1;
	}

	enc->frame_size = opus_rate / (1000 / DEFAULT_TIME_PERIOD);
	enc->sample_rate = opus_rate;

	enc->init = 1;

	return 0;
}

static int opus_dec_set(struct ast_trans_pvt *pvt, struct ast_format *opus_src)
{
	struct opus_decoder_pvt *dec = pvt->pvt;
	int opus_rate = ast_format_rate(opus_src);
	int slin_rate = ast_format_rate(&pvt->t->dst_format);
	int error = 0;

	if (slin_rate != opus_rate) {
		if (!(dec->resamp = speex_resampler_init(1, opus_rate, slin_rate, 5, &error))) {
			return -1;
		}
	}

	if (!(dec->dec = opus_decoder_create(opus_rate, 1))) {
		ast_log(LOG_WARNING, "Failed to create OPUS decoder.\n");
		speex_resampler_destroy(dec->resamp);
		return -1;
	}

	dec->frame_size = opus_rate / (1000 / DEFAULT_TIME_PERIOD);
	dec->init = 1;

	return 0;
}

static void opus_enc_destroy(struct ast_trans_pvt *pvt)
{
	struct opus_encoder_pvt *enc = pvt->pvt;

	if (enc->enc) {
		opus_encoder_destroy(enc->enc);
		enc->enc = NULL;
	}
	if (enc->resamp) {
		speex_resampler_destroy(enc->resamp);
	}
}

static void opus_dec_destroy(struct ast_trans_pvt *pvt)
{
	struct opus_decoder_pvt *dec = pvt->pvt;

	if (dec->dec) {
		opus_decoder_destroy(dec->dec);
		dec->dec = NULL;
	}
	if (dec->resamp) {
		speex_resampler_destroy(dec->resamp);
	}
}

static int opus_enc_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct opus_encoder_pvt *enc = pvt->pvt;
	int16_t *slin_data;
	unsigned char *opus_data;
	int num_bytes = 0;

	if (!enc->init) {
		opus_enc_set(pvt, &f->subclass.format);
	}

	if (!f->datalen) {
		return -1;
	}

	/* bring the slin to the native opus rate we want to encode */
	if (enc->resamp) {
		unsigned int in_samples = f->samples;
		unsigned int out_samples = OUTBUF_SIZE - enc->slin_samples;
		speex_resampler_process_int(enc->resamp,
			0,
			f->data.ptr,
			&in_samples,
			enc->slin_buf + enc->slin_samples,
			&out_samples);

		enc->slin_samples += out_samples;
	} else {
		memcpy(enc->slin_buf + enc->slin_samples, f->data.ptr, f->datalen);
		enc->slin_samples += (f->datalen / 2);
	}

	slin_data = enc->slin_buf;
	opus_data = (unsigned char *) pvt->outbuf.c;

	for ( ; enc->slin_samples >= enc->frame_size; enc->slin_samples -= enc->frame_size) {
		num_bytes = opus_encode(enc->enc, slin_data, enc->frame_size, opus_data, enc->frame_offsets_numbytes);

		if (num_bytes <= 0) {
			/* err */
			break;
		}

		if (enc->frame_offsets_num >= MAX_ENC_RETURN_FRAMES) {
			break;
		}

		enc->frame_offsets[enc->frame_offsets_num].buf = opus_data;
		enc->frame_offsets[enc->frame_offsets_num].len = num_bytes;
		enc->frame_offsets_num++;
		enc->frame_offsets_numbytes += num_bytes;

		pvt->samples += enc->frame_size;

		slin_data += enc->frame_size; /* increments by int16_t samples */
		opus_data += num_bytes; /* increments by bytes */
	}

	return 0;
}

static struct ast_frame *opus_enc_frameout(struct ast_trans_pvt *pvt)
{
	struct opus_encoder_pvt *enc = pvt->pvt;
	struct ast_frame *frame = NULL;
	struct ast_frame *cur = NULL;
	struct ast_frame tmp;
	int i;

	for (i = 0; i < enc->frame_offsets_num; i++) {
		memset(&tmp, 0, sizeof(tmp));
		tmp.frametype = AST_FRAME_VOICE;
		if (pvt->explicit_dst.id) {
			ast_format_copy(&tmp.subclass.format, &pvt->explicit_dst);
		} else {
			ast_format_set(&tmp.subclass.format, AST_FORMAT_OPUS,
				OPUS_ATTR_KEY_SAMP_RATE, enc->sample_rate);
		}
		tmp.datalen = enc->frame_offsets[i].len;
		tmp.data.ptr = enc->frame_offsets[i].buf;
		tmp.samples = enc->frame_size;
		tmp.src = pvt->t->name;
		tmp.offset = AST_FRIENDLY_OFFSET;

		if (frame) {
			AST_LIST_NEXT(cur, frame_list) = ast_frisolate(&tmp);
			cur = AST_LIST_NEXT(cur, frame_list);
		} else {
			frame = ast_frisolate(&tmp);
			cur = frame;
		}
	}


	pvt->samples = 0;
	memset(enc->frame_offsets, 0, sizeof(enc->frame_offsets));
	enc->frame_offsets_num = 0;
	return frame;
}


static int opus_dec_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct opus_decoder_pvt *dec = pvt->pvt;
	int error;

	if (!dec->init) {
		opus_dec_set(pvt, &f->subclass.format);
	}

	error = opus_decode(dec->dec, f->data.ptr, f->datalen, dec->slin_buf, dec->frame_size, 1);
	if (error) {
		ast_log(LOG_WARNING, "error decoding OPUS, error code %d\n", error);
		return -1;
	}

	dec->slin_samples = dec->frame_size;
	pvt->samples = dec->frame_size;

	return 0;
}


static struct ast_frame *opus_dec_frameout(struct ast_trans_pvt *pvt)
{
	struct opus_decoder_pvt *dec = pvt->pvt;
	struct ast_frame tmp;
	int16_t *outslin = dec->slin_buf;
	unsigned int samples = dec->slin_samples;


	if (dec->resamp) {
		unsigned int in_samples = samples;
		unsigned int out_samples = OUTBUF_SIZE;
		speex_resampler_process_int(dec->resamp,
			0,
			dec->slin_buf,
			&in_samples,
			pvt->outbuf.i16,
			&out_samples);
		samples = out_samples;
		outslin = pvt->outbuf.i16;
	}

	if (!samples) {
		return NULL;
	}
	memset(&tmp, 0, sizeof(tmp));
	tmp.frametype = AST_FRAME_VOICE;
	ast_format_copy(&tmp.subclass.format, &pvt->t->dst_format);
	tmp.datalen = samples * 2;
	tmp.data.ptr = outslin;
	tmp.samples = samples;
	tmp.src = pvt->t->name;
	tmp.offset = AST_FRIENDLY_OFFSET;

	pvt->samples = 0;
	return ast_frdup(&tmp);
}


static int unload_module(void)
{
	int res = 0;
	int idx;

	for (idx = 0; idx < trans_size; idx++) {
		res |= ast_unregister_translator(&translators[idx]);
	}
	ast_free(translators);

	return res;
}

static int load_module(void)
{
	int res = 0;
	int x, idx = 0;

	/* We need a translator between every slin fmt to OPUS, and OPUS to every slin fmt */
	trans_size = ARRAY_LEN(id_list) * 2;
	if (!(translators = ast_calloc(1, sizeof(struct ast_translator) * trans_size))) {
		return AST_MODULE_LOAD_FAILURE;
	}

	for (x = 0; x < ARRAY_LEN(id_list); x++) {

		/* SLIN to OPUS */
		translators[idx].newpvt = NULL; /* ENC is created on first frame */
		translators[idx].destroy = opus_enc_destroy;
		translators[idx].framein = opus_enc_framein;
		translators[idx].frameout = opus_enc_frameout;
		translators[idx].desc_size = sizeof(struct opus_encoder_pvt);
		translators[idx].buffer_samples = (OUTBUF_SIZE / sizeof(int16_t));
		translators[idx].buf_size = OUTBUF_SIZE;
		ast_format_set(&translators[idx].src_format, id_list[x], 0);
		ast_format_set(&translators[idx].dst_format, AST_FORMAT_OPUS, 0);
		snprintf(translators[idx].name, sizeof(translators[idx].name), "slin %dkhz -> OPUS",
			ast_format_rate(&translators[idx].src_format));
		res |= ast_register_translator(&translators[idx]);

		idx++;

		/* OPUS to SLIN */
		translators[idx].newpvt = NULL; /* DEC is created on first frame */
		translators[idx].destroy = opus_dec_destroy;
		translators[idx].framein = opus_dec_framein;
		translators[idx].frameout = opus_dec_frameout;
		translators[idx].desc_size = sizeof(struct opus_decoder_pvt);
		translators[idx].buffer_samples = (OUTBUF_SIZE / sizeof(int16_t));
		translators[idx].buf_size = OUTBUF_SIZE;
		ast_format_set(&translators[idx].src_format, AST_FORMAT_OPUS, 0);
		ast_format_set(&translators[idx].dst_format, id_list[x], 0);
		snprintf(translators[idx].name, sizeof(translators[idx].name), "OPUS -> slin %dkhz",
			ast_format_rate(&translators[idx].dst_format));
		res |= ast_register_translator(&translators[idx]);

		idx++;
	}

	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "OPUS Coder/Decoder",
		.load = load_module,
		.unload = unload_module,
);
