/*
 * xform functions
 *
 * The methods / structures provide a fully dynamic method for
 * manipulating server & player names for servers before output
 *
 * The original string passed in is never altered.
 *
 * We use a dynamically allocated set of buffers to ensure we have
 * no memory collision. These buffers are persistent for the life
 * of the qstat process to avoid constant malloc, realloc and free
 * calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#ifndef _WIN32
#include <err.h>
#include <sysexits.h>
#endif

#include "xform.h"

#ifndef EX_OSERR
#define EX_OSERR 71
#endif

/*
 * Flag controlling if xform methods do any processing except transforming
 * NULL to blank string
 */
int xform_names = 1;

/* Flag to determine if we strip all unprintable characters */
int xform_strip_unprintable = 0;

/* Flag to determine if we should print hex based player names */
int xform_hex_player_names = 0;

/* Flag to determine if we should print hex based server names */
int xform_hex_server_names = 0;

/* Flag to determine if we should script carets */
int xform_strip_carets = 1;

/* Flag to detemine if we should generate html names */
int xform_html_names = -1;

extern int html_mode;

/* xform buffer structure */
typedef struct xform  {
	char *buf;
	size_t size;
	struct xform *next;
} xform;

/* First xform buffer */
static struct xform *xform_bufs = NULL;

/* Current xform buffer */
static struct xform *xform_buf = NULL;

/* Size of the used amount of the current xform buffer */
static size_t xform_used;

/* Is there a currently open font tag in the xform buffer */
static int xform_font_tag;

/* Min size of an xform buffer */
static const int xform_buf_min = 256;


/*** Private Methods ***/

static int
xform_html_entity(const char c, char *dest)
{
	if (html_mode) {
		switch (c) {
		case '<':
			strcpy(dest, "&lt;");
			return 4;
		case '>':
			strcpy(dest, "&gt;");
			return 4;
		case '&':
			strcpy(dest, "&amp;");
			return 5;
		default:
			break;
		}
	}

	return 0;
}

/*
 * return the current xform buffer string buffer
 */
static char *
xform_strbuf()
{
	return xform_buf->buf;
}

/*
 * Ensure a the current xform buffer is at least size
 */
static void
xform_buf_resize(size_t size, char **bufp)
{
	char *oldbuf;

	if (size <= xform_buf->size) {
		// already big enough
		xform_used = size;
		return;
	}

	oldbuf = xform_buf->buf;
	if ((xform_buf->buf = realloc(xform_buf->buf, size)) == NULL)
		err(EX_OSERR, NULL);

	if (xform_buf->buf != oldbuf && bufp != NULL) {
		// memory block moved update bufp
		*bufp = xform_buf->buf + ((*bufp) - oldbuf);
	}

	xform_buf->size = size;
	xform_used = size;
}

/*
 * snprintf a string into an xform buffer expanding the buffer
 * by the size of new string if needed
 */
static int
xform_snprintf(char **buf, size_t size, const char* format, ... )
{
	int ret;
    va_list args;

	// ensure buf is large enough
	xform_buf_resize(xform_used + size, buf);

	va_start(args, format);
	ret = vsnprintf(*buf, size, format, args);
	va_end(args);

	return ret;
}

/*
 * Copy a string into an xform buffer expanding the buffer
 * by the size of new string if needed
 */
static int
xform_strcpy(char **buf, const char *str)
{
	size_t size;

	size = strlen(str);

	// ensure buf is large enough
	xform_buf_resize(xform_used + size, buf);

	(void)strcpy(*buf, str);

	return size;
}

/*
 * Close previous html color and start a new one
 * Returns the size of the string added to the buffer
 */
static int
xform_html_color(char **buf, const char *font_color)
{
	size_t size;
	int inc;

	if (xform_html_names != 1)
		return 0;

	size = 15 + strlen(font_color);
	if (xform_font_tag)
		size += 7;

	inc = xform_snprintf(buf, size, "%s<font color=\"%s\">", xform_font_tag ? "</font>" : "", font_color);
	xform_font_tag = 1;

	return inc;
}

