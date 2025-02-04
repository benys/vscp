// duktape_vscp_func.c
//
// This file is part of the VSCP (https://www.vscp.org)
//
// The MIT License (MIT)
//
// Copyright © 2000-2020 Ake Hedman, Grodans Paradis AB
// <info@grodansparadis.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// wxJSON - http://wxcode.sourceforge.net/docs/wxjson/wxjson_tutorial.html
//

#ifdef WIN32
#include <winsock2.h>
#endif

#include <list>
#include <string>

#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <json.hpp> // Needs C++11  -std=c++11

#include <actioncodes.h>
#include <controlobject.h>
//#include <duk_module_node.h>
#include <duktape.h>
#include <duktape_vscp_func.h>
//#include <remotevariable.h>
#include <userlist.h>
#include <version.h>
#include <vscp.h>
#include <vscpdb.h>
#include <vscphelper.h>
#include <vscpremotetcpif.h>

using namespace std;

// https://github.com/nlohmann/json
using json = nlohmann::json;

///////////////////////////////////////////////////
//                   GLOBALS
///////////////////////////////////////////////////

extern CControlObject* gpobj;

///////////////////////////////////////////////////
//                  HELPERS
///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// get_js_Event
//
// Make event from JSON data object on stack
//

bool
get_js_Event(duk_context* ctx, vscpEventEx* pex)
{
    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        return false;
    }

    // Must be an event allocated
    if (NULL == pex) {
        // Not good
        return false;
    }

    // get Head
    pex->head = 0;
    duk_push_string(ctx, "head");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        pex->head =
          (uint16_t)duk_get_int_default(ctx, -1, VSCP_PRIORITY_NORMAL);
    }
    duk_pop(ctx);

    // get timestamp
    pex->timestamp = 0;
    duk_push_string(ctx, "timestamp");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        pex->timestamp = (uint32_t)duk_get_number_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get obid
    pex->obid = 0;
    duk_push_string(ctx, "obid");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        pex->obid = (uint32_t)duk_get_number_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get VSCP class
    pex->vscp_class = 0;
    duk_push_string(ctx, "class");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        pex->vscp_class = (uint16_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get VSCP type
    pex->vscp_type = 0;
    duk_push_string(ctx, "type");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        pex->vscp_type = (uint16_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // GUID
    memset(pex->GUID, 0, 16);
    duk_push_string(ctx, "guid");
    duk_get_prop(ctx, -2);
    if (duk_is_string(ctx, -1)) {
        const char* pGUID = duk_get_string_default(
          ctx, -1, "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00");
        vscp_setEventExGuidFromString(pex, pGUID);
    }
    duk_pop(ctx);

    // get time(block)
    vscpdatetime dt;
    vscp_setEventExDateTimeBlockToNow(pex);
    duk_push_string(ctx, "time");
    duk_get_prop(ctx, -2);
    if (duk_is_string(ctx, -1)) {
        const char* pTime = duk_get_string_default(ctx, -1, "");
        if (dt.set(pTime)) {
            pex->year = dt.getYear();
            pex->month = dt.getMonth() + 1;
            pex->day = dt.getDay();
            pex->hour = dt.getHour();
            pex->minute = dt.getMinute();
            pex->second = dt.getSecond();
        } else {
            pex->year = vscpdatetime::UTCNow().getYear();
            pex->month = vscpdatetime::UTCNow().getMonth() + 1;
            pex->day = vscpdatetime::UTCNow().getDay();
            pex->hour = vscpdatetime::UTCNow().getHour();
            pex->minute = vscpdatetime::UTCNow().getMinute();
            pex->second = vscpdatetime::UTCNow().getSecond();
        }
    }
    duk_pop(ctx);

    // get data
    pex->sizeData = 0;
    memset(pex->data, 0, VSCP_MAX_DATA);
    duk_push_string(ctx, "data");
    duk_get_prop(ctx, -2);

    if (duk_is_array(ctx, -1)) {
        int lengthArray = duk_get_length(ctx, -1);
        // Make sure size is valid
        if (lengthArray > VSCP_MAX_DATA)
            lengthArray = VSCP_MAX_DATA;
        pex->sizeData = lengthArray;
        for (int i = 0; i < lengthArray; i++) {
            if (duk_get_prop_index(ctx, -1, i)) {
                pex->data[i] = (uint8_t)duk_get_int_default(ctx, -1, 0);
            }
            duk_pop(ctx);
        }
    }
    duk_pop(ctx);

    return true;
}

///////////////////////////////////////////////////
//                  CALLABLES
///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// js_resolve_module
//

duk_ret_t
js_resolve_module(duk_context* ctx)
{
    const char* module_id;
    const char* parent_id;

    module_id = duk_require_string(ctx, 0);
    parent_id = duk_require_string(ctx, 1);

    duk_push_sprintf(ctx, "%s.js", module_id);
    printf("resolve_cb: id:'%s', parent-id:'%s', resolve-to:'%s'\n",
           module_id,
           parent_id,
           duk_get_string(ctx, -1));

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// js_load_module
//

duk_ret_t
js_load_module(duk_context* ctx)
{
    const char* filename;
    const char* module_id;

    // TODO Modules can only be loaded from /var/lib/vscp/javascript or
    // configured other location

    module_id = duk_require_string(ctx, 0);
    duk_get_prop_string(ctx, 2, "filename");
    filename = duk_require_string(ctx, -1);

    printf("load_cb: id:'%s', filename:'%s'\n", module_id, filename);

    if (strcmp(module_id, "pig.js") == 0) {
        duk_push_sprintf(
          ctx,
          "module.exports = 'you\\'re about to get eaten by %s';",
          module_id);
    } else if (strcmp(module_id, "cow.js") == 0) {
        duk_push_string(ctx, "module.exports = require('pig');");
    } else if (strcmp(module_id, "ape.js") == 0) {
        duk_push_string(ctx,
                        "module.exports = { module: module, __filename: "
                        "__filename, wasLoaded: module.loaded };");
    } else if (strcmp(module_id, "badger.js") == 0) {
        duk_push_string(ctx, "exports.foo = 123; exports.bar = 234;");
    } else if (strcmp(module_id, "comment.js") == 0) {
        duk_push_string(ctx,
                        "exports.foo = 123; exports.bar = 234; // comment");
    } else if (strcmp(module_id, "shebang.js") == 0) {
        duk_push_string(ctx,
                        "#!ignored\nexports.foo = 123; exports.bar = 234;");
    } else {
        // duk_error(ctx, DUK_ERR_TYPE_ERROR, "cannot find module: %s",
        // module_id);
    }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_print
//
duk_ret_t
js_vscp_print(duk_context* ctx)
{
    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_join(ctx, duk_get_top(ctx) - 1);
    // xxPrintf( "%s\n", duk_safe_to_string(ctx, -1) );
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_log
//
// vscp_log("message"[,log-level,log-type])

duk_ret_t
js_vscp_log(duk_context* ctx)
{
    //int nArgs = duk_get_top(ctx);
    std::string wxDebug = duk_get_string_default(ctx, -1, "---Log fail---");
    syslog(LOG_INFO, "%s", wxDebug.c_str());

    duk_pop_n(ctx, 1);        // Clear stack
    duk_push_boolean(ctx, 1); // Return success
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_sleep
//
// vscp_sleep(Time-to-sleep-in-ms)
//

duk_ret_t
js_vscp_sleep(duk_context* ctx)
{
    uint32_t sleep_ms = 1000;

    int nArgs = duk_get_top(ctx);
    if (nArgs == 0) {
        duk_push_number(ctx, 1000);
    }

    if (nArgs) {
        sleep_ms = duk_to_uint32(ctx, -1);
    }

    usleep(sleep_ms * 1000);

    duk_pop_n(ctx, 1);        // Clear stack
    duk_push_boolean(ctx, 1); // Return success
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_readVariable
//
// set var = vscp_readvariable("name");
//
//{
//	"name": "variable-name",
//	"type": 1,
//	"user": 2,
//	"accessrights": 0x777,
//      "persistence": true|false,
//	"lastchange": "YYYYMMDDTHHMMSS",
//      "isnumerical": true|false
//      "isbase64": true|false
//	"value": "This is a test variable",
//	"note": "This is a note about this variable"
//}
//

duk_ret_t
js_vscp_readVariable(duk_context* ctx)
{
    // CVariable variable;
    // std::string strResult;

    // // get the variable name
    // std::string varName = duk_get_string_default(ctx, -1, "");
    // if (0 == varName.length()) {
    //     duk_pop_n(ctx, 1);  // Clear stack
    //     duk_push_null(ctx); // Return failure
    //     return JAVASCRIPT_OK;
    // }

    // duk_pop_n(ctx, 3); // Clear stack

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if (!gpobj->m_variables.find(varName, pAdminUser, variable)) {
    //     duk_push_null(ctx); // Return failure
    //     return JAVASCRIPT_OK;
    // }

    // // get the variable on JSON format
    // std::string varJSON;
    // variable.getAsJSON(varJSON);
    // duk_push_string(ctx, (const char *)varJSON.c_str());
    // duk_json_decode(ctx, -1);

    // duk_get_prop_string(ctx, -1, "name");
    // std::string str = vscp_str_format("name=%s", duk_to_string(ctx, -1));
    // duk_pop_n(ctx, 1); // Clear stack

    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_writeVariable
//
// writeVariable("name",value )
//

duk_ret_t
js_vscp_writeVariable(duk_context* ctx)
{
    // std::string varName;
    // CVariable variable;
    // duk_ret_t err;

    // //  Should be a JSON variable object
    // if (!duk_is_object(ctx, -1)) {
    //     duk_push_boolean(ctx, 0); // return code false
    //     return JAVASCRIPT_OK;
    // }

    // duk_push_string(ctx, "name");
    // duk_get_prop(ctx, -2);
    // varName = duk_get_string_default(ctx, -1, "");
    // duk_pop_n(ctx, 1);
    // if (0 == varName.length()) {
    //     duk_push_boolean(ctx, 0); // return code false
    //     return JAVASCRIPT_OK;
    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if (!gpobj->m_variables.find(varName, pAdminUser, variable)) {

    //     // Variable does not exist - should be created

    //     // get the type
    //     duk_push_string(ctx, "type");
    //     duk_get_prop(ctx, -2);

    //     uint16_t type;
    //     if (duk_is_number(ctx, -1)) {
    //         type = (uint16_t)duk_get_int_default(
    //           ctx, -1, VSCP_DAEMON_VARIABLE_CODE_STRING);
    //     } else if (duk_is_string(ctx, -1)) {
    //         std::string str = duk_get_string_default(ctx, -1, "string");
    //         duk_pop_n(ctx, 1);
    //         type = CVariable::getVariableTypeFromString(str);
    //     } else {
    //         duk_push_boolean(ctx, 0); // return code false
    //         return JAVASCRIPT_OK;
    //     }
    //     duk_pop_n(ctx, 1);

    //     // get the value
    //     std::string strValue;
    //     duk_push_string(ctx, "value");
    //     duk_get_prop(ctx, -2);

    //     // The value can be number, boolean, string
    //     if (duk_is_number(ctx, -1)) {
    //         double val = duk_get_number_default(ctx, -1, 0.0);
    //         strValue   = vscp_str_format("%lf", val);
    //     } else if (duk_is_string(ctx, -1)) {
    //         strValue = duk_get_string_default(ctx, -1, "");
    //     } else if (duk_is_boolean(ctx, -1)) {
    //         bool bval = duk_get_boolean_default(ctx, -1, false);
    //         strValue  = vscp_str_format("%s", bval ? "true" : "false");
    //     } else {
    //         duk_push_boolean(ctx, 0); // return code false
    //         return JAVASCRIPT_OK;
    //     }
    //     duk_pop_n(ctx, 1);

    //     // get user
    //     uint32_t userid;
    //     duk_push_string(ctx, "user");
    //     duk_get_prop(ctx, -2);

    //     // The user can be given as a number or a  string
    //     if (duk_is_number(ctx, -1)) {
    //         userid = (uint32_t)duk_get_number_default(ctx, -1, 0.0);
    //     } else if (duk_is_string(ctx, -1)) {
    //         std::string strUser(duk_get_string_default(ctx, -1, ""));
    //         CUserItem *pUser;
    //         pUser = gpobj->m_userList.getUser(strUser);
    //         if (NULL == pUser) {
    //             duk_push_boolean(ctx, 0); // return code false
    //             return JAVASCRIPT_OK;
    //         }
    //         userid = pUser->getUserID();
    //     } else {
    //         duk_push_boolean(ctx, 0); // return code false
    //         return JAVASCRIPT_OK;
    //     }

    //     duk_pop_n(ctx, 1);

    //     // get rights (if there)
    //     uint32_t accessRights = PERMISSION_OWNER_ALL;
    //     duk_push_string(ctx, "accessrights");
    //     duk_get_prop(ctx, -2);
    //     if (duk_is_number(ctx, -1)) {
    //         accessRights =
    //           (uint32_t)duk_get_number_default(ctx, -1,
    //           PERMISSION_OWNER_ALL);
    //     }

    //     duk_pop_n(ctx, 1);

    //     // get persistence (if there)
    //     bool bPersistense = false;
    //     duk_push_string(ctx, "persistence");
    //     duk_get_prop(ctx, -2);

    //     if (duk_is_boolean(ctx, -1)) {
    //         bPersistense = duk_get_boolean_default(ctx, -1, false);
    //     }

    //     duk_pop_n(ctx, 1);

    //     // get note (if there)
    //     std::string strNote;
    //     duk_push_string(ctx, "note");
    //     duk_get_prop(ctx, -2);

    //     if (duk_is_string(ctx, -1)) {
    //         strNote = duk_get_string_default(ctx, -1, "");
    //     }

    //     duk_pop_n(ctx, 1);

    //     if (!gpobj->m_variables.add(varName,
    //                                 strValue,
    //                                 type,
    //                                 userid,
    //                                 bPersistense,
    //                                 accessRights,
    //                                 strNote)) {
    //     }

    // } else {

    //     duk_push_string(ctx, "value");
    //     duk_get_prop(ctx, -2);

    //     // The value can be number, boolean, string
    //     if (duk_is_number(ctx, -1)) {
    //         double val         = duk_get_number_default(ctx, -1, 0.0);
    //         std::string strval = vscp_str_format("%lf", val);
    //         variable.setValueFromString(variable.getType(), strval);
    //     } else if (duk_is_string(ctx, -1)) {
    //         std::string strval(duk_get_string_default(ctx, -1, ""));
    //         variable.setValueFromString(variable.getType(), strval);
    //     } else if (duk_is_boolean(ctx, -1)) {
    //         bool bval = duk_get_boolean_default(ctx, -1, false);
    //         variable.setValueFromString(variable.getType(),
    //                                     bval ? "true" : "false");
    //     } else {
    //         duk_push_boolean(ctx, 0); // return code false
    //         return JAVASCRIPT_OK;
    //     }

    //     int nArgs = duk_get_top(ctx);
    //     duk_pop_n(ctx, 2); // Clear stack

    //     // Update variable storage
    //     if (!gpobj->m_variables.update(variable, pAdminUser)) {
    //         duk_push_boolean(ctx, 0); // return code false
    //         return JAVASCRIPT_OK;
    //     }
    // }

    duk_push_boolean(ctx, 1); // return code success
    return JAVASCRIPT_OK;
}

// TODO  writevalue
// TODO writeNote

///////////////////////////////////////////////////////////////////////////////
// js_vscp_deleteVariable
//

duk_ret_t
js_vscp_deleteVariable(duk_context* ctx)
{
    // std::string varName;
    // CVariable variable;
    // bool bResult;

    // // get variable name
    // varName = duk_get_string_default(ctx, -1, "");
    // duk_pop_n(ctx, 1);
    // if (0 == varName.length()) {
    //     duk_push_boolean(ctx, 0); // return code false
    //     return JAVASCRIPT_OK;
    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if (!gpobj->m_variables.remove(varName,pAdminUser)) {
    //     duk_push_boolean(ctx, 0); // return code false
    //     return JAVASCRIPT_OK;
    // }

    duk_push_boolean(ctx, 1); // return code success
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_sendEvent
//
// {
//    "time": "2017-01-13T10:16:02",
//    "head": 2,
//    "timestamp":50817,
//    "obid"; 123,
//    "class": 10,
//    "type": 8,
//    "guid": "00:00:00:00:00:00:00:00:00:00:00:00:00:01:00:02",
//    "data": [1,2,3,4,5,6,7],
//    "note": "This is some text"
// }
//

duk_ret_t
js_vscp_sendEvent(duk_context* ctx)
{
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        // Not good
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    duk_pop_n(ctx, 1);

    // Send the event
    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;
    vscp_convertEventExToEvent(pEvent, &ex);

    duk_push_global_object(ctx); /* -> stack: [ global ] */
    duk_push_string(
      ctx, "vscp_clientitem"); /* -> stack: [ global "vscp_clientItem" ] */
    duk_get_prop(ctx, -2);     /* -> stack: [ global vscp_clientItem ] */
    CClientItem* pClientItem = (CClientItem*)duk_get_pointer(ctx, -1);
    if (NULL == pClientItem) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }
    duk_pop_n(ctx, 2);

    if (!gpobj->sendEvent(pClientItem, pEvent)) {
        // Failed to send event
        vscp_deleteEvent_v2(&pEvent);
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    vscp_deleteEvent_v2(&pEvent);

    duk_push_boolean(ctx, 1); // return code success
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_getEvent
//

duk_ret_t
js_vscp_getEvent(duk_context* ctx)
{
    duk_push_global_object(ctx); /* -> stack: [ global ] */
    duk_push_string(
      ctx, "vscp_clientitem"); /* -> stack: [ global "vscp_clientItem" ] */
    duk_get_prop(ctx, -2);     /* -> stack: [ global vscp_clientItem ] */
    CClientItem* pClientItem = (CClientItem*)duk_get_pointer(ctx, -1);
    if (NULL == pClientItem) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }
    duk_pop_n(ctx, 2);

try_again:

    // Check the client queue
    if (pClientItem->m_bOpen && pClientItem->m_clientInputQueue.size()) {

        vscpEvent* pEvent;

        pthread_mutex_lock(&pClientItem->m_mutexClientInputQueue);
        pEvent = pClientItem->m_clientInputQueue.front();
        pClientItem->m_clientInputQueue.pop_front();

        if (NULL == pEvent) {

            // Exception
            duk_push_null(ctx); // return code failure
            return JAVASCRIPT_OK;
        }

        pthread_mutex_unlock(&pClientItem->m_mutexClientInputQueue);

        if (NULL != pEvent) {

            if (vscp_doLevel2Filter(pEvent, &pClientItem->m_filter)) {

                // Write it out

                std::string strResult;
                vscp_convertEventToJSON(strResult, pEvent);
                // Event is not needed anymore
                vscp_deleteEvent(pEvent);
                duk_push_string(ctx, (const char*)strResult.c_str());
                duk_json_decode(ctx, -1);

                // All OK return event
                return JAVASCRIPT_OK;

            } else {

                // Filtered out
                vscp_deleteEvent(pEvent);
                goto try_again;
            }

            // Remove the event
            vscp_deleteEvent(pEvent);

        } // Valid pEvent pointer
        else {
            // NULL event
            duk_push_null(ctx); // return code failure
            return JAVASCRIPT_OK;
        }

    } // events available

    //int nArgs = duk_get_top(ctx);

    // Fail
    duk_push_null(ctx); // return code failure
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_getCountEvent
//
//

duk_ret_t
js_vscp_getCountEvent(duk_context* ctx)
{
    int count = 0;

    duk_push_global_object(ctx); /* -> stack: [ global ] */
    duk_push_string(
      ctx, "vscp_clientitem"); /* -> stack: [ global "vscp_clientItem" ] */
    duk_get_prop(ctx, -2);     /* -> stack: [ global vscp_clientItem ] */
    CClientItem* pClientItem = (CClientItem*)duk_get_pointer(ctx, -1);
    if (NULL == pClientItem) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }
    duk_pop_n(ctx, 2);

    if (pClientItem->m_bOpen) {
        count = pClientItem->m_clientInputQueue.size();
    } else {
        count = 0;
    }

    duk_push_number(ctx, count); // return count
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_vscp_setFilter
//
// {
//     'mask_priority': number,
//     'mask_class': number,
//     'mask_type': number,
//     'mask_guid': 'string',
//     'filter_priority'; number,
//     'filter_class': number,
//     'filter_type': number,
//     'filter_guid' 'string'
// }

duk_ret_t
js_vscp_setFilter(duk_context* ctx)
{
    vscpEventFilter filter;

    duk_push_global_object(ctx); /* -> stack: [ global ] */
    duk_push_string(
      ctx, "vscp_clientitem"); /* -> stack: [ global "vscp_clientItem" ] */
    duk_get_prop(ctx, -2);     /* -> stack: [ global vscp_clientItem ] */
    CClientItem* pClientItem = (CClientItem*)duk_get_pointer(ctx, -1);
    if (NULL == pClientItem) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }
    duk_pop_n(ctx, 2);

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_boolean(ctx, 0); // return code failure
        return JAVASCRIPT_OK;
    }

    // Mask priority
    duk_push_string(ctx, "mask_priority");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        filter.mask_priority = (uint8_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // Mask class
    duk_push_string(ctx, "mask_class");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        filter.mask_class = (uint16_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // Mask type
    duk_push_string(ctx, "mask_type");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        filter.mask_type = (uint16_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // mask GUID
    memset(filter.mask_GUID, 0, 16); // Default is don't care
    duk_push_string(ctx, "mask_guid");
    duk_get_prop(ctx, -2);
    if (duk_is_string(ctx, -1)) {
        const char* pGUID = duk_get_string_default(
          ctx, -1, "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00");
        vscp_getGuidFromStringToArray(filter.mask_GUID, pGUID);
    }
    duk_pop(ctx);

    // Filter priority
    duk_push_string(ctx, "filter_priority");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        filter.filter_priority = (uint8_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // Filter class
    duk_push_string(ctx, "filter_class");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        filter.filter_class = (uint16_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // Filter type
    duk_push_string(ctx, "filter_type");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        filter.filter_type = (uint16_t)duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // filter GUID
    memset(filter.filter_GUID, 0, 16);
    duk_push_string(ctx, "filter_guid");
    duk_get_prop(ctx, -2);
    if (duk_is_string(ctx, -1)) {
        const char* pGUID = duk_get_string_default(
          ctx, -1, "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00");
        vscp_getGuidFromStringToArray(filter.filter_GUID, pGUID);
    }
    duk_pop(ctx);

    // Set the filter
    vscp_copyVSCPFilter(&pClientItem->m_filter, &filter);

    duk_push_boolean(ctx, 1); // return code success
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_send_Measurement
//
// {
//     'level': 1|2, (defaults to 2)
//     'string': true|false,  (default false, only valid for level II event)
//     'value': 123.5,
//     'guid': FF:FF:80... defaults to
//     "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00" 'vscptype': 6, 'unit':
//     1, (defaults to 0) 'sensorindex': 0, (defaults to 0) 'zone': 0, (defaults
//     to 0) 'subzone': 0 (defaults to 0)
// }
//

duk_ret_t
js_send_Measurement(duk_context* ctx)
{
    vscpEvent* pEvent;
    double value;         // Measurement value
    bool bLevel2 = true;  // True if level II
    bool bString = false; // If level II string or float
    int type;             // VSCP type
    int unit = 0;
    int sensoridx = 0;
    int zone = 0;
    int subzone = 0;

    duk_push_global_object(ctx); /* -> stack: [ global ] */
    duk_push_string(
      ctx, "vscp_clientitem"); /* -> stack: [ global "vscp_clientItem" ] */
    duk_get_prop(ctx, -2);     /* -> stack: [ global vscp_clientItem ] */
    CClientItem* pClientItem = (CClientItem*)duk_get_pointer(ctx, -1);
    if (NULL == pClientItem) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }
    duk_pop_n(ctx, 2);

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    // get measurement Level
    duk_push_string(ctx, "level");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        int level = duk_get_int_default(ctx, -1, 0);
        if (2 != level)
            bLevel2 = false;
    }
    duk_pop(ctx);

    // get measurement string flag
    duk_push_string(ctx, "bstring");
    duk_get_prop(ctx, -2);
    if (duk_is_boolean(ctx, -1)) {
        bString = duk_get_boolean_default(ctx, -1, true);
    }
    duk_pop(ctx);

    // get measurement Value
    duk_push_string(ctx, "value");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        value = duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get measurement GUID
    uint8_t guid[16];
    memset(guid, 0, 16);
    duk_push_string(ctx, "guid");
    duk_get_prop(ctx, -2);
    if (duk_is_string(ctx, -1)) {
        const char* pGUID = duk_get_string_default(
          ctx, -1, "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00");
        vscp_getGuidFromStringToArray(guid, pGUID);
    }
    duk_pop(ctx);

    // get measurement VSCP type
    duk_push_string(ctx, "type");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        type = duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get measurement VSCP unit
    duk_push_string(ctx, "unit");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        unit = duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get measurement sensorindex
    duk_push_string(ctx, "sensorindex");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        sensoridx = duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get measurement zone
    duk_push_string(ctx, "zone");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        zone = duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    // get measurement subzone
    duk_push_string(ctx, "subzone");
    duk_get_prop(ctx, -2);
    if (duk_is_number(ctx, -1)) {
        subzone = duk_get_int_default(ctx, -1, 0);
    }
    duk_pop(ctx);

    if (bLevel2) {

        if (bString) {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }
            pEvent->pdata = NULL;

            // Set GUID
            memcpy(pEvent->GUID, guid, 16);

            if (!vscp_makeLevel2StringMeasurementEvent(
                  pEvent, type, value, unit, sensoridx, zone, subzone)) {
                // Failed
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }

        } else {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
                ;
            }
            pEvent->pdata = NULL;

            // Set GUID
            memcpy(pEvent->GUID, guid, 16);

            if (!vscp_makeLevel2FloatMeasurementEvent(
                  pEvent, type, value, unit, sensoridx, zone, subzone)) {
                // Failed
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }
        }

    } else {

        // Level I

        if (bString) {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }

            memcpy(pEvent->GUID, guid, 16);
            pEvent->vscp_type = type;
            pEvent->vscp_class = VSCP_CLASS1_MEASUREMENT;
            pEvent->obid = 0;
            pEvent->timestamp = 0;
            pEvent->pdata = NULL;

            if (!vscp_makeStringMeasurementEvent(
                  pEvent, value, unit, sensoridx)) {
                vscp_deleteEvent(pEvent);
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }

            //

        } else {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }

            memcpy(pEvent->GUID, guid, 16);
            pEvent->vscp_type = type;
            pEvent->vscp_class = VSCP_CLASS1_MEASUREMENT;
            pEvent->obid = 0;
            pEvent->timestamp = 0;
            pEvent->pdata = NULL;

            if (!vscp_makeFloatMeasurementEvent(
                  pEvent, value, unit, sensoridx)) {
                vscp_deleteEvent(pEvent);
                duk_push_boolean(ctx, 0); // return code failure
                return JAVASCRIPT_OK;
            }
        }
    }

    // Send the event
    if (!gpobj->sendEvent(pClientItem, pEvent)) {
        // Failed to send event
        vscp_deleteEvent_v2(&pEvent);
        duk_push_boolean(ctx, 0); // return code failure
        return JAVASCRIPT_OK;
    }

    vscp_deleteEvent_v2(&pEvent);

    duk_push_boolean(ctx, 0); // return code success
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_is_Measurement
//

duk_ret_t
js_is_Measurement(duk_context* ctx)
{
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        // Not good
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_boolean(ctx, 0); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    bool bMeasurement = vscp_isMeasurement(pEvent);
    vscp_deleteEvent(pEvent);

    duk_push_boolean(ctx, bMeasurement ? 1 : 0); // return code false
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_get_MeasurementValue
//

duk_ret_t
js_get_MeasurementValue(duk_context* ctx)
{
    double value;
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    vscp_getMeasurementAsDouble(&value, pEvent);
    vscp_deleteEvent(pEvent);

    duk_push_number(ctx, value);
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_get_MeasurementUnit
//

duk_ret_t
js_get_MeasurementUnit(duk_context* ctx)
{
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int unit = vscp_getMeasurementUnit(pEvent);
    vscp_deleteEvent(pEvent);

    duk_push_number(ctx, unit);
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_get_MeasurementSensorIndex
//

duk_ret_t
js_get_MeasurementSensorIndex(duk_context* ctx)
{
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int sensorindex = vscp_getMeasurementSensorIndex(pEvent);
    vscp_deleteEvent(pEvent);

    duk_push_number(ctx, sensorindex);
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_get_MeasurementZone
//

duk_ret_t
js_get_MeasurementZone(duk_context* ctx)
{
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int zone = vscp_getMeasurementZone(pEvent);
    vscp_deleteEvent(pEvent);

    duk_push_number(ctx, zone);
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_get_MeasurementSubZone
//

duk_ret_t
js_get_MeasurementSubZone(duk_context* ctx)
{
    vscpEventEx ex;

    //  Should be a JSON variable object
    if (!duk_is_object(ctx, -1)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    if (!get_js_Event(ctx, &ex)) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        duk_push_null(ctx); // return code false
        return JAVASCRIPT_OK;
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int subzone = vscp_getMeasurementSubZone(pEvent);
    vscp_deleteEvent(pEvent);

    duk_push_number(ctx, subzone);
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_connect
//
// Connect to remote server
//

duk_ret_t
js_tcpip_connect(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_connect_ssl
//
// Connect to remote server using SSL
//

duk_ret_t
js_tcpip_connect_ssl(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_connect_info
//
// get connection info
//      remote address
//      remote port
//      server address
//      server port
//      bSSL
//

duk_ret_t
js_tcpip_connect_info(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_close
//
// Close connection to remote server
//

duk_ret_t
js_tcpip_close(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_write
//
// Write data remote sever
//

duk_ret_t
js_tcpip_write(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_read
//
// Read data from remote sever
//

duk_ret_t
js_tcpip_read(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_get_response
//
// Wait for response from remote sever
//

duk_ret_t
js_tcpip_get_response(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_tcpip_download
//
// Download data from remote web server
//

duk_ret_t
js_tcpip_download(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_get_httpd_version
//
// get version for httpd code
//

duk_ret_t
js_get_httpd_version(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_url_decode
//
// URL-decode input buffer into destination buffer.
//

duk_ret_t
js_url_decode(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_url_encode
//
// URL-encode input buffer into destination buffer.
//

duk_ret_t
js_url_encode(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_websocket_connect
//
// Connect to remote websocket client.
//

duk_ret_t
js_websocket_connect(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_websocket_write
//
// URL-encode input buffer into destination buffer.
//

duk_ret_t
js_websocket_write(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_websocket_read
//
// Read data from remote websocket host
//

duk_ret_t
js_websocket_read(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_websocket_lock
//
// lock connection
//

duk_ret_t
js_websocket_lock(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_websocket_unlock
//
// unlock connection
//

duk_ret_t
js_websocket_unlock(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// js_md5
//
// Calculate md5 digest
//

duk_ret_t
js_md5(duk_context* ctx)
{
    return JAVASCRIPT_OK;
}
