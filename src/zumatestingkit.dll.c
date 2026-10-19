/* zumatestingkit.dll - Special Developer Testing Kit Sidecar for Zuma Deluxe
 *
 * Requirements:
 * 1. Active ONLY when "zumatest.log" exists in the root folder.
 * 2. Inactive/inert when "zumatest.log" is absent.
 * 3. Provides a virtual user "DevTester" directly beneath "(CREATE A NEW USER)" in UserDialog (Image 1).
 * 4. Text color of "DevTester" is bright glowing cyan-blue (0x00E5FF).
 * 5. On MainMenu (Image 2), "Welcome to Zuma, DevTester!" is rendered in bright glowing blue.
 * 6. Adventure Mode: All stages up to Stage 12 unlocked.
 * 7. Gauntlet Mode: All levels unlocked and set up to Sun God rank (3).
 * 8. Zero disk pollution: Does not write to users.dat or user%d.dat.
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    void *allocator;
    char buf[16];
    size_t length;
    size_t capacity;
} StdString;

typedef struct {
    int mRed;
    int mGreen;
    int mBlue;
    int mAlpha;
} PopCapColor;

/* MSVC 7.1 (.NET 2003) Dinkumware std::_Tree_nod<std::map<std::string, short>> */
typedef struct MapNode {
    struct MapNode *left;     /* 0x00 */
    struct MapNode *parent;   /* 0x04 */
    struct MapNode *right;    /* 0x08 */
    StdString key;            /* 0x0C .. 0x27 (28 bytes) */
    short val;                /* 0x28 .. 0x29 (2 bytes) */
    char pad_val[2];          /* 0x2A .. 0x2B (2 bytes) */
    char color;               /* 0x2C (0 = Red, 1 = Black) */
    char is_nil;              /* 0x2D (1 = Nil head node!) */
    char pad[2];              /* 0x2E .. 0x2F */
} MapNode;

typedef struct {
    StdString mName;           /* 0x00..0x1B */
    int mId;                   /* 0x1C */
    int mTime;                 /* 0x20 */
    int mSelectedStage;        /* 0x24: 0 = Random map */
    char mField28;             /* 0x28: mIsSurvival (0 = Practice, 1 = Survival) */
    char mPad28[3];            /* alignment */
    int mField2c;              /* 0x2C: 12 */
    int mMaxStage;             /* 0x30: 12 */
    int mHighScore;            /* 0x34: 999999 */
    char mField38;             /* 0x38: 1 */
    char mField39;             /* 0x39: 0 */
    char mField3a;             /* 0x3A: 0 */
    char mPad3b;
    void *mMapAlloc;           /* 0x3C..0x3F */
    MapNode *mMapHead;         /* 0x40..0x43 */
    int mMapSize;              /* 0x44..0x47 */
    char mRest[0x260 - 0x48];  /* Padding up to full UserProfile size (608 bytes) */
} VirtualUserProfile;
#pragma pack(pop)

_Static_assert(sizeof(StdString) == 28, "StdString size must be 28");
_Static_assert(sizeof(MapNode) == 48, "MapNode size must be 48 (0x30)");
_Static_assert(__builtin_offsetof(MapNode, is_nil) == 0x2D, "MapNode is_nil offset must be 0x2D");
_Static_assert(__builtin_offsetof(VirtualUserProfile, mMapHead) == 0x40, "mMapHead offset must be 0x40");
_Static_assert(__builtin_offsetof(VirtualUserProfile, mMapSize) == 0x44, "mMapSize offset must be 0x44");

static PopCapColor g_glow_blue = { 0, 229, 255, 255 }; /* Bright glowing neon blue (0x00E5FF) */
static MapNode g_dev_map_nil;
static VirtualUserProfile g_DevTesterProfile;
static int g_profile_init = 0;
static int g_active = 0;

