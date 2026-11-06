# programs.sh: the real-program suite's table, sourced by tests/lean/scripts/suite.sh (and
# suite-fetch.sh). Archives live in $REAL (default ~/.cache/zune68k-real/<name>/arc, unpacked in
# <name>/x), never in git; tests/lean/real-programs.md has versions, URLs, licences and results.
#
# P ID GROUP SOURCE EXE WAIT EXTRAS
#   ID      short name; the program is copied to <SYS volume>Real/<ID>/ on the test machine
#   GROUP   mui3 (must run on MUI 3.8) or mui5 (needs or wants MUI 4/5)
#   SOURCE  folder under $REAL whose contents become Real/<ID>/
#   EXE     the program, relative to Real/<ID>/ (and its arguments, if any, after a space)
#   WAIT    seconds to wait for its first window
#   EXTRAS  menu paths ("Menu/Item" or "Menu/Item/Sub"), separated by ';', each opened, its new
#           windows screenshot and closed again after the main window; "-" for none
# TITLE[ID]    a regular expression for the program's window titles, for programs whose windows
#              belong to another process than the command that started them
# QUITMENU[ID] the menu item that quits (default Project/Quit; "-": close the windows instead)
# Hooks (shell functions, ID with '-' written as '_'), with the helpers of suite-lib.sh:
#   pre_host_ID DIR DRIVE  on the PC, after copying the program (settings files, ...)
#   pre_ID                 on the Amiga before each start
#   dismiss_ID             after the first window appeared: answer start-up requesters
#   quit_ID                before the quit menu item / close gadgets
P() { local IFS="|"; PROGS+=("$*"); }
PROGS=()
declare -A TITLE QUITMENU

# ---- MCC demos ----
P nlist-demo      mui3 nlist/x/MCC_NList/Demos/AmigaOS3        NList-Demo                30 -
P nlisttree-demo  mui3 nlist/x/MCC_NList/Demos/AmigaOS3        NListtree-Demo            30 -
P texteditor-demo mui3 texteditor/x/MCC_TextEditor/Demos       TextEditor-Demo-AmigaOS3  30 -
TITLE[texteditor-demo]='^TextEditor'      # it starts a process of its own; the command ends
for n in 1 2 3 4 5 6 7 8 9 10; do
  P thebar-demo$n mui3 thebar/x/MCC_TheBar/Demos               demo$n-AmigaOS3           30 -
done
P textinput-demo  mui3 textinput/x/MCC_Textinput/Demos         Textinput-Demo            30 -

# ---- programs, MUI 3.x era (must run on MUI 3.8) ----
P yam25        mui3 "yam25/x/YAM 2.5"                     YAM                  120 "Settings/Configuration...;Settings/Address book..."
P muimine      mui3 muimine/x/MUIMineInstall/progfiles     MUIMine              40 -
P installerng  mui3 installerng/x/InstallerNG              "c/InstallerNG.020 Install" 40 -
P amftp        mui3 amftp/x/AmFTP                          AmFTP                60 -
TITLE[amftp]='^AmFTP'                     # its first start runs AmFTPPrefs, a program of its own
P atc          mui3 atc/x/AmiTradeCenter                   AmiTradeCenter       60 -
P simplemail   mui3 simplemail/x/SimpleMail                SimpleMail           90 -
QUITMENU[simplemail]=-                    # no "wasabi run" while it runs (wasabid dies), see below
P newscoaster  mui3 newscoaster/x/NewsCoaster              NewsCoaster          60 -
P ibrowse25    mui3 "ibrowse25/x/IBrowse2.5.9-020+"        IBrowse              90 -
P amirc        mui3 amirc/x/AmIRC-68000                    AmIRC                60 -
P wookiechat   mui3 wookiechat/x/WookieChat2.11_OS3_Installer WookieChat_OS3    60 -
P amtelnet     mui3 amtelnet/x/AmTelnet-II-24              AmTelnet             60 -
P scout        mui3 scout/x/Scout                          Scout.os3            60 -
P muibase      mui3 muibase/x/MUIbase                      MUIbase-m68k-amigaos 90 -
P homebank     mui3 homebank/x/HomeBank                    HomeBank             60 -
P cmanager     mui3 cmanager/x/CManager                    CManager             60 -
P ro           mui3 ro/x/RO                                Binary/RO,binary     60 -
P multiren     mui3 multiren/x/MultiRen                    MultiRen             60 "Project/Preferences..."
P archiver     mui3 archiver/x/Archiver                    Archiver020_fpu      60 -
P annotate     mui3 annotate/x/Annotate                    Ann.AmigaOS3         60 -
P amigaampprefs mui3 amigaampprefs/x/AmigaAMP-Prefs        AmigaAMP-Prefs       40 -
P viewcsv      mui3 viewcsv/x/ViewCSV                      ViewCSV.020          40 -
P todo         mui3 todo/x/ToDo                            ToDo                 40 -

