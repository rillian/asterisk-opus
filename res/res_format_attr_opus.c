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

/*!
 * \file
 * \brief OPUS format attribute interface
 *
 * \author David Vossel <dvossel@digium.com>
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/module.h"
#include "asterisk/format.h"

/*!
 * \brief OPUS attribute structure.
 *
 * \note The only attribute that affects compatibility here is the sample rate.
 */
struct opus_attr {
	unsigned int samplerate;
	unsigned int maxbitrate;
	unsigned int dtx;
	unsigned int fec;
	unsigned int cbr;
	unsigned int ptime;
	unsigned int mode;
};

static enum ast_format_cmp_res opus_cmp(const struct ast_format_attr *fattr1, const struct ast_format_attr *fattr2)
{
	struct opus_attr *attr1 = (struct opus_attr *) fattr1;
	struct opus_attr *attr2 = (struct opus_attr *) fattr2;

	if (attr1->samplerate == attr2->samplerate) {
		return AST_FORMAT_CMP_EQUAL;
	}
	return AST_FORMAT_CMP_NOT_EQUAL;
}

static int opus_get_val(const struct ast_format_attr *fattr, int key, void *result)
{
	const struct opus_attr *attr = (struct opus_attr *) fattr;
	int *val = result;

	switch (key) {
	case OPUS_ATTR_KEY_SAMP_RATE:
		*val = attr->samplerate;
		break;
	case OPUS_ATTR_KEY_MAX_BITRATE:
		*val = attr->maxbitrate;
		break;
	case OPUS_ATTR_KEY_DTX:
		*val = attr->dtx;
		break;
	case OPUS_ATTR_KEY_FEC:
		*val = attr->fec;
		break;
	case OPUS_ATTR_KEY_CBR:
		*val = attr->cbr;
		break;
	case OPUS_ATTR_KEY_PTIME:
		*val = attr->ptime;
	case OPUS_ATTR_KEY_MODE:
		*val = attr->mode;
	default:
		return -1;
		ast_log(LOG_WARNING, "unknown attribute type %d\n", key);
	}
	return 0;
}

static int opus_isset(const struct ast_format_attr *fattr, va_list ap)
{
	enum opus_attr_keys key;
	const struct opus_attr *attr = (struct opus_attr *) fattr;

	for (key = va_arg(ap, int);
		key != AST_FORMAT_ATTR_END;
		key = va_arg(ap, int))
	{
		switch (key) {
		case OPUS_ATTR_KEY_SAMP_RATE:
			if (attr->samplerate != (va_arg(ap, int))) {
				return -1;
			}
			break;
		case OPUS_ATTR_KEY_MAX_BITRATE:
			if (attr->maxbitrate != (va_arg(ap, int))) {
				return -1;
			}
			break;
		case OPUS_ATTR_KEY_DTX:
			if (attr->dtx != (va_arg(ap, int))) {
				return -1;
			}
			break;
		case OPUS_ATTR_KEY_FEC:
			if (attr->fec != (va_arg(ap, int))) {
				return -1;
			}
			break;
		case OPUS_ATTR_KEY_CBR:
			if (attr->cbr != (va_arg(ap, int))) {
				return -1;
			}
			break;
		case OPUS_ATTR_KEY_PTIME:
			if (attr->ptime != (va_arg(ap, int))) {
				return -1;
			}
			break;
		case OPUS_ATTR_KEY_MODE:
			if (attr->mode != (va_arg(ap, int))) {
				return -1;
			}
			break;
		default:
			return -1;
			ast_log(LOG_WARNING, "unknown attribute type %d\n", key);
		}
	}
	return 0;
}
static int opus_getjoint(const struct ast_format_attr *fattr1, const struct ast_format_attr *fattr2, struct ast_format_attr *result)
{
	struct opus_attr *attr1 = (struct opus_attr *) fattr1;
	struct opus_attr *attr2 = (struct opus_attr *) fattr2;
	struct opus_attr *attr_res = (struct opus_attr *) result;
	int joint = -1;

	attr_res->samplerate = attr1->samplerate & attr2->samplerate;
	/* sample rate is the only attribute that has any bearing on if joint capabilities exist or not */
	if (attr_res->samplerate) {
		joint = 0;
	}
	/* Take the lowest max bitrate */
	attr_res->maxbitrate = MIN(attr1->maxbitrate, attr2->maxbitrate);

	/* Only do dtx if both sides want it. DTX is a trade off between
	 * computational complexity and bandwidth. */
	attr_res->dtx = attr1->dtx && attr2->dtx ? 1 : 0;

	/* Only do FEC if both sides want it.  If a peer specifically requests not
	 * to receive with FEC, it may be a waste of bandwidth. */
	attr_res->fec = attr1->fec && attr2->fec ? 1 : 0;

	/* If CBR is requested, use it */
	attr_res->cbr = attr1->cbr || attr2->cbr ? 1 : 0;

	attr_res->ptime = MIN(attr1->ptime, attr2->ptime);

	attr_res->mode = MIN(attr1->mode, attr2->mode);

	return joint;
}

static void opus_set(struct ast_format_attr *fattr, va_list ap)
{
	enum opus_attr_keys key;
	struct opus_attr *attr = (struct opus_attr *) fattr;

	for (key = va_arg(ap, int);
		key != AST_FORMAT_ATTR_END;
		key = va_arg(ap, int))
	{
		switch (key) {
		case OPUS_ATTR_KEY_SAMP_RATE:
			attr->samplerate = (va_arg(ap, int));
			break;
		case OPUS_ATTR_KEY_MAX_BITRATE:
			attr->maxbitrate = (va_arg(ap, int));
			break;
		case OPUS_ATTR_KEY_DTX:
			attr->dtx = (va_arg(ap, int));
			break;
		case OPUS_ATTR_KEY_FEC:
			attr->fec = (va_arg(ap, int));
			break;
		case OPUS_ATTR_KEY_CBR:
			attr->cbr = (va_arg(ap, int));
			break;
		case OPUS_ATTR_KEY_PTIME:
			attr->ptime = (va_arg(ap, int));
			break;
		case OPUS_ATTR_KEY_MODE:
			attr->mode = (va_arg(ap, int));
			break;
		default:
			ast_log(LOG_WARNING, "unknown attribute type %d\n", key);
		}
	}
}

static struct ast_format_attr_interface opus_interface = {
	.id = AST_FORMAT_OPUS,
	.format_attr_cmp = opus_cmp,
	.format_attr_get_joint = opus_getjoint,
	.format_attr_set = opus_set,
	.format_attr_isset = opus_isset,
	.format_attr_get_val = opus_get_val,
};

static int load_module(void)
{
	if (ast_format_attr_reg_interface(&opus_interface)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_format_attr_unreg_interface(&opus_interface);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "OPUS Format Attribute Module",
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CHANNEL_DEPEND,
);
