#include <cstdint>

#include "httpd/httpd.h"

#include "styles.css.h"
#include "index.js.h"
#include "index.html.h"

struct FilesContent {
	const char *pFileName;
	const char *pContent;
	const uint32_t nContentLength;
	const http::contentTypes contentType;
};

static constexpr struct FilesContent HttpContent[] = {
	{ "styles.css", styles_css, 322, static_cast<http::contentTypes>(1) },
	{ "index.js", index_js, 1248, static_cast<http::contentTypes>(2) },
	{ "index.html", index_html, 346, static_cast<http::contentTypes>(0) },
};