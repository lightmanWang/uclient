/*
 * uclient - ustream based protocol client library
 *
 * Copyright (C) 2014 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <libubox/blobmsg.h>
#include <unistd.h>
#include <stdio.h>

#include "uclient.h"

static void example_header_done(struct uclient *cl)
{
	struct blob_attr *cur;
	int rem;

	printf("Headers (%d): \n", cl->status_code);
	blobmsg_for_each_attr(cur, cl->meta, rem) {
		printf("%s=%s\n", blobmsg_name(cur), (char *) blobmsg_data(cur));
	}

	printf("Contents:\n");
}

static void example_read_data(struct uclient *cl)
{
	char buf[256];
	int len;

	while (1) {
		len = uclient_read(cl, buf, sizeof(buf));
		if (!len)
			return;

		write(STDOUT_FILENO, buf, len);
	}
}

static void example_request_sm(struct uclient *cl)
{
	static int i = 0;

	switch (i++) {
	case 0:
		uclient_connect(cl);
		uclient_http_set_request_type(cl, "HEAD");
		uclient_request(cl);
		break;
	case 1:
		uclient_connect(cl);
		uclient_http_set_request_type(cl, "GET");
		uclient_request(cl);
		break;
	default:
		uloop_end();
		break;
	};
}

static void example_eof(struct uclient *cl)
{
	static int retries;

	if (retries < 10 && uclient_http_redirect(cl)) {
		retries++;
		return;
	}

	retries = 0;
	example_request_sm(cl);
}

static void example_error(struct uclient *cl, int code)
{
	fprintf(stderr, "Error %d!\n", code);
	example_request_sm(cl);
}

static const struct uclient_cb cb = {
	.header_done = example_header_done,
	.data_read = example_read_data,
	.data_eof = example_eof,
	.error = example_error,
};

static int usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s [options] <hostname> <port>\n"
		"Options:\n"
		"	-c <cert>:         Load CA certificates from file <cert>\n"
		"	-C:                Skip certificate CN verification against hostname\n"
		"\n", progname);
	return 1;
}


int main(int argc, char **argv)
{
	struct ustream_ssl_ctx *ctx;
	const char *progname = argv[0];
	struct uclient *cl;
	bool verify = true;
	int ch;

	ctx = ustream_ssl_context_new(false);

	while ((ch = getopt(argc, argv, "Cc:")) != -1) {
		switch(ch) {
		case 'c':
			ustream_ssl_context_add_ca_crt_file(ctx, optarg);
			break;
		case 'C':
			verify = false;
			break;
		default:
			return usage(progname);
		}
	}

	argv += optind;
	argc -= optind;

	if (argc != 1)
		return usage(progname);

	uloop_init();

	cl = uclient_new(argv[0], &cb);
	if (!cl) {
		fprintf(stderr, "Failed to allocate uclient context\n");
		return 1;
	}

	uclient_http_set_ssl_ctx(cl, ctx, verify);
	example_request_sm(cl);
	uloop_run();
	uloop_done();

	uclient_free(cl);
	ustream_ssl_context_free(ctx);


	return 0;
}
