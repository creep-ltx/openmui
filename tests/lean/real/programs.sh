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
# RAMRUN[ID]   1: copy the program's folder to RAM: and start it there (AmTelnet does not start
#              from Amiberry's host-directory drive: its own re-start of itself fails there)
# Hooks (shell functions, ID with '-' written as '_'), with the helpers of suite-lib.sh:
#   pre_host_ID DIR DRIVE  on the PC, after copying the program (settings files, ...)
#   pre_ID                 on the Amiga before each start
#   dismiss_ID             after the first window appeared: answer start-up requesters
#   quit_ID                before the quit menu item / close gadgets
P() { local IFS="|"; PROGS+=("$*"); }
PROGS=()
declare -A TITLE QUITMENU RAMRUN

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
TITLE[simplemail]='^SimpleMail'           # its full task name is longer than "wasabi ps" shows
P newscoaster  mui3 newscoaster/x/NewsCoaster              NewsCoaster          60 -
P ibrowse25    mui3 "ibrowse25/x/IBrowse2.5.9-020+"        IBrowse              90 -
P amirc        mui3 amirc/x/AmIRC-68000                    AmIRC                60 -
P wookiechat   mui3 wookiechat/x/WookieChat2.11_OS3_Installer WookieChat_OS3    60 -
P amtelnet     mui3 amtelnet/x/AmTelnet-II-24              AmTelnet             60 -
RAMRUN[amtelnet]=1
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
# MUI 5's own demos that MUI 3.8 does not have (MUI 4/5 classes and features); each copies the
# whole Demos drawer (anim.gif, boing.png are used by some)
for d in Aboutbox Animals Calltips Class4 Clock ElidedText EmbeddedImages GIFAnim Hyperlink \
         LayGroup List1 MenuImages Pointers Textdata Title; do
  # EmbeddedImages unpacks and dithers five 125x125 ARGB pictures: over 30 s on the 68020
  w=30; [ $d = EmbeddedImages ] && w=150
  P mui5-${d,,} mui5 mui5/x/MUI/Demos $d $w -
done
# its task name (Workbench:Real/mui5-embeddedimages/EmbeddedImages) is longer than wasabi shows
TITLE[mui5-embeddedimages]='^Embedded images'
P passpocket   mui5 "passpocket/x/PassPocket/PassPocket OS3" PassPocket       40 -
P ticklish     mui5 ticklish/x/Ticklish                    Ticklish             60 -
P tabgroup-demo mui5 tabgroup/x/MCC_TabGroup/Demos         TabGroup-Demo        30 -
P lumizip      mui5 lumizip/x/LumiZip                      LumiZip              40 -