/*
 * Reset the xform buffers for re-use
 */
static void
xform_buf_reset()
{
	xform_buf = xform_bufs;
	xform_font_tag = 0;
	xform_used = 0;
}


/*
 * Allocate and init a new xform buffer ensuring min size is used
 */
static char *
xform_buf_create(size_t size)
{
	struct xform *next;
	char *buf;

	/*
	 * We use a min size which should be reasonable for
	 * 99.9% of server and player names
	 */
	size = (size >= xform_buf_min) ? size + 1 : xform_buf_min;

	if((next = malloc(sizeof(xform))) == NULL) {
		err(EX_OSERR, NULL);
	}

	if((buf = malloc(sizeof(char) * size)) == NULL) {
		err(EX_OSERR, NULL);
	}

	next->buf = buf;
	next->next = NULL;
	next->size = size;
	xform_used = size;
	xform_font_tag = 0;
	buf[0] = '\0';

	if (xform_buf == NULL) {
		xform_bufs = next;
	} else {
		xform_buf->next = next;
	}
	xform_buf = next;

	return next->buf;
}

/*
 * Get a xform buffer allocating a new one or expanding an
 * old one as required
 */
static char *
xform_buf_get(size_t size)
{
	if (xform_buf == NULL || xform_buf->next == NULL) {
		return xform_buf_create(size);
	}

	xform_buf = xform_buf->next;
	
	if (size > xform_buf->size) {
		xform_buf_resize(size, NULL);
	} else {
		xform_used = size;
		xform_font_tag = 0;
	}
	
	xform_buf->buf[0] = '\0';

	return xform_buf->buf;
}

static char *quake3_escape_colors[8] =
{
	"black", "red", "green", "yellow", "blue", "cyan", "magenta", "white"
};

/*
 * Transform a quake 3 string
 */
static char *
xform_name_q3(char *string, struct qserver *server)
{
	unsigned char *s;
	char *q;

	q = xform_strbuf();
	s = (unsigned char*)string;

	for (; *s; s++) {
		if (*s == '^' && *(s + 1) != '^') {
			if (*(s + 1) == '\0') {
				break;
			}

			if (xform_html_names == 1) {
				q += xform_html_color(&q, quake3_escape_colors[*(s + 1) &0x7]);
				s++;
			} else if (xform_strip_carets) {
				s++;
			} else {
				*q++ = *s;
			}
		} else {
			int inc = xform_html_entity((char)*s, q);
			if (0 != inc) {
				q += inc;
			} else if (isprint(*s)) {
				*q++ = *s;
			} else if (*s == '\033') {
				/* skip */
			} else if (*s == 0x80) {
				*q++ = '(';
			} else if (*s == 0x81) {
				*q++ = '=';
			} else if (*s == 0x82) {
				*q++ = ')';
			} else if (*s == 0x10 || *s == 0x90) {
				*q++ = '[';
			} else if (*s == 0x11 || *s == 0x91) {
				*q++ = ']';
			} else if (*s >= 0x92 && *s <= 0x9a) {
				*q++ = *s - 98;
			} else if (*s >= 0xa0 && *s <= 0xe0) {
				*q++ = *s - 128;
			} else if (*s >= 0xe1 && *s <= 0xfa) {
				*q++ = *s - 160;
			} else if (*s >= 0xfb && *s <= 0xfe) {
				*q++ = *s - 128;
			}
		}
	}
	*q = '\0';

	return xform_strbuf();
}

/*
 * Transform a tribes 2 string
 */
