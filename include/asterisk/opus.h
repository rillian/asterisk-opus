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
 * \brief OPUS Format Attributes
 *
 * \author David Vossel <dvossel@digium.com>
 */
#ifndef _AST_FORMAT_OPUS_H_
#define _AST_FORMAT_OPUS_H_

/*! OPUS format attribute key value pairs, all are accessible through ast_format_get_value()*/
enum opus_attr_keys {
	OPUS_ATTR_KEY_SAMP_RATE, /*!< value is in opus_attr_vals enum */
	OPUS_ATTR_KEY_DTX, /*!< value is an int, 1 dtx is enabled, 0 dtx not enabled. */
	OPUS_ATTR_KEY_FEC, /*!< value is an int, 1 encode with FEC, 0 do not use FEC. */
	OPUS_ATTR_KEY_CBR, /*!< value is an int, 1 encode with constant bit rate, 0 do not encode with constant bit rate. */
	OPUS_ATTR_KEY_MAX_BITRATE, /*!< value is an int */
	OPUS_ATTR_KEY_PTIME,  /*!< value is in opus_attr_vals enum */
	OPUS_ATTR_KEY_MODE,  /*!< AUDIO or VOICE mode */
};

enum opus_attr_vals {
	OPUS_ATTR_VAL_MODE_VOICE = 0,
	OPUS_ATTR_VAL_MODE_AUDIO = 1,
	OPUS_ATTR_VAL_PTIME_5 = 5, /* 5ms per frame */
	OPUS_ATTR_VAL_PTIME_10 = 10, /* 10ms per frame */
	OPUS_ATTR_VAL_PTIME_20 = 20, /* 20ms per frame */
	OPUS_ATTR_VAL_PTIME_40 = 40, /* 40ms per frame */
	OPUS_ATTR_VAL_PTIME_60 = 60, /* 40ms per frame */
	OPUS_ATTR_VAL_SAMP_8KHZ = 8000,
	OPUS_ATTR_VAL_SAMP_12KHZ = 12000,
	OPUS_ATTR_VAL_SAMP_16KHZ = 16000,
	OPUS_ATTR_VAL_SAMP_24KHZ = 24000,
	OPUS_ATTR_VAL_SAMP_48KHZ = 48000,
};

#endif /* _AST_FORMAT_OPUS_H */