# ---- programs that need or want MUI 4/5 ----
P yam210       mui5 "yam210/x/YAM 2.10dev-20260726"        YAM                  120 "Settings/Configuration..."
P ibrowse30    mui5 ibrowse30/x/IBrowse3.0-OS3             IBrowse              90 -
P dizzytorrent mui5 dizzytorrent/x/dizzytorrent2.22        DizzyTorrent         60 -
P amigagpt     mui5 amigagpt/x/AmigaGPT/AmigaGPT           AmigaGPT_OS3         90 -
P mqttexplorer mui5 mqttexplorer/x/MQTTExplorer            MqttExplorer         40 -

# ---- hooks ----
# YAM: two "Ignore always" requesters (charset, AmiSSL: underlined a), then the error window
# (missing datatypes on 3.1) and the update check's window, both closed
dismiss_yam() {
  local i
  for i in 1 2 3 4 5 6 7 8; do
    if   [ -n "$(winline "Snapshot Warning")" ]; then answer "Go on"      # 2.10dev: a system requester
    elif [ -n "$(winline "charset warning")" ]; then press "charset warning" a
    elif [ -n "$(winline "AmiSSL")" ]; then press "AmiSSL" a
    elif [ -n "$(winline "Update notification")" ]; then wasabi ui click close --window "Update notification" >/dev/null 2>&1
    elif [ -n "$(winline "Error messages")" ]; then wasabi ui click close --window "Error messages" >/dev/null 2>&1
    else break; fi
    sleep 3; settle 8
  done
}
dismiss_yam25() { dismiss_yam; }
dismiss_yam210() { dismiss_yam; }
# NewsCoaster: the first start asks for a user (string gadget first in the cycle chain, Okay = o)
dismiss_newscoaster() {
  [ -n "$(winline "New User")" ] || return 0
  activate "New User"; key press tab; key type suite; key press return; sleep 1; key press o; sleep 8
}
# AmFTP: the first start runs its preferences program; Save (s), then the evaluation notice:
# Test AmFTP (t)
dismiss_amftp() {
  [ -n "$(winline "Preferences")" ] && { press "Preferences" s; sleep 5; }
  waitwin "AmFTP." 30; settle 8
  local l; l=$(wins | awk -F'\t' '$1=="AmFTP"' | head -1)
  [ -n "$l" ] && press "AmFTP" t
}
# WookieChat: Project/Quit asks "Quit Program?" with Yes/No (no underlined letters): Yes is the
# second button from the right
quit_wookiechat() {
  menu "Project/Quit" || wasabi ui click close --window "Wookiechat" >/dev/null 2>&1
  sleep 3
  local l w h; l=$(winline "Quit Program"); [ -n "$l" ] || return 0
  # Yes sits about 3/10 of the width from the right edge, half way down (MUI 3.8 draws the
  # requester in the program's small font, Zune in topaz: no fixed offset fits both)
  w=$(echo "$l" | cut -f5); h=$(echo "$l" | cut -f6)
  clickrel "Quit Program" -$((w * 3 / 10)) -$((h * $(yscale) / 2))
}
# MUIMine: its pictures are in the archive's images drawer, next to progfiles
pre_host_muimine() { cp -a "$REAL/muimine/x/MUIMineInstall/images" "$1/images"; }
# MultiRen: two notices in front of its main window (full screen height): "Please register"
# (OK bottom right, beside "Easy register now!") and the welcome (OK bottom middle)
dismiss_multiren() {
  local i l w h
  for i in 1 2 3; do
    l=$(winline "MultiRen"); [ -n "$l" ] || return 0
    w=$(echo "$l" | cut -f5); h=$(echo "$l" | cut -f6)
    [ "$h" -ge 200 ] && return 0
    if [ "$i" = 1 ] && [ "$w" -lt 450 ]; then clickrel "MultiRen" -28 -22
    else clickrel "MultiRen" $((w / 2)) -22; fi
    sleep 2
  done
}
# AmigaAMP-Prefs: no settings yet ("Could not load the prefs"): OK, the bottom middle button
dismiss_amigaampprefs() {
  local l w; l=$(winline "AmigaAMP"); [ -n "$l" ] || return 0
  w=$(echo "$l" | cut -f5); clickrel "AmigaAMP" $((w / 2)) -22
}
# IBrowse: the demo notice and the error requesters (missing datatypes on 3.1): Ok (o)
dismiss_ibrowse() {
  local i
  for i in 1 2 3 4 5 6; do
    if [ -n "$(winline "Info")" ]; then press "Info" o
    elif [ -n "$(winline "IBrowse Error")" ]; then press "IBrowse Error" o
    elif [ -n "$(winline "IBrowse request")" ]; then press "IBrowse request" o
    else break; fi
    settle 8
  done
}
dismiss_ibrowse25() { dismiss_ibrowse; }
dismiss_ibrowse30() { dismiss_ibrowse; }
# AmTelnet: the demo notice; "Continue testing" is its rightmost button (no underlined letter)
dismiss_amtelnet() { [ -n "$(winline "Demo Notice")" ] && clickrel "Demo Notice" -84 -32; }
# Contact Manager: the first start asks for its database drawer with MUI_AslRequest: on MUI 3.8
# an ASL requester (OK), on Zune Zune's own requester (another process; Save, bottom left)
TITLE[cmanager]='^(Contact Manager|Select the directory)'
dismiss_cmanager() {
  answer OK; sleep 2
  [ -n "$(winline "Select the directory")" ] && clickrel "Select the directory" 82 -39
  waitwin "Contact Manager" 30
}

