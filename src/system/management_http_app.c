#include "system/management_http_app.h"

#include "system/management_app.h"
#include "system/management_http_w5500.h"

static management_result_t execute_request(void* context,
                                           const management_request_t* request,
                                           management_response_t* response)
{
    return management_app_execute((app_ctx_t*)context, request, response);
}

static bool apply_effect(void* context, const management_effect_t* effect)
{
    return management_app_apply_effect((app_ctx_t*)context, effect);
}

void management_http_app_init(app_ctx_t* app)
{
    if (app == NULL)
    {
        return;
    }
    management_http_server_init(&app->http_server,
                                management_http_w5500_interface(),
                                execute_request, apply_effect, app);
}

void management_http_app_poll(app_ctx_t* app)
{
    if (app == NULL)
    {
        return;
    }
    management_http_server_poll(&app->http_server);
}
