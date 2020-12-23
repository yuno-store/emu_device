/***********************************************************************
 *          C_EMU_DEVICE.C
 *          Emu_device GClass.
 *
 *          Emulator of device gates
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include "c_emu_device.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int list_databases(const char *path);
PRIVATE int list_topics(const char *path, const char *database);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_interval(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_window(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_read_parameters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_window[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_COUNTER64, "window",       0,              0,          "Number of messages to send in each interval."),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_interval[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_COUNTER64, "interval",      0,              0,         "Interval in miliseconds to send 'window' frames."),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-----------items---------------json_fn-------------description---------- */
SDATACM (ASN_SCHEMA,    "help",             a_help,         pm_help,            cmd_help,           "Command's help"),
SDATACM (ASN_SCHEMA,    "read-parameters",  0,              0,                  cmd_read_parameters,"View parameters."),
SDATACM (ASN_SCHEMA,    "write-window",     0,              pm_write_window,    cmd_write_window,   "Write window parameter, number of frames to send in each interval."),
SDATACM (ASN_SCHEMA,    "write-interval",   0,              pm_write_interval,  cmd_write_interval, "Write interval parameter, in miliseconds."),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (ASN_UNSIGNED,    "window",           SDF_WR|SDF_PERSIST,         1,              "Number of messages to send in each interval event"),
SDATA (ASN_UNSIGNED,    "interval",         SDF_WR|SDF_PERSIST,         1000,           "Interval in miliseconds to send 'window' frames"),
SDATA (ASN_COUNTER64,   "txMsgs",           SDF_RD,                     0,              "Messages transmitted by this socket"),
SDATA (ASN_COUNTER64,   "rxMsgs",           SDF_RD,                     0,              "Messages received by this socket"),

SDATA (ASN_OCTET_STR,   "url",                  SDF_WR|SDF_PERSIST,         0,   "Url of __output_side__ yuno."                 ),
SDATA (ASN_OCTET_STR,   "path",                 SDF_WR|SDF_PERSIST,         0,   "Path of database."                 ),
SDATA (ASN_OCTET_STR,   "database",             SDF_WR|SDF_PERSIST,         0,   "Database Name."                    ),
SDATA (ASN_OCTET_STR,   "topic",                SDF_WR|SDF_PERSIST,         0,   "Database Topic."                   ),
SDATA (ASN_OCTET_STR,   "leading",              SDF_WR|SDF_PERSIST,         0,   "Leading data."                   ),
SDATA (ASN_OCTET_STR,   "from_t",               SDF_WR|SDF_PERSIST,         0,   "From time."                        ),
SDATA (ASN_OCTET_STR,   "to_t",                 SDF_WR|SDF_PERSIST,         0,   "To time."                          ),
SDATA (ASN_OCTET_STR,   "from_rowid",           SDF_WR|SDF_PERSIST,         0,   "From rowid."                       ),
SDATA (ASN_OCTET_STR,   "to_rowid",             SDF_WR|SDF_PERSIST,         0,   "To rowid."                         ),
SDATA (ASN_OCTET_STR,   "user_flag_mask_set",   SDF_WR|SDF_PERSIST,         0,   "Mask of User Flag set."            ),
SDATA (ASN_OCTET_STR,   "user_flag_mask_notset",SDF_WR|SDF_PERSIST,         0,   "Mask of User Flag not set."        ),
SDATA (ASN_OCTET_STR,   "system_flag_mask_set", SDF_WR|SDF_PERSIST,         0,   "Mask of System Flag set."          ),
SDATA (ASN_OCTET_STR,   "system_flag_mask_notset",SDF_WR|SDF_PERSIST,       0,   "Mask of System Flag not set."      ),
SDATA (ASN_OCTET_STR,   "use_very_first",       SDF_WR|SDF_PERSIST,         0,   "Search from the very first record."),
SDATA (ASN_OCTET_STR,   "key",                  SDF_WR|SDF_PERSIST,         0,   "Key."                              ),
SDATA (ASN_OCTET_STR,   "notkey",               SDF_WR|SDF_PERSIST,         0,   "Not key."                          ),

