/***********************************************************************
 *          C_POSTGRES.C
 *          Postgres GClass.
 *
 *          Postgress uv-mixin for Yuneta
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <postgresql/libpq-fe.h>
#include <uv.h>
#include "c_postgres.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef int curl_socket_t;

typedef struct dl_uv_poll_s {
    DL_ITEM_FIELDS

    uv_poll_t uv_poll;
    curl_socket_t sockfd;
    hgobj gobj;
    hsdata sd_easy;
} dl_uv_poll_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "authz",        0,              0,          "authz about you want help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (ASN_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (ASN_SCHEMA,    "authzs",           0,          pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (ASN_BOOLEAN,     "connected",                    SDF_RD,         0,              ""),
SDATA (ASN_BOOLEAN,     "manual",                       SDF_RD,         0,              "Set true if you want connect manually"),
SDATA (ASN_INTEGER,     "timeout_waiting_connected",    SDF_RD,         60*1000,        ""),
SDATA (ASN_INTEGER,     "timeout_between_connections",  SDF_RD,         2*1000,         "Idle timeout to wait between attempts of connection"),
SDATA (ASN_INTEGER,     "timeout_inactivity",           SDF_RD,         -1,
            "Inactivity timeout to close the connection."
            "Reconnect when new data arrived. With -1 never close."),
SDATA (ASN_OCTET_STR,   "connected_event_name",         SDF_RD,         "EV_CONNECTED", "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "disconnected_event_name",      SDF_RD,         "EV_DISCONNECTED", "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "tx_ready_event_name",          SDF_RD,         "EV_TX_READY",  "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "rx_data_event_name",           SDF_RD,         "EV_RX_DATA",   "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "stopped_event_name",           SDF_RD,         "EV_STOPPED",   "Stopped event name"),
SDATA (ASN_JSON,        "urls",                         SDF_RD,         0,
    "list of destination urls: [rUrl^lUrl, ...]"),
SDATA (ASN_POINTER,     "user_data",        0,                          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                          0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",       0,                          0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendent value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t pm_authz_sample[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (ASN_OCTET_STR,    "param sample",     0,          "",             "Param ..."),
SDATA_END()
};

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name------------flag----alias---items---------------description--*/
SDATAAUTHZ (ASN_SCHEMA, "sample",       0,      0,      pm_authz_sample,    "Permission to ..."),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout_inactivity;
    hgobj timer;

    int idx_dst;
    int n_urls;
    json_t *urls;

    const char *connected_event_name;
    const char *tx_ready_event_name;
    const char *rx_data_event_name;
    const char *disconnected_event_name;
    const char *stopped_event_name;

    PGconn *conn;

    dl_list_t dl_uv_polls;
    json_t *dl_tx_data;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create(gobj_name(gobj), GCLASS_TIMER, 0, gobj);
    dl_init(&priv->dl_uv_polls);
    priv->dl_tx_data = json_array();

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_inactivity,            gobj_read_int32_attr)
    SET_PRIV(connected_event_name,          gobj_read_str_attr)
    SET_PRIV(tx_ready_event_name,           gobj_read_str_attr)
    SET_PRIV(rx_data_event_name,            gobj_read_str_attr)
    SET_PRIV(disconnected_event_name,       gobj_read_str_attr)
    SET_PRIV(stopped_event_name,            gobj_read_str_attr)
    SET_PRIV(urls,                          gobj_read_json_attr)
    if(!json_is_array(priv->urls)) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "urls MUST BE an json array!",
            NULL
        );
    }
    priv->n_urls = json_array_size(priv->urls);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_inactivity,              gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(urls,                          gobj_read_json_attr)
        if(!json_is_array(priv->urls)) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "urls MUST BE an json array!",
                NULL
            );
        }
        priv->n_urls = json_array_size(priv->urls);
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_array_size(priv->dl_tx_data)) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "records LOST",
            NULL
        );
        log_debug_json(0, priv->dl_tx_data, "records LOST");
    }
    JSON_DECREF(priv->dl_tx_data);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // HACK el start de tcp0 lo hace el timer
    gobj_start(priv->timer);
    if(!gobj_read_bool_attr(gobj, "manual")) {
        set_timeout(priv->timer, 100);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}




            /***************************
             *      Commands
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_webix(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return gobj_build_authzs_doc(gobj, cmd, kw, src);
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Return the destination (host,port) tuple to connect from
 *  the ``urls`` attribute.
 *  If there are multiple urls try to connect to each cyclically.
 ***************************************************************************/
