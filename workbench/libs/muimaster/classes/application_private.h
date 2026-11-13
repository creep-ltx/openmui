/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.
*/

/* Application's instance data and what classes/application.c and the
 * application module (appx.c, Zapp.mui: its rarely used parts) share.
 * Private to them. */

#ifndef _MUI_CLASSES_APPLICATION_PRIVATE_H
#define _MUI_CLASSES_APPLICATION_PRIVATE_H

#include <exec/semaphores.h>
#include <devices/timer.h>
#include <dos/notify.h>
#include <libraries/commodities.h>

struct TrackingNode
{
    struct MinNode tn_Node;
    Object *tn_Application;
};

#define ZUNE_STACK_LOW   6000  /* see zune_deep_call() */
#define ZUNE_STACK_SPARE 8192

struct MUI_ApplicationData
{
    struct MUI_GlobalInfo   app_GlobalInfo;
    APTR                    app_WindowFamily; /* delegates window list */
    struct MinList          app_IHList;
    struct MinList          app_MethodQueue;
    struct SignalSemaphore  app_MethodSemaphore;
    struct MinList          app_ReturnIDQueue;
    struct Hook             *app_BrokerHook;
    struct MsgPort          *app_BrokerPort;
    struct MsgPort          *app_TimerPort;
    struct timerequest      *app_TimerReq;
    struct Task             *app_Task;
    CxObj                   *app_Broker;
    Object                  *app_Menustrip;
    Object                  *app_AboutWin;
    APTR                    app_RIDMemChunk;
    STRPTR                  app_Author;
    STRPTR                  app_Base;
    STRPTR                  app_Copyright;
    STRPTR                  app_Description;
    STRPTR                  app_HelpFile;
    STRPTR                  app_Title;
    STRPTR                  app_Version;
    BOOL                    app_VersionAllocated;
    STRPTR                  app_Version_Number;
    STRPTR                  app_Version_Date;
    STRPTR                  app_Version_Extra;
    struct MinList          app_Timers;     /* timerequest_ext.link */
    ULONG                   app_MenuAction; /* Remember last action */
    BOOL                    app_ForceQuit;
    BOOL                    app_Iconified;
    BOOL                    app_SingleTask;
    BOOL                    app_Active;
    BYTE                    app_BrokerPri;
    struct TrackingNode     app_TrackingNode;
    BOOL                    app_is_TNode_in_list;
    ULONG searchwinid;
    /* Remembered window positions, made when the first one is stored:
     * a LONG byte count (the serialized header), then app_WinPosCap
     * records. NULL until needed: a 300-record array was 6 KB in every
     * application. */
    LONG                    *app_WinPos;
    ULONG                   app_WinPosCap;
    struct MsgPort          *app_RexxPort;
    struct RexxMsg          *app_RexxMsg;
    struct Hook             *app_RexxHook;
    struct MUI_Command      *app_Commands;
    STRPTR                  app_RexxString;
    BOOL                    app_UseRexx;
    struct MsgPort          *app_AppPort; /* Port for AppIcon/AppMenu/AppWindow */
    struct AppIcon          *app_AppIcon;
    struct DiskObject       *app_DiskObject; /* This is only pointer to
                                              * client-managed object */
    struct NewMenu          *app_LegacyMenu; /* borrowed source table */
    Object                  *app_DropObject; /* borrowed notification target */
    struct DiskObject       *app_DefaultDiskObject; /* This is complete
                                                     * object managed by
                                                     * the class */
    struct MsgPort          *app_CfgNotifyPort;   /* notification of changes to the prefs file */
    struct NotifyRequest    app_CfgNotifyRequest;
    STRPTR                  app_CfgNotifyName;
    BOOL                    app_NotifyStarted;  /* file notifications set up */
    struct DateStamp        app_Created;        /* prefs older than this were read */
    APTR                    app_SpareStack;     /* see zune_deep_call() */
    BOOL                    app_SpareBusy;
    struct AppMenuItem      *app_AppMenu;  /* Tools menu item while iconified */
};

#define WINPOS(data) ((struct windowpos *)((data)->app_WinPos + 1))

/* CxSender id of the settings' iconification hotkey */
#define ICONIFY_HOTKEY_ID 0x5A49434F
#define WINPOS_MAX (MAXWINS - 1)

struct timerequest_ext
{
    struct timerequest treq;
    struct MUI_InputHandlerNode *ihn;
    struct MinNode link;    /* in app_Timers until RemInputHandler */
};

/*
 * MethodQueueNode
 */
struct MQNode
{
    struct MinNode    mq_Node;
    Object           *mq_Dest;
    LONG              mq_Count; /* argument count and private MQF_* flags */
    IPTR             *mq_Msg;
};

/* Public PushMethod counts use only the low four bits. */
#define MQF_OWNED_WINDOWS (1UL << 31)

/*
 * Queue of Return IDs
 */
struct RIDNode
{
    struct MinNode  rid_Node;
    ULONG           rid_Value;
};

/* classes/application.c, for the module too */
struct MQNode *CreateMQNode(LONG count);
void DeleteMQNode(struct MQNode *mq);
void EnqueueMethod(struct MUI_ApplicationData *data, struct MQNode *mq);
void FreeWindowList(struct List *windows);
BOOL application_return_id(struct MUI_ApplicationData *data, ULONG id);
struct MinList *app_windows(struct MUI_ApplicationData *data);
void app_each_window(struct MUI_ApplicationData *data, ULONG method,
    IPTR arg);

#endif /* _MUI_CLASSES_APPLICATION_PRIVATE_H */
