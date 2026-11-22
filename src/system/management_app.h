#ifndef MANAGEMENT_APP_H
#define MANAGEMENT_APP_H

#include "system/app_context.h"
#include "system/management.h"

void management_app_capture(app_ctx_t* app, management_runtime_view_t* runtime);
management_result_t management_app_execute(app_ctx_t* app,
                                           const management_request_t* request,
                                           management_response_t* response);
bool management_app_apply_effect(app_ctx_t* app, const management_effect_t* effect);

#endif /* MANAGEMENT_APP_H */