static void ztk_log(const char *fmt, ...)
{
    const char *log_name = "zumatest.log";
    if (GetFileAttributesA("zumatest.log") == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesA("zumalog.test") == INVALID_FILE_ATTRIBUTES) return;
    if (GetFileAttributesA("zumatest.log") != INVALID_FILE_ATTRIBUTES)
        log_name = "zumatest.log";
    else
        log_name = "zumalog.test";

    FILE *f = fopen(log_name, "a");
    if (!f) return;
    fprintf(f, "[%8lu][TestingKit] ", (unsigned long)GetTickCount());
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

static void ztk_check_active(void)
{
    g_active = (GetFileAttributesA("zumatest.log") != INVALID_FILE_ATTRIBUTES ||
                GetFileAttributesA("zumalog.test") != INVALID_FILE_ATTRIBUTES);
}

static void ztk_init_dev_profile(void)
{
    if (g_profile_init) return;

    memset(&g_dev_map_nil, 0, sizeof(g_dev_map_nil));
    g_dev_map_nil.left = &g_dev_map_nil;
    g_dev_map_nil.parent = &g_dev_map_nil;
    g_dev_map_nil.right = &g_dev_map_nil;
    g_dev_map_nil.color = 1; /* Black */
    g_dev_map_nil.is_nil = 1; /* Offset 0x2D is strictly 1 */

    memset(&g_DevTesterProfile, 0, sizeof(g_DevTesterProfile));
    g_DevTesterProfile.mName.allocator = NULL;
    strcpy(g_DevTesterProfile.mName.buf, "DevTester");
    g_DevTesterProfile.mName.length = strlen("DevTester");
    g_DevTesterProfile.mName.capacity = 15;

    g_DevTesterProfile.mId = 9999;
    g_DevTesterProfile.mTime = 0;
    g_DevTesterProfile.mSelectedStage = 0; /* Default Gauntlet map: 0 = "Random" */
    g_DevTesterProfile.mField28 = 0;       /* Default Gauntlet mode: 0 = Practice (not Survival) */
    g_DevTesterProfile.mField2c = 12;
    g_DevTesterProfile.mMaxStage = 12;
    g_DevTesterProfile.mHighScore = 999999;
    g_DevTesterProfile.mField38 = 1;
    g_DevTesterProfile.mField39 = 0;
    g_DevTesterProfile.mField3a = 0;

    g_DevTesterProfile.mMapHead = &g_dev_map_nil;
    g_DevTesterProfile.mMapSize = 0;

    g_profile_init = 1;
}

static int is_dev_tester(void *user_ptr)
{
    if (!g_active || !user_ptr) return 0;
    return (user_ptr == &g_DevTesterProfile);
}

__declspec(dllexport) int __cdecl ZTK_IsActive(void)
{
    ztk_check_active();
    return g_active;
}

typedef int (__thiscall *fn_AddLine)(void *this_ptr, const StdString *str, int bSort);
typedef void (__thiscall *fn_SelectLine)(void *this_ptr, int line);

static inline int ztk_is_valid_ptr(const void *p, size_t size)
{
    if (!p || (uintptr_t)p < 0x10000 || (uintptr_t)p >= 0x7FFE0000) return 0;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    return 1;
}

static int ztk_validate_user_dialog_button(void *btn, int *out_is_dev)
{
    if (!ztk_is_valid_ptr(btn, 0x110)) return 0;

    /* 1. Fast hot-path check: Button ID must be 0 (Rename) or 1 (Delete) */
    int btn_id = *(int*)((char*)btn + 0x58);
    if (btn_id != 0 && btn_id != 1) return 0;

    /* 2. Retrieve listener pointer from btn + 0xC0 */
    void *listener = *(void**)((char*)btn + 0xC0);
    if (!ztk_is_valid_ptr(listener, sizeof(void*))) return 0;

    /* 3. Compute dialog = listener - 0x58 */
    char *dialog = (char*)listener - 0x58;
    if (!ztk_is_valid_ptr(dialog, 0x130)) return 0;

    /* 4. Strongest & cheapest check: verify button is registered child of dialog */
    void *registered_btn = (btn_id == 0) ? *(void**)(dialog + 0x120) : *(void**)(dialog + 0x124);
    if (registered_btn != btn) return 0;

    /* 5. Check dialog ID: must be 7 (POPCAP_DIALOG_USER) */
    if (*(int*)(dialog + 0x100) != 7) return 0;

    /* 6. Check dialog visibility */
    if (*(char*)(dialog + 0x18) == 0) return 0;

    /* 7. Check ListBox and retrieve selected item name */
    void *listbox = *(void**)(dialog + 0x11C);
    if (!ztk_is_valid_ptr(listbox, 0xB0)) return 0;

    int sel_idx = *(int*)((char*)listbox + 0xA4);
    if (sel_idx < 0) return 0;

    /* Check if selected line in ListBox is "DevTester" */
    int is_dev = 0;
    StdString *entries = *(StdString**)((char*)listbox + 0x70);
    StdString *entries_end = *(StdString**)((char*)listbox + 0x74);
    if (entries && entries_end && ztk_is_valid_ptr(entries, sizeof(StdString))) {
        size_t count = (size_t)(entries_end - entries);
        if ((size_t)sel_idx < count) {
            StdString *item = &entries[sel_idx];
            const char *item_name = (item->capacity >= 16) ? *(const char**)item->buf : item->buf;
            if (item_name && ztk_is_valid_ptr(item_name, 9) && strcmp(item_name, "DevTester") == 0) {
                is_dev = 1;
            }
        }
    }
    if (!is_dev && sel_idx == 1) {
        is_dev = 1;
    }

    if (out_is_dev) *out_is_dev = is_dev;
    return 1;
}

__declspec(dllexport) void __cdecl ZTK_OnUserDialogInit(void *listbox, void *user_dialog)
{
    if (!ZTK_IsActive()) return;
    if (!listbox) return;

    ztk_log("OnUserDialogInit: user_dialog=%p, listbox=%p", user_dialog, listbox);

    ztk_init_dev_profile();

    StdString dev_str;
    memset(&dev_str, 0, sizeof(dev_str));
    strcpy(dev_str.buf, "DevTester");
    dev_str.length = strlen("DevTester");
    dev_str.capacity = 15;

    void **vtable = *(void***)listbox;
    fn_AddLine addLine = (fn_AddLine)vtable[0xBC / 4];

    /* Insert DevTester right after index 0 "(Create a New User)" -> becomes index 1 */
    int idx = addLine(listbox, &dev_str, 0);
    ztk_log("Inserted DevTester at ListBox index %d", idx);

    /* Check if current active user is DevTester */
    void ***app_ptr = (void***)0x0057EF00;
    if (app_ptr && *app_ptr) {
        void *cur_user = (*app_ptr)[0x614 / 4];
        if (is_dev_tester(cur_user)) {
            fn_SelectLine selectLine = (fn_SelectLine)vtable[0xE0 / 4];
            selectLine(listbox, idx);
            ztk_log("Selected DevTester line at index %d", idx);
        }
    }
}

__declspec(dllexport) int __cdecl ZTK_CheckCurUserSkip(void *curUser)
{
    if (!ZTK_IsActive()) return (curUser == NULL);
    if (!curUser || is_dev_tester(curUser)) return 1;
    return 0;
}

__declspec(dllexport) PopCapColor * __cdecl ZTK_GetItemColor(PopCapColor *in_color, void *listbox, int item_idx)
{
    if (!ZTK_IsActive() || !listbox) return in_color;
    /* Check if item at item_idx is DevTester */
    StdString *entries = *(StdString**)((char*)listbox + 0x70);
    StdString *entries_end = *(StdString**)((char*)listbox + 0x74);
    if (entries && entries_end && ztk_is_valid_ptr(entries, sizeof(StdString))) {
        size_t count = (size_t)(entries_end - entries);
        if ((size_t)item_idx < count) {
            StdString *item = &entries[item_idx];
            const char *item_name = (item->capacity >= 16) ? *(const char**)item->buf : item->buf;
            if (item_name && ztk_is_valid_ptr(item_name, 9) && strcmp(item_name, "DevTester") == 0) {
                return &g_glow_blue;
            }
        }
    }
    return in_color;
}

__declspec(dllexport) unsigned int __cdecl ZTK_GetWelcomeColor(void)
{
    if (!ZTK_IsActive()) return 0xFFFFFF;

    void ***app_ptr = (void***)0x0057EF00;
    if (app_ptr && *app_ptr) {
        void *cur_user = (*app_ptr)[0x614 / 4];
        if (is_dev_tester(cur_user)) {
            /* Keep mSelectedStage reset to 0 (Random map) and mode to Practice whenever on Main Menu */
            g_DevTesterProfile.mSelectedStage = 0;
            g_DevTesterProfile.mField28 = 0;
            return 0x00E5FF; /* Glowing cyan-blue */
        }
    }
    return 0xFFFFFF;
}

typedef void* (__thiscall *fn_DoDialog)(void *this_ptr, int theDialogId, int isModal, const void *theDialogTitle, const void *theDialogLines, const void *theDialogHeader, int theButtonMode);
typedef void (__thiscall *fn_StrAssign)(void *this_str, const char *s, size_t n);
typedef void (__cdecl *fn_Free)(void *p);

static void ztk_init_string(char *str_obj, const char *text)
{
    memset(str_obj, 0, 32);
    *(int*)(str_obj + 0x14) = 0;   /* len = 0 */
    *(int*)(str_obj + 0x18) = 15;  /* cap = 15 */
    fn_StrAssign strAssign = (fn_StrAssign)0x00408FF0;
    strAssign(str_obj, text, strlen(text));
}

static void ztk_free_string(char *str_obj)
{
    if (*(int*)(str_obj + 0x18) >= 16) {
        fn_Free free_fn = (fn_Free)0x004E11D0;
        free_fn(*(void**)(str_obj + 4));
    }
}

typedef void (__cdecl *fn_SetupDialog)(void *theDialog, int theWidth);
typedef void (__thiscall *fn_Resize)(void *this_ptr, int x, int y, int w, int h);

static void ztk_center_dialog(void *dlg, int width)
{
    if (!dlg) return;

    /* Zuma native SetupDialog at 0x0042AB90:
     * - Configures PopCap text wrapping, stone font styles, colors, and preferred height
     * - Repositions buttons
     */
    fn_SetupDialog setupDialog = (fn_SetupDialog)0x0042AB90;
    setupDialog(dlg, width);

    /* Widget size and coordinate offsets:
     * 0x08: mX, 0x0C: mY, 0x10: mWidth, 0x14: mHeight
     */
    int w = *(int*)((char*)dlg + 0x10);
    int h = *(int*)((char*)dlg + 0x14);

    int x = (640 - w) / 2;
    int y = (480 - h) / 2;
    if (y < 10) y = 10;

    /* Dialog::Resize at vtable[0x34 / 4] (0x0047AE00):
     * Updates widget bounds and centers child buttons relative to the dialog frame.
     */
    void **vtable = *(void***)dlg;
    fn_Resize resize = (fn_Resize)vtable[0x34 / 4];
    resize(dlg, x, y, w, h);
}

static void ztk_show_dev_access_dialog(void)
{
    void ***app_ptr = (void***)0x0057EF00;
    if (!app_ptr || !*app_ptr) return;

    void *app = *app_ptr;
    void **vtable = *(void***)app;
    fn_DoDialog doDialog = (fn_DoDialog)vtable[0xD4 / 4];

    char str_title[32];
    char str_lines[32];
    char str_btn[32];

    const char *title = "DEVELOPER ACCESS";
    const char *msg = "DevTester profile active. Special developer privileges enabled:\n\n- All Adventure Stages Unlocked (1-13)\n- All Gauntlet Temples Set to Sun God Rank\n- Volatile Session (Disk Writes Disabled)";

    ztk_init_string(str_title, title);
    ztk_init_string(str_lines, msg);
    ztk_init_string(str_btn, "OK"); /* theButton1Text */

    /* theButtonMode = 3 (BUTTONS_FOOTER / Single "OK" button matching PopCap dialog) */
    void *dlg = doDialog(app, 999, 1, str_title, str_lines, str_btn, 3);
    if (dlg) {
        ztk_center_dialog(dlg, 380);
    }

    ztk_free_string(str_title);
    ztk_free_string(str_lines);
    ztk_free_string(str_btn);

    ztk_log("Showing Developer Access dialog for DevTester selection (OK / Double-Click)");
}

typedef void* (__thiscall *fn_OrigGetProfile)(void *profileMgr, const StdString *name);

__declspec(dllexport) void * __cdecl ZTK_GetProfile(void *profileMgr, const StdString *name_str, fn_OrigGetProfile orig_fn)
{
    if (!name_str) return orig_fn(profileMgr, name_str);
    const char *name = (name_str->capacity >= 16) ? *(const char**)name_str->buf : name_str->buf;
    if (ZTK_IsActive() && name && ztk_is_valid_ptr(name, 9) && strcmp(name, "DevTester") == 0) {
        ztk_init_dev_profile();
        g_DevTesterProfile.mSelectedStage = 0; /* Default Gauntlet map: Random */
        g_DevTesterProfile.mField28 = 0;       /* Default Gauntlet mode: Practice */
        ztk_log("Virtual profile DevTester selected (%s)", name);
        ztk_show_dev_access_dialog();
        return &g_DevTesterProfile;
    }
    return orig_fn(profileMgr, name_str);
}

__declspec(dllexport) void __cdecl ZTK_SyncPractice(void *practiceScreen, char *is_unlocked)
{
    if (!ZTK_IsActive()) return;
    void ***app_ptr = (void***)0x0057EF00;
    if (!app_ptr || !*app_ptr) return;
    void *cur_user = (*app_ptr)[0x614 / 4];
    if (!is_dev_tester(cur_user)) return;

    /* Max rank Sun God (3) and unlock for all maps */
    *(int*)((char*)practiceScreen + 0x8C) = 3;
    *(int*)((char*)practiceScreen + 0x84) = 3;
    *(int*)((char*)practiceScreen + 0x88) = -1;
    if (is_unlocked) *is_unlocked = 1;
}

__declspec(dllexport) int __cdecl ZTK_CheckStageUnlock(void *stageNode)
{
    if (!ZTK_IsActive()) {
        if (!stageNode) return 0;
        return (*(int*)((char*)stageNode + 0x28) >= 0x15);
    }
    void ***app_ptr = (void***)0x0057EF00;
    if (app_ptr && *app_ptr) {
        void *cur_user = (*app_ptr)[0x614 / 4];
        if (is_dev_tester(cur_user)) return 1; /* All stages 1..12 unlocked */
    }
    if (!stageNode) return 0;
    return (*(int*)((char*)stageNode + 0x28) >= 0x15);
}

__declspec(dllexport) int __cdecl ZTK_SaveDetails(void *userProfile)
{
    if (!ZTK_IsActive()) return 0;
    if (is_dev_tester(userProfile)) {
        ztk_log("Prevented disk write for DevTester (userdata preserved)");
        return 1; /* Skip disk write */
    }
    return 0;
}

static void *g_ResMgr = NULL;
static void *g_ImageDialogButtonLocked = NULL;
static void *g_ImageAdvTemple5 = NULL;
static void *g_ImageAdvDoor5 = NULL;
static int g_Door5Hover = 0;
static int g_Door5Selected = 0;

static void *ztk_get_res_image(void *res_mgr, const char *str_name)
{
    if (!res_mgr) {
        void ***app_ptr = (void***)0x0057EF00;
        if (app_ptr && *app_ptr) {
            res_mgr = (*app_ptr)[0x5D8 / 4];
        }
    }
    if (!res_mgr || !str_name) return NULL;
    char str_obj[32];
    memset(str_obj, 0, sizeof(str_obj));
    *(int*)(str_obj + 0x14) = 0;   /* len = 0 */
    *(int*)(str_obj + 0x18) = 15;  /* capacity = 15 */

    typedef void (__thiscall *fn_StrAssign)(void *this_str, const char *s, size_t n);
    fn_StrAssign strAssign = (fn_StrAssign)0x00408FF0;
    strAssign(str_obj, str_name, strlen(str_name));

    /* FUN_00485350: ResourceManager::FindImage (safe, returns NULL if not found, does not throw) */
    typedef void* (__thiscall *fn_FindImage)(void *res_mgr, void *str_id);
    fn_FindImage findImage = (fn_FindImage)0x00485350;
    void *img = findImage(res_mgr, str_obj);

    if (*(int*)(str_obj + 0x18) >= 16) {
        typedef void (__cdecl *fn_Free)(void *p);
        fn_Free free_fn = (fn_Free)0x004E11D0;
        free_fn(*(void**)(str_obj + 4));
    }
    return img;
}

__declspec(dllexport) void __cdecl ZTK_OnRegisterResources(void *res_mgr, void *dialog_button_img)
{
    if (!ZTK_IsActive()) return;

    g_ResMgr = res_mgr;
    ztk_log("OnRegisterResources: res_mgr=%p, dialog_button=%p", res_mgr, dialog_button_img);

    g_ImageDialogButtonLocked = ztk_get_res_image(res_mgr, "IMAGE_DIALOG_BUTTON_LOCKED");
    if (g_ImageDialogButtonLocked) {
        ztk_log("Successfully resolved IMAGE_DIALOG_BUTTON_LOCKED at %p", g_ImageDialogButtonLocked);
    } else {
        ztk_log("Failed to resolve IMAGE_DIALOG_BUTTON_LOCKED from ResourceManager");
    }
}

__declspec(dllexport) void __cdecl ZTK_PreButtonDraw(void *btn)
{
    if (!ZTK_IsActive() || !btn) return;

    int is_dev = 0;
    int is_target = ztk_validate_user_dialog_button(btn, &is_dev);

    void **btn_img_ptr = (void**)((char*)btn + 0xE0); /* mButtonImage */
    void *vanilla_img = *(void**)0x0054802c;           /* IMAGE_DIALOG_BUTTON */
    PopCapColor *colors = *(PopCapColor**)((char*)btn + 0x2C); /* std::vector<PopCapColor> */

    if (is_target && is_dev) {
        /* Swap button image to locked image if available */
        if (g_ImageDialogButtonLocked) {
            *btn_img_ptr = g_ImageDialogButtonLocked;
        }
        /* Disabled label text color: dimmed gray */
        if (colors && ztk_is_valid_ptr(colors, sizeof(PopCapColor) * 2)) {
            colors[0].mRed = 128; colors[0].mGreen = 128; colors[0].mBlue = 128; colors[0].mAlpha = 255;
            colors[1].mRed = 128; colors[1].mGreen = 128; colors[1].mBlue = 128; colors[1].mAlpha = 255;
        }
        /* Never activate finger pulse overlay on Rename/Delete (prevents green-to-purple animation loop) */
        *(char*)((char*)btn + 0x10d) = 0;
    } else if (is_target) {
        /* Restore vanilla button image */
        if (vanilla_img) {
            *btn_img_ptr = vanilla_img;
        }
        /* Normal label text colors: gold and white */
        if (colors && ztk_is_valid_ptr(colors, sizeof(PopCapColor) * 2)) {
            colors[0].mRed = 0xD5; colors[0].mGreen = 0xE5; colors[0].mBlue = 0x20; colors[0].mAlpha = 255;
            colors[1].mRed = 0xFF; colors[1].mGreen = 0xFF; colors[1].mBlue = 0xFF; colors[1].mAlpha = 255;
        }
    }
}

__declspec(dllexport) int __cdecl ZTK_CheckUserDialogAction(const StdString *name_str, int btn_id)
{
    if (!ZTK_IsActive()) return 1;

    /* Check if currently targeted profile is DevTester */
    int is_virtual_dev = 0;
    if (name_str) {
        const char *name = (name_str->capacity >= 16) ? *(const char**)name_str->buf : name_str->buf;
        if (name && ztk_is_valid_ptr(name, 9) && strcmp(name, "DevTester") == 0) {
            is_virtual_dev = 1;
        }
    }

    if (is_virtual_dev) {
        const char *title = NULL;
        const char *msg = NULL;
        int action_result = 1;

        if (btn_id == 0) {
            /* RENAME button clicked */
            title = "WARNING";
            msg = "DevTester is a privileged developer testing profile with full game access and cannot be renamed.";
            action_result = 0; /* Action BLOCKED */
            ztk_log("Blocked Rename operation on virtual DevTester profile");
        } else if (btn_id == 1) {
            /* DELETE button clicked */
            title = "WARNING";
            msg = "DevTester is a privileged developer testing profile with full game access and cannot be deleted.";
            action_result = 0; /* Action BLOCKED */
            ztk_log("Blocked Delete operation on virtual DevTester profile");
        } else {
            /* OK (btn_id >= 2000), CANCEL (btn_id >= 3000), or other:
             * Do NOT show dialog here.
             * (OK and double-click are both unified in ZTK_GetProfile!)
             */
            return 1;
        }

        if (title && msg) {
            void ***app_ptr = (void***)0x0057EF00;
            if (app_ptr && *app_ptr) {
                void *app = *app_ptr;
                void **vtable = *(void***)app;
                fn_DoDialog doDialog = (fn_DoDialog)vtable[0xD4 / 4];

                char str_title[32];
                char str_lines[32];
                char str_btn[32];

                ztk_init_string(str_title, title);
                ztk_init_string(str_lines, msg);
                ztk_init_string(str_btn, "OK"); /* theButton1Text */

                /* theButtonMode = 3 (BUTTONS_FOOTER / Single "OK" button matching PopCap dialog) */
                void *dlg = doDialog(app, 999, 1, str_title, str_lines, str_btn, 3);
                if (dlg) {
                    ztk_center_dialog(dlg, 380);
                }

                ztk_free_string(str_title);
                ztk_free_string(str_lines);
                ztk_free_string(str_btn);
            }
        }

        return action_result;
    }

    return 1; /* Action ALLOWED for all real profiles */
}

static int is_current_user_dev_tester(void)
{
    if (!ZTK_IsActive()) return 0;
    void ***app_ptr = (void***)0x0057EF00;
    if (!app_ptr || !*app_ptr) return 0;
    void *cur_user = (*app_ptr)[0x614 / 4];
    return is_dev_tester(cur_user);
}

typedef void (__thiscall *fn_DrawImage)(void *g, void *img, int x, int y);
typedef void (__thiscall *fn_DrawImageCel)(void *g, void *img, int x, int y, int cel);
typedef struct {
    int mX;
    int mY;
    int mWidth;
    int mHeight;
} PopCapRect;
typedef void (__thiscall *fn_DrawImageRotatedF)(void *g, void *img, float x, float y, double rot, const PopCapRect *srcRect);
typedef void (__thiscall *fn_SetColorizeImages)(void *g, int colorize);
typedef void (__thiscall *fn_SetColor)(void *g, const PopCapColor *color);
typedef void (__thiscall *fn_SetFont)(void *g, void *font);
typedef void (__thiscall *fn_DrawString)(void *g, void *str, int x, int y);
typedef int (__thiscall *fn_StringWidth)(void *font, void *str);

static void *g_fn_DrawImage = NULL;
static void *g_fn_DrawImageCel = NULL;
static void *g_fn_DrawImageRotatedF = (void*)0x00450740;
static void *g_fn_SetFont = NULL;
static void *g_fn_SetColor = NULL;
static void *g_fn_DrawString = NULL;
static void **g_ppFontBrownTitle = NULL;
static void **g_ppFontDialog = NULL;
static void **g_ppFontPlain = NULL;
static void **g_ppFontCancun10 = NULL;
static void **g_ppFontCancun14 = NULL;
static void **g_ppImageHelpBack = NULL;
static void *g_ImageBallBlue = NULL;
static void *g_ImageBallYellow = NULL;
static void *g_ImageBallRed = NULL;
static void *g_ImageBallGreen = NULL;
static void *g_ImageBallPurple = NULL;
static void *g_ImageExplosion = NULL;
static int g_help_resolved = 0;

static void *ztk_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
    if (!haystack || !needle || needlelen == 0 || haystacklen < needlelen) return NULL;
    const unsigned char *h = (const unsigned char*)haystack;
    const unsigned char *n = (const unsigned char*)needle;
    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (h[i] == n[0] && memcmp(&h[i], n, needlelen) == 0) {
            return (void*)&h[i];
        }
    }
    return NULL;
}

