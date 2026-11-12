/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"

/* #define MYDEBUG 1 */
#include "debug.h"

/* The objects a tag list hands over to the object being made (a group's
 * children, a window's root object and menustrip, an application's windows
 * and menustrip, a listview's list, a scrollgroup's contents, a popup's
 * string and button), disposed when the object cannot be made and nothing
 * has read the list yet. MUI's rule (Group.mui autodoc, MUIA_Group_Child):
 * a failed MUI_NewObject() call leaves no previously created object
 * behind, however deep the failure. A class whose OM_NEW fails before it
 * reads its tags calls this; one that fails after reading them disposes
 * the children it took in its OM_DISPOSE. Register argument and noinline:
 * the callers' frames stay as they were (docs/lean/README.md, the stack
 * rule). */
static inline BOOL zune_child_tag(Tag t)
{
    switch (t)
    {
    case MUIA_Group_Child:
    case MUIA_Family_Child:
    case MUIA_Window_RootObject:
    case MUIA_Window_Menustrip:
    case MUIA_Application_Window:
    case MUIA_Application_Menustrip:
    case MUIA_Listview_List:
    case MUIA_Scrollgroup_Contents:
    case MUIA_Popstring_String:
    case MUIA_Popstring_Button:
    case MUIA_Popobject_Object:
        return TRUE;
    }
    return FALSE;
}

/* The same object again further down the chain under a child-carrying tag:
 * Popstring passes its string and button to Group as children with its own
 * tag list after them (TAG_MORE), Listview its list, so a Group whose
 * superclass failed met each of them twice and disposed them twice (the
 * fault sweep, stability round 2: Popup's resets). The last one disposes.
 * The check and the disposal here, given the current item (its rest starts
 * right after it), as MUI_DisposeObject was called: done in
 * zune_dispose_children itself, its frame grew 16 -> 24 (the stack rule). */
static void __attribute__((noinline)) zune_dispose_if_last(struct TagItem *tag)
{
    struct TagItem *rest = tag + 1, *t;

    while ((t = NextTagItem(&rest)))
        if (t->ti_Data == tag->ti_Data && zune_child_tag(t->ti_Tag))
            return;
    MUI_DisposeObject((Object *)tag->ti_Data);
}

void __attribute__((noinline)) zune_dispose_children(struct TagItem *tags __asm("a0"))
{
    struct TagItem *tag;

    while ((tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Group_Child:
        case MUIA_Family_Child:
        case MUIA_Window_RootObject:
        case MUIA_Window_Menustrip:
        case MUIA_Application_Window:
        case MUIA_Application_Menustrip:
        case MUIA_Listview_List:
        case MUIA_Scrollgroup_Contents:
        case MUIA_Popstring_String:
        case MUIA_Popstring_Button:
        case MUIA_Popobject_Object:
            if (tag->ti_Data)
                zune_dispose_if_last(tag);
            break;
        }
    }
}

/*****************************************************************************

    NAME */
        AROS_LH2(Object *, MUI_NewObjectA,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR, classid,     A0),
        AROS_LHA(struct TagItem *, tags, A1),

/*  LOCATION */
        struct Library *, MUIMasterBase, 5, MUIMaster)

/*  FUNCTION
        Create an instance of the named MUI class with the supplied attributes.
        The class is looked up through MUI_GetClass(), then instantiated
        through intuition.library/NewObjectA().

    INPUTS
        classid - NUL-terminated class identifier. Built-in class names are
                  case-sensitive. Use the MUIC_* identifiers where available.
        tags - Attribute/value pairs terminated by TAG_DONE, or NULL for
               the class defaults. Attribute types and ownership are defined
               by the selected class.

    RESULT
        The new object, or NULL if the class lookup or construction fails.

    NOTES
        MUI_NewObject() is the variadic tag-list wrapper.

        Release an independently owned object with MUI_DisposeObject(). If
        ownership is transferred to a parent, follow that parent's disposal
        contract instead of disposing of the child twice.
        This function does not establish a separate MUI error code.

    SEE ALSO
        MUI_DisposeObject(), MUI_GetClass(),
        intuition.library/NewObjectA()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    Class  *cl;

    cl = MUI_GetClass(classid);
    if (!cl)
        zune_dispose_children(tags);   /* no class: nothing read the tag list */
    else
    {
        Object *obj = NewObjectA(cl, NULL, tags);

        /* Objects keep no reference to their class, like MUI 3.9: a
         * built-in class stays while it has objects (expunge checks
         * cl_ObjectCount), and so does a loaded external class
         * (ZUNE_ReleaseExternalClasses). One freed with DisposeObject()
         * instead of MUI_DisposeObject() then leaves nothing behind. */
        MUI_FreeClass(cl);
        if (obj) return obj;

        bug("*** Could not create object of %s\n", classid);
    }

    bug("*** Couldn't find %s\n", classid);

    return NULL;

    AROS_LIBFUNC_EXIT
} /* MUIA_NewObjectA */