SDATA (ASN_INTEGER,     "timeout",          SDF_RD,                     2*1000,         "Timeout"),
SDATA (ASN_POINTER,     "user_data",        0,                          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                          0,              "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_INFO = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"info",        "Trace user information"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    uint32_t interval;
    uint32_t window;
    hgobj timer_interval;
    uint64_t *ptxMsgs;
    uint64_t *prxMsgs;
    hgobj gobj_output_side;

    json_t *tranger;
    json_t *htopic;
    json_t *match_cond;
    md_record_t md_record;
    uint64_t last_id;

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

    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");

    priv->timer_interval = gobj_create("", GCLASS_TIMER, 0, gobj);
    gobj_write_str_attr(priv->timer_interval, "timeout_event_name", "EV_TICK2SEND");

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(interval,              gobj_read_uint32_attr)
    SET_PRIV(window,                gobj_read_uint32_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(interval,         gobj_read_uint32_attr)
        if(gobj_find_service("agent_client", FALSE)) {
            gobj_save_persistent_attrs(gobj);
        }
        if(gobj_is_playing(gobj)) {
            set_timeout_periodic(priv->timer_interval, priv->interval);
        }

    ELIF_EQ_SET_PRIV(window,        gobj_read_uint32_attr)
        if(gobj_find_service("agent_client", FALSE)) {
            gobj_save_persistent_attrs(gobj);
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj agent_client = gobj_find_service("agent_client", FALSE);

    const char *url = gobj_read_str_attr(gobj, "url");
    const char *path = gobj_read_str_attr(gobj, "path");
    const char *database = gobj_read_str_attr(gobj, "database");
    const char *topic = gobj_read_str_attr(gobj, "topic");

    if(empty_string(url)) {
        if(agent_client) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "What yuno __input_side__ url?",
                NULL
            );
            gobj_send_event(agent_client, "EV_PAUSE_YUNO", 0, gobj);
        } else {
            fprintf(stderr, "What yuno __input_side__ url?\n");
            exit(-1);
        }
    }

    /*
     *  Check if path contains all
     */
    char bftemp[PATH_MAX];
    snprintf(bftemp, sizeof(bftemp), "%s%s%s",
        path,
        (path[strlen(path)-1]=='/')?"":"/",
        "topic_desc.json"
    );
    if(is_regular_file(bftemp)) {
        pop_last_segment(bftemp); // pop topic_desc.json
        topic = pop_last_segment(bftemp);
        database = pop_last_segment(bftemp);
        path = bftemp;
    }

    if(empty_string(path)) {
        if(agent_client) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "What TimeRanger path?",
                NULL
            );
            gobj_send_event(agent_client, "EV_PAUSE_YUNO", 0, gobj);
        } else {
            fprintf(stderr, "What TimeRanger path?\n");
            exit(-1);
        }
    }
    if(empty_string(database)) {
        if(agent_client) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "What Database?",
                NULL
            );
            gobj_send_event(agent_client, "EV_PAUSE_YUNO", 0, gobj);
        } else {
            fprintf(stderr, "What Database?\n\n");
            list_databases(path);
            exit(-1);
        }
    }
    if(empty_string(topic)) {
        if(agent_client) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "What Topic?",
                NULL
            );
            gobj_send_event(agent_client, "EV_PAUSE_YUNO", 0, gobj);
        } else {
            fprintf(stderr, "What Topic?\n\n");
            list_topics(path, database);
            exit(-1);
        }
    }

    /*----------------------------------*
     *  Match conditions
     *----------------------------------*/
    const char *from_t = gobj_read_str_attr(gobj, "from_t");
    const char *to_t = gobj_read_str_attr(gobj, "to_t");

    const char *from_rowid = gobj_read_str_attr(gobj, "from_rowid");
    const char *to_rowid = gobj_read_str_attr(gobj, "to_rowid");

    const char *user_flag_mask_set = gobj_read_str_attr(gobj, "user_flag_mask_set");
    const char *user_flag_mask_notset = gobj_read_str_attr(gobj, "user_flag_mask_notset");

    const char *system_flag_mask_set = gobj_read_str_attr(gobj, "system_flag_mask_set");
    const char *system_flag_mask_notset = gobj_read_str_attr(gobj, "system_flag_mask_notset");

    const char *key = gobj_read_str_attr(gobj, "key");
    const char *notkey = gobj_read_str_attr(gobj, "notkey");

    priv->match_cond = json_object();

    if(from_t) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(from_t)) {
            timestamp = atoll(from_t);
        } else {
            parse_date_basic(from_t, &timestamp, &offset);
        }
        json_object_set_new(priv->match_cond, "from_t", json_integer(timestamp));
    }
    if(to_t) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(to_t)) {
            timestamp = atoll(to_t);
        } else {
            parse_date_basic(to_t, &timestamp, &offset);
        }
        json_object_set_new(priv->match_cond, "to_t", json_integer(timestamp));
    }

    if(from_rowid) {
        json_object_set_new(priv->match_cond, "from_rowid", json_integer(atoll(from_rowid)));
    }
    if(to_rowid) {
        json_object_set_new(priv->match_cond, "to_rowid", json_integer(atoll(to_rowid)));
    }

    if(user_flag_mask_set) {
        json_object_set_new(
            priv->match_cond,
            "user_flag_mask_set",
            json_integer(atol(user_flag_mask_set))
        );
    }
    if(user_flag_mask_notset) {
        json_object_set_new(
            priv->match_cond,
            "user_flag_mask_notset",
            json_integer(atol(user_flag_mask_notset))
        );
    }
    if(system_flag_mask_set) {
        json_object_set_new(
            priv->match_cond,
            "system_flag_mask_set",
            json_integer(atol(system_flag_mask_set))
        );
    }
    if(system_flag_mask_notset) {
        json_object_set_new(
            priv->match_cond,
            "system_flag_mask_notset",
            json_integer(atol(system_flag_mask_notset))
        );
    }
    if(key) {
        json_object_set_new(
            priv->match_cond,
            "key",
            json_string(key)
        );
    }
    if(notkey) {
        json_object_set_new(
            priv->match_cond,
            "notkey",
            json_string(notkey)
        );
    }

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);

    hgobj gobj_connex = gobj_find_bottom_child_by_gclass(priv->gobj_output_side, "Connex");
    json_t *jn_urls = json_pack("[s]", url);
    gobj_write_json_attr(gobj_connex, "urls", jn_urls);
    json_decref(jn_urls);

    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side);
    gobj_start(priv->timer_interval);

    /*-------------------------------*
     *  Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s}",
        "path", path,
        "database", database
    );
    priv->tranger = tranger_startup(jn_tranger);
    if(agent_client) {
        log_error(LOG_OPT_EXIT_ZERO,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger_startup() FAILED",
            "path",         "%s", path,
            "database",     "%s", database,
            NULL
        );
    } else {
        if(!priv->tranger) {
            fprintf(stderr, "Can't startup tranger %s/%s\n\n", path, database);
            exit(-1);
        }
    }

    /*-------------------------------*
     *  Open topic
     *-------------------------------*/
    priv->htopic = tranger_open_topic(
        priv->tranger,
        topic,
        FALSE
    );
    if(agent_client) {
        log_error(LOG_OPT_EXIT_ZERO,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger_open_topic() FAILED",
            "path",         "%s", path,
            "database",     "%s", database,
            "topic",        "%s", topic,
            NULL
        );
    } else {
        if(!priv->htopic) {
            fprintf(stderr, "Can't open topic %s\n\n", topic);
            list_topics(path, database);
            exit(-1);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_find_service("agent_client", FALSE)) {
        gobj_save_persistent_attrs(gobj);
    }
    clear_timeout(priv->timer_interval);
    gobj_stop(priv->timer_interval);
    if(priv->gobj_output_side) {
        gobj_stop_tree(priv->gobj_output_side);
    }

    JSON_DECREF(priv->match_cond);
    EXEC_AND_RESET(tranger_shutdown, priv->tranger);

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
PRIVATE json_t *cmd_write_interval(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *interval_ = kw_get_str(kw, "interval", "1000", 0);
    gobj_write_uint32_attr(gobj, "interval", atoi(interval_));
    gobj_save_persistent_attrs(gobj);

    return msg_iev_build_webix(
        gobj,
        0,
        json_local_sprintf("interval: %d", priv->interval),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_write_window(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *window_ = kw_get_str(kw, "window", "1", 0);
    gobj_write_uint32_attr(gobj, "window", atoi(window_));
    gobj_save_persistent_attrs(gobj);

    return msg_iev_build_webix(
        gobj,
        0,
        json_local_sprintf("window: %d", priv->window),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_read_parameters(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return msg_iev_build_webix(
        gobj,
        0,
        json_local_sprintf("window: %d\ninterval: %d\nlast_id: %"PRIu64"",
            priv->window,
            priv->interval,
            priv->last_id
        ),
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_db_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    char *p = strrchr(directory, '/');
    if(p) {
        printf("  %s\n", p+1);
    } else {
        printf("  %s\n", directory);
    }
    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    printf("Databases found:\n");
    walk_dir_tree(
        path,
        "__timeranger__.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_db_cb,
        0
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_topic_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    char *p = strrchr(directory, '/');
    if(p) {
        printf("  %s\n", p+1);
    } else {
        printf("  %s\n", directory);
    }
    return TRUE; // to continue
}

PRIVATE int list_topics(const char *path, const char *database)
{
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s/%s", path, database);
    printf("Topics found:\n");
    walk_dir_tree(
        temp,
        ".*\\.md",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_topic_cb,
        0
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char bin[64*1024];
GBUFFER *get_next_frame(hgobj gobj, BOOL *empty_frame)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL end = FALSE;
    BOOL first_time = FALSE;
    *empty_frame = FALSE;
    if(priv->last_id == 0) {
        first_time = TRUE;
        json_incref(priv->match_cond);
        if(tranger_find_record(priv->tranger, priv->htopic, priv->match_cond, &priv->md_record)<0) {
            return FALSE;
        }
    } else {
        if(tranger_next_record(priv->tranger, priv->htopic, &priv->md_record)<0) {
            return FALSE;
        }
    }
    priv->last_id = priv->md_record.__rowid__;

    while(!end) {
        if(tranger_match_record(
                priv->tranger,
                priv->htopic,
                priv->match_cond,
                &priv->md_record,
                &end
            )) {
            priv->last_id = priv->md_record.__rowid__;
            json_t *jn_record = tranger_read_record_content(
                priv->tranger,
                priv->htopic,
                &priv->md_record
            );
            if(jn_record) {
                const char *frame = kw_get_str(jn_record, "frame64", "", KW_REQUIRED);
                size_t len = strlen(frame);
                if(len == 0) {
                    if(first_time) {
                        if(tranger_next_record(
                            priv->tranger,
                            priv->htopic,
                            &priv->md_record
                        )<0) {
                            return 0;
                        }
                        priv->last_id = priv->md_record.__rowid__;
                        continue;
                    }
                    *empty_frame = TRUE;
                    json_decref(jn_record);
                    return 0;
                }
                len = b64_decode(frame, (uint8_t *)bin, sizeof(bin));
                frame = bin;
                GBUFFER *gbuf = gbuf_create(len, len, 0, 0);
                gbuf_append(gbuf, (void *)frame, len);

                json_decref(jn_record);
                return gbuf;
            } else  {
                // TODO falta probar
                return 0;
            }
        }
        if(end) {
            break;
        }
        if(tranger_next_record(priv->tranger, priv->htopic, &priv->md_record)<0) {
            return 0;
        }
        priv->last_id = priv->md_record.__rowid__;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_gps_msgs(hgobj gobj, hgobj channel_gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint32_t window = gobj_read_uint32_attr(gobj, "window");
    for(int i=0; i<window; i++) {
        /*
         *  Envia el mensaje al destino
         */
        GBUFFER *gbuf = 0;

        BOOL empty_frame;
        gbuf = get_next_frame(gobj, &empty_frame);
        if(!gbuf) {
            if(empty_frame) {
                if(gobj_trace_level(gobj) & TRACE_INFO) {
                    info_msg0("Droping connection");
                }
                gobj_send_event(channel_gobj, "EV_DROP", 0, gobj);
                return 0;
            }
            return -1;
        }

        if(gobj_trace_level(gobj) & TRACE_INFO) {
            char metadata[1024];
            print_md1_record(priv->tranger, priv->htopic, &priv->md_record, metadata, sizeof(metadata));
            info_msg0("Sending: %s", metadata);
        }

        json_t *kw_tx = json_pack("{s:I}",
            "gbuffer", (json_int_t)(size_t)gbuf
        );
        gobj_send_event(channel_gobj, "EV_SEND_MESSAGE", kw_tx, gobj);

        (*priv->ptxMsgs)++;
        gobj_incr_qs(QS_TXMSGS, 1);
    }

    return 1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int emulate_gps_msgs(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Recorre la tabla de output channels y comprueba que están en una ruta
     */
    dl_list_t *iter = gobj_match_childs_by_strict_gclass(priv->gobj_output_side, "Channel");
    int ret;
    hgobj channel_gobj;
    rc_first_instance(iter, (rc_resource_t **)&channel_gobj);

    BOOL opened = gobj_read_bool_attr(channel_gobj, "opened");
    if(!opened) {
        return 0;
    }
    ret = send_gps_msgs(gobj, channel_gobj);
    if(ret < 0) {
        clear_timeout(priv->timer_interval);
        if(!gobj_find_service("agent_client", FALSE)) {
            info_msg0("Finished, sent %"PRIu64" messages", *priv->ptxMsgs);
            exit(0);
        }
    } else if(ret == 0) {
        // drop channel sended
        // WARNING realmente estamos considerando una única conexión (channel)
        clear_timeout(priv->timer_interval);
    }
    rc_free_iter(iter, TRUE, 0);
    return ret;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        info_msg0("Connected");
    }

    if(gobj_is_playing(gobj)) {
        const char *frame = gobj_read_str_attr(gobj, "leading");
        if(!empty_string(frame)) {
            /*
             *  Envia el mensaje al destino
             */
            if(gobj_trace_level(gobj) & TRACE_INFO) {
                info_msg0("Sending: %s", frame);
            }

            size_t len = strlen(frame);
            len = b64_decode(frame, (uint8_t *)bin, sizeof(bin));
            frame = bin;
            GBUFFER *gbuf = gbuf_create(len, len, 0, 0);
            gbuf_append(gbuf, (void *)frame, len);

            json_t *kw_tx = json_pack("{s:I}",
                "gbuffer", (json_int_t)(size_t)gbuf
            );
            gobj_send_event(src, "EV_SEND_MESSAGE", kw_tx, gobj);

            (*priv->ptxMsgs)++;
            gobj_incr_qs(QS_TXMSGS, 1);
        }

        if(emulate_gps_msgs(gobj)>0) {
            // WARNING realmente estamos considerando una única conexión (channel)
            set_timeout_periodic(priv->timer_interval, priv->interval);
        }
    }


    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        info_msg0("Disconnected");
    }
    clear_timeout(priv->timer_interval);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_interval(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    emulate_gps_msgs(gobj);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_ON_MESSAGE",       0,  0,  0},
    {"EV_TICK2SEND",        0,  0,  0},
    {"EV_ON_OPEN",          0,  0,  0},
    {"EV_ON_CLOSE",         0,  0,  0},
    // bottom input
    {"EV_STOPPED",          0,  0,  0},
    // internal
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_ON_MESSAGE",           0,                      0},
    {"EV_TICK2SEND",            ac_interval,            0},
    {"EV_ON_OPEN",              ac_on_open,             0},
    {"EV_ON_CLOSE",             ac_on_close,            0},
    {"EV_STOPPED",              ac_stopped,             0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
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
    GCLASS_EMU_DEVICE_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        mt_play,
        mt_pause,
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
        0, //mt_future39,
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
    0,  // acl
    s_user_trace_level,
    command_table,  // command_table
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_emu_device(void)
{
    return &_gclass;
}