# ---- programs added 2026-11-11 (branch compat1): more of what people use, old and new ----
# Chosen from Aminet's most downloaded MUI programs and its 2026 uploads (tests/lean/real-programs.md
# has versions, licences and what each asks for). All say MUI 3.8 (or less); the 2026 ones
# use MUI 4/5 features where present. Programs that need AmiSSL to get to a window were left out.
P voyager      mui3 voyager/x/Voyager                      V                    90 -
P evenmore     mui3 evenmore/x/EvenMore                    EvenMore             40 -
P igame        mui3 igame/x/iGame-v2.6.1                   iGame                60 -
P yam29        mui3 "yam29/x/YAM 2.9p1"                    YAM                  120 "Settings/Configuration..."
P amigg        mui3 amigg/x/AmiGG                          AmiGG_m68k           60 -
P freeciv      mui3 freeciv/x/Freeciv                      civclient            90 -
P apdf         mui3 apdf/x/Apdf                            Apdf                 60 -
P mcpprefs     mui3 mcp/x/MCP                              Prefs/MCPPrefs       60 -
P screentabprefs mui3 screentab/x/ScreenTab                Prefs/ScreenTabPrefs 40 -
P tinymeterprefs mui3 tinymeter/x/TinyMeter                Prefs/TinyMeter      40 -
P newsrog      mui3 newsrog/x/NewsRog                      NewsRog              90 -
P md2          mui3 md2/x/MicroDot-II-14                   MicroDot             90 -
P amplifier    mui3 amplifier/x/amplifier                  AMPlifier            60 -
P makecd       mui3 makecd/x/MakeCD_3.2d                   MakeCD               60 -
P ahiprefs     mui3 ahiusr/x/AHI/User                      Prefs/AHI_MUI        40 -
P fiasco       mui3 fiasco/x/Fiasco_2.22                   Fiasco               60 -
P amster       mui3 amster/x/Amster_Install                Amster               60 -
P beebase      mui3 beebase/x/BeeBase                      BeeBase-m68k-amigaos 120 -
P amimodradio  mui3 amimodradio/x/AmiModRadio              AmiModRadio_MUI      90 -
P dmodp        mui3 dmodp/x/DASModPlayer                   DASModPlayer         60 -
P amfinger     mui3 amfinger/x                             AmFinger             40 -
P abacus       mui3 abacus/x/Abacus                        Abacus               40 -
P address      mui3 address/x/Address                      Bin/Address          40 -
P rqchoicemui  mui3 rqchoicemui/x/RequestChoiceMUI_1.1     "RequestChoiceMUI Suite \"A question for the suite?\" \"Yes|No\"" 30 -
P assampler    mui3 assampler/x/Assampler                  Assampler            60 -
P cyclone      mui3 cyclone/x/Cyclone_125                  Bin020/Cyclone       60 -
P amigamark    mui3 amigamark/x/AmigaMARK                  AmigaMARK            60 -
P ali          mui3 ali/x/ALI                              ALI                  40 -
P lumiamp      mui3 lumiamp/x/LumiAMP                      LumiAMP              60 -
P lumipass     mui3 lumipass/x/LumiPass                    LumiPass             90 -
P lumitorrent  mui3 lumitorrent/x/LumiTorrent              LumiTorrent          60 -
P lumiftp      mui3 lumiftp/x/LumiFTP                      LumiFTP              60 -
P tunefinder   mui3 tunefinder/x/TuneFinderMUI             TuneFinderMUI        40 -
P u64ctl       mui3 u64ctl/x/u64                           u64mui               40 -
P amifritzi    mui3 amifritzi/x/AmiFritzi                  AmiFritzi            40 -
P nfsgui       mui3 nfsgui/x/NFS-GUI                       NFS-GUI              40 -
P ami2ha       mui3 ami2ha/x/ami2ha                        ami2ha.020           40 -
P amihomeassist mui3 amihomeassist/x/AmiHomeassist         AmiHomeassist        40 -
P amiatari2600 mui3 amiatari2600/x/AmiAtari2600            AmiAtari2600         40 -
P amipg        mui3 amipg/x/AmiPG                          AmiPG_MUI            40 -
P xsysinfo     mui3 xsysinfo/x/xSysInfo-0.12.0             "xSysInfo MUI"       40 -
P kompensator  mui3 kompensator/x/Kompensator              Kompensator          40 -
P rsgui        mui3 rsgui/x/rsGUI                          RSGUI                40 -
P layview      mui3 layview/x/LayView                      LayView              40 -
P 3doc         mui3 3doc/x/3dconverter                     3dconverter          90 -
P aminetuploader mui3 aminetuploader/x/Aminet-Uploader     Aminet-Uploader-MUI  40 -
P jmri         mui3 jmri/x/JMRIController                  JMRIController       40 -
P amidrop      mui3 amidrop/x/AmiDrop_MUI                  Setup/AmiDrop_MUI    40 -

# ---- add-on class (MCC) demos, group mcc (2026-11-11): each class's own demo program ----
# Run on every target with the same class files (CLASSES below); the classes that the suite
# already installs (NList, TextEditor, BetterString, TheBar, Toolbar 15.6, Textinput, Popph,
# Urltext, Busy, Lamp, Calendar, MiniMailer, Guigfx, LayGroup, GIFAnim) stay as they are.
P htmlview-demo   mcc htmlview/x/MCC_HTMLview/Demos            HTMLview-Demo        30 -
P toolbar-demo    mcc toolbar/x/MCC_Toolbar/Demos              Toolbar-Demo         30 -
P lamp-demo       mcc lamp/x/MCC_Lamp/Demos                    Lamp-Demo            30 -
for n in 1 2 3 4 5 6 7 8 9; do
  P speedbar-demo$n mcc speedbar/x/MCC_SpeedBar/Demos          demo$n               30 -