static char *
xform_name_t2(char *string, struct qserver *server)
{
	char *s, *q;

	q = xform_strbuf();
	s = string;

	for (; *s; s++) {
		int inc = xform_html_entity(*s, q);
		if (0 != inc) {
			q += inc;
			continue;
		} else if (isprint(*s)) {
			*q++ = *s;
			continue;
		}

		if (xform_html_names == 1 && s[1] != '\0') {
			char *font_color;
			switch (*s) {
			case 0x8:
				font_color = "white";
				break; /* normal */
			case 0xb:
				font_color = "yellow";
				break; /* tribe tag */
			case 0xc:
				font_color = "blue";
				break; /* alias */
			case 0xe:
				font_color = "green";
				break; /* bot */
			default:
				font_color = NULL;
			}

			if (font_color) {
				q += xform_html_color(&q, font_color);
			}
		}
	}
	*q = '\0';

	return xform_strbuf();
}

static const char *unreal_rgb_colors[] =
{
	"#F0F8FF", "#FAEBD7", "#00FFFF", "#7FFFD4", "#F0FFFF", "#F5F5DC", "#FFE4C4", "#000000", "#FFEBCD", "#0000FF", "#8A2BE2", "#A52A2A",
	"#DEB887", "#5F9EA0", "#7FFF00", "#D2691E", "#FF7F50", "#6495ED", "#FFF8DC", "#DC143C", "#00FFFF", "#00008B", "#008B8B",
	"#B8860B", "#A9A9A9", "#006400", "#BDB76B", "#8B008B", "#556B2F", "#FF8C00", "#9932CC", "#8B0000", "#E9967A", "#8FBC8F",
	"#483D8B", "#2F4F4F", "#00CED1", "#9400D3", "#FF1493", "#00BFFF", "#696969", "#1E90FF", "#B22222", "#FFFAF0", "#228B22",
	"#FF00FF", "#DCDCDC", "#F8F8FF", "#FFD700", "#DAA520", "#808080", "#008000", "#ADFF2F", "#F0FFF0", "#FF69B4", "#CD5C5C",
	"#4B0082", "#FFFFF0", "#F0E68C", "#E6E6FA", "#FFF0F5", "#7CFC00", "#FFFACD", "#ADD8E6", "#F08080", "#E0FFFF", "#FAFAD2",
	"#90EE90", "#D3D3D3", "#FFB6C1", "#FFA07A", "#20B2AA", "#87CEFA", "#778899", "#B0C4DE", "#FFFFE0", "#00FF00", "#32CD32",
	"#FAF0E6", "#FF00FF", "#800000", "#66CDAA", "#0000CD", "#BA55D3", "#9370DB", "#3CB371", "#7B68EE", "#00FA9A", "#48D1CC",
	"#C71585", "#191970", "#F5FFFA", "#FFE4E1", "#FFE4B5", "#FFDEAD", "#000080", "#FDF5E6", "#808000", "#6B8E23", "#FFA500",
	"#FF4500", "#DA70D6", "#EEE8AA", "#98FB98", "#AFEEEE", "#DB7093", "#FFEFD5", "#FFDAB9", "#CD853F", "#FFC0CB", "#DDA0DD",
	"#B0E0E6", "#800080", "#FF0000", "#BC8F8F", "#4169E1", "#8B4513", "#FA8072", "#F4A460", "#2E8B57", "#FFF5EE", "#A0522D",
	"#C0C0C0", "#87CEEB", "#6A5ACD", "#708090", "#FFFAFA", "#00FF7F", "#4682B4", "#D2B48C", "#008080", "#D8BFD8", "#FF6347",
	"#40E0D0", "#EE82EE", "#F5DEB3", "#FFFFFF", "#F5F5F5", "#FFFF00", "#9ACD32",

};

/*
 * Transform a unreal 2 string
 */
