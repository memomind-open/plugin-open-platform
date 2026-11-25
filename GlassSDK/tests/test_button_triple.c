#define TEST_PLUGIN_SOURCE "../examples/input/input.c"
#define main existing_call_tests
#include "test_examples_call.c"
#undef main

static char latest[40];
static void capture_text(gm_plugin_lvgl_obj_t *object, const char *text)
{
    (void)object;
    snprintf(latest, sizeof(latest), "%s", text);
}

int main(void)
{
    assert(existing_call_tests() == 0);
    gm_plugin_host_api_t host = {0};
    gm_plugin_descriptor_t plugin = {0};
    host.struct_size = sizeof(host); host.abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    host.capabilities = GM_PLUGIN_CAP_BUTTON;
    host.graphics.lvgl = &t_ui; host.display_get_info = t_info;
    host.extension_get = t_discover; host.log = t_log;
    plugin.struct_size = sizeof(plugin);
    t_ui.label_set_text = capture_text;
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK);
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_OK);
    gm_plugin_event_t event = {0};
    event.type = GM_PLUGIN_EVENT_BUTTON;
    event.data.button.button = GM_PLUGIN_BUTTON_PRIMARY;
    event.data.button.action = GM_PLUGIN_BUTTON_ACTION_TRIPLE;
    assert(GM_PLUGIN_BUTTON_ACTION_TRIPLE == 7);
    assert(plugin.on_event(plugin.context, &event));
    assert(strcmp(latest, "TRIPLE CLICK") == 0);
    event.data.button.action = GM_PLUGIN_BUTTON_ACTION_DOUBLE;
    assert(plugin.on_event(plugin.context, &event));
    assert(strcmp(latest, "DOUBLE CLICK") == 0);
    event.data.button.action = 999;
    assert(!plugin.on_event(plugin.context, &event));
    plugin.on_stop(plugin.context);
    puts("Button triple-click example passed");
    return 0;
}