static void **ztk_resolve_res_global_ptr(const unsigned char *text_ptr, size_t text_size,
                                         const unsigned char *rdata_ptr, size_t rdata_size,
                                         const char *res_id)
{
    if (!text_ptr || !rdata_ptr || !res_id) return NULL;
    size_t id_len = strlen(res_id) + 1;
    const unsigned char *s_hit = (const unsigned char*)ztk_memmem(rdata_ptr, rdata_size, res_id, id_len);
    if (!s_hit) return NULL;
    uint32_t s_va = (uint32_t)(uintptr_t)s_hit;

    unsigned char pat_push[5];
    pat_push[0] = 0x68;
    memcpy(&pat_push[1], &s_va, 4);

    const unsigned char *p_hit = (const unsigned char*)ztk_memmem(text_ptr, text_size, pat_push, sizeof(pat_push));
    if (!p_hit) return NULL;

    for (size_t i = 0; i < 50 && (p_hit + i + 5) <= (text_ptr + text_size); i++) {
        if (p_hit[i] == 0xA3) {
            uint32_t g_va = *(const uint32_t*)&p_hit[i + 1];
            return (void**)(uintptr_t)g_va;
        }
    }
    return NULL;
}

static void ztk_resolve_help_signatures(void)
{
    if (g_help_resolved) return;
    g_help_resolved = 1;

    unsigned char *base = (unsigned char*)GetModuleHandleA(NULL);
    if (!base) return;

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    unsigned char *text_ptr = NULL;
    size_t text_size = 0;
    unsigned char *rdata_ptr = NULL;
    size_t rdata_size = 0;

    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            text_ptr = base + sec[i].VirtualAddress;
            text_size = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
        } else if (memcmp(sec[i].Name, ".rdata", 6) == 0) {
            rdata_ptr = base + sec[i].VirtualAddress;
            rdata_size = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
        }
    }

    if (!text_ptr || text_size == 0 || !rdata_ptr || rdata_size == 0) return;

    /* 1. Global resource font & image pointers */
    g_ppFontBrownTitle = ztk_resolve_res_global_ptr(text_ptr, text_size, rdata_ptr, rdata_size, "FONT_BROWNTITLE");
    g_ppFontDialog     = ztk_resolve_res_global_ptr(text_ptr, text_size, rdata_ptr, rdata_size, "FONT_DIALOG");
    g_ppFontPlain      = ztk_resolve_res_global_ptr(text_ptr, text_size, rdata_ptr, rdata_size, "FONT_PLAIN");
    g_ppFontCancun10   = ztk_resolve_res_global_ptr(text_ptr, text_size, rdata_ptr, rdata_size, "FONT_MAIN10");
    g_ppFontCancun14   = ztk_resolve_res_global_ptr(text_ptr, text_size, rdata_ptr, rdata_size, "FONT_FLOAT");
    g_ppImageHelpBack  = ztk_resolve_res_global_ptr(text_ptr, text_size, rdata_ptr, rdata_size, "IMAGE_HELP_BACK");

    /* 2. Resolve DrawImage via IMAGE_HELP_BACK draw widget (0x00434443) which is never patched:
     *    A1 <g_ppImageHelpBack> 8B 4D 08 6A 33 6A 0B 50 E8 <rel32>
     */
    if (g_ppImageHelpBack) {
        uint32_t va_hb = (uint32_t)(uintptr_t)g_ppImageHelpBack;
        unsigned char pat_mov_hb[5];
        pat_mov_hb[0] = 0xA1;
        memcpy(&pat_mov_hb[1], &va_hb, 4);

        const unsigned char *hit_hb = (const unsigned char*)ztk_memmem(text_ptr, text_size, pat_mov_hb, sizeof(pat_mov_hb));
        if (hit_hb && (hit_hb + 18) <= (text_ptr + text_size)) {
            if (hit_hb[13] == 0xE8) {
                int rel_draw = *(const int*)(hit_hb + 14);
                g_fn_DrawImage = (void*)((hit_hb + 13) + 5 + rel_draw);
            }
        }
    }

    /* 3. SetFont, SetColor via "Don't show this screen at startup." string in .rdata */
    const char str_dont[] = "Don't show this screen at startup.";
    const unsigned char *s_dont = (const unsigned char*)ztk_memmem(rdata_ptr, rdata_size, str_dont, sizeof(str_dont));
    if (s_dont) {
        uint32_t va_dont = (uint32_t)(uintptr_t)s_dont;
        unsigned char pat_mov[5];
        pat_mov[0] = 0xB8;
        memcpy(&pat_mov[1], &va_dont, 4);

        const unsigned char *hit_dont = (const unsigned char*)ztk_memmem(text_ptr, text_size, pat_mov, sizeof(pat_mov));
        if (hit_dont && hit_dont >= (text_ptr + 30)) {
            if (hit_dont[-29] == 0xE8) {
                int rel_font = *(const int*)(hit_dont - 28);
                g_fn_SetFont = (void*)((hit_dont - 29) + 5 + rel_font);
            }
            if (hit_dont[-5] == 0xE8) {
                int rel_col = *(const int*)(hit_dont - 4);
                g_fn_SetColor = (void*)((hit_dont - 5) + 5 + rel_col);
            }
        }
    }

    /* 4. DrawString, DrawImageCel via "Right mouse button will swap the balls." string in .rdata */
    const char str_rmouse[] = "Right mouse button will swap the balls.";
    const unsigned char *s_rmouse = (const unsigned char*)ztk_memmem(rdata_ptr, rdata_size, str_rmouse, sizeof(str_rmouse));
    if (s_rmouse) {
        uint32_t va_rmouse = (uint32_t)(uintptr_t)s_rmouse;
        unsigned char pat_mov_r[5];
        pat_mov_r[0] = 0xB8;
        memcpy(&pat_mov_r[1], &va_rmouse, 4);

        const unsigned char *hit_rmouse = (const unsigned char*)ztk_memmem(text_ptr, text_size, pat_mov_r, sizeof(pat_mov_r));
        if (hit_rmouse && hit_rmouse >= (text_ptr + 95)) {
            if (hit_rmouse[-92] == 0xE8) {
                int rel_str = *(const int*)(hit_rmouse - 91);
                g_fn_DrawString = (void*)((hit_rmouse - 92) + 5 + rel_str);
            }
            if (hit_rmouse[-29] == 0xE8) {
                int rel_cel = *(const int*)(hit_rmouse - 28);
                g_fn_DrawImageCel = (void*)((hit_rmouse - 29) + 5 + rel_cel);
            }
        }
    }

    /* 5. DrawImageRotatedF (Graphics::DrawImageRotatedF at 0x00450740) */
    const unsigned char pat_rot[] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xF1, 0x8B, 0x4D, 0x1C };
    if (text_ptr && ((uintptr_t)0x00450740 >= (uintptr_t)text_ptr) &&
        ((uintptr_t)0x00450740 + sizeof(pat_rot) <= (uintptr_t)text_ptr + text_size)) {
        if (memcmp((void*)0x00450740, pat_rot, sizeof(pat_rot)) == 0) {
            g_fn_DrawImageRotatedF = (void*)0x00450740;
        }
    }

    ztk_log("Help Signatures Resolved (zero hardcoded addresses):");
    ztk_log("  DrawImage          = %p", g_fn_DrawImage);
    ztk_log("  DrawImageCel       = %p", g_fn_DrawImageCel);
    ztk_log("  DrawImageRotatedF  = %p", g_fn_DrawImageRotatedF);
    ztk_log("  SetFont            = %p", g_fn_SetFont);
    ztk_log("  SetColor           = %p", g_fn_SetColor);
    ztk_log("  DrawString         = %p", g_fn_DrawString);
    ztk_log("  FontBrownTitle     = %p (%p)", g_ppFontBrownTitle, g_ppFontBrownTitle ? *g_ppFontBrownTitle : NULL);
    ztk_log("  FontDialog         = %p (%p)", g_ppFontDialog, g_ppFontDialog ? *g_ppFontDialog : NULL);
    ztk_log("  FontPlain          = %p (%p)", g_ppFontPlain, g_ppFontPlain ? *g_ppFontPlain : NULL);
    ztk_log("  ImageHelpBack      = %p (%p)", g_ppImageHelpBack, g_ppImageHelpBack ? *g_ppImageHelpBack : NULL);
}

