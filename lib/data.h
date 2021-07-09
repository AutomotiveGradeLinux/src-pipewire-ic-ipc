/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2018 Wim Taymans
 * Copyright © 2021 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Trimmed down version of struct spa_pod from PipeWire's Simple Plugin API
 * This aims to be compatible with spa_pod, so you can cast icipc_data
 * to spa_pod and still be able to use it */

struct icipc_data {
        uint32_t size;  /* size of the body */
        uint32_t type;  /* type of the body */
        /* type-specific body follows */
};

#define ICIPC_DATA_BODY_SIZE(d)  (((const struct icipc_data*)(d))->size)
#define ICIPC_DATA_TYPE(d)       (((const struct icipc_data*)(d))->type)
#define ICIPC_DATA_SIZE(d)       (sizeof(struct icipc_data) + ICIPC_DATA_BODY_SIZE(d))
#define ICIPC_DATA_BODY(d)       ((void*)(((uint8_t *) d) + sizeof(struct icipc_data)))
#define ICIPC_DATA_BODY_CONST(d) ((const void*)(((const uint8_t *) d) + sizeof(struct icipc_data)))

enum {
        DATA_TYPE_NONE = 1,     /* SPA_TYPE_None */
        DATA_TYPE_ID = 3,       /* SPA_TYPE_Id */
        DATA_TYPE_INT = 4,      /* SPA_TYPE_Int */
        DATA_TYPE_STRING = 8,   /* SPA_TYPE_String */
        DATA_TYPE_STRUCT = 14,  /* SPA_TYPE_Struct */
};

#define PTR_ALIGNMENT 8
#define ROUND_UP_TO_ALIGN(num) (((num) + (PTR_ALIGNMENT - 1)) & ~((PTR_ALIGNMENT) - 1))
#define PTR_IS_ALIGNED(p)      (((intptr_t)(p) & (PTR_ALIGNMENT-1)) == 0)

typedef struct DataBuilder {
        union {
                uint8_t *buffer;
                struct icipc_data *data;
        };
        union {
                uint8_t *ptr;
                struct icipc_data *ptr_as_data;
        };
        size_t buf_size;
} DataBuilder;

typedef struct DataParser {
        union {
                const uint8_t *buffer;
                const struct icipc_data *data;
        };
        union {
                const uint8_t *ptr;
                const struct icipc_data *ptr_as_data;
        };
        size_t buf_size;
} DataParser;

static inline void data_builder_init(
                DataBuilder *b,
                uint8_t *buffer,
                size_t size) {
        assert(size >= sizeof(struct icipc_data));
        memset(buffer, 0, size);
        b->buffer = buffer;
        b->buf_size = size;
        b->data->type = DATA_TYPE_STRUCT;
        b->ptr = ICIPC_DATA_BODY(b->data);
        assert(PTR_IS_ALIGNED(b->ptr));
}

static inline size_t data_type_id_calc_size(void) {
        return sizeof(struct icipc_data) + ROUND_UP_TO_ALIGN(sizeof(uint32_t));
}

static inline void data_builder_push_id(DataBuilder *b, uint32_t id) {

        const size_t body_size = ROUND_UP_TO_ALIGN(sizeof(uint32_t));
        const size_t additional_size = sizeof(struct icipc_data) + body_size;
        assert(b->buf_size >= ICIPC_DATA_SIZE(b->data) + additional_size);

        b->ptr_as_data->size = body_size;
        b->ptr_as_data->type = DATA_TYPE_ID;
        *((uint32_t *) ICIPC_DATA_BODY(b->ptr)) = id;

        b->data->size += additional_size;
        b->ptr += additional_size;
        assert(PTR_IS_ALIGNED(b->ptr));
}

static inline size_t data_type_string_calc_size(const char *str) {
        size_t str_size = strlen(str) + 1;
        return sizeof(struct icipc_data) + ROUND_UP_TO_ALIGN(str_size);
}