# ---- classes and libraries ----
# The MUI classes every target gets (third-party, from their own archives), as "LOCAL [NAME]"
# (NAME: the file name on the Amiga if it differs). On MUI 3.8 and MUI 5 a class that the MUI
# install already has is left alone (theirs is the reference). The mui38usr ones are the classes
# that come with MUI 3.8 itself: Zune gets its own build of them where it has one.
# Toolbar.mcc is the original 15.6 (NewsCoaster's copy): TheBar's Toolbar.mcc 26.2 left
# NewsCoaster's main window empty and its task waiting on MUI 3.8.
CLASSES=(
  nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NBalance.mcc nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NBitmap.mcc
  nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NFloattext.mcc nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NList.mcc
  nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NListtree.mcc nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NListtree.mcp
  nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NListview.mcc nlist/x/MCC_NList/Libs/MUI/AmigaOS3/NListviews.mcp
  texteditor/x/MCC_TextEditor/Libs/MUI/AmigaOS3/TextEditor.mcc texteditor/x/MCC_TextEditor/Libs/MUI/AmigaOS3/TextEditor.mcp
  betterstring/x/MCC_BetterString/Libs/MUI/AmigaOS3/BetterString.mcc betterstring/x/MCC_BetterString/Libs/MUI/AmigaOS3/BetterString.mcp
  betterstring/x/MCC_BetterString/Libs/MUI/AmigaOS3/HotkeyString.mcc
  thebar/x/MCC_TheBar/Libs/MUI/AmigaOS3/TheBar.mcc thebar/x/MCC_TheBar/Libs/MUI/AmigaOS3/TheBar.mcp
  thebar/x/MCC_TheBar/Libs/MUI/AmigaOS3/TheBarVirt.mcc thebar/x/MCC_TheBar/Libs/MUI/AmigaOS3/TheButton.mcc
  newscoaster/x/NewsCoaster/MUI/Toolbar.mcc newscoaster/x/NewsCoaster/MUI/Toolbar.mcp
  newscoaster/x/NewsCoaster/MUI/InfoText.mcc
  textinput/x/MCC_Textinput/MUI/Textinput.mcc textinput/x/MCC_Textinput/MUI/Textinput.mcp
  textinput/x/MCC_Textinput/MUI/Textinputscroll.mcc
  popph/x/MCC_Popph/Libs/mui/Popplaceholder.mcc
  urltext/x/MCC_Urltext/MUI/Urltext.mcc urltext/x/MCC_Urltext/MUI/Urltext.mcp
  mui38usr/x/MUI/Libs/mui/Busy.mcc mui38usr/x/MUI/Libs/mui/Busy.mcp mui38usr/x/MUI/Libs/mui/Crawling.mcc
  mui38usr/x/MUI/Libs/mui/Listtree.mcc mui38usr/x/MUI/Libs/mui/Listtree.mcp mui38usr/x/MUI/Libs/mui/ListtreeName.mcc
  amtelnet/x/AmTelnet-II-24/MUI/Term.mcc
  cmanager/x/CManager/MUI/Calendar.mcc cmanager/x/CManager/MUI/Calendar.mcp cmanager/x/CManager/MUI/CManager.mcc
  "cmanager/x/CManager/MUI/lamp.mcc Lamp.mcc" "cmanager/x/CManager/MUI/lamp.mcp Lamp.mcp"
  cmanager/x/CManager/MUI/MiniMailer.mcc
  "ibrowse25/x/IBrowse2.5.9-020+/mui/Newstring.mcc" "ibrowse25/x/IBrowse2.5.9-020+/mui/Newstring.mcp"
  "ibrowse25/x/IBrowse2.5.9-020+/mui/StringHistory.mcc" "ibrowse25/x/IBrowse2.5.9-020+/mui/TextHistory.mcc"
  "ibrowse25/x/IBrowse2.5.9-020+/ibcc/IBCacheBrowser.mcc"
  guigfxmcc/x/MCC_Guigfx/Libs/MUI/Guigfx.mcc
  "amigagpt/x/AmigaGPT/MCC/ActionTextEditor_OS3.mcc ActionTextEditor.mcc"
  "amigagpt/x/AmigaGPT/MCC/AudioPlayer_OS3.mcc AudioPlayer.mcc"
)
# Libraries into SYS:Libs (left alone where the machine has the library already)
LIBRARIES=(
  codesets/x/codesets/Libs/AmigaOS3/codesets.library
  textinput/x/MCC_Textinput/Libs/vapor_toolkit.library
  amtelnet/x/AmTelnet-II-24/Libs/vapor_update.library amtelnet/x/AmTelnet-II-24/Libs/vapor_registration.library
  cmanager/x/CManager/Libs/CManager.library
  guigfxmcc/x/MCC_Guigfx/Libs/guigfx.library guigfxmcc/x/MCC_Guigfx/Libs/render.library
  simplemail/x/SimpleMail/Libs/expat.library
)