static void ztk_draw_text_centered(void *g, void *font, const char *text, int center_x, int y, const PopCapColor *color)
{
    if (!g || !font || !text) return;
    if (!g_fn_SetFont || !g_fn_SetColor || !g_fn_DrawString) {
        ztk_resolve_help_signatures();
    }
    if (!g_fn_SetFont || !g_fn_SetColor || !g_fn_DrawString) return;

    fn_SetFont setFont = (fn_SetFont)g_fn_SetFont;
    fn_SetColor setColor = (fn_SetColor)g_fn_SetColor;
    fn_DrawString drawStr = (fn_DrawString)g_fn_DrawString;
    void **font_vt = *(void***)font;
    fn_StringWidth strWidth = (fn_StringWidth)font_vt[0x1C / 4];

    char str_obj[32];
    ztk_init_string(str_obj, text);

    setFont(g, font);
    setColor(g, color);
    int w = strWidth(font, str_obj);
    drawStr(g, str_obj, center_x - (w / 2), y);

    ztk_free_string(str_obj);
}

static void ztk_draw_text_left(void *g, void *font, const char *text, int x, int y, const PopCapColor *color)
{
    if (!g || !font || !text) return;
    if (!g_fn_SetFont || !g_fn_SetColor || !g_fn_DrawString) {
        ztk_resolve_help_signatures();
    }
    if (!g_fn_SetFont || !g_fn_SetColor || !g_fn_DrawString) return;

    fn_SetFont setFont = (fn_SetFont)g_fn_SetFont;
    fn_SetColor setColor = (fn_SetColor)g_fn_SetColor;
    fn_DrawString drawStr = (fn_DrawString)g_fn_DrawString;

    char str_obj[32];
    ztk_init_string(str_obj, text);

    setFont(g, font);
    setColor(g, color);
    drawStr(g, str_obj, x, y);

    ztk_free_string(str_obj);
}