static inline void data_builder_push_string(DataBuilder *b, const char *str) {

        size_t str_size = strlen(str) + 1;
        const size_t body_size = ROUND_UP_TO_ALIGN(str_size);
        const size_t additional_size = sizeof(struct icipc_data) + body_size;
        assert(b->buf_size >= ICIPC_DATA_SIZE(b->data) + additional_size);

        b->ptr_as_data->size = body_size;
        b->ptr_as_data->type = DATA_TYPE_STRING;
        strncpy((char *) ICIPC_DATA_BODY(b->ptr), str, str_size);

        b->data->size += additional_size;
        b->ptr += additional_size;
        assert(PTR_IS_ALIGNED(b->ptr));
}

static inline size_t data_type_raw_calc_size(const struct icipc_data *raw) {
        return sizeof(struct icipc_data) +
               ROUND_UP_TO_ALIGN(ICIPC_DATA_BODY_SIZE(raw));
}

static inline void data_builder_push_raw(
                DataBuilder *b,
                const struct icipc_data *raw) {

        const size_t body_size = ROUND_UP_TO_ALIGN(ICIPC_DATA_BODY_SIZE(raw));
        const size_t additional_size = sizeof(struct icipc_data) + body_size;
        assert(b->buf_size >= ICIPC_DATA_SIZE(b->data) + additional_size);

        b->ptr_as_data->size = body_size;
        b->ptr_as_data->type = raw->type;
        memcpy(ICIPC_DATA_BODY(b->ptr),
               ICIPC_DATA_BODY(raw),
               ICIPC_DATA_BODY_SIZE(raw));

        b->data->size += additional_size;
        b->ptr += additional_size;
        assert(PTR_IS_ALIGNED(b->ptr));
}

static inline bool data_parser_init(
                DataParser *p,
                const uint8_t *buffer,
                size_t size) {
        assert(size >= sizeof(struct icipc_data));
        p->buffer = buffer;
        p->buf_size = size;

        if (p->data->type != DATA_TYPE_STRUCT ||
            ICIPC_DATA_SIZE(p->data) > size)
                return false;

        p->ptr = ICIPC_DATA_BODY(p->data);
        assert(PTR_IS_ALIGNED(p->ptr));
        return true;
}

static inline bool data_parser_get_id(DataParser *p, uint32_t *id) {

        if (p->ptr_as_data->type != DATA_TYPE_ID ||
            ICIPC_DATA_BODY_SIZE(p->ptr) != ROUND_UP_TO_ALIGN(sizeof(uint32_t)))
                return false;

        *id = *((const uint32_t *) ICIPC_DATA_BODY_CONST(p->ptr));

        p->ptr += ICIPC_DATA_SIZE(p->ptr);
        assert(PTR_IS_ALIGNED(p->ptr));
        return true;
}

static inline bool data_parser_get_string(DataParser *p, const char **str) {

        if (p->ptr_as_data->type != DATA_TYPE_STRING ||
            ICIPC_DATA_BODY_SIZE(p->ptr) == 0 ||
            ICIPC_DATA_BODY_SIZE(p->ptr) !=
                ROUND_UP_TO_ALIGN(ICIPC_DATA_BODY_SIZE(p->ptr)) ||
            (p->ptr - p->buffer) + ICIPC_DATA_SIZE(p->ptr) > p->buf_size)
                return false;

        *str = (const char *) ICIPC_DATA_BODY_CONST(p->ptr);

        /* check that the string has a terminating null character */
        if (strnlen(*str, ICIPC_DATA_BODY_SIZE(p->ptr)) ==
            ICIPC_DATA_BODY_SIZE(p->ptr))
                return false;

        p->ptr += ICIPC_DATA_SIZE(p->ptr);
        assert(PTR_IS_ALIGNED(p->ptr));
        return true;
}

static inline bool data_parser_get_raw(
                DataParser *p,
                const struct icipc_data **raw) {
        if (ICIPC_DATA_BODY_SIZE(p->ptr) !=
                ROUND_UP_TO_ALIGN(ICIPC_DATA_BODY_SIZE(p->ptr)) ||
            (p->ptr - p->buffer) + ICIPC_DATA_SIZE(p->ptr) > p->buf_size)
                return false;

        *raw = p->ptr_as_data;

        p->ptr += ICIPC_DATA_SIZE(p->ptr);
        assert(PTR_IS_ALIGNED(p->ptr));
        return true;
}