PRIVATE BOOL get_next_dst(
    hgobj gobj,
    char *url, int url_len
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->n_urls) {
        const char *url_ = json_list_str(priv->urls, priv->idx_dst);
        snprintf(url, url_len, "%s", url_);

        // Point to next dst
        ++priv->idx_dst;
        priv->idx_dst = priv->idx_dst % priv->n_urls;
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE dl_uv_poll_t *new_uv_poll(hgobj gobj, hsdata sd_easy, curl_socket_t sockfd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_uv_poll_t *poll_item = gbmem_malloc(sizeof(dl_uv_poll_t));
    poll_item->sockfd = sockfd;
    poll_item->gobj = gobj;
    poll_item->sd_easy = sd_easy;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>> uv_poll_init_socket p=%p, s=%d", &poll_item->uv_poll, sockfd);
    }
    uv_poll_init_socket(
        yuno_uv_event_loop(),
        &poll_item->uv_poll,
        sockfd
    );
    poll_item->uv_poll.data = poll_item;

    dl_add(&priv->dl_uv_polls, poll_item);
    return poll_item;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE dl_uv_poll_t *find_uv_poll(hgobj gobj, curl_socket_t sockfd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_uv_poll_t *poll_item = dl_first(&priv->dl_uv_polls);
    while(poll_item) {
        if(poll_item->sockfd == sockfd) {
            return poll_item;
        }
        poll_item = dl_next(poll_item);
    }
    return 0;
}

/***************************************************************************
  *
  ***************************************************************************/
PRIVATE void on_close_cb(uv_handle_t* handle)
{
    dl_uv_poll_t *poll_item = handle->data;
    hgobj gobj = poll_item->gobj;
    if(gobj) {
        if(gobj_trace_level(gobj) & TRACE_UV) {
            log_debug_printf(0, "<<<< on_close_cb poll p=%p", handle);
        }
    }
    gbmem_free(poll_item);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void rm_uv_poll(hgobj gobj, dl_uv_poll_t *poll_item)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_delete(&priv->dl_uv_polls, poll_item, 0);
    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>> uv_poll_stop & uv_close p=%p", &poll_item->uv_poll);
    }
    uv_poll_stop(&poll_item->uv_poll);
    uv_close((uv_handle_t*)&poll_item->uv_poll, on_close_cb);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char url[PATH_MAX];
    get_next_dst(
        gobj,
        url, sizeof(url)
    );
    if(empty_string(url)) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Not postgres URI has been configured!",
            NULL
        );
    }
    if(priv->conn) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "PGConn ALREADY set",
            NULL
        );
        PQfinish(priv->conn);
        priv->conn = 0;
    }
    priv->conn = PQconnectStart(url);
    if(priv->conn == NULL) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "PQconnectStart FAILED",
            NULL
        );
    }

    if(PQstatus(priv->conn) == CONNECTION_BAD) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "PGstatus CONNECTION_BAD",
            NULL
        );
    }

    set_timeout(priv->timer, gobj_read_int32_attr(gobj, "timeout_waiting_connected"));

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if (gobj_read_bool_attr(gobj, "manual")) {
        return 0;
    }

    gobj_send_event(gobj, "EV_CONNECT", 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_running(gobj)) {
        if (priv->n_urls > 0 && priv->idx_dst > 0) {
            set_timeout(priv->timer, 100);
        } else {
            set_timeout(
                priv->timer,
                gobj_read_int32_attr(gobj, "timeout_between_connections")
            );
        }
    }

    if(!empty_string(priv->disconnected_event_name)) {
        gobj_publish_event(gobj, priv->disconnected_event_name, 0);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_wait_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(
        priv->timer,
        gobj_read_int32_attr(gobj, "timeout_between_connections")
    );

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    if (priv->timeout_inactivity > 0) {
        set_timeout(priv->timer, priv->timeout_inactivity);
    }

    /*
     *  process enqueued data
     */
    if(json_array_size(priv->dl_tx_data)>0) {
        int idx; json_t *jn_msg;
        json_array_foreach(priv->dl_tx_data, idx, jn_msg) {
            gobj_send_event(gobj, "EV_ADD_RECORD", json_incref(jn_msg), gobj);
        }
    }

    if (!empty_string(priv->connected_event_name)) {
        gobj_publish_event(gobj, priv->connected_event_name, 0);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if (priv->timeout_inactivity > 0)
        set_timeout(priv->timer, priv->timeout_inactivity);
    gobj_publish_event(gobj, priv->rx_data_event_name, kw); // use the same kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
    {
        "topic": "tracks_geodb2",
        "rowid": (json_int_t)md_record.__rowid__,
        "record": msg
    }
 ***************************************************************************/
PRIVATE int ac_add_record(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if (priv->timeout_inactivity > 0)
        set_timeout(priv->timer, priv->timeout_inactivity);
    // TODO gobj_send_event(gobj_bottom_gobj(gobj), "EV_ADD_RECORD", kw, gobj); // own kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_enqueue_record(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_array_append(priv->dl_tx_data, kw);
    if(gobj_in_this_state(gobj, "ST_DISCONNECTED")) {
        gobj_send_event(gobj, "EV_CONNECT", 0, gobj);
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_transmit_ready(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if (!empty_string(priv->tx_ready_event_name)) {
        const char *event_name = priv->tx_ready_event_name;
        json_t *kw_ex = json_pack("{s:I}",
            "connex", (json_int_t)(size_t)gobj
        );
        gobj_publish_event(gobj, event_name, kw_ex);
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);

// TODO    if(gobj_is_running(gobj_bottom_gobj(gobj))) {
//         set_timeout(
//             priv->timer,
//             gobj_read_int32_attr(gobj, "timeout_between_connections")
//         );
//         gobj_stop(gobj_bottom_gobj(gobj));
//     }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);

// TODO    if(gobj_bottom_gobj(gobj)==src) {
//         if(gobj_is_running(gobj)) {
//             if (priv->n_urls > 0 && priv->idx_dst > 0) {
//                 set_timeout(priv->timer, 100);
//             } else {
//                 set_timeout(
//                     priv->timer,
//                     gobj_read_int32_attr(gobj, "timeout_between_connections")
//                 );
//             }
//         }
//     }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_ADD_RECORD",       0,  0,  ""},
    {"EV_CONNECT",          0,  0,  ""},
    {"EV_DROP",             0,  0,  ""},

    // bottom input
    {"EV_CONNECTED",        0,  0,  ""},
    {"EV_DISCONNECTED",     0,  0,  ""},
    {"EV_RX_DATA",          0,  0,  ""},
    {"EV_TX_READY",         0,  0,  ""},

    {"EV_TIMEOUT",          0,  0,  ""},
    {"EV_STOPPED",          0,  0,  ""},
    // internal
    {NULL, 0, 0, ""}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, ""}
};
PRIVATE const char *state_names[] = {
    "ST_DISCONNECTED",
    "ST_WAIT_CONNECTED",
    "ST_CONNECTED",
    "ST_WAIT_DISCONNECTED",
    NULL
};

PRIVATE EV_ACTION ST_DISCONNECTED[] = {
    {"EV_CONNECT",          ac_connect,                 "ST_WAIT_CONNECTED"},
    {"EV_ADD_RECORD",       ac_enqueue_record,          0},
    {"EV_TIMEOUT",          ac_timeout_disconnected,    0},
    {"EV_STOPPED",          ac_stopped,                 0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_CONNECTED[] = {
    {"EV_ADD_RECORD",       ac_enqueue_record,          0},
    {"EV_CONNECTED",        ac_connected,               "ST_CONNECTED"},
    {"EV_DISCONNECTED",     ac_disconnected,            "ST_DISCONNECTED"},
    {"EV_STOPPED",          ac_stopped,                 "ST_DISCONNECTED"},
    {"EV_TIMEOUT",          ac_timeout_wait_connected,  "ST_DISCONNECTED"},
    {"EV_DROP",             ac_drop,                    "ST_WAIT_DISCONNECTED"},
    {0,0,0}
};
PRIVATE EV_ACTION ST_CONNECTED[] = {
    {"EV_RX_DATA",          ac_rx_data,                 0},
    {"EV_ADD_RECORD",       ac_add_record,              0},
    {"EV_DISCONNECTED",     ac_disconnected,            "ST_DISCONNECTED"},
    {"EV_TIMEOUT",          ac_timeout_data,            0},
    {"EV_TX_READY",         ac_transmit_ready,          0},
    {"EV_DROP",             ac_drop,                    "ST_WAIT_DISCONNECTED"},
    {"EV_STOPPED",          ac_stopped,                 0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_DISCONNECTED[] = {
    {"EV_DISCONNECTED",     ac_disconnected,            "ST_DISCONNECTED"},
    {"EV_STOPPED",          ac_stopped,                 "ST_DISCONNECTED"},
    {"EV_TIMEOUT",          ac_stopped,                 "ST_DISCONNECTED"},
    {0,0,0}
};


PRIVATE EV_ACTION *states[] = {
    ST_DISCONNECTED,
    ST_WAIT_CONNECTED,
    ST_CONNECTED,
    ST_WAIT_DISCONNECTED,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_POSTGRES_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command_parser,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_update_resource,
        0, //mt_delete_resource,
        0, //mt_add_child_resource_link
        0, //mt_delete_child_resource_link
        0, //mt_get_resource
        0, //mt_authorization_parser,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_authzs,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_link_nodes2,
        0, //mt_unlink_nodes,
        0, //mt_unlink_nodes2,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_node_instances,
        0, //mt_save_node,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    authz_table,
    s_user_trace_level,
    command_table,  // command_table
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_postgres(void)
{
    return &_gclass;
}