static void ztk_draw_text_sandy_glow(void *g, void *font, const char *text, int center_x, int y)
{
    if (!g || !font || !text) return;

    /* Stippled sandy grain shadow/glow offsets and colors:
     * Simulates rough sandstone carving with granular particles around the text
     */
    static const struct {
        int dx;
        int dy;
        int r, g_col, b, a;
    } s_sand_grains[] = {
        /* Distant sand dust */
        { -3, -1, 140,  95, 30, 140 },
        {  3, -1, 140,  95, 30, 140 },
        { -2,  2, 140,  95, 30, 140 },
        {  2,  2, 140,  95, 30, 140 },
        /* Mid-grain sandstone particles */
        { -2, -2, 185, 135, 45, 180 },
        {  2, -2, 185, 135, 45, 180 },
        { -3,  1, 185, 135, 45, 180 },
        {  3,  1, 185, 135, 45, 180 },
        {  0, -2, 210, 160, 60, 200 },
        {  0,  2, 120,  80, 25, 200 },
        /* Inner warm sand highlight */
        { -1, -1, 235, 195, 90, 220 },
        {  1, -1, 235, 195, 90, 220 },
        { -1,  1, 160, 110, 35, 220 },
        {  1,  1, 160, 110, 35, 220 },
    };

    for (size_t i = 0; i < sizeof(s_sand_grains)/sizeof(s_sand_grains[0]); i++) {
        PopCapColor grainCol = {
            s_sand_grains[i].r,
            s_sand_grains[i].g_col,
            s_sand_grains[i].b,
            s_sand_grains[i].a
        };
        ztk_draw_text_centered(g, font, text, center_x + s_sand_grains[i].dx, y + s_sand_grains[i].dy, &grainCol);
    }

    /* Solid sharp core text in vibrant luminous cyan */
    PopCapColor coreCol = { 0, 229, 255, 255 };
    ztk_draw_text_centered(g, font, text, center_x, y, &coreCol);
}

static inline int ztk_sx(int x) {
    void ***app_ptr = (void***)0x0057EF00;
    if (!app_ptr || !*app_ptr) return x;
    int app_w = *(int*)((char*)*app_ptr + 0x5C);
    int app_h = *(int*)((char*)*app_ptr + 0x60);
    float sw = (float)app_w / 640.0f;
    float sh = (float)app_h / 480.0f;
    float s = sw < sh ? sw : sh;
    int cw = (int)(640.0f * s + 0.5f);
    int xo = (app_w - cw) / 2;
    return (int)((float)x * s + 0.5f) + xo;
}

static inline int ztk_sy(int y) {
    void ***app_ptr = (void***)0x0057EF00;
    if (!app_ptr || !*app_ptr) return y;
    int app_w = *(int*)((char*)*app_ptr + 0x5C);
    int app_h = *(int*)((char*)*app_ptr + 0x60);
    float sw = (float)app_w / 640.0f;
    float sh = (float)app_h / 480.0f;
    float s = sw < sh ? sw : sh;
    int ch = (int)(480.0f * s + 0.5f);
    int yo = (app_h - ch) / 2;
    return (int)((float)y * s + 0.5f) + yo;
}

__declspec(dllexport) int __cdecl ZTK_CheckDoor5Hit(void *mapScreen, int mouseX, int mouseY, int vanilla_hit)
{
    (void)mapScreen;
    if (!is_current_user_dev_tester()) return vanilla_hit;

    if (vanilla_hit != -1) {
        g_Door5Hover = 0;
        return vanilla_hit;
    }

    /* advdoor5: center(x=323, y=201), frame size=64x50 -> x=[291, 355), y=[176, 226) */
    int d5_x1 = ztk_sx(291), d5_x2 = ztk_sx(355);
    int d5_y1 = ztk_sy(176), d5_y2 = ztk_sy(226);
    if (mouseX >= d5_x1 && mouseX < d5_x2 && mouseY >= d5_y1 && mouseY < d5_y2) {
        g_Door5Hover = 1;
        return 12; /* Hand cursor trigger in Zuma MapScreen */
    }

    g_Door5Hover = 0;
    return -1;
}

__declspec(dllexport) int __cdecl ZTK_CheckDoor5Click(void *mapScreen, int mouseX, int mouseY)
{
    if (!is_current_user_dev_tester()) return 0;

    /* advdoor5 bounding box: x in [291, 355), y in [176, 226) */
    int d5_x1 = ztk_sx(291), d5_x2 = ztk_sx(355);
    int d5_y1 = ztk_sy(176), d5_y2 = ztk_sy(226);
    if (mouseX >= d5_x1 && mouseX < d5_x2 && mouseY >= d5_y1 && mouseY < d5_y2) {
        if (!g_Door5Selected) {
            /* First click: select Door 5 */
            g_Door5Selected = 1;
            /* Deselect vanilla doors so MapScreen::Draw does not read local_48[4] and crash */
            *(int*)((char*)mapScreen + 0x1B8) = -1;

            /* MarkDirty */
            void **vtable = *(void***)mapScreen;
            typedef void (__thiscall *fn_MarkDirty)(void *this_ptr);
            fn_MarkDirty markDirty = (fn_MarkDirty)vtable[0xAC / 4];
            markDirty(mapScreen);
            ztk_log("Door 5 selected by DevTester (Stage 13)");
            return 1;
        } else {
            /* Second click: launch Stage 13-1 directly */
            ztk_log("Door 5 double-clicked: Launching Stage 13-1!");
            g_Door5Selected = 0;
            void ***app_ptr = (void***)0x0057EF00;
            if (app_ptr && *app_ptr) {
                void *app = *app_ptr;
                typedef void (__thiscall *fn_StartAdventure)(void *app, int stage);
                fn_StartAdventure startAdv = (fn_StartAdventure)0x00440990;
                startAdv(app, 12);
            }
            return 1;
        }
    } else {
        /* Clicked outside Door 5: reset Door 5 selection */
        g_Door5Selected = 0;
        return 0;
    }
}

__declspec(dllexport) void __cdecl ZTK_DrawTemple5(void *mapScreen, void *graphics)
{
    if (!is_current_user_dev_tester()) {
        g_Door5Selected = 0;
        g_Door5Hover = 0;
        return;
    }

    /* If entering Adventure mode with mMaxStage >= 12, vanilla set [mapScreen + 0x1B8] >= 12.
     * Auto-select Door 5 and keep [0x1B8] = -1 to avoid vanilla stack buffer overrun.
     */
    int cur_sel = *(int*)((char*)mapScreen + 0x1B8);
    if (cur_sel >= 12) {
        g_Door5Selected = 1;
        *(int*)((char*)mapScreen + 0x1B8) = -1;
        ztk_log("DevTester entered MapScreen: Auto-selected Door 5 (Stage 13)");
    } else if (cur_sel != -1) {
        /* Player explicitly clicked a vanilla door 0..11 */
        g_Door5Selected = 0;
    }

    /* Lazy resolve images if needed */
    if (!g_ImageAdvTemple5) {
        g_ImageAdvTemple5 = ztk_get_res_image(g_ResMgr, "IMAGE_ADVTEMPLE5");
        if (g_ImageAdvTemple5) {
            ztk_log("Resolved IMAGE_ADVTEMPLE5 at %p", g_ImageAdvTemple5);
        }
    }
    if (!g_ImageAdvDoor5) {
        g_ImageAdvDoor5 = ztk_get_res_image(g_ResMgr, "IMAGE_ADVDOOR5");
        if (g_ImageAdvDoor5) {
            ztk_log("Resolved IMAGE_ADVDOOR5 at %p", g_ImageAdvDoor5);
        }
    }

    if (!g_ImageAdvTemple5 || !g_ImageAdvDoor5) return;

    fn_DrawImage drawImage = (fn_DrawImage)0x0044D470;
    fn_DrawImageCel drawImageCel = (fn_DrawImageCel)0x00450840;
    fn_SetColorizeImages setColorize = (fn_SetColorizeImages)0x004478B0;
    fn_SetColor setColor = (fn_SetColor)0x00447850;

    /* 1. Render advtemple5 at center(321, 198), size(391, 100) -> top-left (126, 148) */
    drawImage(graphics, g_ImageAdvTemple5, ztk_sx(126), ztk_sy(148));

    /* 2. Render advdoor5 at center(323, 201), size(64, 50) -> top-left (291, 176) */
    int door_cel = 0; /* Frame 0: Open (Hitam) */
    if (g_Door5Selected) {
        door_cel = 2; /* Frame 2: Click (Merah) */
    } else if (g_Door5Hover) {
        door_cel = 1; /* Frame 1: Hover (Hijau) */
    }
    drawImageCel(graphics, g_ImageAdvDoor5, ztk_sx(291), ztk_sy(176), door_cel);

    /* 3. Re-apply 50% Fog (1/2 pengaruh kabut) via IMAGE_ADVMIDDLE overlay */
    void *advmiddle = *(void**)0x00547EB4;
    if (advmiddle) {
        setColorize(graphics, 1);
        PopCapColor fogColor = { 255, 255, 255, 128 }; /* 50% opacity */
        setColor(graphics, &fogColor);
        drawImage(graphics, advmiddle, ztk_sx(0), ztk_sy(72));
        setColorize(graphics, 0);
    }
}