done
for n in 1 2 3; do
  P bwin-demo$n   mcc bwin/x/MCC_BWin/Demos                    demo$n               30 -
done
for n in 1 2 3 4; do
  P calendar-demo$n mcc calendar/x/MCC_Calendar/Demos          demo$n               30 -
done
P monthnavi-demo  mcc monthnavi/x/MCC_MonthNavigator/Demos     MonthNavigator-Demo  30 -
P busy-demo       mcc busy24/x/MCC_Busy/Demos                  Busy-Demo            30 -
P hexedit-demo    mcc hexedit/x/MCC_HexEdit/Demos              HexEdit-Demo         30 -
P twfmultiled-demo mcc twfmultiled/x/MCC_TWFmultiLED/Demos     TWFmultiLED-demo     30 -
P tearoff-demo    mcc tearoff/x/MCC_TearOff/demo               TearOffDemo          30 -
P compactwin-demo mcc compactwin/x/MCC_CompactWindow/demo      CompactWindowDemo    30 -
P tablegroup-demo mcc tablegroup/x/MCC_TableGroup/Demos        MCCTableDemo         30 -
P mailtext-demo   mcc mailtext/x/MCC_Mailtext/Demos            Mailtext-Demo        30 -
P mathstring-demo mcc mathstring/x/MCC_MathString/Demos        MathString-Demo      30 -
P tipofday-demo   mcc tipofday/x/MCC_TipOfDay/Demos            tip_demo             30 -
P transferanim-demo mcc transferanim/x/mcc_transferanim/demos  TransferAnim-Demo    30 -
P minimailer-demo mcc minimailer/x/MCC_MiniMailer/Demos        demo                 30 -
P htmltext-demo   mcc htmltext/x/MCC_HTMLtext/Demos            HTMLtext-DEMO        30 -
P settingswindow-demo mcc settingswindow/x/MCC_SettingsWindow/Demos SettingsWindow-Demo 30 -
P mysticview-demo mcc mysticview/x/mcc_mysticview/demos        MysticView-Demo      30 -
P icon-demo       mcc iconmcc/x/MCC_Icon/Demos                 Icon-Demo            30 -
P datestring-demo mcc datestring/x/MCC_DateString/Demos        DateString-Demo      30 -
P timestring-demo mcc timestring/x/MCC_TimeString/Demos        TimeString-Demo      30 -
P envelope-demo   mcc envelope/x/MCC_Envelope/Demos            Envelope-Demo        30 -
P pkb-demo        mcc pkb/x/MCC_Pkb/Demos                      Pkb-Demo             30 -
P wheel-demo      mcc wheel/x/MCC_Wheel/Demos                  Wheel-Demo           30 -
P hotkeystring-test mcc hotkeystring/x/MCC_HotkeyString/AmigaOS-68k/Demos HotkeyStringTest 30 -
P popph-demo      mcc popph/x/MCC_Popph/Demos                  popph_demo           30 -
P urltext-demo    mcc urltext/x/MCC_Urltext/Demos              Urltext              30 -
P guigfx-demo     mcc guigfxmcc/x/MCC_Guigfx/Demos             Guigfx-Demo          30 -
P laygroup-demo   mcc laygroup/x/MCC_LayGroup-19.2/Demos       LayGroup-Demo        30 -
P gifanim-demo    mcc gifanim/x/MCC_GIFAnim/Demos              demo                 30 -
P nbitmap-demo    mcc nlist/x/MCC_NList/Demos/AmigaOS3         NBitmap-Demo         30 -
P nlist102-demo   mcc nlist102/x/demo                          NList-Demo           30 -
P tron-demo       mcc mui38dev/x/MUI/Developer/ExtClasses/MCC_Tron/Demos Tron-Demo 30 -