# ---- downloads (suite-fetch.sh): NAME URL ----
FETCH=(
  "nlist https://aminet.net/dev/mui/MCC_NList-0.128.lha"
  "texteditor https://aminet.net/dev/mui/MCC_TextEditor-15.56.lha"
  "betterstring https://aminet.net/dev/mui/MCC_BetterString-11.36.lha"
  "thebar https://aminet.net/dev/mui/MCC_TheBar-26.22.lha"
  "codesets https://aminet.net/util/libs/codesets-6.22.lha"
  "textinput https://aminet.net/dev/mui/MCC_TextInput.lha"
  "popph https://aminet.net/dev/mui/MCC_Popph.lha"
  "urltext https://aminet.net/dev/mui/MCC_Urltext.lha"
  "guigfxmcc https://aminet.net/dev/mui/MCC_Guigfx.lha"
  "mui38usr https://aminet.net/util/libs/mui38usr.lha"
  "yam25 https://github.com/jens-maus/yam/releases/download/2.5/YAM25-AmigaOS3.lha"
  "muimine https://aminet.net/game/think/MUIMine.lha"
  "installerng https://aminet.net/util/sys/InstallerNG.lha"
  "amftp https://aminet.net/comm/tcp/AmFTP191.lha"
  "atc https://aminet.net/comm/tcp/AmiTradeCenter.lha"
  "simplemail https://downloads.sourceforge.net/project/simplemail/simplemail/simplemail-0.45/simplemail-0.45-os3.lha"
  "newscoaster https://aminet.net/comm/news/newscoaster.lha"
  "ibrowse25 https://www.ibrowse-dev.net/resources/IBrowse2.5.9-020%2B.lha"
  "amirc https://aminet.net/comm/irc/amirc_362_68000.lha"
  "wookiechat https://aminet.net/comm/irc/WookieChat2.11_OS3.lha"
  "amtelnet https://aminet.net/comm/tcp/amtelnet.lha"
  "scout https://aminet.net/util/moni/Scout_os3.lha"
  "muibase https://downloads.sourceforge.net/project/beebase/muibase/MUIbase-4.5/MUIbase-4.5.lha"
  "homebank https://aminet.net/biz/misc/HomeBank.lha"
  "cmanager https://aminet.net/comm/misc/CManager.lha"
  "ro https://aminet.net/util/dir/RO_V127.lha"
  "multiren https://aminet.net/util/misc/MultiRen.lha"
  "archiver https://aminet.net/util/arc/Archiver.lha"
  "annotate https://aminet.net/text/edit/Annotate_usr.lha"
  "amigaampprefs https://aminet.net/mus/play/AmigaAMP-Prefs.lha"
  "viewcsv https://aminet.net/biz/dbase/ViewCSV.lha"
  "todo https://aminet.net/util/misc/ToDo.lha"
  "yam210 https://github.com/jens-maus/yam/releases/download/nightly-builds/YAM210dev-20260726-79ed41-AmigaOS3.lha"
  "ibrowse30 https://www.ibrowse-dev.net/resources/IBrowse3.0a-OS3.lha"
  "dizzytorrent https://aminet.net/comm/tcp/dizzytorrent2.lha"
  "amigagpt https://aminet.net/util/misc/AmigaGPT.lha"
  "mqttexplorer https://aminet.net/comm/net/MQTTExplorer.lha"
)