__declspec(dllexport) void __cdecl ZTK_DrawStage13Banner(void *mapScreen, void *graphics)
{
    (void)mapScreen;
    if (!is_current_user_dev_tester() || !g_Door5Selected || !graphics) return;

    fn_DrawImage drawImage = (fn_DrawImage)0x0044D470;
    fn_SetFont setFont = (fn_SetFont)0x00447840;
    fn_SetColor setColor = (fn_SetColor)0x00447850;
    fn_DrawString drawStr = (fn_DrawString)0x00447DF0;

    void *stageBadge = *(void**)0x00548080;  /* IMAGE_ADVSTAGE */
    void *fontMain14 = *(void**)0x00547EF8;  /* FONT_MAIN14 */
    void *fontTitle32 = *(void**)0x00547F50; /* FONT_TITLE32 */
    void *fontBrowser = *(void**)0x00547FE0; /* FONT_BROWSER */

    PopCapColor colorWhite = { 255, 255, 255, 255 };
    PopCapColor colorGold  = { 255, 234, 1, 255 }; /* 0xFFEA01 matching vanilla description color */

    /* 1. Stage Badge: IMAGE_ADVSTAGE at (289, 382) -> (0x121, 0x17E) */
    if (stageBadge) {
        drawImage(graphics, stageBadge, ztk_sx(289), ztk_sy(382));
    }

    /* 2. Stage number "13" beside STAGE badge at (344, 396) -> (0x158, 0x18C) */
    if (fontMain14) {
        char str_num[32];
        ztk_init_string(str_num, "13");
        setFont(graphics, fontMain14);
        setColor(graphics, &colorWhite);
        drawStr(graphics, str_num, ztk_sx(344), ztk_sy(396));
        ztk_free_string(str_num);
    }

    /* 3. Temple Title: "THE VOID" centered at (326, 422) in white (matches vanilla Title style) */
    if (fontTitle32) {
        ztk_draw_text_centered(graphics, fontTitle32, "THE VOID", ztk_sx(326), ztk_sy(422), &colorWhite);
    }

    /* 4. Door Description: "THE PLACE OF NO ENDING" centered at (326, 456) in gold 0xFFEA01 */
    if (fontBrowser) {
        ztk_draw_text_centered(graphics, fontBrowser, "THE PLACE OF NO ENDING", ztk_sx(326), ztk_sy(456), &colorGold);
    }
}

__declspec(dllexport) int __cdecl ZTK_GetSelectedStage(void *mapScreen)
{
    if (ZTK_IsActive() && is_current_user_dev_tester() && g_Door5Selected) {
        ztk_log("Play button clicked with Door 5 selected: Launching Stage 13-1!");
        g_Door5Selected = 0;
        return 12; /* Stage 13-1 (space) */
    }
    if (!mapScreen) return -1;
    return *(int*)((char*)mapScreen + 0x1B8);
}

/* =========================================================================
 * DevVIP Cheat: Instant Score Full, Floating Text, and Instant DestroyAll
 * Trigger: Middle Mouse Button (VK_MBUTTON) during active gameplay
 * Zero hardcoded addresses: Dynamic AOB scanning on base module
 * ========================================================================= */
static void *g_fn_IncScore = NULL;
static void *g_fn_FT_Ctor = NULL;
static void *g_fn_FT_Add = NULL;
static void *g_fn_FT_Dtor = NULL;
static void *g_fn_free = NULL;
static void *g_fn_DestroyAll = NULL;
static int g_cheat_resolved = 0;

static void ztk_resolve_cheat_signatures(void)
{
    if (g_cheat_resolved) return;
    g_cheat_resolved = 1;

    unsigned char *base = (unsigned char*)GetModuleHandleA(NULL);
    if (!base) return;

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    unsigned char *text_ptr = NULL;
    size_t text_size = 0;

    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, ".text\0\0\0", 8) == 0 || memcmp(sec[i].Name, ".text", 5) == 0) {
            text_ptr = base + sec[i].VirtualAddress;
            text_size = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
            break;
        }
    }

    if (!text_ptr || text_size == 0) return;

    /* 1. IncScore: 55 8b ec 8b 45 08 85 c0 56 8b f1 0f 8e be 00 00 00 8b 8e b8 */
    static const unsigned char pat_inc[] = {
        0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x85, 0xc0,
        0x56, 0x8b, 0xf1, 0x0f, 0x8e, 0xbe, 0x00, 0x00, 0x00, 0x8b, 0x8e, 0xb8
    };
    g_fn_IncScore = ztk_memmem(text_ptr, text_size, pat_inc, sizeof(pat_inc));

    /* 2. FT_Ctor: 83 ec 2c 53 56 57 33 db bf 0f 00 00 00 8b f1 89 7d e0 89 5d dc (entry = match - 24) */
    static const unsigned char pat_ft_ctor[] = {
        0x83, 0xec, 0x2c, 0x53, 0x56, 0x57, 0x33, 0xdb, 0xbf, 0x0f, 0x00, 0x00, 0x00,
        0x8b, 0xf1, 0x89, 0x7d, 0xe0, 0x89, 0x5d, 0xdc
    };
    unsigned char *hit_ft_ctor = (unsigned char*)ztk_memmem(text_ptr, text_size, pat_ft_ctor, sizeof(pat_ft_ctor));
    if (hit_ft_ctor) {
        g_fn_FT_Ctor = (void*)(hit_ft_ctor - 24);
    }

    /* 3. FT_Add: 55 8b ec 83 ec 08 57 8b 79 04 85 ff 89 4d f8 0f 84 24 01 00 00 */
    static const unsigned char pat_ft_add[] = {
        0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x57, 0x8b, 0x79, 0x04,
        0x85, 0xff, 0x89, 0x4d, 0xf8, 0x0f, 0x84, 0x24, 0x01, 0x00, 0x00
    };
    g_fn_FT_Add = ztk_memmem(text_ptr, text_size, pat_ft_add, sizeof(pat_ft_add));

    /* 4. FT_Dtor: 55 8b ec 56 8b f1 e8 85 b3 07 00 f6 45 08 01 74 09 */
    static const unsigned char pat_ft_dtor[] = {
        0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0xe8, 0x85, 0xb3, 0x07, 0x00, 0xf6, 0x45, 0x08, 0x01, 0x74, 0x09
    };
    unsigned char *hit_ft_dtor = (unsigned char*)ztk_memmem(text_ptr, text_size, pat_ft_dtor, sizeof(pat_ft_dtor));
    if (hit_ft_dtor) {
        g_fn_FT_Dtor = (void*)hit_ft_dtor;
        /* Resolve CRT delete/free function called by FT_Dtor: look for \x56\xE8 in next 35 bytes */
        for (int j = 0; j < 35; j++) {
            if (hit_ft_dtor[j] == 0x56 && hit_ft_dtor[j+1] == 0xE8) {
                int rel32 = *(int*)&hit_ft_dtor[j+2];
                g_fn_free = (void*)((hit_ft_dtor + j + 1) + 5 + rel32);
                break;
            }
        }
    }

    /* 5. CurveMgr_DestroyAll: 53 8b d9 8b 43 30 85 c0 57 75 04 33 ff eb 02 8b 38 3b 7b 30 */
    static const unsigned char pat_destroy_all[] = {
        0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x30, 0x85, 0xc0,
        0x57, 0x75, 0x04, 0x33, 0xff, 0xeb, 0x02, 0x8b, 0x38, 0x3b, 0x7b, 0x30
    };
    g_fn_DestroyAll = ztk_memmem(text_ptr, text_size, pat_destroy_all, sizeof(pat_destroy_all));

    ztk_log("Cheat Signatures Resolved (zero hardcoded addresses):");
    ztk_log("  IncScore   = 0x%p", g_fn_IncScore);
    ztk_log("  FT_Ctor    = 0x%p", g_fn_FT_Ctor);
    ztk_log("  FT_Add     = 0x%p", g_fn_FT_Add);
    ztk_log("  FT_Dtor    = 0x%p", g_fn_FT_Dtor);
    ztk_log("  CRT_Free   = 0x%p", g_fn_free);
    ztk_log("  DestroyAll = 0x%p", g_fn_DestroyAll);
}