static char *
xform_name_u2(char *string, struct qserver *server)
{
	unsigned char *s;
	char *q;

	q = xform_strbuf();
	s = (unsigned char *)string;

	for (; *s; s++) {
		if (memcmp(s, "^\1", 2) == 0) {
			// xmp color
			s += 2;
			q += xform_html_color((char **)&s, unreal_rgb_colors[*s - 1]);
		} else if (memcmp(s, "\x1b", 1) == 0) {
			// ut2k4 color
			// A 3 byte array, for example { 0xF8, 0x40, 0x40 }
			// is encoded as a 8 char sized string including
			// the '#' prefix and the null-terminator:
			// { '#', 'F', '8', '4', '0', '4', '0', 0x00 }
			char color[8];
			s += 1;
			sprintf(color, "#%02hhx%02hhx%02hhx", s[0], s[1], s[2]);
			q += xform_html_color(&q, color);
			s += 3;
		} else {
			int inc = xform_html_entity(*s, q);
			if (0 != inc) {
				q += inc;
			} else if (isprint(*s)) {
				*q++ = *s;
			} else if (0xa0 == *s) {
				*q++ = ' ';
			}
		}
	}
	*q = '\0';

	return xform_strbuf();
}

/*
 * Transform a trackmania string
 */
static char *
xform_name_tm(char *string, struct qserver *server)
{
	char *s, *q;
	int open = 0;
	char c1, c2, c3;

	q = xform_strbuf();
	s = string;

	for (; *s; s++) {
		if (*s == '$') {
			s++;
			switch (*s) {
			case 'i':
			case 'I':
				// italic
				if (xform_html_names == 1) {
					q += xform_strcpy(&q, "<span style=\"font-style:italic\">");
					open++;
				}
				break;
			case 's':
			case 'S':
				// shadowed
				break;
			case 'w':
			case 'W':
				// wide
				break;
			case 'n':
			case 'N':
				// narrow
				break;
			case 'm':
			case 'M':
				// normal
				if (xform_html_names == 1) {
					q += xform_strcpy(&q, "<span style=\"font-style:normal\">");
					open++;
				}
				break;
			case 'o':
			case 'O':
				// bold
				if (xform_html_names == 1) {
					q += xform_strcpy(&q, "<span style=\"font-style:bold\">");
					open++;
				}
				break;
			case 'g':
			case 'G':
				// default color
				if (xform_html_names == 1) {
					q += xform_strcpy(&q, "<span style=\"color:black\">");
					open++;
				}
				break;
			case 'z':
			case 'Z':
				// reset all
				while (open) {
					q += xform_strcpy(&q, "</span>");
					open--;
				}
				break;
			case 't':
			case 'T':
				// capitalise
				if (xform_html_names == 1) {
					q += xform_strcpy(&q, "<span style=\"text-transform:capitalize\">");
					open++;
				}
				break;
			case '$':
				// literal $
				*q++ = '$';
				break;
			case '\0':
				// Unexpected end
				break;
			default:
				// color
				c3 = '\0';
				c1 = *s;
				s++;
				c2 = *s;
				if (c2) {
					s++;
					c3 = *s;
					if (c3 && xform_html_names == 1) {
						q += xform_snprintf(&q, 34, "<span style=\"color:#%c%c%c%c%c%c\">", c1, c1, c2, c2, c3, c3);
						open++;
					}
				}
				break;
			}
		} else {
			*q++ = *s;
		}
	}

	while (open) {
		q += xform_strcpy(&q, "</span>");
		open--;
	}
	*q = '\0';

	return xform_strbuf();
}


static char *sof_colors[32] =
{
	"FFFFFF", "FFFFFF", "FF0000", "00FF00", "FFFF00", "0000FF", "FF00FF", "00FFFF", "000000", "7F7F7F", "702D07", "7F0000", "007F00",
	"FFFFFF", "007F7F", "00007F", "564D28", "4C5E36", "370B65", "005572", "54647E", "1E2A63", "66097B", "705E61", "980053",
	"960018", "702D07", "54492A", "61A997", "CB8F39", "CF8316", "FF8020"
};

/*
 * Transform a soldier of fortune player name
 */
