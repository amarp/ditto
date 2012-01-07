
#include <ostream>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
//typedef unsigned long long uint64_t;

struct MAC {
    uint64_t val;
    MAC() {}
    MAC(const uint8_t *stream);
    MAC(uint64_t val);
    MAC(const MAC& o);

    bool operator==(const MAC& o) const {
	return val == o.val;
    }
    bool operator!=(const MAC& o) const {
	return val != o.val;
    }
    bool operator<(const MAC& o) const {
	return val < o.val;
    }

    static MAC broadcast;
    static MAC null;
};

std::ostream& operator<<(std::ostream& out, const MAC& mac);
std::ostream& operator<<(std::ostream& out, const struct in_addr& ip);

char *va(char *format, ...);

struct tok {
	int v;			/* value */
	const char *s;		/* string */
};

extern const char *
tok2str(register const struct tok *lp, register const char *fmt,
	register int v);