__declspec(dllexport) void __cdecl ZTK_UpdateDevCheat(void *board)
{
    if (!ZTK_IsActive() || !is_current_user_dev_tester() || !board) return;

    /* Pastikan hanya dieksekusi saat gameplay aktif:
     * [board + 0xC8] (mGameState) == 1 (Board::UpdatePlaying)
     * [board + 0xCC] (mPauseCount) == 0 (tidak sedang pause / dialog) */
    int game_state = *(int*)((char*)board + 0xC8);
    int pause_count = *(int*)((char*)board + 0xCC);
    if (game_state != 1 || pause_count != 0) return;

    /* Rising-edge debounce for VK_MBUTTON */
    static int s_mbutton_down = 0;
    int is_down = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    if (is_down && !s_mbutton_down) {
        s_mbutton_down = 1;
    } else if (!is_down) {
        s_mbutton_down = 0;
        return;
    } else {
        return; /* Still held down */
    }

    if (!g_cheat_resolved) {
        ztk_resolve_cheat_signatures();
    }

    int cur_score = *(int*)((char*)board + 0xB8);
    int target_score = *(int*)((char*)board + 0x134);
    int diff = target_score - cur_score;

    /* 1. Tambahkan selisih skor ke Board agar skor mencapai targetScore */
    if (diff > 0) {
        if (g_fn_IncScore) {
            typedef void (__thiscall *fn_IncScore)(void *b, int amt, char p2);
            ((fn_IncScore)g_fn_IncScore)(board, diff, 1);
        } else {
            *(int*)((char*)board + 0xB8) = target_score;
        }

        /* 2. Munculkan FloatingText "+<diff>" tepat di posisi pointer mouse */
        POINT pt;
        GetCursorPos(&pt);
        void *app = *(void**)((char*)board + 0xB4);
        HWND hwnd = NULL;
        if (app) {
            hwnd = *(HWND*)((char*)app + 0x2FC);
        }
        if (!hwnd) {
            hwnd = GetActiveWindow();
        }
        if (hwnd) {
            ScreenToClient(hwnd, &pt);
        }

        if (g_fn_FT_Ctor && g_fn_FT_Add && g_fn_FT_Dtor) {
            char text_buf[32];
            sprintf(text_buf, "+%d", diff);

            char str_obj[32];
            ztk_init_string(str_obj, text_buf);

            /* 16-byte std::vector for FloatingText */
            char vec_buf[16];
            memset(vec_buf, 0, sizeof(vec_buf));

            /* Font 0x0B (Combo Font), Neon Cyan-Blue DevTester (0x00E5FF) */
            typedef void (__thiscall *fn_FT_Ctor)(void *vec, void *s, int font_id, int color);
            ((fn_FT_Ctor)g_fn_FT_Ctor)(vec_buf, str_obj, 0x0B, 0x00E5FF);

            void *fx_mgr = *(void**)((char*)board + 0x7C);
            if (fx_mgr) {
                typedef void (__thiscall *fn_FT_Add)(void *vec, void *fx, int x, int y, int delay, int extra);
                ((fn_FT_Add)g_fn_FT_Add)(vec_buf, fx_mgr, pt.x, pt.y, 0, 0);
            }

            /* Destruct and free the temporary vector elements */
            void *first = *(void**)((char*)vec_buf + 4);
            void *last  = *(void**)((char*)vec_buf + 8);
            if (first) {
                for (char *p = (char*)first; p != (char*)last; p += 0x2C) {
                    typedef void (__thiscall *fn_FT_Dtor)(void *ft, int flags);
                    ((fn_FT_Dtor)g_fn_FT_Dtor)(p, 0);
                }
                if (g_fn_free) {
                    typedef void (__cdecl *fn_CRT_Free)(void *ptr);
                    ((fn_CRT_Free)g_fn_free)(first);
                } else {
                    free(first);
                }
            }

            ztk_free_string(str_obj);
        }
    }

    /* 3. Picu transisi native Zuma Bar-Full + DestroyAll pada tick FUN_00421a10 berikutnya:
     *    - JANGAN menyentuh [board + 0x15B] (mIsSurvival)!
     *    - JANGAN memanggil WinLevel secara paksa!
     *    - [board + 0x140] = 0x100: bar state ready
     *    - [board + 0x144] = 0x100: progress bar penuh
     *    - [board + 0x154] = 0: aktifkan flag DestroyAll pada Board
     *    - [board + 0x155] = 0: reset mHasZuma agar blok 0x00421AC7 dieksekusi di tick berikutnya,
     *      memanggil FUN_00411310 pada semua curve, memutar suara ZUMA! + DestroyAll,
     *      dan menyelesaikan stage otomatis begitu semua bola meledak */
    *(int*)((char*)board + 0x140)  = 0x100;
    *(int*)((char*)board + 0x144)  = 0x100;
    *(char*)((char*)board + 0x154) = 0;
    *(char*)((char*)board + 0x155) = 0;

    /* 4. Khusus mode Survival ([board + 0x15B] != 0):
     *    Di mode Survival vanilla, blok 0x00421B1F melompati panggilan CurveMgr::DestroyAll (0x00411310).
     *    Panggil CurveMgr::DestroyAll secara eksplisit pada setiap curve aktif agar bola di papan
     *    langsung meledak/hancur saat cheat ditekan, sama seperti di Adventure dan Practice! */
    if (*(char*)((char*)board + 0x15B) != 0 && g_fn_DestroyAll) {
        int num_curves = *(int*)((char*)board + 0x8C);
        for (int i = 0; i < num_curves; i++) {
            void *curve = *(void**)((char*)board + 0x5C + i * 4);
            if (curve) {
                typedef void (__thiscall *fn_DestroyAll_t)(void *c);
                ((fn_DestroyAll_t)g_fn_DestroyAll)(curve);
            }
        }
    }

    ztk_log("DevVIP Cheat triggered: added %d points (score: %d/%d), primed destroyall",
            diff, cur_score + (diff > 0 ? diff : 0), target_score);
}

