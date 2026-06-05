/*
 * value_render.c - jq-style colored pretty-printing of an ScValue (view layer).
 *
 * The view layer bridges serde + output and is opt-in: include
 * <view/sparcli_view.h> explicitly.
 *
 * Run (after `make`):
 *   make run-example EX=c/data/value_render
 */

#include <sparcli.h>
#include <serde/sparcli_serde.h>
#include <view/sparcli_view.h>

#include <string.h>


int main(void) {
    const char *json =
        "{\"name\":\"sparcli\",\"version\":\"0.1.0\","
        "\"features\":[\"output\",\"input\",\"serde\"],"
        "\"limits\":{\"width\":80,\"color\":true,\"ratio\":0.75}}";

    ScValue *root = sc_json_parse(json, strlen(json), NULL);
    sc_value_render(root, (ScValueRenderOpts){ .sort_keys = false });
    sc_value_free(root);
    return 0;
}
