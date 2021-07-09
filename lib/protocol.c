/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "data.h"
#include "protocol.h"

enum {
        REPLY_CODE_ERROR = 0,
        REPLY_CODE_OK,
};

static bool is_reply(const uint8_t *buffer, size_t size, uint32_t code) {
        DataParser p;
        uint32_t parsed_code = 0;

        if (!data_parser_init(&p, buffer, size) ||
            !data_parser_get_id(&p, &parsed_code))
                return false;

        return parsed_code == code;
}

/* API */

size_t icipc_protocol_calculate_request_size(
                const char *name,
                const struct icipc_data *args) {
        assert (name);
        return sizeof(struct icipc_data) +
               data_type_string_calc_size(name) +
               (args ? data_type_raw_calc_size(args) : sizeof(struct icipc_data));
}

void icipc_protocol_build_request(
                uint8_t *buffer,
                size_t size,
                const char *name,
                const struct icipc_data *args) {
        DataBuilder b;
        const struct icipc_data none = { 0, DATA_TYPE_NONE };
        if (args == NULL)
                args = &none;

        data_builder_init(&b, buffer, size);
        data_builder_push_string(&b, name);
        data_builder_push_raw(&b, args);
}

bool icipc_protocol_parse_request(
                const uint8_t *buffer,
                size_t size,
                const char **name,
                const struct icipc_data **args) {
        DataParser p;
        const char *parsed_name = NULL;
        const struct icipc_data *parsed_args = NULL;

        if (!data_parser_init(&p, buffer, size) ||
            !data_parser_get_string(&p, &parsed_name) ||
            !data_parser_get_raw(&p, &parsed_args))
                return false;

        if (name != NULL)
                *name = parsed_name;
        if (args != NULL)
                *args = parsed_args;
        return true;
}

size_t icipc_protocol_calculate_reply_ok_size(const struct icipc_data *value) {
        return sizeof(struct icipc_data) +
               data_type_id_calc_size() +
               (value ? data_type_raw_calc_size(value) : sizeof(struct icipc_data));
}

size_t icipc_protocol_calculate_reply_error_size(const char *msg) {
        assert (msg);
        return sizeof(struct icipc_data) +
               data_type_id_calc_size() +
               data_type_string_calc_size(msg);
}

void icipc_protocol_build_reply_ok(
                uint8_t *buffer,
                size_t size,
                const struct icipc_data *value) {
        DataBuilder b;
        const struct icipc_data none = { 0, DATA_TYPE_NONE };
        if (value == NULL)
                value = &none;

        data_builder_init(&b, buffer, size);
        data_builder_push_id(&b, REPLY_CODE_OK);
        data_builder_push_raw(&b, value);
}

void icipc_protocol_build_reply_error(
                uint8_t *buffer,
                size_t size,
                const char *msg) {
        DataBuilder b;
        data_builder_init(&b, buffer, size);
        data_builder_push_id(&b, REPLY_CODE_ERROR);
        data_builder_push_string(&b, msg);
}

bool icipc_protocol_is_reply_ok(const uint8_t *buffer, size_t size) {
        return is_reply (buffer, size, REPLY_CODE_OK);
}

bool icipc_protocol_is_reply_error(const uint8_t *buffer, size_t size) {
        return is_reply (buffer, size, REPLY_CODE_ERROR);
}

bool icipc_protocol_parse_reply_ok(
                const uint8_t *buffer,
                size_t size,
                const struct icipc_data **value) {
        DataParser p;
        uint32_t parsed_code = 0;
        const struct icipc_data *parsed_value = NULL;

        if (!data_parser_init(&p, buffer, size) ||
            !data_parser_get_id(&p, &parsed_code) ||
            !data_parser_get_raw(&p, &parsed_value))
                return false;

        if (value != NULL)
                *value = parsed_value;
        return true;
}

bool icipc_protocol_parse_reply_error(
                const uint8_t *buffer,
                size_t size,
                const char **msg) {
        DataParser p;
        uint32_t parsed_code = 0;
        const char *parsed_msg = NULL;

        if (!data_parser_init(&p, buffer, size) ||
            !data_parser_get_id(&p, &parsed_code) ||
            !data_parser_get_string(&p, &parsed_msg))
                return false;

        if (msg != NULL)
                *msg = parsed_msg;
        return true;
}