# ---- hooks ----
# YAM: two "Ignore always" requesters (charset, AmiSSL: underlined a), then the error window
# (OS 3.1 has no PNG datatype: YAM 2.10's 50 theme pictures all fail, so it comes on every run
# on the zune target) and the update check's window, both closed. The loop ends only after two
# quiet checks a few seconds apart, as the error window may come late. With a "wasabi snoop"
# attached (snooplog.sh's LoadSeg trace) YAM 2.10 does not answer for minutes while it tries
# its pictures through datatypes (every DOS call is traced): neither the close gadget nor the
# window's Close button closes the error window, YAM does not quit, and the second run is lost.
# That was the extra "Error messages" window of smaller2's MUI 4/5 group runs (2026-11-11,
# branch listslim: yam210 alone with a snoop shows it, without one never). Trace the MUI 4/5
# group without yam210, or run yam210 apart without the trace.
dismiss_yam() {
  local i quiet=0
  for i in 1 2 3 4 5 6 7 8 9 10; do
    if   [ -n "$(winline "Snapshot Warning")" ]; then answer "Go on"      # 2.10dev: a system requester
    elif [ -n "$(winline "charset warning")" ]; then press "charset warning" a
    elif [ -n "$(winline "AmiSSL")" ]; then press "AmiSSL" a
    elif [ -n "$(winline "Update notification")" ]; then wasabi ui click close --window "Update notification" >/dev/null 2>&1
    # 2.10dev's "Invalid configuration" request (Ok): a MUI request, the application asleep until
    # it is answered, so the error window's close gadget does nothing while it is up (on MUI 3.8
    # too; seen on Amiberry, where the errors come faster and the request is up before the window
    # could be closed) and the menus are gone
    elif [ -n "$(winline "Confirmation Request")" ]; then press "Confirmation Request" o
    elif [ -n "$(winline "Error messages")" ]; then log "$P_ID: dismiss $i Error messages"
      wasabi ui click close --window "Error messages" >/dev/null 2>&1
    else
      quiet=$((quiet + 1)); log "$P_ID: dismiss $i quiet"; [ $quiet -ge 2 ] && break
      sleep 5; settle 8; continue
    fi
    quiet=0; sleep 3; settle 8
  done
}
dismiss_yam25() { dismiss_yam; }
dismiss_yam210() { dismiss_yam; }
dismiss_yam29() { dismiss_yam; }
# AHI's MUI prefs: ahi.device and its drivers from the same archive into the drive's Devs
pre_host_ahiprefs() {
  mkdir -p "$2/Devs/AHI"; cp "$REAL/ahiusr/x/AHI/User/Devs/ahi.device" "$2/Devs/"
  cp "$REAL/ahiusr/x/AHI/User/Devs/AHI/"*.audio "$2/Devs/AHI/"
}
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
  tabgroup/x/MCC_TabGroup/Libs/MUI/TabGroup.mcc tabgroup/x/MCC_TabGroup/Libs/MUI/TabGroup.mcp
  # MUI 5 brings its own LayGroup and GIFAnim (kept there); MUI 3.8 and Zune users install
  # the classic Aminet ones
  laygroup/x/MCC_LayGroup-19.2/Libs/MUI/LayGroup.mcc laygroup/x/MCC_LayGroup-19.2/Libs/MUI/LayGroup.mcp
  gifanim/x/MCC_GIFAnim/MUI/GIFAnim.mcc
  # the add-on class set (group mcc, 2026-11-11)
  htmlview/x/MCC_HTMLview/Libs/MUI/AmigaOS3/HTMLview.mcc htmlview/x/MCC_HTMLview/Libs/MUI/AmigaOS3/HTMLview.mcp
  speedbar/x/MCC_SpeedBar/MUI/SpeedBar.mcc speedbar/x/MCC_SpeedBar/MUI/SpeedBar.mcp
  speedbar/x/MCC_SpeedBar/MUI/SpeedBarCfg.mcc speedbar/x/MCC_SpeedBar/MUI/SpeedBarVirt.mcc
  speedbar/x/MCC_SpeedBar/MUI/SpeedButton.mcc
  bwin/x/MCC_BWin/MUI/BWin.mcc bwin/x/MCC_BWin/MUI/BWin.mcp
  monthnavi/x/MCC_MonthNavigator/Libs/mui/MonthNavigator.mcc monthnavi/x/MCC_MonthNavigator/Libs/mui/MonthNavigator.mcp
  hexedit/x/MCC_HexEdit/Libs/MUI/HexEdit.mcc hexedit/x/MCC_HexEdit/Libs/MUI/HexEdit.mcp
  twfmultiled/x/MCC_TWFmultiLED/Libs/mui/TWFmultiLED.mcc twfmultiled/x/MCC_TWFmultiLED/Libs/mui/TWFmultiLED.mcp
  tearoff/x/MCC_TearOff/libs/CompactWindow.mcc tearoff/x/MCC_TearOff/libs/CompactWindow.mcp
  tearoff/x/MCC_TearOff/libs/TearOff.mcp tearoff/x/MCC_TearOff/libs/TearOffBay.mcc tearoff/x/MCC_TearOff/libs/TearOffPanel.mcc
  tablegroup/x/MCC_TableGroup/Libs/mui/TableGroup.mcc
  mailtext/x/MCC_Mailtext/Libs/MUI/Mailtext.mcc mailtext/x/MCC_Mailtext/Libs/MUI/Mailtext.mcp
  mathstring/x/MCC_MathString/Libs/MUI/MathString.mcc
  tipofday/x/MCC_TipOfDay/Libs/mui/tipboard.mcc tipofday/x/MCC_TipOfDay/Libs/mui/tipwindow.mcc
  transferanim/x/mcc_transferanim/libs/mui/TransferAnim.mcc transferanim/x/mcc_transferanim/libs/mui/TransferAnim.mcp
  htmltext/x/MCC_HTMLtext/Libs/MUI/HTMLtext.mcc htmltext/x/MCC_HTMLtext/Libs/MUI/HTMLtext.mcp
  settingswindow/x/MCC_SettingsWindow/Libs/MUI/SettingsWindow.mcc
  mysticview/x/mcc_mysticview/libs/mui/MysticView.mcc
  iconmcc/x/MCC_Icon/Libs/MUI/Icon.mcc
  datestring/x/MCC_DateString/Libs/mui/DateString.mcc
  timestring/x/MCC_TimeString/Libs/mui/TimeString.mcc
  envelope/x/MCC_Envelope/Libs/MUI/Envelope.mcc envelope/x/MCC_Envelope/Libs/MUI/Envelope.mcp
  pkb/x/MCC_Pkb/Libs/MUI/Pkb.mcc pkb/x/MCC_Pkb/Libs/MUI/Pkb.mcp
  wheel/x/MCC_Wheel/Libs/mui/Wheel.mcc wheel/x/MCC_Wheel/Libs/mui/Wheel.mcp
)
# Libraries into SYS:Libs (left alone where the machine has the library already)
LIBRARIES=(
  codesets/x/codesets/Libs/AmigaOS3/codesets.library
  textinput/x/MCC_Textinput/Libs/vapor_toolkit.library
  amtelnet/x/AmTelnet-II-24/Libs/vapor_update.library amtelnet/x/AmTelnet-II-24/Libs/vapor_registration.library
  cmanager/x/CManager/Libs/CManager.library
  guigfxmcc/x/MCC_Guigfx/Libs/guigfx.library guigfxmcc/x/MCC_Guigfx/Libs/render.library
  simplemail/x/SimpleMail/Libs/expat.library
  # for the programs and classes added 2026-11-11
  voyager/x/Voyager/Libs/amitls.library
  fiasco/x/Fiasco_2.22/gtlayout.library
  xsysinfo/x/xSysInfo-0.12.0/Libs/identify.library
  mcp/x/MCP/Libs/mcpgfx.library mcp/x/MCP/Libs/reqtools.library
  monthnavi/x/MCC_MonthNavigator/Libs/date.library
  transferanim/x/mcc_transferanim/libs/imagepool.library
  mysticview/x/mcc_mysticview/mysticviewlibrary/libs/mysticview.library
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
  "mui5 https://github.com/amiga-mui/muidev/releases/download/MUI-5.0-20210831/MUI-5.0-20210831-os3.lha"
  "passpocket https://aminet.net/util/misc/PassPocket.lha"
  "ticklish https://aminet.net/util/misc/Ticklish_m68k.lha"
  "tabgroup https://aminet.net/dev/mui/MCC_TabGroup.lha"
  "lumizip https://aminet.net/util/arc/LumiZip.lha"
  "laygroup https://aminet.net/dev/mui/MCC_LayGroup-19.2.lha"
  "gifanim https://aminet.net/dev/mui/MCC_GIFAnim.lha"
  # ---- add-on class (MCC) demos, group mcc (2026-11-11) ----
  "htmlview https://aminet.net/dev/mui/MCC_HTMLview-13.4.lha"
  "toolbar https://aminet.net/dev/mui/mcc_toolbar-15.12.lha"
  "lamp https://aminet.net/dev/mui/MCC_Lamp.lha"
  "speedbar https://aminet.net/dev/mui/MCC_SpeedBar.lha"
  "bwin https://aminet.net/dev/mui/MCC_BWin.lha"
  "calendar https://aminet.net/dev/mui/MCC_Calendar.lha"
  "monthnavi https://aminet.net/dev/mui/MCC_MonthNavi.lha"
  "busy24 https://aminet.net/dev/mui/MCC_Busy2_4.lha"
  "nlist102 https://aminet.net/dev/mui/MCC_NList-0.102_68k.lha"
  "betterstring68k https://aminet.net/dev/mui/MCC_BetterString_68k.lha"
  "texteditor68k https://aminet.net/dev/mui/MCC_TextEditor_68k.lha"
  "thebar68k https://aminet.net/dev/mui/MCC_TheBar_68k.lha"
  "hexedit https://aminet.net/dev/mui/MCC_HexEdit.lha"
  "imgbutton https://aminet.net/dev/mui/MCC_ImgButton.lha"
  "twfmultiled https://aminet.net/dev/mui/MCC_TWFmultiLE.lha"
  "tearoff https://aminet.net/dev/mui/MCC_TearOff.lha"
  "compactwin https://aminet.net/dev/mui/MCC_CompactWin.lha"
  "tablegroup https://aminet.net/dev/mui/MCC_TableGroup.lha"
  "mailtext https://aminet.net/dev/mui/MCC_Mailtext.lha"
  "mathstring https://aminet.net/dev/mui/MCC_MathString.lha"
  "tipofday https://aminet.net/dev/mui/MCC_TipOfDay.lha"
  "transferanim https://aminet.net/dev/mui/MCC_TransferAn.lha"
  "bbalance https://aminet.net/dev/mui/MCC_BBalance.lha"
  "minimailer https://aminet.net/dev/mui/MCC_MiniMailer.lha"
  "htmltext https://aminet.net/dev/mui/MCC_HTMLtext.lha"
  "settingswindow https://aminet.net/dev/mui/MCC_SettingsWi.lha"
  "mysticview https://aminet.net/dev/mui/MCC_MysticView.lha"
  "volumelist https://aminet.net/dev/mui/DC-VLmui.lha"
  "iconmcc https://aminet.net/dev/mui/MCCIcon_1_1.lha"
  "datemcc https://aminet.net/dev/mui/MCC_Date.lha"
  "datestring https://aminet.net/dev/mui/MCC_DateString.lha"
  "timemcc https://aminet.net/dev/mui/MCC_Time.lha"
  "timestring https://aminet.net/dev/mui/MCC_TimeString.lha"
  "envelope https://aminet.net/dev/mui/MCC_Envelope.lha"
  "pkb https://aminet.net/dev/mui/MCC_Pkb14_6.lha"
  "wheel https://aminet.net/dev/mui/MCC_Wheel.lha"
  "hotkeystring https://aminet.net/dev/mui/MCC_HotkeyStr.lha"
  "muirexx https://aminet.net/dev/mui/MUIRexx_3_0a.lha"
  # ---- programs added 2026-11-11 (branch compat1) ----
  "voyager https://aminet.net/comm/www/Voyager-3.5.3-AmigaOS.lha"
  "evenmore https://aminet.net/text/show/EvenMore.lha"
  "igame https://aminet.net/util/misc/iGame.lha"
  "yam29 https://aminet.net/comm/yam/YAM29p1-AmigaOS3.lha"
  "amigg https://aminet.net/comm/misc/amigg.lha"
  "freeciv https://aminet.net/game/strat/freeciv.lha"
  "apdf https://aminet.net/gfx/show/Apdf_nofpu.lha"
  "mcp https://aminet.net/util/cdity/MCP130.lha"
  "screentab https://aminet.net/util/cdity/ScreenTab_3_2.lha"
  "tinymeter https://aminet.net/util/wb/Tinymeter_V431.lha"
  "newsrog https://aminet.net/comm/news/NewsRogDemo.lha"
  "md2 https://aminet.net/comm/news/md2_14.lha"
  "amplifier https://aminet.net/mus/play/AMPlifier.lha"
  "makecd https://aminet.net/disk/cdrom/MakeCD_3.2d.lha"
  "ahiusr https://aminet.net/driver/audio/ahiusr_4.18.lha"
  "fiasco https://aminet.net/biz/dbase/Fiasco_main.lha"
  "amster https://aminet.net/comm/tcp/Amster-main.lha"
  "beebase https://aminet.net/biz/dbase/BeeBase-1.2.lha"
  "amimodradio https://aminet.net/mus/play/AmiModRadio.lha"
  "dmodp https://aminet.net/mus/play/DMODP35b.lha"
  "amfinger https://aminet.net/comm/tcp/AmFinger1.5.lha"
  "abacus https://aminet.net/game/board/abacus220.lha"
  "address https://aminet.net/biz/dbase/Address.lha"
  "rqchoicemui https://aminet.net/util/sys/2b_RqChoiceMUI.lha"
  "assampler https://aminet.net/mus/edit/Assampler.lha"
  "cyclone https://aminet.net/comm/fido/Cyclone125c.lha"
  "amigamark https://aminet.net/util/moni/AmigaMARK-amigaos3.lha"
  "ali https://aminet.net/comm/irc/ALI-amigaos3.lha"
  "lumiamp https://aminet.net/mus/play/LumiAMP.lha"
  "lumipass https://aminet.net/util/crypt/LumiPass.lha"
  "lumitorrent https://aminet.net/comm/net/LumiTorrent.lha"
  "lumiftp https://aminet.net/comm/tcp/LumiFTP.lha"
  "tunefinder https://aminet.net/comm/misc/TuneFinderMUI.lha"
  "u64ctl https://aminet.net/comm/misc/u64ctl.lha"
  "amifritzi https://aminet.net/comm/tcp/AmiFritzi.lha"
  "nfsgui https://aminet.net/comm/net/NFS-GUI.lha"
  "ami2ha https://aminet.net/comm/tcp/ami2ha-0.5.lha"
  "amihomeassist https://aminet.net/comm/tcp/AmiHomeassist.lha"
  "amiatari2600 https://aminet.net/misc/emu/AmiAtari2600.lha"
  "amipg https://aminet.net/mus/midi/AmiPG_MUI.lha"
  "xsysinfo https://aminet.net/util/moni/xSysInfo.lha"
  "kompensator https://aminet.net/util/batch/Kompensator.lha"
  "rsgui https://aminet.net/comm/net/RSGUI.lha"
  "layview https://aminet.net/misc/sci/LayView.lha"
  "3doc https://aminet.net/gfx/3d/3doc-m68k-amigaos_v1.90.lha"
  "aminetuploader https://aminet.net/util/misc/AminetUploader.lha"
  "jmri https://aminet.net/misc/misc/JMRIController.lha"
  "amidrop https://aminet.net/comm/net/AmiDrop_MUI.lha"
)