static char *
xform_name_sof(char *string, struct qserver *server)
{
	unsigned char *s;
	char *q;

	q = xform_strbuf();
	s = (unsigned char*)string;

	// The may not be the intention but is needed for q1 at least
	for (; *s; s++) {
		int inc = xform_html_entity(*s, q);
		if (0 != inc) {
			q += inc;
			continue;
		}

		if (*s < ' ') {
			q += xform_html_color(&q, sof_colors[*(s)]);
		} else if (isprint(*s)) {
			*q++ = *s;
		// ## more fixes below; double check against real sof servers
		} else if (*s >= 0xa0) {
			*q++ = *s &0x7f;
		} else if (*s >= 0x92 && *s < 0x9c) {
			*q++ = '0' + (*s - 0x92);
		} else if (*s >= 0x12 && *s < 0x1c) {
			*q++ = '0' + (*s - 0x12);
		} else if (*s == 0x90 || *s == 0x10) {
			*q++ = '[';
		} else if (*s == 0x91 || *s == 0x11) {
			*q++ = ']';
		} else if (*s == 0xa || *s == 0xc || *s == 0xd) {
			*q++ = ']';
		}
	}
	*q = '\0';

	return xform_strbuf();
}

/*** Public Methods ***/

/*
 * perform a printf containing xform_name based arguments
 */
int
xform_printf(FILE *file, const char *format, ...)
{
	int ret;
	va_list args;

	xform_buf_reset();

	va_start(args, format);
	ret = vfprintf(file, format, args);
	va_end(args);

	return ret;
}

/*
 * Clear out and free all memory used by xform buffers
 */
void
xform_buf_free()
{
	struct xform *cur;
	struct xform *next;

	for (cur = xform_bufs; cur != NULL; cur = next) {
		next = cur->next;
		if (cur->buf != NULL) {
			free(cur->buf);
			cur->buf = NULL;
		}
		free(cur);
	}

	xform_bufs = NULL;
	xform_buf = NULL;
}

/*
 * Transforms a string based on the details stored on the server
 */
char *
xform_name(char *string, struct qserver *server)
{
	char *buf, *bufp, *s;
	int is_server_name;

	if (string == NULL) {
		buf = xform_buf_get(1);
		strcpy(buf, "?");

		return buf;
	}
	
	if (!xform_names) {
		return string;
	}

	s = string;

	if (xform_strip_unprintable) {
		buf = xform_buf_get(strlen(string));
		bufp = buf;
		for (; *s; s++) {
			if (isprint(*s)) {
				*bufp = *s;
				bufp++;
			}
		}
		*bufp = '\0';

		if (*buf == '\0') {
			strcpy(buf, "?");
			return buf;
		}
		s = buf;
	}

	is_server_name = (string == server->server_name);

	if ((xform_hex_player_names && !is_server_name) || (xform_hex_server_names && is_server_name)) {
		buf = xform_buf_get(strlen(s) * 2);
		bufp = buf;
		for (; *s; s++, bufp += 2) {
			sprintf(bufp, "%02hhx", *s);
		}
		*bufp = '\0';

		return buf;
	}

	buf = xform_buf_get(strlen(s));
	if (server->type->flags &TF_QUAKE3_NAMES) {
		s = xform_name_q3(s, server);
	} else if (!is_server_name && (server->type->flags &TF_TRIBES2_NAMES)) {
		s = xform_name_t2(s, server);
	} else if (server->type->flags &TF_U2_NAMES) {
		s = xform_name_u2(s, server);
	} else if (server->type->flags &TF_TM_NAMES) {
		s = xform_name_tm(s, server);
	} else if (!is_server_name || server->type->flags &TF_SOF_NAMES) {
		// Catch all for NOT is_server_name OR TF_SOF_NAMES
		s = xform_name_sof(s, server);
	}

	if (xform_font_tag) {
		xform_strcpy(&s, "</font>");
	}

	return s;
}

