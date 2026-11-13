"""Offsets in Window's instance data (struct MUI_WindowData, window.c) that
the white-box Window checks use, from the start of the instance data
(classic m68k layout, measured with offsetof). One place to change when the
layout changes."""
MRI_WINDOW = 20         # wd_RenderInfo.mri_Window (wd_AreaRenderInfo first)
EH_LIST = 184           # wd_EHList
CC_LIST = 196           # wd_CCList
ACTIVE_OBJECT = 216     # wd_ActiveObject
DEFAULT_OBJECT = 224    # wd_DefaultObject
ROOT_OBJECT = 272       # wd_RootObject
FLAGS = 276             # wd_Flags
EVENT_CURSORS = 460     # wd_EventCursors
CRT_FLAGS = 212         # wd_CrtFlags
USER_SCREEN = 368       # wd_UserScreen
CHAIN = 170             # wd_Chain (the cycle chain's block)
CHAIN_COUNT = 174       # wd_ChainCount (UWORD)
CYCLE_ORDER = 398       # wd_CycleOrder
