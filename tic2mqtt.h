#ifndef __TICD_H__
#define __TICD_H__ 1

#define TIC_QOS 0

struct tag_desc {
    const char *tag; // Name of tag.
    const int len;   // Length of data.
    char *data;      // Last data received.
};

#endif /* __TICD_H__ */