__declspec(dllexport) int __cdecl ZTK_DrawHelpScreen(void *helpScreen, void *graphics, void *fnDrawImage)
{
    if (fnDrawImage) {
        g_fn_DrawImage = fnDrawImage;
    }

    if (!ZTK_IsActive() || !is_current_user_dev_tester() || !helpScreen || !graphics) {
        return 0; /* Vanilla fallback */
    }

    if (!g_help_resolved) {
        ztk_resolve_help_signatures();
    }
    if (fnDrawImage) {
        g_fn_DrawImage = fnDrawImage;
    }
    if (!g_fn_DrawImage || !g_fn_DrawImageCel || !g_fn_SetFont || !g_fn_SetColor || !g_fn_DrawString) {
        return 0;
    }

    /* 1. Sembunyikan keempat sibling widget tutorial vanilla (+0x60, +0x64, +0x68, +0x6C)
     *    dengan mengatur field mVisible (+0x19) = 0.
     *    Biarkan Checkbox (+0x70) dan tombol OK (+0x74) tetap aktif dan terlihat. */
    for (int off = 0x60; off <= 0x6C; off += 4) {
        void *w = *(void**)((char*)helpScreen + off);
        if (w) {
            *(char*)((char*)w + 0x19) = 0; /* mVisible = false */
        }
    }

    /* 2. Ambil pointer image dari ResourceManager */
    void *imgHelpBackDev  = ztk_get_res_image(g_ResMgr, "IMAGE_HELP_BACK_DEV");
    void *imgHelpFrontDev = ztk_get_res_image(g_ResMgr, "IMAGE_HELP_FRONT_DEV");
    void *imgTutMouseM    = ztk_get_res_image(g_ResMgr, "IMAGE_TUT_MOUSE_M");
    void *imgTutTemple5   = ztk_get_res_image(g_ResMgr, "IMAGE_TUT_TEMPLE5");

    /* Bola tutorial diambil langsung dari gambar bola baBall PopCap */
    void *imgBallRed    = ztk_get_res_image(g_ResMgr, "IMAGE_RED_BALL");
    void *imgBallYellow = ztk_get_res_image(g_ResMgr, "IMAGE_YELLOW_BALL");
    void *imgBallGreen  = ztk_get_res_image(g_ResMgr, "IMAGE_GREEN_BALL");
    void *imgBallBlue   = ztk_get_res_image(g_ResMgr, "IMAGE_BLUE_BALL");
    void *imgBallPurple = ztk_get_res_image(g_ResMgr, "IMAGE_PURPLE_BALL");

    if (!imgBallRed)    imgBallRed    = *(void**)0x00547FF8;
    if (!imgBallYellow) imgBallYellow = *(void**)0x00547ED8;
    if (!imgBallGreen)  imgBallGreen  = *(void**)0x00547E48;
    if (!imgBallBlue)   imgBallBlue   = *(void**)0x00547F08;
    if (!imgBallPurple) imgBallPurple = *(void**)0x00547FE4;
    if (!g_ImageExplosion) g_ImageExplosion  = ztk_get_res_image(g_ResMgr, "IMAGE_EXPLOSION");

    if (!imgHelpBackDev && g_ppImageHelpBack) {
        imgHelpBackDev = *g_ppImageHelpBack;
    }
    if (!imgHelpFrontDev) {
        return 0; /* Fallback jika aset belum termuat */
    }

    fn_DrawImage drawImage       = (fn_DrawImage)g_fn_DrawImage;
    fn_DrawImageCel drawImageCel = (fn_DrawImageCel)g_fn_DrawImageCel;

    /* 3. Render latar batu utuh (11, 51) dan bingkai emas ber-mask (0, 0) */
    if (imgHelpBackDev) {
        drawImage(graphics, imgHelpBackDev, 11, 51);
    }
    drawImage(graphics, imgHelpFrontDev, 0, 0);

    /* 4. Font PopCap Native:
     *    fontTitle: FONT_BROWNTITLE (NativeAlienExtended16) dengan Sandy Aztec Glow
     *    fontHeading: FONT_FLOAT (CancunFloat14) asli PopCap
     *    fontBody: FONT_MAIN10 (Cancun10) untuk teks penjelasan
     *    fontPlain: FONT_PLAIN (Arial12Bold) khusus untuk CLICK!
     */
    void *fontTitle   = g_ppFontBrownTitle ? *g_ppFontBrownTitle : NULL;
    void *fontHeading = (g_ppFontCancun14 && *g_ppFontCancun14) ? *g_ppFontCancun14 : (g_ppFontDialog ? *g_ppFontDialog : fontTitle);
    void *fontBody    = (g_ppFontCancun10 && *g_ppFontCancun10) ? *g_ppFontCancun10 : (g_ppFontPlain ? *g_ppFontPlain : fontHeading);
    void *fontPlain   = (g_ppFontPlain && *g_ppFontPlain) ? *g_ppFontPlain : fontBody;

    PopCapColor colCyan   = {   0, 229, 255, 255 }; /* 0x00E5FF - DevTester Neon Cyan */
    PopCapColor colGold   = { 255, 234,   1, 255 }; /* 0xFFEA01 - Aztec Gold */
    PopCapColor colYellow = { 255, 255,   0, 255 }; /* 0xFFFF00 - Highlight Yellow */
    PopCapColor colAmber  = { 255, 185,  45, 255 }; /* 0xFFB92D - Aztec Warm Amber */
    PopCapColor colWhite  = { 255, 255, 255, 255 }; /* 0xFFFFFF - Crisp White */

    /* =========================================================================
     * HEADER: "DEVELOPER MODE" seimbang di tengah antara batas window & bingkai (Y = 32)
     * Efek: Sandy Grain Glow (tekstur granular pasir Aztec)
     * ========================================================================= */
    if (fontTitle) {
        ztk_draw_text_sandy_glow(graphics, fontTitle, "DEVELOPER MODE", 320, 28);
    }
    if (fontBody) {
        ztk_draw_text_centered(graphics, fontBody,
            "Special Testing Profile  -  Unrestricted Game Access", 320, 78, &colGold);
    }

    /* =========================================================================
     * SECTION 1 (KIRI): ADVENTURE MODE (X: 45 .. 315)
     * ========================================================================= */
    if (fontHeading) {
        ztk_draw_text_centered(graphics, fontHeading, "ADVENTURE MODE", 162, 108, &colYellow);
    }

    /* Gambar preview Temple 5 dengan pintu hijau bersinar (190x75) */
    if (imgTutTemple5) {
        drawImage(graphics, imgTutTemple5, 69, 114);
    }

    if (fontBody) {
        ztk_draw_text_left(graphics, fontBody, "* All 13 Stages Unlocked From Start", 30, 206, &colWhite);
        ztk_draw_text_left(graphics, fontBody, "* Direct Temple 5 Access to Secret", 30, 224, &colWhite);
        ztk_draw_text_left(graphics, fontBody, "  Stage 13 (\"The Void\")", 30, 242, &colCyan);
    }

    /* =========================================================================
     * SECTION 2 (KANAN): GAUNTLET MODE (X: 325 .. 595)
     * ========================================================================= */
    if (fontHeading) {
        ztk_draw_text_centered(graphics, fontHeading, "GAUNTLET MODE", 450, 108, &colYellow);
    }

    if (fontBody) {
        /* Jarak judul ke kalimat pertama lebih jauh (Y = 132, gap 36px) */
        ztk_draw_text_left(graphics, fontBody, "* All Levels Unlocked in Gauntlet", 327, 132, &colWhite);

        /* Tiga baris berikutnya dibuat lebih berdekatan per barisnya (gap 19px) */
        ztk_draw_text_left(graphics, fontBody, "* Practice & Survival Modes Active", 327, 153, &colWhite);
        ztk_draw_text_left(graphics, fontBody, "* Full Sun God Rank Access Unlocked", 327, 172, &colGold);
        ztk_draw_text_left(graphics, fontBody, "* Jump to Any Difficulty Tier or Map", 327, 191, &colAmber);
    }

    /* =========================================================================
     * SECTION 3 (BAWAH): MIDDLE MOUSE BUTTON CHEAT
     * ========================================================================= */
    if (fontHeading) {
        ztk_draw_text_centered(graphics, fontHeading,
            "MIDDLE MOUSE BUTTON", 190, 282, &colCyan);
    }

    if (fontBody) {
        /* Penjelasan instruksi di sebelah kiri (X = 55 .. 360) */
        ztk_draw_text_left(graphics, fontBody,
            "Just play normally, or click Middle Mouse", 30, 304, &colWhite);
        ztk_draw_text_left(graphics, fontBody,
            "Button at any time to end it all and advance!", 30, 322, &colWhite);
        ztk_draw_text_left(graphics, fontBody,
            "* Instantly awards full target score", 30, 344, &colGold);
        ztk_draw_text_left(graphics, fontBody,
            "* Triggers Zuma DestroyAll & clears all balls", 30, 362, &colGold);
        ztk_draw_text_left(graphics, fontBody,
            "* Automatically completes the current stage", 30, 380, &colAmber);
    }

    /* =========================================================================
     * DEMO INTERAKTIF: 5 BOLA + ANIMASI KLIK TENGAH DESTROYALL
     * ========================================================================= */
    int update_cnt = *(int*)((char*)helpScreen + 0x4C);
    int cycle = update_cnt % 180;
    int is_clicking = (cycle >= 100 && cycle <= 145);

    /* 5 Bola Zuma berjejer rapat 32px */
    int ball_x[5] = { 430, 462, 494, 526, 558 };
    int ball_y = 295;

    /* Putaran bola ke arah kanan dengan tempo lambat */
    int ball_cel = (50 - ((update_cnt / 3) % 50)) % 50;

    void *ball_imgs[5] = {
        imgBallRed,
        imgBallYellow,
        imgBallGreen,
        imgBallBlue,
        imgBallPurple
    };

    if (cycle < 100) {
        /* FASE 1: 5 Bola berputar ke arah kanan dengan orientasi diputar 90 derajat CCW (-PI/2) */
        for (int i = 0; i < 5; i++) {
            if (ball_imgs[i]) {
                int rows = *(int*)((char*)ball_imgs[i] + 0xC);
                if (rows <= 0) rows = 50;
                int cel = (update_cnt / 3) % rows;
                if (g_fn_DrawImageRotatedF) {
                    PopCapRect rc;
                    rc.mX = 0;
                    rc.mY = cel * 32;
                    rc.mWidth = 32;
                    rc.mHeight = 32;
                    ((fn_DrawImageRotatedF)g_fn_DrawImageRotatedF)(graphics, ball_imgs[i],
                        (float)ball_x[i], (float)ball_y, -1.5707963267948966, &rc);
                } else {
                    drawImageCel(graphics, ball_imgs[i], ball_x[i], ball_y, cel);
                }
            }
        }
    } else if (cycle <= 145) {
        /* FASE 2: Middle Click terpicu -> Animasi ledakan DestroyAll dengan Additive Blending (mDrawMode=1) */
        int exp_frame = (cycle - 100) / 3; /* 0 .. 15 */
        if (exp_frame > 16) exp_frame = 16;

        if (g_ImageExplosion) {
            int old_draw_mode = *(int*)((char*)graphics + 0x44);
            *(int*)((char*)graphics + 0x44) = 1; /* PopCap Additive Blending: hilangkan kotak hitam */
            for (int i = 0; i < 5; i++) {
                drawImageCel(graphics, g_ImageExplosion, ball_x[i] - 26, ball_y - 27, exp_frame);
            }
            *(int*)((char*)graphics + 0x44) = old_draw_mode; /* Kembalikan ke mode normal */
        }

        /* Skor floating bonus (+100) melayang naik ke atas khas DestroyAll Zuma */
        int float_offset = (cycle - 100) / 2;
        if (fontBody) {
            for (int i = 0; i < 5; i++) {
                ztk_draw_text_centered(graphics, fontBody, "+100", ball_x[i] + 16, ball_y - 6 - float_offset, &colGold);
            }
        }
    } else {
        /* FASE 3: Bola telah hancur sepenuhnya (Clear!) */
        if (fontBody) {
            ztk_draw_text_centered(graphics, fontBody, "STAGE CLEARED!", 495, ball_y + 8, &colGold);
        }
    }

    /* Ikon Animasi Mouse terpusat di bawah 5 bola (Center X = 495) */
    if (imgTutMouseM) {
        int cel = is_clicking ? 1 : 0;
        int mouse_x = 540;
        int mouse_y = 350;

        /* Label status animasi di atas mouse: Arial CLICK! saat tombol tengah ditekan */
        if (is_clicking && fontPlain) {
            ztk_draw_text_centered(graphics, fontPlain, "CLICK!", mouse_x - 48, mouse_y + 12, &colWhite);
        }

        /* Sprite tut_mouse_m (40x29) */
        drawImageCel(graphics, imgTutMouseM, mouse_x, mouse_y, cel);
    }

    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        ztk_check_active();
        ztk_init_dev_profile();
        ztk_resolve_cheat_signatures();
        ztk_resolve_help_signatures();
        ztk_log("zumatestingkit.dll loaded (Active: %d)", g_active);
    }
    return TRUE;
}
