#include <stdarg.h>

#include "nc_application.h"
#include "nc_application_eval.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_config.h"
#include "nc_console.h"
#include "nc_content.h"
#include "nc_defines.h"
#include "nc_draw.h"
#include "nc_eval.h"
#include "nc_file.h"
#include "nc_file_stream.h"
#include "nc_log.h"
#include "nc_math.h"
#include "nc_metadesk.h"
#include "nc_profile.h"
#include "nc_render.h"
#include "nc_string.h"
#include "nc_system.h"
#include "nc_thread.h"
#include "nc_time.h"
#include "nc_tls.h"
#include "nc_ui.h"
#include "nc_ui_components.h"

AppViewState              __EMPTY_APP_VIEW_STATE_VALUE;
AppViewState* const       EMPTY_APP_VIEW_STATE_VALUE            = &__EMPTY_APP_VIEW_STATE_VALUE;
AppCommandKindInfo        __EMPTY_APP_COMMAND_KIND_INFO_VALUE;
AppCommandKindInfo* const EMPTY_APP_COMMAND_KIND_INFO_VALUE     = &__EMPTY_APP_COMMAND_KIND_INFO_VALUE;
AppVocabInfo              __EMPTY_APP_VOCAB_INFO_VALUE;
AppVocabInfo* const       EMPTY_APP_VOCAB_INFO_VALUE            = &__EMPTY_APP_VOCAB_INFO_VALUE;
AppViewUIRule             __EMPTY_APP_VIEW_UI_RULE_VALUE;
AppViewUIRule* const      EMPTY_APP_VIEW_UI_RULE_VALUE          = &__EMPTY_APP_VIEW_UI_RULE_VALUE;
AppState*                 APP_STATE                             = NULL;
ConfigID                  APP_LAST_DRAG_DROP_PANEL              = 0;
ConfigID                  APP_LAST_DRAG_DROP_PREV_TAB           = 0;

EXTERN_C_LINK_BEGIN
AppCommandKindInfo APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_COUNT] = {
    {},
    {
        Str8Lit("exit"),
        Str8Lit("Exits the application."),
        Str8Lit("quit,close,end,abort"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("open_palette"), 
        Str8Lit("Opens the palette."), 
        Str8Lit("help,cmd,lister,commands"), 
        Str8Lit(""), 
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("run_command"), 
        Str8Lit("Runs a command from the command palette."), 
        Str8Lit("help,cmd"), 
        Str8Lit(""), 
        0, 
        {
            APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED, 
            APP_REGISTERS_SLOT_COMMAND_NAME, 
            Str8Lit("query:commands")
        }
    },
    {
        Str8Lit("os_event")
    },
    {
        Str8Lit("popup_accept"),
        Str8Lit("Accepts the active popup prompt.")
    },
    {
        Str8Lit("popup_cancel"),
        Str8Lit("Cancels the active popup prompt.")
    },
    {
        Str8Lit("reset_to_default_bindings"),
        Str8Lit("Resets all keybindings to their defaults."),
        Str8Lit(""),
        Str8Lit(""),
        0
    },
    {
        Str8Lit("new_panel_left"),
        Str8Lit("Creates a new panel to the left of the active panel."),
        Str8Lit("panel"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("new_panel_up"),
        Str8Lit("Creates a new panel at the top of the active panel."),
        Str8Lit("panel"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("new_panel_right"),
        Str8Lit("Creates a new panel to the right of the active panel."),
        Str8Lit("panel"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("new_panel_down"),
        Str8Lit("Creates a new panel at the bottom of the active panel."),
        Str8Lit("panel"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("split_panel"),
        Str8Lit("Creates a new panel in a given direction, and moves a tab to it, if specified.")
    },
    {
        Str8Lit("next_panel"),
        Str8Lit("Cycles the active panel forward."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("prev_panel"),
        Str8Lit("Cycles the active panel backwards."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("focus_panel"),
        Str8Lit("Focuses a new panel.")
    },
    {
        Str8Lit("focus_panel_right"),
        Str8Lit("Focuses a panel rightward of the currently focused panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("focus_panel_left"),
        Str8Lit("Focuses a panel leftward of the currently focused panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("focus_panel_up"),
        Str8Lit("Focuses a panel upward of the currently focused panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("focus_panel_down"),
        Str8Lit("Focuses a panel downward of the currently focused panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("undo"),
        Str8Lit("Undoes the previous action.")
    },
    {
        Str8Lit("redo"),
        Str8Lit("Redoes the first previously undone action.")
    },
    {
        Str8Lit("go_back"),
        Str8Lit("Returns to the previously selected panel and tab in recorded history.")
    },
    {
        Str8Lit("go_forward"),
        Str8Lit("Returns to the next selected panel and tab in recorded history.")
    },
    {
        Str8Lit("close_panel"),
        Str8Lit("Closes the currently active panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("focus_tab"),
        Str8Lit("Focuses the passed tab within its containing panel.")
    },
    {
        Str8Lit("next_tab"),
        Str8Lit("Focuses the next tab on the active panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("prev_tab"),
        Str8Lit("Focuses the previous tab on the active panel."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_tab_right"),
        Str8Lit("Moves the selected tab right one slot."),
        Str8Lit(""),
        Str8Lit("$tab,"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_tab_left"),
        Str8Lit("Moves the selected tab left one slot."),
        Str8Lit(""),
        Str8Lit("$tab,"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("open_tab"),
        Str8Lit("Opens a new tab."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_COMMAND_NAME,
            Str8Lit("query:tab_commands"),
            Str8Lit("commands")
        }
    },
    {
        Str8Lit("build_tab"),
        Str8Lit("Opens a new tab with the parameterized view specification.")
    },
    {
        Str8Lit("duplicate_tab"),
        Str8Lit("Duplicates a tab."),
        Str8Lit(""),
        Str8Lit("$tab,"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("copy_tab_full_path"),
        Str8Lit("Copies the full path of the file being viewed by this tab."),
        Str8Lit(""),
        Str8Lit("$tab,")
    },
    {
        Str8Lit("close_tab"),
        Str8Lit("Closes the currently opened tab."),
        Str8Lit(""),
        Str8Lit("$tab,"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_view"),
        Str8Lit("Moves a view to a new panel.")
    },
    {
        Str8Lit("set_tab_view"),
        Str8Lit("Changes the view that the tab is currently showing.")
    },
    {
        Str8Lit("change_file"),
        Str8Lit("Changes the tlog file that the tab is showing."),
        Str8Lit("file,tlog,load"),
        Str8Lit("$tab,"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI | APP_COMMAND_KIND_FLAG_LIST_IN_TAB,
        {
            (
                APP_QUERY_KIND_ALLOW_FILES | 
                APP_QUERY_KIND_FLOATING | 
                APP_QUERY_KIND_REQUIRED
            ),
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("query:recent_files, query:flv_files")
        }
    },
    {
        Str8Lit("set_current_path"),
        Str8Lit("Sets the folder that the file list starts in."),
        Str8Lit("")
    },
    {
        Str8Lit("open"),
        Str8Lit("Opens a tlog file, or all tlog files in a folder."),
        Str8Lit("file,folder,tlog,load"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            (
                APP_QUERY_KIND_ALLOW_FILES | 
                APP_QUERY_KIND_ALLOW_FOLDERS | 
                APP_QUERY_KIND_FLOATING | 
                APP_QUERY_KIND_REQUIRED
            ),
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("switch"),
        Str8Lit("Switches to a recent file."),
        Str8Lit("flv,view,file"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_CONFIG,
            Str8Lit("query:recent_files, query:flv_files")
        }
    },
    {
        Str8Lit("show_file_in_explorer"),
        Str8Lit("Opens the operating system's file explorer and shows the selected file."),
        Str8Lit(""),
        Str8Lit("$file"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("new_session"),
        Str8Lit("Creates a new session file, and sets the current session path as that file's path."),
        Str8Lit("new,session,layout"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_ALLOW_FILES | APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("open_session"),
        Str8Lit("Opens a session file path, immediately loading it, and begins autosaving to it."),
        Str8Lit("session"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_ALLOW_FILES | APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("open_recent_session"),
        Str8Lit("Opens a recently used session file."),
        Str8Lit("session"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_CONFIG,
            Str8Lit("query:recent_sessions")
        }
    },
    {
        Str8Lit("save_session"),
        Str8Lit("Saves session data to a file, and sets the current session path as that path."),
        Str8Lit("session"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_ALLOW_FILES | APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("write_session_data"),
        Str8Lit("Writes session data to the active session file.")
    },
    {
        Str8Lit("session_settings"),
        Str8Lit("Opens session settings."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("edit"),
        Str8Lit("Edits the current selection."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("accept"),
        Str8Lit("Accepts current changes, or answers prompts in the affirmative."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("cancel"),
        Str8Lit("Rejects current changes, exits temporary menus, or answers prompts in the negative."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_left"),
        Str8Lit("Moves the cursor or selection left."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_right"),
        Str8Lit("Moves the cursor or selection right."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up"),
        Str8Lit("Moves the cursor or selection up."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down"),
        Str8Lit("Moves the cursor or selection down."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_left_select"),
        Str8Lit("Moves the cursor or selection left, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_right_select"),
        Str8Lit("Moves the cursor or selection right, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_select"),
        Str8Lit("Moves the cursor or selection up, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_select"),
        Str8Lit("Moves the cursor or selection down, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_left_chunk"),
        Str8Lit("Moves the cursor or selection left one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_right_chunk"),
        Str8Lit("Moves the cursor or selection right one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_chunk"),
        Str8Lit("Moves the cursor or selection up one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_chunk"),
        Str8Lit("Moves the cursor or selection down one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_left_chunk_select"),
        Str8Lit("Moves the cursor or selection left one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_right_chunk_select"),
        Str8Lit("Moves the cursor or selection right one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_chunk_select"),
        Str8Lit("Moves the cursor or selection up one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_chunk_select"),
        Str8Lit("Moves the cursor or selection down one chunk."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_page"),
        Str8Lit("Moves the cursor or selection up one page."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_page"),
        Str8Lit("Moves the cursor or selection down one page."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_whole"),
        Str8Lit("Moves the cursor or selection to the beginning of the relevant content."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_whole"),
        Str8Lit("Moves the cursor or selection to the end of the relevant content."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_page_select"),
        Str8Lit("Moves the cursor or selection up one page, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_page_select"),
        Str8Lit("Moves the cursor or selection down one page, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_up_whole_select"),
        Str8Lit("Moves the cursor or selection to the beginning of the relevant content, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_down_whole_select"),
        Str8Lit("Moves the cursor or selection to the end of the relevant content, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_home"),
        Str8Lit("Moves the cursor to the beginning of the line."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_end"),
        Str8Lit("Moves the cursor to the end of the line."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_home_select"),
        Str8Lit("Moves the cursor to the beginning of the line, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_end_select"),
        Str8Lit("Moves the cursor to the end of the line, while selecting."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("select_all"),
        Str8Lit("Selects everything possible."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("delete_single"),
        Str8Lit("Deletes a single element to the right of the cursor, or the active selection."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("delete_chunk"),
        Str8Lit("Deletes a chunk to the right of the cursor, or the active selection."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("backspace_single"),
        Str8Lit("Deletes a single element to the left of the cursor, or the active selection."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("backspace_chunk"),
        Str8Lit("Deletes a chunk to the left of the cursor, or the active selection."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("copy"),
        Str8Lit("Copies the active selection to the clipboard."),
        Str8Lit(""),
        Str8Lit("$text_rng,"),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("cut"),
        Str8Lit("Copies the active selection to the clipboard, then deletes it."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("paste"),
        Str8Lit("Pastes the current contents of the clipboard."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("insert_text"),
        Str8Lit("Inserts the text that was used to cause this command.")
    },
    {
        Str8Lit("move_next"),
        Str8Lit("Moves the cursor or selection to the next element."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("move_prev"),
        Str8Lit("Moves the cursor or selection to the previous element."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("centre_cursor"),
        Str8Lit("Snaps the current code view to centre the cursor."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("contain_cursor"),
        Str8Lit("Snaps the current code view to contain the cursor."),
        Str8Lit(""),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("goto_time"),
        Str8Lit("Goes to the first row at or after a time. The time is in seconds from the start of the flight."),
        Str8Lit("jump,seek,timestamp"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI | APP_COMMAND_KIND_FLAG_LIST_IN_FILE,
        {
            APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_STRING,
            Str8Lit("")
        }
    },
    {
        Str8Lit("search"),
        Str8Lit("Finds the next row that contains the text."),
        Str8Lit("find"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            (
                APP_QUERY_KIND_REQUIRED | 
                APP_QUERY_KIND_KEEP_OLD_INPUT | 
                APP_QUERY_KIND_SELECT_OLD_INPUT
            ),
            APP_REGISTERS_SLOT_STRING,
            Str8Lit("")
        }
    },
    {
        Str8Lit("search_backwards"),
        Str8Lit("Finds the previous row that contains the text."),
        Str8Lit("find"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            (
                APP_QUERY_KIND_REQUIRED | 
                APP_QUERY_KIND_KEEP_OLD_INPUT | 
                APP_QUERY_KIND_SELECT_OLD_INPUT
            ),
            APP_REGISTERS_SLOT_STRING,
            Str8Lit("")
        }
    },
    {
        Str8Lit("find_next"),
        Str8Lit("Searches the current code file forward (from the cursor) for the last searched string."),
        Str8Lit("search"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_KEEP_OLD_INPUT,
            APP_REGISTERS_SLOT_NULL
        }
    },
    {
        Str8Lit("find_prev"),
        Str8Lit("Searches the current code file backwards (from the cursor) for the last searched string."),
        Str8Lit("search"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_KEEP_OLD_INPUT,
            APP_REGISTERS_SLOT_NULL
        }
    },
    {
        Str8Lit("enable_cfg"),
        Str8Lit("Enables a config tree.")
    },
    {
        Str8Lit("disable_cfg"),
        Str8Lit("Disables a config tree.")
    },
    {
        Str8Lit("select_cfg"),
        Str8Lit("Selects a config tree, disabling all others of the same kind.")
    },
    {
        Str8Lit("deselect_cfg"),
        Str8Lit("Deselects a config tree.")
    },
    {
        Str8Lit("remove_cfg"),
        Str8Lit("Removes a config tree.")
    },
    {
        Str8Lit("name_cfg"),
        Str8Lit("Equips a config tree with a label.")
    },
    {
        Str8Lit("duplicate_cfg"),
        Str8Lit("Duplicates a config tree.")
    },
    {
        Str8Lit("save_cfg_to_session"),
        Str8Lit("Saves the config tree to the session.")
    },
    {
        Str8Lit("edit_session_theme"),
        Str8Lit("Edits the current session theme."),
        Str8Lit("colour"),
        Str8Lit(""),
        0,
        {
            0,
            APP_REGISTERS_SLOT_STRING,
            Str8Lit("query:themes")
        }
    },
    {
        Str8Lit("add_theme_colour"),
        Str8Lit("Adds a new theme colour")
    },
    {
        Str8Lit("fork_theme"),
        Str8Lit("Imports all colours from the current theme so they can be individually edited")
    },
    {
        Str8Lit("save_theme"),
        Str8Lit("Saves all theme colours to a new theme file."),
        Str8Lit(""),
        Str8Lit(""),
        0,
        {
            APP_QUERY_KIND_ALLOW_FILES | APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_STRING
        }
    },
    {
        Str8Lit("save_and_set_theme"),
        Str8Lit("Saves all theme colours to a new theme file, and then sets that theme as the selected theme."),
        Str8Lit(""),
        Str8Lit(""),
        0,
        {
            (
                APP_QUERY_KIND_ALLOW_FILES | 
                APP_QUERY_KIND_FLOATING | 
                APP_QUERY_KIND_REQUIRED
            ),
            APP_REGISTERS_SLOT_STRING
        }
    },
    {
        Str8Lit("pick_file"),
        Str8Lit("Opens the file browser to pick a file."),
        Str8Lit(""),
        Str8Lit(""),
        0,
        {
            (
                APP_QUERY_KIND_ALLOW_FILES | 
                APP_QUERY_KIND_FLOATING | 
                APP_QUERY_KIND_REQUIRED
            ),
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("pick_folder"),
        Str8Lit("Opens the file browser to pick a folder."),
        Str8Lit(""),
        Str8Lit(""),
        0,
        {
            (
                APP_QUERY_KIND_ALLOW_FOLDERS | 
                APP_QUERY_KIND_FLOATING | 
                APP_QUERY_KIND_REQUIRED
            ),
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("pick_file_or_folder"),
        Str8Lit("Opens the file browser to pick a file or folder."),
        Str8Lit(""),
        Str8Lit(""),
        0,
        {
            (
                APP_QUERY_KIND_ALLOW_FILES | 
                APP_QUERY_KIND_ALLOW_FOLDERS | 
                APP_QUERY_KIND_FLOATING | 
                APP_QUERY_KIND_REQUIRED
            ),
            APP_REGISTERS_SLOT_FILE_PATH,
            Str8Lit("folder:\"$input\"")
        }
    },
    {
        Str8Lit("filter"),
        Str8Lit("Shows only the rows that agree with the filter."),
        Str8Lit("fuzzy,find,rows"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_REQUIRED | APP_QUERY_KIND_KEEP_OLD_INPUT,
            APP_REGISTERS_SLOT_STRING,
            Str8Lit("")
        }
    },
    {
        Str8Lit("set_theme"),
        Str8Lit("Changes the colours of the program."),
        Str8Lit("colour,color,style"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI,
        {
            APP_QUERY_KIND_FLOATING | APP_QUERY_KIND_REQUIRED,
            APP_REGISTERS_SLOT_STRING,
            Str8Lit("themes")
        }
    },
    {
        Str8Lit("toggle_dev_menu"),
        Str8Lit("Opens and closes the developer menu."),
        Str8Lit("debug"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("record_session_as_last_opened"),
        Str8Lit("Records a file path as the last opened session.")
    },
    {
        Str8Lit("record_recent_session"),
        Str8Lit("Records a file path as a recent session.")
    },
    {
        Str8Lit("query"),
        Str8Lit("Submits the active query")
    },
    {
        Str8Lit("push_query"),
        Str8Lit("Opens a new temporary query interface")
    },
    {
        Str8Lit("record_file_in_session"),
        Str8Lit("Records the passed file path as a recent file in the currently loaded session")
    },
    {
        Str8Lit("complete_query"),
        Str8Lit("Completes and closes a query")
    },
    {
        Str8Lit("cancel_query"),
        Str8Lit("Cancels the active query")
    },
    {
        Str8Lit("update_query"),
        Str8Lit("Updates the query input buffer")
    },
    {
        Str8Lit("files"),
        Str8Lit("Opens a tab that shows the list of flights."),
        Str8Lit("logs,bin,fli,flights"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("views"),
        Str8Lit("Opens a tab that shows the list of View files."),
        Str8Lit("flv,analysis"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    },
    {
        Str8Lit("flv"),
        Str8Lit("Opens a tab that allows the viewing and editting of an FLV file."),
        Str8Lit("flv,edit"),
        Str8Lit(""),
        APP_COMMAND_KIND_FLAG_LIST_IN_UI
    }
};

BindingTableEntry APP_DEFAULT_BINDING_TABLE[81] = {
    {
        Str8Lit("exit"),
        {
            INPUT_KIND_Q,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("next_panel"),
        {
            INPUT_KIND_COMMA,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("prev_panel"),
        {
            INPUT_KIND_COMMA,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("focus_panel_right"),
        {
            INPUT_KIND_RIGHT,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("focus_panel_left"),
        {
            INPUT_KIND_LEFT,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("focus_panel_up"),
        {
            INPUT_KIND_UP,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("focus_panel_down"),
        {
            INPUT_KIND_DOWN,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("go_back"),
        {
            INPUT_KIND_LEFT,
            INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("go_forward"),
        {
            INPUT_KIND_RIGHT,
            INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("next_tab"),
        {
            INPUT_KIND_PAGEDOWN,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("prev_tab"),
        {
            INPUT_KIND_PAGEUP,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_tab_right"),
        {
            INPUT_KIND_PAGEDOWN,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_tab_left"),
        {
            INPUT_KIND_PAGEUP,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("close_tab"),
        {
            INPUT_KIND_W,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("new_panel_right"),
        {
            INPUT_KIND_P,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("new_panel_down"),
        {
            INPUT_KIND_MINUS,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("close_panel"),
        {
            INPUT_KIND_P,
            (
                INPUT_MOD_KIND_CTRL |
                INPUT_MOD_KIND_SHIFT |
                INPUT_MOD_KIND_ALT
            )
        }
    },
    {
        Str8Lit("open_tab"),
        {
            INPUT_KIND_T,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("open"),
        {
            INPUT_KIND_O,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("edit"),
        {
            INPUT_KIND_FUNC_2,
            0
        }
    },
    {
        Str8Lit("accept"),
        {
            INPUT_KIND_RETURN,
            0
        }
    },
    {
        Str8Lit("accept"),
        {
            INPUT_KIND_SPACE,
            0
        }
    },
    {
        Str8Lit("cancel"),
        {
            INPUT_KIND_ESC,
            0
        }
    },
    {
        Str8Lit("move_left"),
        {
            INPUT_KIND_LEFT,
            0
        }
    },
    {
        Str8Lit("move_right"),
        {
            INPUT_KIND_RIGHT,
            0
        }
    },
    {
        Str8Lit("move_up"),
        {
            INPUT_KIND_UP,
            0
        }
    },
    {
        Str8Lit("move_down"),
        {
            INPUT_KIND_DOWN,
            0
        }
    },
    {
        Str8Lit("move_left_select"),
        {
            INPUT_KIND_LEFT,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_right_select"),
        {
            INPUT_KIND_RIGHT,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_up_select"),
        {
            INPUT_KIND_UP,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_down_select"),
        {
            INPUT_KIND_DOWN,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_left_chunk"),
        {
            INPUT_KIND_LEFT,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_right_chunk"),
        {
            INPUT_KIND_RIGHT,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_up_chunk"),
        {
            INPUT_KIND_UP,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_down_chunk"),
        {
            INPUT_KIND_DOWN,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_left_chunk_select"),
        {
            INPUT_KIND_LEFT,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_right_chunk_select"),
        {
            INPUT_KIND_RIGHT,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_up_chunk_select"),
        {
            INPUT_KIND_UP,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_down_chunk_select"),
        {
            INPUT_KIND_DOWN,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_up_page"),
        {
            INPUT_KIND_PAGEUP,
            0
        }
    },
    {
        Str8Lit("move_down_page"),
        {
            INPUT_KIND_PAGEDOWN,
            0
        }
    },
    {
        Str8Lit("move_up_whole"),
        {
            INPUT_KIND_HOME,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_down_whole"),
        {
            INPUT_KIND_END,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("move_up_page_select"),
        {
            INPUT_KIND_PAGEUP,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_down_page_select"),
        {
            INPUT_KIND_PAGEDOWN,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_up_whole_select"),
        {
            INPUT_KIND_HOME,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_down_whole_select"),
        {
            INPUT_KIND_END,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_home"),
        {
            INPUT_KIND_HOME,
            0
        }
    },
    {
        Str8Lit("move_end"),
        {
            INPUT_KIND_END,
            0
        }
    },
    {
        Str8Lit("move_home_select"),
        {
            INPUT_KIND_HOME,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("move_end_select"),
        {
            INPUT_KIND_END,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("select_all"),
        {
            INPUT_KIND_A,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("delete_single"),
        {
            INPUT_KIND_DELETE,
            0
        }
    },
    {
        Str8Lit("delete_chunk"),
        {
            INPUT_KIND_DELETE,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("backspace_single"),
        {
            INPUT_KIND_BACKSPACE,
            0
        }
    },
    {
        Str8Lit("backspace_chunk"),
        {
            INPUT_KIND_BACKSPACE,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("copy"),
        {
            INPUT_KIND_C,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("copy"),
        {
            INPUT_KIND_INSERT,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("cut"),
        {
            INPUT_KIND_X,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("paste"),
        {
            INPUT_KIND_V,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("paste"),
        {
            INPUT_KIND_INSERT,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("insert_text"),
        {
            INPUT_KIND_NULL,
            0
        }
    },
    {
        Str8Lit("move_next"),
        {
            INPUT_KIND_TAB,
            0
        }
    },
    {
        Str8Lit("move_prev"),
        {
            INPUT_KIND_TAB,
            INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("goto_time"),
        {
            INPUT_KIND_G,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("search"),
        {
            INPUT_KIND_F,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("search_backwards"),
        {
            INPUT_KIND_R,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("find_next"),
        {
            INPUT_KIND_FUNC_3,
            0
        }
    },
    {
        Str8Lit("find_prev"),
        {
            INPUT_KIND_FUNC_3,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("filter"),
        {
            INPUT_KIND_L,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("open_palette"),
        {
            INPUT_KIND_FUNC_1,
            0
        }
    },
    {
        Str8Lit("open_palette"),
        {
            INPUT_KIND_P,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("undo"),
        {
            INPUT_KIND_Z,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("redo"),
        {
            INPUT_KIND_Y,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("switch"),
        {
            INPUT_KIND_I,
            INPUT_MOD_KIND_CTRL
        }
    },
    {
        Str8Lit("open_session"),
        {
            INPUT_KIND_RETURN,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("save_session"),
        {
            INPUT_KIND_S,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT
        }
    },
    {
        Str8Lit("toggle_dev_menu"),
        {
            INPUT_KIND_D,
            INPUT_MOD_KIND_CTRL | INPUT_MOD_KIND_SHIFT | INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("files"),
        {
            INPUT_KIND_1,
            INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("views"),
        {
            INPUT_KIND_2,
            INPUT_MOD_KIND_ALT
        }
    },
    {
        Str8Lit("flv"),
        {
            INPUT_KIND_3,
            INPUT_MOD_KIND_ALT
        }
    }
};

Str8 APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_COUNT] = {
    Str8Lit(""),
    Str8Lit("\xE2\x96\xB8"),
    Str8Lit("\xE2\x97\x8B"),
    Str8Lit("i"),
    Str8Lit("!"),
    Str8Lit("\xE2\x86\x90"),
    Str8Lit("\xE2\x86\x92"),
    Str8Lit("\xE2\x86\x91"),
    Str8Lit("\xE2\x86\x93"),
    Str8Lit("\xE2\x97\x82"),
    Str8Lit("\xE2\x96\xB8"),
    Str8Lit("\xE2\x96\xB2"),
    Str8Lit("\xE2\x96\xBC"),
    Str8Lit("[ ]"),
    Str8Lit("[X]"),
    Str8Lit("( )"),
    Str8Lit("(\xE2\x80\xA2)"),
    Str8Lit("+"),
    Str8Lit("x"),
    Str8Lit("/"),
    Str8Lit("\xE2\x80\xA2")
};

Str8 APP_THEME_PRESET_DISPLAY_STR_TABLE[APP_THEME_PRESET_COUNT] = {
    Str8Lit("Ayu Dark"),
    Str8Lit("Default Dark"),
    Str8Lit("Default Light"),
    Str8Lit("GitHub Dark"),
    Str8Lit("GitHub Light"),
    Str8Lit("One Dark Pro"),
    Str8Lit("Dracula"),
    Str8Lit("Eva Light")
};

Str8 APP_THEME_PRESET_CODE_STR_TABLE[APP_THEME_PRESET_COUNT] = {
    Str8Lit("ayu_dark"),
    Str8Lit("default_dark"),
    Str8Lit("default_light"),
    Str8Lit("github_dark"),
    Str8Lit("github_light"),
    Str8Lit("one_dark_pro"),
    Str8Lit("dracula"),
    Str8Lit("eva_light")
};

Str8 APP_THEME_PRESET_CONFIG_STR_TABLE[APP_THEME_PRESET_COUNT] = {
    // ayu_dark________________________________________________________________
    Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0x0a0e14ff}\n"
        "  theme_colour:{tags: text, value: 0xb3b1adff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x5c6773ff}\n"
        "  theme_colour:{tags: \"good text\", value: 0xc2d94cff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xf07178ff}\n"
        "  theme_colour:{tags: border, value: 0x393f4cff}\n"
        "  theme_colour:{tags: hover, value: 0xe6e4e1ff}\n"
        "  theme_colour:{tags: focus, value: 0xffb454ff}\n"
        "  theme_colour:{tags: overlay, value: 0x00000080}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0x59c2ffff}\n"
        "  theme_colour:{tags: accent, value: 0xffb454ff}\n"
        "  theme_colour:{tags: selection, value: 0x59c2ff40}\n"
        "  theme_colour:{tags: cursor, value: 0xffb454ff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0x131721ff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0x27374eff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0xe6e4e1ff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0x8a9199ff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0x131721ff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0xb3b1adff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0x393f4cff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0xe6e4e1ff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x5c6773ff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0x59c2ffff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0x0a0e14ff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0x27374eff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0xe6e4e1ff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0x131721ff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0xb3b1adff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0xe6e4e1ff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x5c6773ff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xf07178ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0x1f2430ff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0xffb454ff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0x0a0e14ff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0x131721ff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0xb3b1adff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0x27374eff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0xe6e4e1ff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0x1f2430ff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0xffb454ff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0xffb45440}\n"
        "}\n"
    ),
    // default_dark____________________________________________________________
    Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0x1b1b1bff}\n"
        "  theme_colour:{tags: text, value: 0xe5e5e5ff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0xa4a4a4ff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x32a852ff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xe5534bff}\n"
        "  theme_colour:{tags: border, value: 0x404040ff}\n"
        "  theme_colour:{tags: hover, value: 0xffffffff}\n"
        "  theme_colour:{tags: focus, value: 0x3a90bbff}\n"
        "  theme_colour:{tags: overlay, value: 0x00000080}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0x3a90bbff}\n"
        "  theme_colour:{tags: accent, value: 0x355b6eff}\n"
        "  theme_colour:{tags: selection, value: 0xffffff30}\n"
        "  theme_colour:{tags: cursor, value: 0xffffffff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0x2b2b2bff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0x355b6eff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0x222222ff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0xe5e5e5ff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0x404040ff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0xa4a4a4ff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0x3a90bbff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0x1b1b1bff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0x355b6eff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0x222222ff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0xe5e5e5ff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0xa4a4a4ff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xe5534bff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0x355b6eff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0x222222ff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0xa4a4a4ff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0x222222ff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0xa4a4a4ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0x333333ff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0x2c3e48ff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0xe5e5e5ff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0x3a90bb40}\n"
        "}\n"
    ),
    // default_light___________________________________________________________
    Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0xf6f8faff}\n"
        "  theme_colour:{tags: text, value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x5c6875ff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x1a7f37ff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xc4281cff}\n"
        "  theme_colour:{tags: border, value: 0xc8d1daff}\n"
        "  theme_colour:{tags: hover, value: 0x1c2530ff}\n"
        "  theme_colour:{tags: focus, value: 0x1a5b7cff}\n"
        "  theme_colour:{tags: overlay, value: 0x00000040}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0x0a66b0ff}\n"
        "  theme_colour:{tags: accent, value: 0x2a7ba8ff}\n"
        "  theme_colour:{tags: selection, value: 0x2a7ba840}\n"
        "  theme_colour:{tags: cursor, value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0xebeff3ff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0xb9d9eeff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0x0e1820ff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0x3f4f5eff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0xb9c4ceff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x5c6875ff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0x0a66b0ff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0xf0f3f6ff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0xb9d9eeff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0x0e1820ff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0xe3e9efff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x4a5764ff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xa81e14ff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0xb9d9eeff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0x0e1820ff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0xebeff3ff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0x5c6875ff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0xebeff3ff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0x8c98a4ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0xd0d9e2ff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0xdde3e9ff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0x1c2530ff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0x1a5b7c40}\n"
        "}\n"
    ),
    // github_dark_____________________________________________________________
    Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0x0d1117ff}\n"
        "  theme_colour:{tags: text, value: 0xc9d1d9ff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x8b949eff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x3fb950ff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xf85149ff}\n"
        "  theme_colour:{tags: border, value: 0x30363dff}\n"
        "  theme_colour:{tags: hover, value: 0xf0f6fcff}\n"
        "  theme_colour:{tags: focus, value: 0x1f6febff}\n"
        "  theme_colour:{tags: overlay, value: 0x01040980}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0xe3b341ff}\n"
        "  theme_colour:{tags: accent, value: 0x58a6ffff}\n"
        "  theme_colour:{tags: selection, value: 0x388bfd40}\n"
        "  theme_colour:{tags: cursor, value: 0x58a6ffff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0x161b22ff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0x1e4273ff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0xf0f6fcff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0x9fb1c5ff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0x161b22ff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0xc9d1d9ff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0x30363dff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0xf0f6fcff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x8b949eff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0xe3b341ff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0x21262dff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0x1e4273ff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0xf0f6fcff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0x161b22ff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0xc9d1d9ff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0xf0f6fcff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x8b949eff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xf85149ff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0x1e4273ff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0xf0f6fcff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0x161b22ff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0x8b949eff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0x161b22ff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0x6e7681ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0x21262dff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0x263447ff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0xc9d1d9ff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0x58a6ff40}\n"
        "}\n"
    ),
    // github_light____________________________________________________________
        Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0xffffffff}\n"
        "  theme_colour:{tags: text, value: 0x24292fff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x57606aff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x1a7f37ff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xcf222eff}\n"
        "  theme_colour:{tags: border, value: 0xd0d7deff}\n"
        "  theme_colour:{tags: hover, value: 0x24292fff}\n"
        "  theme_colour:{tags: focus, value: 0x0969daff}\n"
        "  theme_colour:{tags: overlay, value: 0x1b1f2440}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0x0550aeff}\n"
        "  theme_colour:{tags: accent, value: 0x0969daff}\n"
        "  theme_colour:{tags: selection, value: 0x0969da33}\n"
        "  theme_colour:{tags: cursor, value: 0x0969daff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0xf6f8faff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0xbbdfffff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0x4b5560ff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0xd0d7deff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x57606aff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0x0550aeff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0xf6f8faff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0xbbdfffff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0xf6f8faff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x57606aff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xcf222eff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0xbbdfffff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0xf6f8faff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0x57606aff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0xf6f8faff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0x8c959fff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0xeaeef2ff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0xd8dee4ff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0x24292fff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0x0969da40}\n"
        "}\n"
    ),
    // one_dark_pro____________________________________________________________
        Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0x282c34ff}\n"
        "  theme_colour:{tags: text, value: 0xabb2bfff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x7f848eff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x98c379ff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xe06c75ff}\n"
        "  theme_colour:{tags: border, value: 0x4b5263ff}\n"
        "  theme_colour:{tags: hover, value: 0xd7dae0ff}\n"
        "  theme_colour:{tags: focus, value: 0x528bffff}\n"
        "  theme_colour:{tags: overlay, value: 0x00000080}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0xe5c07bff}\n"
        "  theme_colour:{tags: accent, value: 0x61afefff}\n"
        "  theme_colour:{tags: selection, value: 0x67769660}\n"
        "  theme_colour:{tags: cursor, value: 0x528bffff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0x21252bff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0x39536cff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0xa9b4c2ff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0x21252bff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0xabb2bfff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0x4b5263ff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0xd7dae0ff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x7f848eff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0xe5c07bff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0x2c313aff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0x39536cff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0x21252bff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0xabb2bfff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0xd7dae0ff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x7f848eff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xe06c75ff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0x39536cff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0xffffffff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0x21252bff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0x7f848eff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0x21252bff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0x5c6370ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0x2c313aff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0x323a47ff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0xabb2bfff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0x61afef40}\n"
        "}\n"
    ),
    // dracula_________________________________________________________________
        Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0x282a36ff}\n"
        "  theme_colour:{tags: text, value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x7b88b8ff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x50fa7bff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xff5555ff}\n"
        "  theme_colour:{tags: border, value: 0x44475aff}\n"
        "  theme_colour:{tags: hover, value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: focus, value: 0xbd93f9ff}\n"
        "  theme_colour:{tags: overlay, value: 0x00000080}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0x8be9fdff}\n"
        "  theme_colour:{tags: accent, value: 0xff79c6ff}\n"
        "  theme_colour:{tags: selection, value: 0xbd93f940}\n"
        "  theme_colour:{tags: cursor, value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0x21222cff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0x554a71ff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0xc0c4ddff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0x21222cff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0x6272a4ff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x7b88b8ff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0x8be9fdff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0x191a21ff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0x554a71ff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0x21222cff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x7b88b8ff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xff5555ff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0x554a71ff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0x21222cff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0x7b88b8ff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0x21222cff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0x6272a4ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0x343746ff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0x3a3c4eff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0xf8f8f2ff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0xbd93f940}\n"
        "}\n"
    ),
    // eva_light_______________________________________________________________
    Str8Lit(
        "theme:\n{\n"
        "  theme_colour:{tags: background, value: 0xebeef5ff}\n"
        "  theme_colour:{tags: text, value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: \"weak text\", value: 0x787a7fff}\n"
        "  theme_colour:{tags: \"good text\", value: 0x499149ff}\n"
        "  theme_colour:{tags: \"bad text\", value: 0xec0000ff}\n"
        "  theme_colour:{tags: border, value: 0xced1d7ff}\n"
        "  theme_colour:{tags: hover, value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: focus, value: 0x4480f4ff}\n"
        "  theme_colour:{tags: overlay, value: 0x00000040}\n"
        "  theme_colour:{tags: fuzzy_match, value: 0x7c4dffff}\n"
        "  theme_colour:{tags: accent, value: 0x4480f4ff}\n"
        "  theme_colour:{tags: selection, value: 0x0065ff3f}\n"
        "  theme_colour:{tags: cursor, value: 0xfc8357ff}\n"
        "  theme_colour:{tags: \"alt background\", value: 0xe1e4ebff}\n"
        "  theme_colour:{tags: \"pop background\", value: 0xbcd3f7ff}\n"
        "  theme_colour:{tags: \"pop text\", value: 0x535355ff}\n"
        "  theme_colour:{tags: \"pop weak text\", value: 0x626264ff}\n"
        "  theme_colour:{tags: \"floating background\", value: 0xf1f4fbff}\n"
        "  theme_colour:{tags: \"floating text\", value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: \"floating border\", value: 0xced1d7ff}\n"
        "  theme_colour:{tags: \"floating hover\", value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: \"floating weak text\", value: 0x787a7fff}\n"
        "  theme_colour:{tags: \"floating fuzzy_match\", value: 0x7c4dffff}\n"
        "  theme_colour:{tags: \"floating alt background\", value: 0xe5e8efff}\n"
        "  theme_colour:{tags: \"floating pop background\", value: 0xbcd3f7ff}\n"
        "  theme_colour:{tags: \"floating pop text\", value: 0x535355ff}\n"
        "  theme_colour:{tags: \"menu_bar background\", value: 0xe1e4ebff}\n"
        "  theme_colour:{tags: \"menu_bar text\", value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: \"menu_bar hover\", value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: \"menu_bar weak text\", value: 0x787a7fff}\n"
        "  theme_colour:{tags: \"menu_bar bad text\", value: 0xec0000ff}\n"
        "  theme_colour:{tags: \"tab background\", value: 0xbcd3f7ff}\n"
        "  theme_colour:{tags: \"tab text\", value: 0x535355ff}\n"
        "  theme_colour:{tags: \"tab inactive background\", value: 0xe1e4ebff}\n"
        "  theme_colour:{tags: \"tab inactive text\", value: 0x787a7fff}\n"
        "  theme_colour:{tags: \"scroll_bar background\", value: 0xe1e4ebff}\n"
        "  theme_colour:{tags: \"scroll_bar accent\", value: 0xaaadb4ff}\n"
        "  theme_colour:{tags: \"menu_bar alt background\", value: 0xd6d9e0ff}\n"
        "  theme_colour:{tags: \"tab unfocused background\", value: 0xd3dae8ff}\n"
        "  theme_colour:{tags: \"tab unfocused text\", value: 0x5d5d5fff}\n"
        "  theme_colour:{tags: \"drop_site background\", value: 0x0065ff40}\n"
        "}\n"
    )
};

AppNameSchemaInfo APP_NAME_SCHEMA_INFO_TABLE[8] = {
    {
        Str8Lit("session"),
        FALSE,
        Str8Lit("@expand_commands(edit_session_theme) x:\n{\n@display_name('Animations') @description(\"Enables animations.\")\n\n@default(1) 'animations': bool,\n\n@display_name('Scrolling Animations') @description(\"Enables scrolling animations.\") @expand_if(\"$.animations\") @default(1) 'scrolling_animations': bool,\n\n@display_name('Tooltip Animations') @description(\"Enables tooltip animations.\") @expand_if(\"$.animations\") @default(1) 'tooltip_animations': bool,\n\n@display_name('Menu Animations') @description(\"Enables menu animations.\") @expand_if(\"$.animations\") @default(1) 'menu_animations': bool,\n\n@no_expand @default('query:commands') 'cmd': string,\n\n@display_name('Theme') @description(\"The session theme, which describes all colours used throughout the UI.\") @default(\"Ayu Dark\") 'theme': string,\n\n@no_expand @display_name('Theme Colours') 'theme_colours': query,\n\n@display_name('Autocompletion Lister') @description(\"Enables the autocompletion lister while typing expressions.\") @default(1) 'autocompletion_lister': bool,\n\n@display_name('Cursor Trail') @description(\"Shows a trail after the cursor as it moves\") @default(1) 'cursor_trail': bool,\n\n@display_name('Tab Width') @default(2) 'tab_width': @range[1, 32] u64,\n\n@display_name('Focus Menu Bar With Alt') @description(\"Mimics standard Windows behavior of focusing the menu bar using the Alt key.\") @default(1) 'focus_menu_bar_with_alt': bool,\n}")
    },
    {
        Str8Lit("theme_colour"),
        FALSE,
        Str8Lit("@collection_commands(add_theme_colour, fork_theme, save_theme, save_and_set_theme)\n@row_commands(duplicate_cfg, remove_cfg)\nx:\n{\n  @display_name('Tags') tags: string,\n  @display_name('Value') value: @colour @hex u32,\n}\n")
    },
    {
        Str8Lit("tab"),
        FALSE,
        Str8Lit("@row_commands(@file copy_tab_full_path, @file show_file_in_explorer, duplicate_tab, close_tab)")
    },
    // TODO(nc): implement schema for time. can show as timestamp or as relative to start time and in different units
    {
        Str8Lit("flv"),
        TRUE,
        Str8Lit("@inherit(tab) x:\n{\n  @description(\"An expression to describe data which should be viewed as FLV script.\")\n  'expression': expr_string,\n  @default(1) @description(\"Controls whether or not line numbers are shown.\")\n  'show_line_numbers':bool,\n  @no_callee_helper @default(1) @display_name('Line Wrapping') @description(\"Splits textual lines into multiple visual lines, so that all text is within the visible area.\")\n  'line_wrapping': bool,\n  @no_callee_helper @default(1) @display_name('Show Cursor Trail') @description(\"Shows cursor trail when cursor position is changed.\")\n  'cursor_trail': bool,\n}\n")
    },
    {
        Str8Lit("colour"),
        TRUE,
        Str8Lit("@inherit(tab) x:\n{\n  @display_name(\"Value\") @description(\"An expression to describe the value or location of the colour.\")\n  'expression': expr_string,\n}\n")
    },
    {
        Str8Lit("getting_started"),
        FALSE,
        Str8Lit("@inherit(tab) x:\n{\n}\n")
    },
    {
        Str8Lit("recent_session"),
        FALSE,
        Str8Lit("x:{'path':path}")
    },
    {
        Str8Lit("recent_file"),
        FALSE,
        Str8Lit("x:{'path':path}")
    }
    // TODO(nc): implement schema for views (add view(s)/folder, remove view(s)), files (add file(s)/folder, remove file(s)), and just view (switch flv file, edit flv file).
};

Str8 APP_REGISTERS_SLOT_CODE_NAME_TABLE[APP_REGISTERS_SLOT_COUNT] = {
    {},
    Str8Lit("Window"),
    Str8Lit("Panel"),
    Str8Lit("Tab"),
    Str8Lit("View"),
    Str8Lit("PrevTab"),
    Str8Lit("DstPanel"),
    Str8Lit("Config"),
    Str8Lit("ConfigList"),
    Str8Lit("FilePath"),
    Str8Lit("Cursor"),
    Str8Lit("Mark"),
    Str8Lit("FlightIndex"),
    Str8Lit("Expression"),
    Str8Lit("UI"),
    Str8Lit("ForceConfirm"),
    Str8Lit("String"),
    Str8Lit("CommandName"),
    Str8Lit("Direction"),
    Str8Lit("OSEvent")
};

r1u64 APP_REGISTERS_SLOT_RANGE_TABLE[APP_REGISTERS_SLOT_COUNT] = {
    {},
    {
        OFFSETOF(AppRegisters, Window),
        OFFSETOF(AppRegisters, Window) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, Panel),
        OFFSETOF(AppRegisters, Panel) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, Tab),
        OFFSETOF(AppRegisters, Tab) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, View),
        OFFSETOF(AppRegisters, View) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, PrevTab),
        OFFSETOF(AppRegisters, PrevTab) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, DstPanel),
        OFFSETOF(AppRegisters, DstPanel) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, Config),
        OFFSETOF(AppRegisters, Config) + sizeof(ConfigID)
    },
    {
        OFFSETOF(AppRegisters, ConfigList),
        OFFSETOF(AppRegisters, ConfigList) + sizeof(ConfigIDList)
    },
    {
        OFFSETOF(AppRegisters, FilePath),
        OFFSETOF(AppRegisters, FilePath) + sizeof(Str8)
    },
    {
        OFFSETOF(AppRegisters, Cursor),
        OFFSETOF(AppRegisters, Cursor) + sizeof(TextPoint)
    },
    {
        OFFSETOF(AppRegisters, Mark),
        OFFSETOF(AppRegisters, Mark) + sizeof(TextPoint)
    },
    {
        OFFSETOF(AppRegisters, FlightIndex),
        OFFSETOF(AppRegisters, FlightIndex) + sizeof(u32)
    },
    {
        OFFSETOF(AppRegisters, Expression),
        OFFSETOF(AppRegisters, Expression) + sizeof(Str8)
    },
    {
        OFFSETOF(AppRegisters, UI),
        OFFSETOF(AppRegisters, UI) + sizeof(UIKey)
    },
    {
        OFFSETOF(AppRegisters, ForceConfirm),
        OFFSETOF(AppRegisters, ForceConfirm) + sizeof(b32)
    },
    {
        OFFSETOF(AppRegisters, String),
        OFFSETOF(AppRegisters, String) + sizeof(Str8)
    },
    {
        OFFSETOF(AppRegisters, CommandName),
        OFFSETOF(AppRegisters, CommandName) + sizeof(Str8)
    },
    {
        OFFSETOF(AppRegisters, Direction),
        OFFSETOF(AppRegisters, Direction) + sizeof(Direction2D)
    },
    {
        OFFSETOF(AppRegisters, OSEvent),
        OFFSETOF(AppRegisters, OSEvent) + sizeof(InputEvent*)
    }
};

Str8 APP_TAB_FAST_PATH_VIEW_NAME_TABLE[APP_COMMAND_KIND_COUNT - APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD] = {
    Str8Lit("files"),
    Str8Lit("views"),
    Str8Lit("flv")
};

// TODO(nc): figure out what needs to be here
Str8 APP_TAB_FAST_PATH_QUERY_NAME[APP_COMMAND_KIND_COUNT - APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD] = {
    Str8Lit(""),
    Str8Lit(""),
    Str8Lit("")
};

AppVocabInfo APP_VOCAB_INFO_TABLE[162] = {
    { 
        Str8Lit("view"),
        Str8Lit("views"),
        Str8Lit("View"),
        Str8Lit("Views"),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("colour"),
        Str8Lit("colours"),
        Str8Lit("Colour"),
        Str8Lit("Colours"),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("theme_colour"),
        Str8Lit("theme_colours"),
        Str8Lit("Theme Colour"),
        Str8Lit("Theme Colours"),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("min"),
        Str8Lit("mins"),
        Str8Lit("Minimum"),
        Str8Lit("Minimums"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("max"),
        Str8Lit("maxs"),
        Str8Lit("Maximum"),
        Str8Lit("Maximums"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("panel"),
        Str8Lit("panels"),
        Str8Lit("Panel"),
        Str8Lit("Panels"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("tab"),
        Str8Lit("tabs"),
        Str8Lit("Tab"),
        Str8Lit("Tabs"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("recent_file"),
        Str8Lit("recent_files"),
        Str8Lit("Recent File"),
        Str8Lit("Recent Files"),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("src"),
        Str8Lit("srcs"),
        Str8Lit("Source"),
        Str8Lit("Sources"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("dst"),
        Str8Lit("dsts"),
        Str8Lit("Destination"),
        Str8Lit("Destinations"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("source"),
        Str8Lit("sources"),
        Str8Lit("Source"),
        Str8Lit("Sources"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("dest"),
        Str8Lit("dests"),
        Str8Lit("Destination"),
        Str8Lit("Destinations"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("label"),
        Str8Lit("labels"),
        Str8Lit("Label"),
        Str8Lit("Labels"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("getting_started"),
        Str8Lit(""),
        Str8Lit("Getting Started"),
        Str8Lit(""),
        APP_ICON_KIND_INFO
    },
    { 
        Str8Lit("flv"),
        Str8Lit(""),
        Str8Lit("FLV"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("enabled"),
        Str8Lit(""),
        Str8Lit("Enabled"),
        Str8Lit("Enabled"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("disabled"),
        Str8Lit(""),
        Str8Lit("Disabled"),
        Str8Lit("Disabled"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("id"),
        Str8Lit("ids"),
        Str8Lit("ID"),
        Str8Lit("IDs"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("last_modified_time"),
        Str8Lit("last_modified_times"),
        Str8Lit("Last Modified Time"),
        Str8Lit("Last Modified Times"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("creation_time"),
        Str8Lit("creation_times"),
        Str8Lit("Creation Time"),
        Str8Lit("Creation Times"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("data"),
        Str8Lit("datas"),
        Str8Lit("Data"),
        Str8Lit("Datas"),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("session"),
        Str8Lit("sessions"),
        Str8Lit("Session"),
        Str8Lit("Sessions"),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("recent_session"),
        Str8Lit("recent_sessions"),
        Str8Lit("Recent Session"),
        Str8Lit("Recent Sessions"),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("recent_file"),
        Str8Lit("recent_files"),
        Str8Lit("Recent File"),
        Str8Lit("Recent Files"),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("row_height"),
        Str8Lit(""),
        Str8Lit("Row Height"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("tab_height"),
        Str8Lit(""),
        Str8Lit("Tab Height"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("rgba"),
        Str8Lit(""),
        Str8Lit("RGBA"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("exit"),
        Str8Lit(""),
        Str8Lit("Exit"),
        Str8Lit(""),
        APP_ICON_KIND_X 
    },
    { 
        Str8Lit("open_palette"),
        Str8Lit(""),
        Str8Lit("Open Palette"),
        Str8Lit(""),
        APP_ICON_KIND_INFO
    },
    { 
        Str8Lit("run_command"),
        Str8Lit(""),
        Str8Lit("Run Command"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("os_event"),
        Str8Lit(""),
        Str8Lit("OS Event"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("select_entity"),
        Str8Lit(""),
        Str8Lit("Select"),
        Str8Lit(""),
        APP_ICON_KIND_RADIO_HOLLOW 
    },
    { 
        Str8Lit("deselect_entity"),
        Str8Lit(""),
        Str8Lit("Deselect"),
        Str8Lit(""),
        APP_ICON_KIND_RADIO_FILLED 
    },
    { 
        Str8Lit("popup_accept"),
        Str8Lit(""),
        Str8Lit("Popup Accept"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("popup_cancel"),
        Str8Lit(""),
        Str8Lit("Popup Cancel"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("reset_to_default_bindings"),
        Str8Lit(""),
        Str8Lit("Reset To Default Bindings"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("new_panel_left"),
        Str8Lit(""),
        Str8Lit("Split Panel Left"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW
    },
    { 
        Str8Lit("new_panel_up"),
        Str8Lit(""),
        Str8Lit("Split Panel Up"),
        Str8Lit(""),
        APP_ICON_KIND_UP_ARROW
    },
    { 
        Str8Lit("new_panel_right"),
        Str8Lit(""),
        Str8Lit("Split Panel Right"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW
    },
    { 
        Str8Lit("new_panel_down"),
        Str8Lit(""),
        Str8Lit("Split Panel Down"),
        Str8Lit(""),
        APP_ICON_KIND_DOWN_ARROW
    },
    { 
        Str8Lit("split_panel"),
        Str8Lit(""),
        Str8Lit("Split Panel"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("next_panel"),
        Str8Lit(""),
        Str8Lit("Focus Next Panel"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW 
    },
    { 
        Str8Lit("prev_panel"),
        Str8Lit(""),
        Str8Lit("Focus Previous Panel"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW 
    },
    { 
        Str8Lit("focus_panel"),
        Str8Lit(""),
        Str8Lit("Focus Panel"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("focus_panel_right"),
        Str8Lit(""),
        Str8Lit("Focus Panel Right"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW 
    },
    { 
        Str8Lit("focus_panel_left"),
        Str8Lit(""),
        Str8Lit("Focus Panel Left"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW 
    },
    { 
        Str8Lit("focus_panel_up"),
        Str8Lit(""),
        Str8Lit("Focus Panel Up"),
        Str8Lit(""),
        APP_ICON_KIND_UP_ARROW 
    },
    { 
        Str8Lit("focus_panel_down"),
        Str8Lit(""),
        Str8Lit("Focus Panel Down"),
        Str8Lit(""),
        APP_ICON_KIND_DOWN_ARROW 
    },
    { 
        Str8Lit("undo"),
        Str8Lit(""),
        Str8Lit("Undo"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW
    },
    { 
        Str8Lit("redo"),
        Str8Lit(""),
        Str8Lit("Redo"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW
    },
    { 
        Str8Lit("go_back"),
        Str8Lit(""),
        Str8Lit("Go Back"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW 
    },
    { 
        Str8Lit("go_forward"),
        Str8Lit(""),
        Str8Lit("Go Forward"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW 
    },
    { 
        Str8Lit("close_panel"),
        Str8Lit(""),
        Str8Lit("Close Panel"),
        Str8Lit(""),
        APP_ICON_KIND_X
    },
    { 
        Str8Lit("focus_tab"),
        Str8Lit(""),
        Str8Lit("Focus Tab"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("next_tab"),
        Str8Lit(""),
        Str8Lit("Focus Next Tab"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW 
    },
    { 
        Str8Lit("prev_tab"),
        Str8Lit(""),
        Str8Lit("Focus Previous Tab"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW 
    },
    { 
        Str8Lit("move_tab_right"),
        Str8Lit(""),
        Str8Lit("Move Tab Right"),
        Str8Lit(""),
        APP_ICON_KIND_RIGHT_ARROW 
    },
    { 
        Str8Lit("move_tab_left"),
        Str8Lit(""),
        Str8Lit("Move Tab Left"),
        Str8Lit(""),
        APP_ICON_KIND_LEFT_ARROW 
    },
    { 
        Str8Lit("open_tab"),
        Str8Lit(""),
        Str8Lit("Open New Tab"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("build_tab"),
        Str8Lit(""),
        Str8Lit("Build Tab"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("duplicate_tab"),
        Str8Lit(""),
        Str8Lit("Duplicate Tab"),
        Str8Lit(""),
        APP_ICON_KIND_ADD
    },
    { 
        Str8Lit("copy_tab_full_path"),
        Str8Lit(""),
        Str8Lit("Copy Full Path"),
        Str8Lit(""),
        APP_ICON_KIND_ADD
    },
    { 
        Str8Lit("close_tab"),
        Str8Lit(""),
        Str8Lit("Close Tab"),
        Str8Lit(""),
        APP_ICON_KIND_X 
    },
    { 
        Str8Lit("move_view"),
        Str8Lit(""),
        Str8Lit("Move View"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("tab_settings"),
        Str8Lit(""),
        Str8Lit("Tab Settings"),
        Str8Lit(""),
        APP_ICON_KIND_INFO
    },
    { 
        Str8Lit("set_current_path"),
        Str8Lit(""),
        Str8Lit("Set Current Path"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("open"),
        Str8Lit(""),
        Str8Lit("Open"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("switch"),
        Str8Lit(""),
        Str8Lit("Switch"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("record_file_in_session"),
        Str8Lit(""),
        Str8Lit("Record File In Session"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("show_file_in_explorer"),
        Str8Lit(""),
        Str8Lit("Show File In Explorer"),
        Str8Lit(""),
        APP_ICON_KIND_FOLDER
    },
    { 
        Str8Lit("set_file_replacement_path"),
        Str8Lit(""),
        Str8Lit("Set File Replacement Path"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("new_session"),
        Str8Lit(""),
        Str8Lit("New Session"),
        Str8Lit(""),
        APP_ICON_KIND_ADD 
    },
    { 
        Str8Lit("open_session"),
        Str8Lit(""),
        Str8Lit("Open Session"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("open_recent_session"),
        Str8Lit(""),
        Str8Lit("Open Recent Session"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("save_session"),
        Str8Lit(""),
        Str8Lit("Save Session"),
        Str8Lit(""),
        APP_ICON_KIND_INFO
    },
    { 
        Str8Lit("record_session_as_last_opened"),
        Str8Lit(""),
        Str8Lit("Record Session As Last Opened"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("record_recent_session"),
        Str8Lit(""),
        Str8Lit("Records Session In Recents"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("write_session_data"),
        Str8Lit(""),
        Str8Lit("Write Session Data"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("session_settings"),
        Str8Lit(""),
        Str8Lit("Session Settings"),
        Str8Lit(""),
        APP_ICON_KIND_INFO
    },
    { 
        Str8Lit("edit"),
        Str8Lit(""),
        Str8Lit("Edit"),
        Str8Lit(""),
        APP_ICON_KIND_DOT
    },
    { 
        Str8Lit("accept"),
        Str8Lit(""),
        Str8Lit("Accept"),
        Str8Lit(""),
        APP_ICON_KIND_CHECK_FILLED 
    },
    { 
        Str8Lit("cancel"),
        Str8Lit(""),
        Str8Lit("Cancel"),
        Str8Lit(""),
        APP_ICON_KIND_X 
    },
    { 
        Str8Lit("move_left"),
        Str8Lit(""),
        Str8Lit("Move Left"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_right"),
        Str8Lit(""),
        Str8Lit("Move Right"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up"),
        Str8Lit(""),
        Str8Lit("Move Up"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down"),
        Str8Lit(""),
        Str8Lit("Move Down"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_left_select"),
        Str8Lit(""),
        Str8Lit("Move Left Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_right_select"),
        Str8Lit(""),
        Str8Lit("Move Right Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_select"),
        Str8Lit(""),
        Str8Lit("Move Up Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_select"),
        Str8Lit(""),
        Str8Lit("Move Down Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_left_chunk"),
        Str8Lit(""),
        Str8Lit("Move Left Chunk"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_right_chunk"),
        Str8Lit(""),
        Str8Lit("Move Right Chunk"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_chunk"),
        Str8Lit(""),
        Str8Lit("Move Up Chunk"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_chunk"),
        Str8Lit(""),
        Str8Lit("Move Down Chunk"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_page"),
        Str8Lit(""),
        Str8Lit("Move Up Page"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_page"),
        Str8Lit(""),
        Str8Lit("Move Down Page"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_whole"),
        Str8Lit(""),
        Str8Lit("Move Up Whole"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_whole"),
        Str8Lit(""),
        Str8Lit("Move Down Whole"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_left_chunk_select"),
        Str8Lit(""),
        Str8Lit("Move Left Chunk Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_right_chunk_select"),
        Str8Lit(""),
        Str8Lit("Move Right Chunk Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_chunk_select"),
        Str8Lit(""),
        Str8Lit("Move Up Chunk Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_chunk_select"),
        Str8Lit(""),
        Str8Lit("Move Down Chunk Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_page_select"),
        Str8Lit(""),
        Str8Lit("Move Up Page Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_page_select"),
        Str8Lit(""),
        Str8Lit("Move Down Page Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_whole_select"),
        Str8Lit(""),
        Str8Lit("Move Up Whole Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_whole_select"),
        Str8Lit(""),
        Str8Lit("Move Down Whole Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_up_reorder"),
        Str8Lit(""),
        Str8Lit("Move Up Reorder"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_down_reorder"),
        Str8Lit(""),
        Str8Lit("Move Down Reorder"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_home"),
        Str8Lit(""),
        Str8Lit("Move Home"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_end"),
        Str8Lit(""),
        Str8Lit("Move End"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_home_select"),
        Str8Lit(""),
        Str8Lit("Move Home Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_end_select"),
        Str8Lit(""),
        Str8Lit("Move End Select"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("select_all"),
        Str8Lit(""),
        Str8Lit("Select All"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("delete_single"),
        Str8Lit(""),
        Str8Lit("Delete Single"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("delete_chunk"),
        Str8Lit(""),
        Str8Lit("Delete Chunk"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("backspace_single"),
        Str8Lit(""),
        Str8Lit("Backspace Single"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("backspace_chunk"),
        Str8Lit(""),
        Str8Lit("Backspace Chunk"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("copy"),
        Str8Lit(""),
        Str8Lit("Copy"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("cut"),
        Str8Lit(""),
        Str8Lit("Cut"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("paste"),
        Str8Lit(""),
        Str8Lit("Paste"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("insert_text"),
        Str8Lit(""),
        Str8Lit("Insert Text"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_next"),
        Str8Lit(""),
        Str8Lit("Move Next"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("move_prev"),
        Str8Lit(""),
        Str8Lit("Move Previous"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("centre_cursor"),
        Str8Lit(""),
        Str8Lit("Centre Cursor"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("contain_cursor"),
        Str8Lit(""),
        Str8Lit("Contain Cursor"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("find_next"),
        Str8Lit(""),
        Str8Lit("Find Next"),
        Str8Lit(""),
        APP_ICON_KIND_FIND 
    },
    { 
        Str8Lit("find_prev"),
        Str8Lit(""),
        Str8Lit("Find Previous"),
        Str8Lit(""),
        APP_ICON_KIND_FIND 
    },
    { 
        Str8Lit("enable_cfg"),
        Str8Lit(""),
        Str8Lit("Enable"),
        Str8Lit(""),
        APP_ICON_KIND_CHECK_HOLLOW 
    },
    { 
        Str8Lit("disable_cfg"),
        Str8Lit(""),
        Str8Lit("Disable"),
        Str8Lit(""),
        APP_ICON_KIND_CHECK_FILLED 
    },
    { 
        Str8Lit("select_cfg"),
        Str8Lit(""),
        Str8Lit("Select"),
        Str8Lit(""),
        APP_ICON_KIND_RADIO_HOLLOW 
    },
    { 
        Str8Lit("deselect_cfg"),
        Str8Lit(""),
        Str8Lit("Deselect"),
        Str8Lit(""),
        APP_ICON_KIND_RADIO_FILLED 
    },
    { 
        Str8Lit("remove_cfg"),
        Str8Lit(""),
        Str8Lit("Remove"),
        Str8Lit(""),
        APP_ICON_KIND_X
    },
    { 
        Str8Lit("name_cfg"),
        Str8Lit(""),
        Str8Lit("Name"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("duplicate_cfg"),
        Str8Lit(""),
        Str8Lit("Duplicate"),
        Str8Lit(""),
        APP_ICON_KIND_ADD
    },
    { 
        Str8Lit("relocate_cfg"),
        Str8Lit(""),
        Str8Lit("Relocate"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("save_cfg_to_session"),
        Str8Lit(""),
        Str8Lit("Save To Session"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("clear_output"),
        Str8Lit(""),
        Str8Lit("Clear Output"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("edit_session_theme"),
        Str8Lit(""),
        Str8Lit("Edit Session Theme"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("add_theme_colour"),
        Str8Lit(""),
        Str8Lit("Add Theme Colour"),
        Str8Lit(""),
        APP_ICON_KIND_ADD
    },
    { 
        Str8Lit("fork_theme"),
        Str8Lit(""),
        Str8Lit("Fork Theme"),
        Str8Lit(""),
        APP_ICON_KIND_ADD
    },
    { 
        Str8Lit("save_theme"),
        Str8Lit(""),
        Str8Lit("Save Theme"),
        Str8Lit(""),
        APP_ICON_KIND_INFO
    },
    { 
        Str8Lit("save_and_set_theme"),
        Str8Lit(""),
        Str8Lit("Save And Set Theme"),
        Str8Lit(""),
        APP_ICON_KIND_ADD
    },
    { 
        Str8Lit("search"),
        Str8Lit(""),
        Str8Lit("Search"),
        Str8Lit(""),
        APP_ICON_KIND_FIND 
    },
    { 
        Str8Lit("search_backwards"),
        Str8Lit(""),
        Str8Lit("Search Backwards"),
        Str8Lit(""),
        APP_ICON_KIND_FIND 
    },
    { 
        Str8Lit("pick_file"),
        Str8Lit(""),
        Str8Lit("Pick File"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("pick_folder"),
        Str8Lit(""),
        Str8Lit("Pick Folder"),
        Str8Lit(""),
        APP_ICON_KIND_FOLDER
    },
    { 
        Str8Lit("pick_file_or_folder"),
        Str8Lit(""),
        Str8Lit("Pick File/Folder"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("push_query"),
        Str8Lit(""),
        Str8Lit("Push Query"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("complete_query"),
        Str8Lit(""),
        Str8Lit("Complete Query"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("cancel_query"),
        Str8Lit(""),
        Str8Lit("Cancel Query"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("update_query"),
        Str8Lit(""),
        Str8Lit("Update Query"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("toggle_dev_menu"),
        Str8Lit(""),
        Str8Lit("Toggle Developer Menu"),
        Str8Lit(""),
        APP_ICON_KIND_NULL 
    },
    { 
        Str8Lit("files"),
        Str8Lit(""),
        Str8Lit("Files"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("views"),
        Str8Lit(""),
        Str8Lit("Views"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("flv"),
        Str8Lit(""),
        Str8Lit("FLV"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("colour"),
        Str8Lit(""),
        Str8Lit("Colour"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("change_file"),
        Str8Lit(""),
        Str8Lit("Change File"),
        Str8Lit(""),
        APP_ICON_KIND_FILE
    },
    { 
        Str8Lit("goto_time"),
        Str8Lit(""),
        Str8Lit("Go To Time"),
        Str8Lit(""),
        APP_ICON_KIND_FIND
    },
    { 
        Str8Lit("filter"),
        Str8Lit(""),
        Str8Lit("Filter"),
        Str8Lit(""),
        APP_ICON_KIND_FIND
    },
    { 
        Str8Lit("set_theme"),
        Str8Lit(""),
        Str8Lit("Set Theme"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("set_tab_view"),
        Str8Lit(""),
        Str8Lit("Set Tab View"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    },
    { 
        Str8Lit("query"),
        Str8Lit(""),
        Str8Lit("Query"),
        Str8Lit(""),
        APP_ICON_KIND_NULL
    }
};
EXTERN_C_LINK_END

struct QueryTypeMapping {
    Str8                   Name;
    AppQueryExpressionKind Kind;
};

global QueryTypeMapping Mappings[] = {
    {
        Str8Lit("commands"),
        APP_QUERY_EXPR_KIND_COMMANDS
    },
    {
        Str8Lit("tab_commands"),
        APP_QUERY_EXPR_KIND_TAB_COMMANDS
    },
    {
        Str8Lit("recent_files"),
        APP_QUERY_EXPR_KIND_RECENT_FILES
    },
    {
        Str8Lit("flv_files"),
        APP_QUERY_EXPR_KIND_FLV_FILES
    },
    {
        Str8Lit("recent_sessions"),
        APP_QUERY_EXPR_KIND_RECENT_SESSIONS
    },
    {
        Str8Lit("themes"),
        APP_QUERY_EXPR_KIND_THEMES
    },
    {
        Str8Lit("session_settings"),
        APP_QUERY_EXPR_KIND_SESSION_SETTINGS
    },
    {
        Str8Lit("file_path"),
        APP_QUERY_EXPR_KIND_FILE_PATH
    }
};

APP_VIEW_UI_FUNC_DEF(Null)
{}

INTERNAL AppQueryExpressionKind
AppQueryExpressionKindFromStr(Str8 Expression)
{
    AppQueryExpressionKind Result = APP_QUERY_EXPR_KIND_NULL;
    Str8 Name = Expression;

    if (StrMatch(StrPrefix(Name, 6), "query:"_s8, 0))
        Name = StrSkip(Name, 6);

    if (StrMatch(StrPrefix(Name, 7), "folder:"_s8, 0)) {
        Result = APP_QUERY_EXPR_KIND_FILE_PATH;
    } else if (StrMatch(StrPrefix(Name, 8), "config.$"_s8, 0)) {
        Result = (StrFindSubStr(Name, "."_s8, 8, 0) < Name.Size)
            ? APP_QUERY_EXPR_KIND_CONFIG_CHILD
            : APP_QUERY_EXPR_KIND_CONFIG;
    } else {
        for (u64 Index = 0; Index < ARRAY_COUNT(Mappings); ++Index) {
            if (StrMatch(Name, Mappings[Index].Name, 0)) {
                Result = Mappings[Index].Kind;
                break;
            }
        }
    }

    return Result;
}

ConfigNode* 
AppConfigFromEvalSpace(EvalSpace Space)
{
    ConfigNode* Result = EMPTY_CFG_NODE_VALUE;

    if (Space.Kind == APP_EVAL_SPACE_KIND_META_CFG) {
        ConfigID ID = Space.U64s[0];

        Result = ConfigNodeFromID(ID);
    }

    return Result;
}

EvalSpace 
AppEvalSpaceFromConfig(ConfigNode* Cfg)
{
    EvalSpace Result = EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_CFG);

    Result.U64s[0] = Cfg->ID;

    return Result;
}

Str8 
AppCommandNameFromEval(EvalEval Eval)
{
    Str8 Result = {};

    if (Eval.Space.Kind == APP_EVAL_SPACE_KIND_META_CMD)
        Result = EvalStrFromID(Eval.Value.U64);

    return Result;
}

u64 
AppEvalSpaceGen(EvalSpace Space)
{
    u64 Result = 0;

    switch (Space.Kind) {
        default: {
            Result = ConfigChangeGeneration();
        } break;
    }

    return Result;
}

b32 
AppEvalSpaceRead(
    EvalSpace Space, 
    OUT void* Data, 
    OUT EvalSpaceRangeInfo* RangeInfo, 
    r1u64 Range
) {

    TempArena Scratch = GetScratch(NULL, 0);
    b32 Result = FALSE;

    switch (Space.Kind) {
        default: {} break;

        case APP_EVAL_SPACE_KIND_META_CFG: {
            ConfigNode* RootCfg = AppConfigFromEvalSpace(Space);
            Str8 ChildKey = EvalStrFromID(Space.U64s[1]);
            ConfigNode* Cfg = RootCfg;

            if (ChildKey.Size)
                Cfg = ConfigNodeChildFromStr(RootCfg, ChildKey);

            Str8 ReadData = {};

            if (ChildKey.Size) {
                MDNode* ChildSchema = EMPTY_MD_NODE_VALUE;
                MDNode* ExprChildSchema = EMPTY_MD_NODE_VALUE;

                {
                    MDNodePtrList Schemas = ConfigSchemasFromName(
                        Scratch.MemPool,
                        APP_STATE->CfgSchemaTable,
                        RootCfg->String
                    );

                    for (
                        MDNodePtrNode* N = Schemas.Head;
                        N && ChildSchema == EMPTY_MD_NODE_VALUE;
                        N = N->Next
                    ) {
                        ChildSchema = MDChildFromStr(N->V, ChildKey, 0);

                        if (ChildSchema != EMPTY_MD_NODE_VALUE) {
                            ExprChildSchema = MDChildFromStr(
                                N->V,
                                "expression"_s8,
                                0
                            );
                        }
                    }
                }

                Str8 ChildTypeName = ChildSchema->Head->String;
                Str8 ValueString = Cfg->Head->String;

                if (!ValueString.Size) {
                    ValueString = MDTagFromStr(
                        ChildSchema,
                        "default"_s8,
                        0
                    )->Head->String;
                }

                if (
                    !ValueString.Size &&
                    !MDNodeIsEmpty(
                        MDTagFromStr(
                            ChildSchema, 
                            "override"_s8, 
                            0
                        )
                    )
                ) {
                    for (
                        ConfigNode* Parent = RootCfg->Parent; 
                        Parent != EMPTY_CFG_NODE_VALUE; 
                        Parent = Parent->Parent
                    ) {
                        ConfigNode* ParentChildWithKey = ConfigNodeChildFromStr(
                            Parent,
                            ChildKey
                        );

                        if (ParentChildWithKey != EMPTY_CFG_NODE_VALUE) {
                            ValueString = ParentChildWithKey->Head->String;
                            break;
                        }

                        ValueString = AppDefaultSettingFromNames(
                            Parent->String,
                            ChildKey
                        );

                        if (ValueString.Size)
                            break;
                    }
                }

                if (
                    StrMatch(ChildTypeName, "path"_s8, 0) ||
                    StrMatch(ChildTypeName, "path_pt"_s8, 0) ||
                    StrMatch(ChildTypeName, "code_string"_s8, 0) ||
                    StrMatch(ChildTypeName, "expr_string"_s8, 0) ||
                    StrMatch(ChildTypeName, "string"_s8, 0)
                ) {
                    ReadData = ValueString;
                } else {
                    EvalKey ParentKey = {};

                    if (
                        ExprChildSchema != EMPTY_MD_NODE_VALUE && 
                        ChildSchema != ExprChildSchema
                    ) {
                        ParentKey = EvalKeyFromStr(
                            ConfigNodeChildFromStr(
                                RootCfg,
                                ExprChildSchema->String
                            )->Head->String
                        );
                    }

                    EvalParentKey(ParentKey) {
                        if (StrMatch(ChildTypeName, "bool"_s8, 0)) {
                            b32 Value = !!EvalValueFromStrFmt(
                                "(bool)(%S)",
                                PRINT_STR(ValueString)
                            ).U64;

                            ReadData = ArenaPushStrCpy(
                                Scratch.MemPool,
                                Str8Struct(&Value)
                            );
                        } else if (StrMatch(ChildTypeName, "u64"_s8, 0)) {
                            u64 Value = EvalValueFromStrFmt(
                                "(uint64)(%S)",
                                PRINT_STR(ValueString)
                            ).U64;

                            ReadData = ArenaPushStrCpy(
                                Scratch.MemPool,
                                Str8Struct(&Value)
                            );
                        } else if (StrMatch(ChildTypeName, "u32"_s8, 0)) {
                            u64 Value = EvalValueFromStrFmt(
                                "(uint32)(%S)",
                                PRINT_STR(ValueString)
                            ).U64;

                            ReadData = ArenaPushStrCpy(
                                Scratch.MemPool,
                                Str8Struct(&Value)
                            );
                        } else if (StrMatch(ChildTypeName, "f32"_s8, 0)) {
                            f32 Value = EvalValueFromStrFmt(
                                "(float32)(%S)",
                                PRINT_STR(ValueString)
                            ).F32;

                            ReadData = ArenaPushStrCpy(
                                Scratch.MemPool,
                                Str8Struct(&Value)
                            );
                        }
                    }
                }
            }

            if (!ChildKey.Size)
                ReadData = Cfg->Head->String;

            r1u64 LegalRange = Rng(0ULL, ReadData.Size);
            r1u64 ReadRange = Intersect(Range, LegalRange);

            if (ReadRange.Min < ReadRange.Max) {
                Result = TRUE;
                MemCpy(
                    Data, 
                    ReadData.Str + ReadRange.Min,
                    Length(ReadRange)
                );
            }
        } break;
    }

    ReleaseScratch(Scratch);

    return Result;
}

b32 
AppEvalSpaceWrite(
    EvalSpace Space, 
    IN void* Data, 
    r1u64 Range
) {
    b32 Result = FALSE;

    switch (Space.Kind) {
        default: {} break;

        case APP_EVAL_SPACE_KIND_META_CFG: {
            Result = TRUE;

            Str8 WriteString = StrRange(
                (u8*) Data,
                (u8*) Data + Length(Range)
            );
            ConfigNode* RootCfg = AppConfigFromEvalSpace(Space);
            Str8 ChildKey = EvalStrFromID(Space.U64s[1]);

            if (!ChildKey.Size) {
                ConfigNodeNewReplace(
                    APP_STATE->Config,
                    RootCfg,
                    WriteString
                );
            } else {
                if (StrMatch(ChildKey, "label"_s8, 0)) {
                    Str8 PreEditLabel = AppLabelFromConfig(RootCfg);

                    if (!StrMatch(PreEditLabel, WriteString, 0)) {
                        EvalExpr* Expr = EvalStr2ExprMapLookup(
                            GetTLS()->EVAL_IR_CTX->MacroMap,
                            PreEditLabel
                        );

                        if (Expr != EMPTY_EVAL_EXPR_VALUE) {
                            EvalStr2ExprMapPoisonIncrement(
                                GetTLS()->EVAL_IR_CTX->MacroMap,
                                PreEditLabel
                            );
                            MapInsert(
                                GetTLS()->EVAL_CACHE->MemPool,
                                GetTLS()->EVAL_IR_CTX->MacroMap,
                                WriteString,
                                Expr
                            );
                        }
                    }
                }

                if (Range.Min == Range.Max) {
                    ConfigNodeRelease(
                        APP_STATE->Config,
                        ConfigNodeChildFromStr(RootCfg, ChildKey)
                    );
                } else {
                    ConfigNode* ChildCfg = ConfigNodeChildFromStrOrAlloc(
                        APP_STATE->Config,
                        RootCfg,
                        ChildKey
                    );

                    ConfigNodeNewReplace(
                        APP_STATE->Config,
                        ChildCfg,
                        WriteString
                    );
                }
            }
        } break;
    }

    return Result;
}

b32 
AppCommitEvalValueStr(EvalEval Eval, Str8 String)
{
    b32 Result = FALSE;

    if (Eval.IRTree.Mode == EVAL_MODE_OFFSET) {
        TempArena Scratch = GetScratch(NULL, 0);
        EvalTypeKey TypeKey = EvalTypeKeyUnwrap(
            Eval.IRTree.TypeKey,
            EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
        );
        EvalTypeKind TypeKind = EvalTypeKindFromKey(TypeKey);
        EvalTypeKey DirectTypeKey = EvalTypeKeyUnwrap(
            Eval.IRTree.TypeKey,
            EVAL_TYPE_UNWRAP_FLAG_ALL
        );
        EvalTypeKind DirectTypeKind = EvalTypeKindFromKey(DirectTypeKey);
        b32 GotCommitData = FALSE;
        Str8 CommitData = {};
        b32 CommitAtPtrDst = FALSE;

        if (TypeKey != EMPTY_EVAL_TYPE_KEY_VALUE) {
            if (
                !GotCommitData && 
                Eval.Space.Kind == APP_EVAL_SPACE_KIND_META_CFG
            ) {
                GotCommitData = TRUE;
                CommitData = String;
            }

            if (
                !GotCommitData &&
                (
                    (
                        TypeKind >= EVAL_TYPE_KIND_FIRST_BASIC &&
                        TypeKind <= EVAL_TYPE_KIND_LAST_BASIC
                    ) ||
                    TypeKind == EVAL_TYPE_KIND_ENUM
                )
            ) {
                GotCommitData = TRUE;

                EvalEval SrcEval = EvalEvalFromStrFmt(
                    "(%S)(%S)",
                    PRINT_STR(EvalTypeStrFromKey(Scratch.MemPool, TypeKey)),
                    PRINT_STR(String)
                );

                CommitData = ArenaPushStrCpy(
                    Scratch.MemPool,
                    Str8Struct(&SrcEval.Value)
                );
                CommitData.Size = MIN(
                    CommitData.Size, 
                    EvalByteSizeFromKey(TypeKey)
                );
            }

            b32 IsQuoted = FALSE;

            if (String.Size && String.Str[0] == '"') {
                String = StrSkip(String, 1);
                IsQuoted = TRUE;
            }

            if (
                !GotCommitData &&
                (
                    (
                        (IsQuoted && TypeKind == EVAL_TYPE_KIND_PTR) ||
                        TypeKind == EVAL_TYPE_KIND_ARRAY
                    ) &&
                    (
                        DirectTypeKind == EVAL_TYPE_KIND_CHAR8 ||
                        DirectTypeKind == EVAL_TYPE_KIND_CHAR16 ||
                        DirectTypeKind == EVAL_TYPE_KIND_CHAR32 ||
                        DirectTypeKind == EVAL_TYPE_KIND_UCHAR8 ||
                        DirectTypeKind == EVAL_TYPE_KIND_UCHAR16 ||
                        DirectTypeKind == EVAL_TYPE_KIND_UCHAR32 ||
                        EvalTypeKindIsInteger(DirectTypeKind)
                    )
                )
            ) {
                GotCommitData = TRUE;

                if (String.Size && String.Str[String.Size - 1] == '"') {
                    String = StrChop(String, 1);
                }

                if (IsQuoted) {
                    CommitData = RawFromEscapedStr8(
                        Scratch.MemPool,
                        String
                    );
                } else {
                    CommitData = ArenaPushStrCpy(Scratch.MemPool, String);
                }

                ++CommitData.Size;

                if (TypeKind == EVAL_TYPE_KIND_PTR)
                    CommitAtPtrDst = TRUE;

                switch (DirectTypeKind) {
                    default: {}  break;

                    case EVAL_TYPE_KIND_I16:
                    case EVAL_TYPE_KIND_U16:
                    case EVAL_TYPE_KIND_CHAR16:
                    case EVAL_TYPE_KIND_UCHAR16: {
                        Str16 Data16 = Str16FromStr8(
                            Scratch.MemPool,
                            CommitData
                        );

                        CommitData = Str(
                            (u8*) Data16.Str,
                            Data16.Size * sizeof(u16)
                        );
                    } break;

                    case EVAL_TYPE_KIND_I32:
                    case EVAL_TYPE_KIND_U32:
                    case EVAL_TYPE_KIND_CHAR32:
                    case EVAL_TYPE_KIND_UCHAR32: {
                        Str32 Data32 = Str32FromStr8(
                            Scratch.MemPool,
                            CommitData
                        );

                        CommitData = Str(
                            (u8*) Data32.Str,
                            Data32.Size * sizeof(u32)
                        );
                    } break;
                }
            }

            if (!GotCommitData && TypeKind == EVAL_TYPE_KIND_PTR) {
                EvalEval SrcEval = EvalEvalFromStr(String);
                EvalEval SrcEvalValue = EvalValueEvalFromEval(SrcEval);
                EvalTypeKind SrcEvalValueTypeKind = EvalTypeKindFromKey(
                    SrcEvalValue.IRTree.TypeKey
                );

                if (
                    (
                        EvalTypeKindIsPtrOrRef(SrcEvalValueTypeKind) || 
                        EvalTypeKindIsInteger(SrcEvalValueTypeKind)
                    ) &&
                    SrcEvalValue.IRTree.Mode == EVAL_MODE_VALUE
                ) {
                    GotCommitData = TRUE;
                    CommitData = ArenaPushStrCpy(
                        Scratch.MemPool,
                        Str8Struct(&SrcEval.Value)
                    );
                    CommitData.Size = MAX(
                        CommitData.Size, 
                        EvalByteSizeFromKey(SrcEval.IRTree.TypeKey)
                    );
                    CommitData.Size = MAX(
                        CommitData.Size, 
                        EvalByteSizeFromKey(TypeKey)
                    );
                }
            }
        }

        u64 DstOffset = Eval.Value.U64;

        if (GotCommitData && CommitAtPtrDst) {
            EvalEval DstValueEval = EvalValueEvalFromEval(Eval);

            DstOffset = DstValueEval.Value.U64;
        }

        if (GotCommitData) {
            Result = EvalSpaceWrite(
                Eval.Space,
                CommitData.Str,
                Rng(DstOffset, DstOffset + CommitData.Size)
            );
        }

        ReleaseScratch(Scratch);
    }

    return Result;
}

CKey 
AppKeyFromEvalSpaceRange(
    EvalSpace Space, 
    r1u64 Range, 
    b32 IsZeroTerminated
) {}

r1u64 
AppWholeRangeFromEvalSpace(EvalSpace Space)
{}

Str8 
AppFilePathFromEval(Arena* MemPool, EvalEval Eval)
{
    Str8 Result = {};

    switch (Eval.Space.Kind) {
        default: {} break;

        case EVAL_SPACE_KIND_FILE: {
            Result = ArenaPushStrCpy(
                MemPool,
                EvalStrFromID(Eval.Space.U64)
            );
        } break;

        case EVAL_SPACE_KIND_FILESYSTEM: {
            Result = ArenaPushStrCpy(
                MemPool,
                EvalStrFromID(Eval.Value.U64)
            );
        } break;
    }

    return Result;
}

Str8 
AppFilePathFromEvalStr(Arena* MemPool, Str8 String)
{
    Str8 Result = {};
    TempArena Scratch = GetScratch(&MemPool, 1);
    EvalEval Eval = EvalEvalFromStr(String);

    Result = AppFilePathFromEval(MemPool, Eval);
    ReleaseScratch(Scratch);

    return Result;
}

Str8 
AppQueryFromEvalStr(Arena* MemPool, Str8 String)
{
    Str8 Result = {};
    TempArena Scratch = GetScratch(&MemPool, 1);
    EvalExpr* Expr = EvalParseFromStr(String).Expr;

    if (
        Expr->Kind == EVAL_EXPR_KIND_LEAF_IDENT &&
        StrMatch(Expr->Qualifier, "query"_s8, 0)
    ) {
        Result = Expr->String;
    }

    ReleaseScratch(Scratch);

    return Result;
}

r1u64 
AppSpaceRangeFromEval(EvalEval Eval)
{
    r1u64 Result = EvalRangeFromEval(Eval);
    u64 SizeSetting = AppViewSettingValueFromName("size"_s8).U64;

    if (SizeSetting)
        Result.Max = Result.Min + SizeSetting;

    return Result;
}

EvalValue 
AppViewSettingValueFromName(Str8 String)
{
    Str8 Expr = AppViewSettingFromNameStr8(String);
    EvalEval Eval = EvalEvalFromStr(Expr);
    EvalValue Result = EvalValueEvalFromEval(Eval).Value;

    return Result;
}

Str8 
AppFilePathFromConfig(ConfigNode* Config)
{
    ConfigNode* Root = ConfigNodeChildFromStr(Config, "path"_s8);
    Str8 Result = Root->Head->String;

    return Result;
}

Str8 
AppNameFromConfig(ConfigNode* Config)
{
    ConfigNode* NameRoot = ConfigNodeChildFromStr(Config, "name"_s8);
    Str8 Result = NameRoot->Head->String;

    return Result;
}

AppRegisters*
AppGetHoverRegisters(void)
{
    return APP_STATE->HoverRegisters;
}

AppViewUIRule*
AppViewUIRuleFromStr(Str8 String)
{
    AppViewUIRule* Result = EMPTY_APP_VIEW_UI_RULE_VALUE;
    AppViewUIRuleMap* Map = APP_STATE->ViewUIRuleMap;
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;

    for (
        AppViewUIRuleNode* Node = Map->Slots[SlotIndex].Head;
        Node;
        Node = Node->Next
    ) {
        if (StrMatch(Node->V.Name, String, 0)) {
            Result = &Node->V;
            break;
        }
    }

    return Result;
}

FancyStrList
AppTitleFStrFromCodeName(Arena* MemPool, Str8 CodeName)
{
    FancyStrList Result = {};
    AppVocabInfo* Info = AppVocabInfoFromCodeName(CodeName);
    FancyStrParams Params = {
        UIColourFromName("text"_s8)
    };
    FancyStrParams Overrides = {};

    Overrides.Colour = UIColourFromName("text"_s8);

    if (Info->IconKind != APP_ICON_KIND_NULL) {
        UITag("weak"_s8) {
            ListPush(
                MemPool,
                &Result,
                &Params,
                &Overrides,
                APP_ICON_KIND_TEXT_TABLE[Info->IconKind]
            );
            ListPush(
                MemPool,
                &Result,
                &Params,
                NULL,
                "  "_s8
            );
        }

        if (Info->DisplayName.Size) {
            ListPush(MemPool, &Result, &Params, NULL, Info->DisplayName);
        } else {
            Overrides.Colour = Vec(0.0f, 0.0f, 0.0f, 0.0f);
            ListPush(
                MemPool,
                &Result,
                &Params,
                &Overrides,
                CodeName
            );
        }
    }

    return Result;
}

FancyStrList
AppTitleFStrFromFilePath(Arena* MemPool, Str8 FilePath, b32 IncludeFolder)
{
    FancyStrList Result = {};
    Str8 FileName = StrSkipLastSlash(FilePath);
    FileProperties Props = SystemGetFileProperties(FilePath);
    AppIconKind IconKind = APP_ICON_KIND_FILE;

    if (Props.Kind & SYS_FILE_IS_DIR)
        IconKind = APP_ICON_KIND_FOLDER;

    if (
        !FilePath.Size ||
        StrMatch(FilePath, "/"_s8, STR_MATCH_SKIP_SLASH)
    ) {
        IconKind = APP_ICON_KIND_INFO;
        FileName = "File System"_s8;
    }

    FancyStrParams Params = {
        UIColourFromName("text"_s8)
    };
    FancyStrParams Overrides = {};

    Overrides.Colour = UIColourFromName("text"_s8);

    UITag("weak"_s8) {
        ListPush(
            MemPool,
            &Result,
            &Params,
            &Overrides,
            APP_ICON_KIND_TEXT_TABLE[IconKind]
        );
    }

    ListPush(
        MemPool,
        &Result,
        &Params,
        NULL,
        "  "_s8
    );
    ListPush(
        MemPool,
        &Result,
        &Params,
        NULL,
        FileName
    );

    if (IncludeFolder) {
        ListPush(
            MemPool,
            &Result,
            &Params,
            NULL,
            "  "_s8
        );

        UITag("weak"_s8) {
            Overrides = {};
            Overrides.Colour = UIColourFromName("text"_s8);
            ListPush(
                MemPool,
                &Result,
                &Params,
                &Overrides,
                StrChopLastSlash(FilePath)
            );
        }
    }

    return Result;
}

b32 
AppIsProjectFiltered(ConfigNode* Config)
{
    ConfigNode* Session = ConfigNodeChildFromStr(Config, "session"_s8);
    b32 Result = (
        Session != EMPTY_CFG_NODE_VALUE &&
        Session->Head->String.Size &&
        !PathMatchNormalised(
            APP_STATE->SessionPath,
            Session->Head->String
        )
    );

    return Result;
}

AppViewUIRuleMap*
AppViewUIRuleMapAlloc(Arena* MemPool, u64 SlotsCount)
{
    AppViewUIRuleMap* Result = ArenaPushArrayZero(
        MemPool,
        AppViewUIRuleMap,
        1
    );

    Result->SlotsCount = SlotsCount;
    Result->Slots = ArenaPushArrayZero(
        MemPool,
        AppViewUIRuleSlot,
        Result->SlotsCount
    );

    return Result;
}

void
MapInsert(
    Arena* MemPool, 
    AppViewUIRuleMap* Map, 
    Str8 String, 
    AppViewUIFuncType* ViewUIFunc
) {
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;
    AppViewUIRuleNode* Node = ArenaPushArrayZero(
        MemPool,
        AppViewUIRuleNode,
        1
    );

    Node->V.Name = ArenaPushStrCpy(MemPool, String);
    Node->V.ViewUIFunc = ViewUIFunc;
    SLL_QUEUE_PUSH(
        Map->Slots[SlotIndex].Head,
        Map->Slots[SlotIndex].Tail,
        Node
    );
}

Str8 
AppExpressionFromConfig(ConfigNode* Cfg)
{
    ConfigNode* ExpressionRoot = ConfigNodeChildFromStr(Cfg, "expression"_s8);
    Str8 Result = ExpressionRoot->Head->String;

    return Result;
}

Str8
AppLabelFromConfig(ConfigNode *Config)
{
    ConfigNode* LabelRoot = ConfigNodeChildFromStr(Config, "label"_s8);
    Str8 Result = LabelRoot->Head->String;

    return Result;
}

void 
CopyRegisters(Arena* MemPool, AppRegisters* Dst, AppRegisters* Src)
{
    MemCpy(Dst, Src, sizeof(AppRegisters));
    Dst->ConfigList = ListCpy(MemPool, &Src->ConfigList);
    Dst->FilePath = ArenaPushStrCpy(MemPool, Src->FilePath);
    Dst->Expression = ArenaPushStrCpy(MemPool, Src->Expression);
    Dst->String = ArenaPushStrCpy(MemPool, Src->String);
    Dst->CommandName = ArenaPushStrCpy(MemPool, Src->CommandName);

    if (!Dst->ConfigList.Count && Dst->Config)
        ListPush(MemPool, &Dst->ConfigList, Dst->Config);
}

AppRegisters* 
CopyRegisters(Arena* MemPool, AppRegisters* Regs)
{
    AppRegisters* Result = ArenaPushArrayZero(MemPool, AppRegisters, 1);

    CopyRegisters(MemPool, Result, Regs);

    return Result;
}

b32
AppDragIsActive(void)
{
    return (
        APP_STATE->DragDropState == APP_DRAG_DROP_STATE_DRAGGING ||
        APP_STATE->DragDropState == APP_DRAG_DROP_STATE_DROPPING
    );
}

void
AppDragBegin(AppRegistersSlot Slot)
{
    if (!AppDragIsActive()) {
        ArenaClear(APP_STATE->DragDropMemPool);
        APP_STATE->DragDropRegisters = CopyRegisters(
            APP_STATE->DragDropMemPool,
            Registers()
        );
        APP_STATE->DragDropRegistersSlot = Slot;
        APP_STATE->DragDropState = APP_DRAG_DROP_STATE_DRAGGING;
    }
}

b32
AppDragDrop(void)
{
    b32 Result = FALSE;

    if (APP_STATE->DragDropState == APP_DRAG_DROP_STATE_DROPPING) {
        Result = TRUE;
        APP_STATE->DragDropState = APP_DRAG_DROP_STATE_NULL;
    }

    return Result;
}

void
AppDragKill(void)
{
    APP_STATE->DragDropState = APP_DRAG_DROP_STATE_NULL;
}

void 
ListPush(Arena* MemPool, AppCommandList* List, Str8 Name, AppRegisters* Regs)
{
    AppCommandNode* Node = ArenaPushArrayZero(MemPool, AppCommandNode, 1);

    Node->Command.Name = ArenaPushStrCpy(MemPool, Name);
    Node->Command.Registers = CopyRegisters(MemPool, Regs);
    DLL_PUSH_BACK(List->Head, List->Tail, Node);
    ++List->Count;
}

void 
ListPush(Arena* MemPool, AppQueryResultList* List, AppQueryResult Entry)
{
    AppQueryResultNode* Node = ArenaPushArrayZero(
        MemPool, 
        AppQueryResultNode, 
        1
    );

    Node->V = Entry;
    DLL_PUSH_BACK(List->Head, List->Tail, Node);
    ++List->Count;
}

void
AppSetHoverEval(v2i32 Position, Str8 String)
{
    if (
        APP_STATE->HoverEvalLastTimeUSecs < APP_STATE->TimeUSecs &&
        UIActiveKey(UI_MOUSE_BTN_KIND_LEFT) == EMPTY_UI_KEY_VALUE &&
        UIActiveKey(UI_MOUSE_BTN_KIND_MIDDLE) == EMPTY_UI_KEY_VALUE &&
        UIActiveKey(UI_MOUSE_BTN_KIND_RIGHT) == EMPTY_UI_KEY_VALUE
    ) {
        b32 IsNewString = !StrMatch(APP_STATE->HoverEvalString, String, 0);

        if (IsNewString) {
            APP_STATE->HoverEvalFirstTimeUSecs = APP_STATE->TimeUSecs;
            APP_STATE->HoverEvalLastTimeUSecs = APP_STATE->TimeUSecs;
            ArenaClear(APP_STATE->HoverEvalMemPool);
            APP_STATE->HoverEvalString = ArenaPushStrCpy(
                APP_STATE->HoverEvalMemPool, 
                String
            );
            APP_STATE->HoverEvalFocused = FALSE;
        }

        APP_STATE->HoverEvalSpawnPosition = Position;
        APP_STATE->HoverEvalLastTimeUSecs = APP_STATE->TimeUSecs;
    }
}

void
__AppSetAutoCompleteRegisters(Str8 Expr, AppRegisters *Regs)
{
    if (
        APP_STATE->AutoCompletePrevFrameIndex < APP_STATE->FrameIndex &&
        AppSettingFromNameB32("autocompletion_lister"_s8)
    ) {
        AppAutoCompleteCursorInfo CursorInfo = {};

        ArenaClear(APP_STATE->AutoCompleteMemPool);
        CursorInfo.ListExpr = ArenaPushStrCpy(
            APP_STATE->AutoCompleteMemPool,
            Expr
        );
        CursorInfo.Filter = ArenaPushStrCpy(
            APP_STATE->AutoCompleteMemPool,
            Regs->String
        );
        CursorInfo.ReplacedRanged = Rng(
            (u64) 0ULL,
            Regs->String.Size
        );
        APP_STATE->AutoCompletePrevFrameIndex = APP_STATE->FrameIndex;
        APP_STATE->AutoCompleteRegs = CopyRegisters(
            APP_STATE->AutoCompleteMemPool,
            Regs
        );
        APP_STATE->AutoCompleteCursorInfo = CursorInfo;
    }
}

Str8 
AppSettingFromNameStr8(Str8 Name)
{
    Str8 Result = {};

    if (Name.Size) {
        ConfigNode* Cfg = EMPTY_CFG_NODE_VALUE;

        Cfg = ConfigNodeFromID(Registers()->View);

        if (Cfg != EMPTY_CFG_NODE_VALUE) {
            for (
                ConfigNode* C = Cfg; 
                C != EMPTY_CFG_NODE_VALUE; 
                C = C->Parent
            ) {
                ConfigNode* Setting = ConfigNodeChildFromStr(C, Name);

                if (Setting != EMPTY_CFG_NODE_VALUE) {
                    Result = Setting->Head->String;
                    goto END;
                }
            }
        }

        Cfg = ConfigNodeFromID(Registers()->Panel);

        if (Cfg != EMPTY_CFG_NODE_VALUE) {
            for (
                ConfigNode* C = Cfg; 
                C != EMPTY_CFG_NODE_VALUE; 
                C = C->Parent
            ) {
                ConfigNode* Setting = ConfigNodeChildFromStr(C, Name);

                if (Setting != EMPTY_CFG_NODE_VALUE) {
                    Result = Setting->Head->String;
                    goto END;
                }
            }
        }

        {
            ConfigNode* Session = ConfigNodeChildFromStr(
                ConfigNodeRoot(), 
                "session"_s8
            );
            ConfigNode* Settings = ConfigNodeChildFromStr(
                Session, 
                "settings"_s8
            );
            ConfigNode* Setting = ConfigNodeChildFromStr(Settings, Name);

            if (Setting != EMPTY_CFG_NODE_VALUE)
                Result = Setting->Head->String;
        }
    }

END:
    return Result;
}

b32 
AppSettingFromNameB32(Str8 Name)
{
    Str8 Value = AppSettingFromNameStr8(Name);
    b32 Result = (
        Value.Size &&
        (
            U64FromStr(Value) != 0 ||
            StrMatch(Value, "true"_s8, STR_MATCH_ALL_CASES)
        )
    );

    return Result;
}

u64 
AppSettingFromNameU64(Str8 Name)
{
    Str8 Value = AppSettingFromNameStr8(Name);
    u64 Result = Value.Size 
        ? U64FromStr(Value) 
        : 0;

    return Result;
}

f32 
AppSettingFromNameF32(Str8 Name)
{
    Str8 Value = AppSettingFromNameStr8(Name);
    f32 Result = Value.Size 
        ? (f32) F64FromStr(Value) 
        : 0.0f;

    return Result;
}

AppViewState* 
AppViewStateFromConfig(ConfigNode* Config)
{
    AppViewState* Result = EMPTY_APP_VIEW_STATE_VALUE;
    ConfigID ID = Config->ID;

    if (
        ID &&
        ID == APP_STATE->ViewStateLastAccessedID &&
        ID == APP_STATE->ViewStateLastAccessed->CfgID
    ) {
        Result = APP_STATE->ViewStateLastAccessed;
    } else {
        u64 IDHash = Hash(Str8Struct(&ID));
        u64 SlotIndex = IDHash % APP_STATE->ViewStateSlotsCount;
        AppViewStateSlot* Slot = &APP_STATE->ViewStateSlots[SlotIndex];

        for (AppViewState* V = Slot->Head; V; V = V->HashNext) {
            if (V->CfgID == ID) {
                Result = V;
                break;
            }
        }
    }

    if (Result == EMPTY_APP_VIEW_STATE_VALUE) {
        Result = APP_STATE->FreeViewState;

        if (Result)
            SLL_STACK_POP_EX(APP_STATE->FreeViewState, HashNext);
        else
            Result = ArenaPushArrayZero(APP_STATE->MemPool, AppViewState, 1);

        MemCpy(Result, EMPTY_APP_VIEW_STATE_VALUE, sizeof(*Result));

        u64 IDHash = Hash(Str8Struct(&ID));
        u64 SlotIndex = IDHash % APP_STATE->ViewStateSlotsCount;
        AppViewStateSlot* Slot = &APP_STATE->ViewStateSlots[SlotIndex];

        DLL_PUSH_BACK_EX(
            NULL, 
            Slot->Head, 
            Slot->Tail, 
            Result, 
            HashNext, 
            HashPrev
        );
        Result->CfgID = ID;
        Result->MemPool = ArenaAlloc();
        Result->MemPoolResetPosition = ArenaGetPosition(Result->MemPool);
    }

    if (Result != EMPTY_APP_VIEW_STATE_VALUE)
        Result->LastFrameIndexTouched = APP_STATE->FrameIndex;

    APP_STATE->ViewStateLastAccessed = Result;
    APP_STATE->ViewStateLastAccessedID = ID;

    return Result;
}

void 
AppSetHoverRegisters(AppRegistersSlot Slot)
{
    APP_STATE->NextHoverRegisters = CopyRegisters(
        AppFrameMemPool(), 
        Registers()
    );
    APP_STATE->NextHoverRegistersSlot = Slot;
}

v4f32 
AppHSVAFromConfig(ConfigNode* Config)
{
    v4f32 Result = {};
    ConfigNode* HSVARoot = ConfigNodeChildFromStr(Config, "hsva"_s8);
    ConfigNode* H = HSVARoot->Head;
    ConfigNode* S = H->Next;
    ConfigNode* V = S->Next;
    ConfigNode* A = V->Next;

    Result.X = (f32) F64FromStr(H->String);
    Result.Y = (f32) F64FromStr(S->String);
    Result.Z = (f32) F64FromStr(V->String);
    Result.W = (f32) F64FromStr(A->String);

    return Result;
}

v4f32 
AppColourFromConfig(ConfigNode* Config)
{
    v4f32 HSVA = AppHSVAFromConfig(Config);
    v4f32 Result = LinearFromSRGBA(RGBAFromHSVA(HSVA));

    return Result;
}

b32 
AppIsDisabled(ConfigNode* Config)
{
    TempArena Scratch = GetScratch(NULL, 0);
    MDNode* ChildSchema = EMPTY_MD_NODE_VALUE;
    MDNodePtrList Schemas = ConfigSchemasFromName(
        Scratch.MemPool,
        APP_STATE->CfgSchemaTable,
        Config->String
    );

    for (
        MDNodePtrNode* N = Schemas.Head;
        N && ChildSchema == EMPTY_MD_NODE_VALUE;
        N = N->Next
    ) {
        ChildSchema = MDChildFromStr(N->V, "enabled"_s8, 0);
    }

    MDNode* DefaultTag = MDTagFromStr(ChildSchema, "default"_s8, 0);
    Str8 ValueString = ConfigNodeChildFromStr(
        Config, 
        "enabled"_s8
    )->Head->String;

    if (!ValueString.Size)
        ValueString = DefaultTag->Head->String;

    b32 IsEnabled = !!U64FromStr(ValueString);
    b32 IsDisabled = !IsEnabled;

    if (!ValueString.Size)
        IsDisabled = FALSE;

    ReleaseScratch(Scratch);

    return IsDisabled;
}

Str8 
LabelFromConfig(ConfigNode* Config)
{
    ConfigNode* LabelRoot = ConfigNodeChildFromStr(Config, "label"_s8);
    Str8 Result = LabelRoot->Head->String;

    return Result;
}

Str8 
ExpressionFromConfig(ConfigNode* Config)
{
    ConfigNode* ExpressionRoot = ConfigNodeChildFromStr(
        Config, 
        "expression"_s8
    );
    Str8 Result = ExpressionRoot->Head->String;

    return Result;
}

AppVocabInfo*
AppVocabInfoFromCodeName(Str8 CodeName)
{
    AppVocabInfo* Result = EMPTY_APP_VOCAB_INFO_VALUE;

    if (CodeName.Size) {
        u64 CodeHash = Hash(CodeName);
        u64 SlotIndex = CodeHash % APP_STATE->VocabInfoMap.SingleSlotsCount;

        for (
            AppVocabInfoMapNode* N = APP_STATE->VocabInfoMap.SingleSlots[SlotIndex].Head;
            N;
            N = N->SingleNext
        ) {
            if (StrMatch(N->V.CodeName, CodeName, 0)) {
                Result = &N->V;
                break;
            }
        }
    }

    return Result;
}

AppVocabInfo*
AppVocabInfoFromCodeNamePlural(Str8 CodeName)
{
    AppVocabInfo* Result = EMPTY_APP_VOCAB_INFO_VALUE;

    if (CodeName.Size) {
        u64 CodeHash = Hash(CodeName);
        u64 SlotIndex = CodeHash % APP_STATE->VocabInfoMap.PluralSlotsCount;

        for (
            AppVocabInfoMapNode* N = APP_STATE->VocabInfoMap.PluralSlots[SlotIndex].Head;
            N;
            N = N->SingleNext
        ) {
            if (StrMatch(N->V.CodeName, CodeName, 0)) {
                Result = &N->V;
                break;
            }
        }
    }

    return Result;
}

Str8 
PathFromConfig(ConfigNode* Config)
{
    ConfigNode* PathRoot = ConfigNodeChildFromStr(Config, "path"_s8);
    Str8 Result = PathRoot->Head->String;

    return Result;
}

MDNodePtrList 
AppSchemasFromName(Str8 Name)
{
    MDNodePtrList Result = {};

    for (
        u64 Index = 0; 
        Index < ARRAY_COUNT(APP_NAME_SCHEMA_INFO_TABLE); 
        ++Index
    ) {
        if (StrMatch(Name, APP_NAME_SCHEMA_INFO_TABLE[Index].Name, 0)) {
            Result = APP_STATE->Schemas[Index];
            break;
        }
    }

    return Result;
}

Str8 
AppDefaultSettingFromNames(Str8 SchemaName, Str8 SettingName)
{
    Str8 Result = {};
    TempArena Scratch = GetScratch(NULL, 0);
    MDNode* SettingSchema = EMPTY_MD_NODE_VALUE;
    MDNodePtrList Schemas = ConfigSchemasFromName(
        Scratch.MemPool,
        APP_STATE->CfgSchemaTable,
        SchemaName
    );

    for (
        MDNodePtrNode* N = Schemas.Head;
        N && SettingSchema == EMPTY_MD_NODE_VALUE;
        N = N->Next
    ) {
        SettingSchema = MDChildFromStr(N->V, SettingName, 0);
    }

    if (SettingSchema != EMPTY_MD_NODE_VALUE) {
        MDNode* DefaultTag = MDTagFromStr(SettingSchema, "default"_s8, 0);

        if (DefaultTag != EMPTY_MD_NODE_VALUE)
            Result = DefaultTag->Head->String;
    }

    ReleaseScratch(Scratch);

    return Result;
}

Arena* 
AppViewMemPool(void)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);

    return VS->MemPool;
}

UIScrollPoint2D 
AppViewScrollPosition(void)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);

    return VS->ScrollPosition;
}

Str8 
AppViewQueryCommand(void)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    ConfigNode* Query = ConfigNodeChildFromStr(View, "query"_s8);
    ConfigNode* Cmd = ConfigNodeChildFromStr(Query, "cmd"_s8);
    Str8 String = Cmd->Head->String;

    return String;
}

Str8 
AppViewQueryInput(void)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    ConfigNode* Query = ConfigNodeChildFromStr(View, "query"_s8);
    ConfigNode* Input = ConfigNodeChildFromStr(Query, "input"_s8);
    Str8 String = Input->Head->String;

    return String;
}

Str8 
AppViewFilePath(void)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    ConfigNode* File = ConfigNodeChildFromStr(View, "file"_s8);
    Str8 String = File->Head->String;

    return String;
}

Str8 
AppViewSettingFromNameStr8(Str8 Name)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    Str8 Result = ConfigNodeChildFromStr(View, Name)->Head->String;

    if (!Result.Size)
        Result = AppDefaultSettingFromNames(View->String, Name);

    return Result;
}

b32 
AppViewSettingFromNameB32(Str8 Name)
{
    Str8 Value = AppViewSettingFromNameStr8(Name);
    b32 Result = (
        Value.Size &&
        (
            U64FromStr(Value) != 0 ||
            StrMatch(Value, "true"_s8, STR_MATCH_ALL_CASES)
        )
    );

    return Result;
}

u64 
AppViewSettingFromNameU64(Str8 Name)
{
    Str8 Value = AppViewSettingFromNameStr8(Name);
    u64 Result = Value.Size 
        ? U64FromStr(Value) 
        : 0;

    return Result;
}

f32 
AppViewSettingFromNameF32(Str8 Name)
{
    Str8 Value = AppViewSettingFromNameStr8(Name);
    f32 Result = Value.Size 
        ? (f32) F64FromStr(Value) 
        : 0.0f;

    return Result;
}

void* 
AppViewStateFromSize(u64 Size)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);

    if (!VS->UserData)
        VS->UserData = ArenaPushArrayZero(VS->MemPool, u8, Size);

    return VS->UserData;
}

Arena* 
AppPushViewMemPool(void)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);
    AppArenaExt* Ext = ArenaPushArrayZero(VS->MemPool, AppArenaExt, 1);

    Ext->MemPool = ArenaAlloc();
    SLL_QUEUE_PUSH(VS->HeadArenaExt, VS->TailArenaExt, Ext);

    return Ext->MemPool;
}

void
AppStoreViewExprStr(Str8 String)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    ConfigNode* Expr = ConfigNodeChildFromStrOrAlloc(
        APP_STATE->Config,
        View,
        "expression"_s8
    );

    ConfigNodeNewReplace(APP_STATE->Config, Expr, String);
}

void 
AppStoreViewLoadingInfo(b32 IsLoading, u64 Progress, u64 ProgressTarget)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);
    b32 LoadingStateIsNew = (
        IsLoading &&
        VS->LoadingTTarget != (f32) !!IsLoading
    );

    VS->LoadingTTarget = (f32) !!IsLoading;
    VS->LoadingProgressV = Progress;
    VS->LoadingProgressVTarget = ProgressTarget;

    if (
        LoadingStateIsNew ||
        VS->LastFrameIndexBuilt + 1 < APP_STATE->FrameIndex
    ) {
        VS->LoadingT = VS->LoadingTTarget;
    }
}

void 
AppStoreViewScrollPosition(UIScrollPoint2D Position)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);

    VS->ScrollPosition = Position;
}

void 
AppStoreViewParameter(Str8 Key, Str8 Value)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    ConfigNode* Child = ConfigNodeChildFromStrOrAlloc(
        APP_STATE->Config,
        View,
        Key
    );

    ConfigNodeNewReplace(APP_STATE->Config, Child, Value);
}

void 
AppStoreViewParameter(Str8 Key, char* Fmt, ...)
{
    TempArena Scratch = GetScratch(NULL, 0);
    va_list Args;

    va_start(Args, Fmt);

    Str8 String = ArenaPushStrFmtV(
        Scratch.MemPool,
        Fmt,
        Args
    );

    AppStoreViewParameter(Key, String);
    va_end(Args);
    ReleaseScratch(Scratch);
}

Str8
AppPushConsoleTitle(Arena *MemPool)
{
    Str8 Result = ArenaPushStrFmt(
        MemPool,
        "%S - %s",
        PRINT_STR(StrSkipLastSlash(APP_STATE->SessionPath)),
        NC_DATA_DIR
    );

    return Result;
}

void 
UILoadingOverlay(r2f32 Rect, f32 LoadingT, u64 ProgressV, u64 ProgressVTarget)
{
    if (LoadingT >= 0.001f) {
        UIFocus(UI_FOCUS_KIND_OFF) {
            v2f32 RectLength = Length(Rect);
            f32 Width = MIN(30.0f, RectLength.X);
            r2f32 IndicatorRect = Rng(
                FloorF32((RectLength.X - Width) / 2.0f),
                FloorF32(RectLength.Y / 2.0f),
                FloorF32((RectLength.X - Width) / 2.0f) + Width,
                FloorF32(RectLength.Y / 2.0f) + 1.0f
            );
            UIBox* Box = EMPTY_UI_BOX_VALUE;

            UITag("pop"_s8) {
                UIRect(IndicatorRect) {
                    UISetNextChildLayoutAxis(AXIS_2D_X);
                    Box = UIBuildBoxFromStr(
                        (
                            UI_BOX_KIND_DRAW_BACKGROUND |
                            UI_BOX_KIND_FLOATING |
                            UI_BOX_KIND_CLICKABLE
                        ),
                        "###loading_overlay"_s8
                    );
                }

                UIParent(Box) {
                    UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                        UIProgressSpinner(1.0f, "###loading_spinner"_s8);

                        if (ProgressVTarget && ProgressV != ProgressVTarget) {
                            UISetNextPreferredWidth(UI_PERCENT(1.0f, 0.0f));
                            UIProgressBar(
                                (f32) ((f64) ProgressV / (f64) ProgressVTarget),
                                "###loading_progress"_s8
                            );
                        } else {
                            UILabel("Loading"_s8);
                        }
                    }
                }
            }

            UISignalFromBox(Box);
        }
    }
}

void 
AppRequestFrame(void)
{
    APP_STATE->NumFramesRequested = 4;
}

Arena* 
AppFrameMemPool(void)
{
    return APP_STATE->FrameMemPools[APP_STATE->FrameIndex % ARRAY_COUNT(APP_STATE->FrameMemPools)];
}

void 
AppRegistersFillSlotFromStr(
    AppRegistersSlot Slot, 
    Str8 QueryExpr, 
    Str8 String
) {
    UNUSED(QueryExpr);

    switch (Slot) {
        default:
        case APP_REGISTERS_SLOT_STRING: 
        case APP_REGISTERS_SLOT_FILE_PATH: {
            Str8TextPointPair Pair = Str8TextPointPairFromStr(String);

            Registers()->String = ArenaPushStrCpy(
                AppFrameMemPool(),
                String
            );

            if (Pair.Point.Line != 0) {
                Registers()->FilePath = ArenaPushStrCpy(
                    AppFrameMemPool(),
                    Pair.String
                );
                Registers()->Cursor = Pair.Point;
            }
        } break;

        case APP_REGISTERS_SLOT_COMMAND_NAME: {
            Registers()->CommandName = ArenaPushStrCpy(
                AppFrameMemPool(), 
                String
            );
        } break;

        case APP_REGISTERS_SLOT_CONFIG:
        case APP_REGISTERS_SLOT_WINDOW:
        case APP_REGISTERS_SLOT_PANEL:
        case APP_REGISTERS_SLOT_TAB:
        case APP_REGISTERS_SLOT_VIEW:
        case APP_REGISTERS_SLOT_PREV_TAB:
        case APP_REGISTERS_SLOT_DST_PANEL: {
            b32 IsOkay = FALSE;

            if (!IsOkay && StrMatch(StrPrefix(String, 1), "$"_s8, 0)) {
                Str8 NumericPart = StrSkip(String, 1);
                ConfigID ID = U64FromStr(NumericPart, 16);

                Registers()->Config = ID;
                IsOkay = TRUE;
            }
        } break;

        case APP_REGISTERS_SLOT_CURSOR: {
            u64 Line = 0;
            u64 Column = 0;
            u64 ColonIndex = 0;

            if (StrFindSubStr(String, ":"_s8, 0, 0) != String.Size) {
                Line = U64FromStr(StrPrefix(String, ColonIndex));
                Column = U64FromStr(StrSkip(String, ColonIndex + 1));
            } else {
                Line = U64FromStr(String);
                Column = 1;
            }

            if (Line) {
                Registers()->Cursor = TxtPt(Line, MAX(1, Column));
                Registers()->Mark = Registers()->Cursor;
            }
        } break;
    }
}

AppCommandKind 
AppCommandKindFromStr(Str8 String)
{
    AppCommandKind Result = APP_COMMAND_KIND_NULL;

    for (u64 I = 0; I < ARRAY_COUNT(APP_COMMAND_KIND_INFO_TABLE); ++I) {
        if (StrMatch(String, APP_COMMAND_KIND_INFO_TABLE[I].String, 0)) {
            Result = (AppCommandKind) I;
            break;
        }
    }

    if (Result == APP_COMMAND_KIND_NULL) {
        LogError(
            "'%S' [%llu] is not a valid command", 
            PRINT_STR(String), 
            String.Size
        );
    }

    return Result;
}

AppCommandKindInfo* 
AppCommandKindInfoFromStr(Str8 String)
{
    AppCommandKindInfo* Result = EMPTY_APP_COMMAND_KIND_INFO_VALUE;

    if (String.Size && String.Str) {
        AppCommandKind Kind = AppCommandKindFromStr(String);

        if (Kind != APP_COMMAND_KIND_NULL)
            Result = &APP_COMMAND_KIND_INFO_TABLE[Kind];
    }

    return Result;
}

void 
AppPushCmd(Str8 Name, AppRegisters* Regs)
{
    ListPush(
        APP_STATE->CommandMemPools[0], 
        &APP_STATE->Commands[0], 
        Name, 
        Regs
    );
}

b32 
AppNextCommand(AppCommand** Command)
{
    u64 Slot = APP_STATE->CommandsGeneration % ARRAY_COUNT(APP_STATE->Commands);
    AppCommandNode* StartNode = APP_STATE->Commands[Slot].Head;

    if (Command[0]) {
        StartNode = CAST_FROM_MEMBER(AppCommandNode, Command, Command[0]);
        StartNode = StartNode->Next;
    }

    Command[0] = NULL;

    if (StartNode)
        Command[0] = &StartNode->Command;

    return !!Command[0];
}

b32 
AppNextViewCommand(AppCommand** Command)
{
    for (; AppNextCommand(Command); ) {
        if (Registers()->View == Command[0]->Registers->View)
            break;
    }

    b32 Result = !!Command[0];

    return Result;
}

AppRegisters* 
__AppPushRegisters(AppRegisters* Regs)
{
    AppRegistersNode* Node = ArenaPushArrayZero(
        AppFrameMemPool(), 
        AppRegistersNode, 
        1
    );

    CopyRegisters(AppFrameMemPool(), &Node->V, Regs);
    SLL_STACK_PUSH(APP_STATE->HeadRegisters, Node);

    return &Node->V;
}

AppRegisters* 
AppPopRegisters(void)
{
    AppRegisters* Regs = &APP_STATE->HeadRegisters->V;

    SLL_STACK_POP(APP_STATE->HeadRegisters);

    if (!APP_STATE->HeadRegisters)
        APP_STATE->HeadRegisters = &APP_STATE->BaseRegisters;

    return Regs;
}

MDNode* 
AppThemeTreeFromName(Arena* MemPool, Access* Acc, Str8 Name)
{
    MDNode* Result = EMPTY_MD_NODE_VALUE;

    for (u32 Index = 0; Index < APP_THEME_PRESET_COUNT; ++Index) {
        if (
            StrMatch(Name, APP_THEME_PRESET_DISPLAY_STR_TABLE[Index], 0) ||
            StrMatch(Name, APP_THEME_PRESET_CODE_STR_TABLE[Index], 0)
        ) {
            Result = APP_STATE->ThemePresetTrees[Index];
            break;
        }
    }

    if (Result == EMPTY_MD_NODE_VALUE) {
        Str8 ThemesFolder = ArenaPushStrFmt(
            MemPool,
            "%S/%s/themes",
            PRINT_STR(GetProcessProperties()->UserProgramDataPath),
            NC_DATA_DIR
        );
        Str8 Path = ArenaPushStrFmt(
            MemPool,
            "%S/%S",
            PRINT_STR(ThemesFolder),
            PRINT_STR(Name)
        );
        CKey Key = FSKeyFromPathRange(Path, Rng((u64) 0ULL, U64_MAX), 0);
        u128 FileHash = CHashFromKey(Key, 0);
        Str8 Text = CDataFromHash(Acc, FileHash);

        if (Text.Size)
            Result = MDTreeFromStr(MemPool, Text);
    }

    if (Result == EMPTY_MD_NODE_VALUE)
        Result = APP_STATE->ThemePresetTrees[APP_THEME_PRESET_AYU_DARK];

    return Result;
}

FancyStrList
AppTitleFStrFromConfig(Arena* MemPool, ConfigNode* Config, b32 IncludeExtras)
{
    FancyStrList Result = {};
    FancyStrParams Params = {};
    Str8 DisplayName = Config->String;
    Str8 FileName = ConfigNodeChildFromStr(
        Config,
        "file"_s8
    )->Head->String;

    if (!IncludeExtras)
        FileName = StrSkipLastSlash(FileName);

    for (
        u64 Index = 0;
        Index < ARRAY_COUNT(APP_TAB_FAST_PATH_VIEW_NAME_TABLE);
        ++Index
    ) {
        if (
            StrMatch(
                Config->String, 
                APP_TAB_FAST_PATH_VIEW_NAME_TABLE[Index], 
                0
            )
        ) {
            DisplayName = AppVocabInfoFromCodeName(
                APP_COMMAND_KIND_INFO_TABLE[
                    APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD + Index
                ].String
            )->DisplayName;
            break;
        }
    }

    Params.Colour = UIColourFromName("text"_s8);
    ListPush(MemPool, &Result, &Params, NULL, DisplayName);

    if (FileName.Size) {
        Params.Flags = FANCY_STR_FLAG_DIMMED;
        ListPush(MemPool, &Result, &Params, NULL, " "_s8);
        ListPush(MemPool, &Result, &Params, NULL, FileName);
    }

    return Result;
}

INTERNAL void
AppQueryPushSessionPaths(
    Arena* MemPool,
    AppQueryResultList* List,
    Str8 ListName,
    b32 PathIsChild
) {
    ConfigNode* Session = ConfigNodeChildFromStr(
        ConfigNodeRoot(),
        "session"_s8
    );

    for (
        ConfigNode* Child = Session->Head;
        Child != EMPTY_CFG_NODE_VALUE;
        Child = Child->Next
    ) {
        if (!StrMatch(Child->String, ListName, 0))
            continue;

        Str8 Path = PathIsChild
            ? PathFromConfig(Child)
            : Child->Head->String;

        AppQueryResult Entry = {};

        Entry.String = Path;
        Entry.DisplayString = StrSkipLastSlash(Path);
        Entry.Description = StrChopLastSlash(Path);
        Entry.IconKind = APP_ICON_KIND_FILE;
        Entry.CfgID = Child->ID;
        ListPush(MemPool, List, Entry);
    }
}

AppQueryResultList 
AppQueryResultsFromExpression(Arena* MemPool, Str8 Expression, Str8 Input)
{
    AppQueryResultList Result = {};
    u8 SplitChars[] = {
        ',',
        ' '
    };
    Str8List Terms = StrSplit(
        MemPool,
        Expression,
        SplitChars,
        ARRAY_COUNT(SplitChars),
        FALSE
    );

    for (Str8Node* Term = Terms.Head; Term; Term = Term->Next) {
        AppQueryExpressionKind Kind = AppQueryExpressionKindFromStr(
            Term->String
        );

        switch (Kind) {
            default: {} break;

            case APP_QUERY_EXPR_KIND_COMMANDS:
            case APP_QUERY_EXPR_KIND_TAB_COMMANDS: {
                u64 HeadIndex = (Kind == APP_QUERY_EXPR_KIND_TAB_COMMANDS)
                    ? APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD
                    : 1;

                for (
                    u64 Index = HeadIndex;
                    Index < APP_COMMAND_KIND_COUNT;
                    ++Index
                ) {
                    AppCommandKindInfo* Info = &APP_COMMAND_KIND_INFO_TABLE[Index];

                    if (!(Info->Flags & APP_COMMAND_KIND_FLAG_LIST_IN_UI))
                        continue;

                    AppVocabInfo* Vocab = AppVocabInfoFromCodeName(
                        Info->String
                    );
                    AppQueryResult Entry = {};

                    Entry.String = Info->String;
                    Entry.DisplayString = Vocab->DisplayName;
                    Entry.Description = Info->Description;
                    Entry.SearchTags = Info->SearchTags;
                    Entry.IconKind = Vocab->IconKind;
                    Entry.CommandInfo = Info;
                    ListPush(MemPool, &Result, Entry);
                }
            } break;

            case APP_QUERY_EXPR_KIND_RECENT_FILES: {
                AppQueryPushSessionPaths(
                    MemPool,
                    &Result,
                    "file"_s8,
                    FALSE
                );
            } break;

            case APP_QUERY_EXPR_KIND_FLV_FILES: {
                AppQueryPushSessionPaths(
                    MemPool,
                    &Result,
                    "view"_s8,
                    FALSE
                );
            } break;

            case APP_QUERY_EXPR_KIND_RECENT_SESSIONS: {
                AppQueryPushSessionPaths(
                    MemPool,
                    &Result,
                    "recent_session"_s8,
                    TRUE
                );
            } break;

            case APP_QUERY_EXPR_KIND_SESSION_SETTINGS: {} break;
            case APP_QUERY_EXPR_KIND_CONFIG: {} break;
            case APP_QUERY_EXPR_KIND_CONFIG_CHILD: {} break;

            case APP_QUERY_EXPR_KIND_THEMES: {
                for EACH_ENUM(AppThemePreset, T, APP_THEME_PRESET_COUNT) {
                    AppQueryResult Entry = {};

                    Entry.String = APP_THEME_PRESET_DISPLAY_STR_TABLE[T];
                    Entry.DisplayString = APP_THEME_PRESET_DISPLAY_STR_TABLE[T];
                    ListPush(MemPool, &Result, Entry);
                }
            } break;

            case APP_QUERY_EXPR_KIND_FILE_PATH: {
                Str8 Folder = StrChopLastSlash(Input);
                FileIter* Iter = SystemFileIterBegin(MemPool, Folder, 0);

                for (
                    FileInfo Info = {};
                    SystemFileIterNext(MemPool, Iter, &Info);
                ) {
                    b32 IsFolder = !!(Info.Properties.Kind & SYS_FILE_IS_DIR);
                    Str8 Ext = StrSkipLastDot(Info.Name);
                    b32 IsWanted = (
                        IsFolder ||
                        StrMatch(Ext, "fli"_s8, STR_MATCH_ALL_CASES) ||
                        StrMatch(Ext, "bin"_s8, STR_MATCH_ALL_CASES) ||
                        StrMatch(Ext, "flv"_s8, STR_MATCH_ALL_CASES)
                    );

                    // NOTE(nc): we only care about folders and *.fli or *.BIN files
                    if (!IsWanted)
                        continue;

                    AppQueryResult Entry = {};

                    Entry.String = ArenaPushStrFmt(
                        MemPool,
                        "%S/%S",
                        PRINT_STR(Folder),
                        PRINT_STR(Info.Name)
                    );
                    Entry.DisplayString = StrSkipLastSlash(Entry.String);
                    Entry.IconKind = IsFolder 
                        ? APP_ICON_KIND_FOLDER 
                        : APP_ICON_KIND_FILE;
                    Entry.Description = IsFolder
                        ? "Folder"_s8
                        : ArenaPushStrFmt(
                            MemPool, 
                            "%llu bytes", 
                            Info.Properties.Size
                        );
                    ListPush(MemPool, &Result, Entry);
                }

                SystemFileIterEnd(Iter);
            } break;
        }
    }

    return Result;
}

void 
AppQueryViewUI(r2f32 Rect)
{
    TempArena Scratch = GetScratch(NULL, 0);
    AppRegisters* QueryRegs = APP_STATE->QueryRegs;
    Str8 CommandName = QueryRegs->CommandName;
    AppCommandKindInfo* Info = AppCommandKindInfoFromStr(CommandName);
    Str8 Expression = QueryRegs->Expression;
    b32 IsFilePath = (
        AppQueryExpressionKindFromStr(
            Expression
        ) == APP_QUERY_EXPR_KIND_FILE_PATH
    );
    Str8 Input = Str(APP_STATE->QueryBuffer, APP_STATE->QueryStringSize);
    Str8 Needle = IsFilePath ? StrSkipLastSlash(Input) : Input;
    AppQueryResultList Results = AppQueryResultsFromExpression(
        Scratch.MemPool,
        Expression,
        Input
    );
    AppQueryResult** Rows = ArenaPushArrayZero(
        Scratch.MemPool,
        AppQueryResult*,
        Results.Count
    );
    u64 RowsCount = 0;

    for (AppQueryResultNode* N = Results.Head; N; N = N->Next) {
        FMRangeList NameMatches = FuzzyFind(
            Scratch.MemPool,
            Needle,
            N->V.DisplayString
        );
        FMRangeList TagsMatches = FuzzyFind(
            Scratch.MemPool,
            Needle,
            N->V.SearchTags
        );

        if (
            NameMatches.Count == NameMatches.StrPartCount ||
            TagsMatches.Count == TagsMatches.StrPartCount
        ) {
            Rows[RowsCount++] = &N->V;
        }
    }

    v2f32 RectLength = Length(Rect);
    f32 Width = CLAMP(20.0f, RectLength.X - 8.0f, 76.0f);
    f32 ListHeight = CLAMP(1.0f, RectLength.Y - 6.0f, 16.0f);
    r2f32 ListerRect = Rng(
        Rect.X0 + FloorF32((RectLength.X - Width) / 2.0f),
        Rect.Y0 + 1.0f,
        Rect.X0 + FloorF32((RectLength.X - Width) / 2.0f) + Width,
        Rect.Y0 + 1.0f + ListHeight + 4.0f
    );
    AppQueryResult* Accepted = NULL;
    b32 IsAccepted = FALSE;
    UISignal EditSig = {};

    APP_STATE->TextEditMode = TRUE;
    APP_STATE->QueryListCursor.Y = MIN(
        APP_STATE->QueryListCursor.Y, 
        (i64) RowsCount
    );
    APP_STATE->QueryListMark = APP_STATE->QueryListCursor;

    UIFocus(UI_FOCUS_KIND_ON) {
        UITag("floating"_s8) {
            UISetNextFlags(UI_BOX_KIND_DRAW_DROP_SHADOW);

            UIPane(ListerRect, "###query_lister"_s8) {
                Str8 InputPreEdit = ArenaPushStrCpy(Scratch.MemPool, Input);

                UISpacer(UI_PX(1.0f, 1.0f));
                UISetNextPreferredHeight(UI_PX(ListHeight + 2.0f, 1.0f));
                UIRowBegin();
                UISpacer(UI_PX(1.0f, 1.0f));
                UISetNextPreferredWidth(UI_PX(Width - 2.0f, 1.0f));
                UISetNextPreferredHeight(UI_PERCENT(1.0f, 0.0f));
                UIColumnBegin();

                UIWidthFill() {
                    UINamedRow("###query_input_row"_s8) {
                        UIBox* InputRow = UIHeadParent();
                        UIKey InputKey = UIKeyFromStr(
                            InputRow->Key,
                            "###query_input"_s8
                        );

                        InputRow->DefaultNavFocusActiveKey = InputKey;
                        InputRow->DefaultNavFocusNextActiveKey = InputKey;

                        UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                            UITag("weak"_s8) {
                                UILabel(APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_FIND]);

                                if (CommandName.Size) {
                                    UILabel(
                                        AppVocabInfoFromCodeName(
                                            Info->String
                                        )->DisplayName
                                    );
                                }
                            }
                        }

                        UITag("alt"_s8) {
                            EditSig = UILineEdit(
                                &APP_STATE->QueryCursor,
                                &APP_STATE->QueryMark,
                                APP_STATE->QueryBuffer,
                                sizeof(APP_STATE->QueryBuffer),
                                &APP_STATE->QueryStringSize,
                                Input,
                                "###query_input"_s8
                            );
                        }
                    }

                    if (
                        !StrMatch(
                            InputPreEdit,
                            Str(APP_STATE->QueryBuffer, APP_STATE->QueryStringSize),
                            0
                        )
                    ) {
                        APP_STATE->QueryListCursor = {};
                        APP_STATE->QueryListCursor.Y = IsFilePath ? 0 : 1;
                        APP_STATE->QueryListMark = APP_STATE->QueryListCursor;
                        APP_STATE->QueryScroll = {};
                        AppRequestFrame();
                    }

                    UIDivider(UI_PX(1.0f, 1.0f));

                    UIScrollListParameters Params = {};
                    UIScrollListSignal ListSig = {};
                    r1i64 Visible = {};

                    Params.Kind = UI_SCROLL_LIST_KIND_ALL;
                    Params.DimensionsPX = Vec(Width - 2.0f, ListHeight);
                    Params.RowHeightPX = 1.0f;
                    Params.CursorRange.Max.Y = (i64) RowsCount;
                    Params.ItemRange = Rng((i64) 0, (i64) RowsCount);
                    Params.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;

                    UIScrollList(
                        &Params,
                        &APP_STATE->QueryScroll,
                        &APP_STATE->QueryListCursor,
                        &APP_STATE->QueryListMark,
                        &Visible,
                        &ListSig
                    ) {
                        for (
                            i64 Row = Visible.Min;
                            Row <= Visible.Max && Row < (i64) RowsCount;
                            ++Row
                        ) {
                            AppQueryResult* Entry = Rows[Row];
                            b32 IsCursor = (
                                APP_STATE->QueryListCursor.Y == Row + 1
                            );
                            FMRangeList Matches = FuzzyFind(
                                Scratch.MemPool,
                                Needle,
                                Entry->DisplayString
                            );
                            Str8 Note = Entry->Description;

                            UITag(
                                IsCursor 
                                    ? "pop"_s8 
                                    : ""_s8
                            ) {
                                UISetNextChildLayoutAxis(AXIS_2D_X);

                                UIBox* RowBox = UIBuildBoxFromStrFmt(
                                    (
                                        UI_BOX_KIND_MOUSE_CLICKABLE |
                                        UI_BOX_KIND_DRAW_BACKGROUND |
                                        UI_BOX_KIND_DRAW_HOT_EFFECTS
                                    ),
                                    "###query_row_%lld",
                                    Row
                                );

                                UIParent(RowBox) {
                                    UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                                        if (Entry->IconKind != APP_ICON_KIND_NULL) {
                                            UITag("weak"_s8) {
                                                UILabel(
                                                    APP_ICON_KIND_TEXT_TABLE[Entry->IconKind]
                                                );
                                            }
                                        }

                                        UIBox* LabelBox = UILabel(
                                            Entry->DisplayString
                                        ).Box;

                                        UIBoxEquipFuzzyMatchRanges(
                                            LabelBox, 
                                            &Matches
                                        );
                                        UISpacer(UI_PERCENT(1.0f, 0.0f));

                                        if (Entry->CommandInfo) {
                                            AppCommandBindingButtons(
                                                Entry->String,
                                                ""_s8,
                                                U64_MAX,
                                                APP_COMMAND_BINDING_BTN_FLAG_ADD_NEW
                                            );
                                        } else if (Note.Size) {
                                            UITag("weak"_s8) {
                                                UILabel(Note);
                                            }
                                        }
                                    }
                                }

                                UISignal RowSig = UISignalFromBox(RowBox);

                                if (UI_PRESSED(RowSig)) {
                                    APP_STATE->QueryListCursor.Y = Row + 1;
                                    APP_STATE->QueryListMark = APP_STATE->QueryListCursor;
                                }

                                if (UI_CLICKED(RowSig)) {
                                    Accepted = Entry;
                                    IsAccepted = TRUE;
                                }
                            }
                        }
                    }

                    APP_STATE->QueryScroll.Offset = 0.0f;
                }

                    UIColumnEnd();
                    UIRowEnd();
            }
        }

        for (UIEvent* Evt = NULL; UINextEvent(&Evt); ) {
            if (
                Evt->Kind == UI_EVENT_KIND_PRESS &&
                Evt->Input == INPUT_KIND_LEFT_MOUSE_BTN &&
                !InRange(ListerRect, Evt->Position)
            ) {
                AppCmd(APP_COMMAND_KIND_CANCEL_QUERY);
                break;
            }
        }

        if (UISlotPress(UI_EVENT_ACTION_SLOT_CANCEL))
            AppCmd(APP_COMMAND_KIND_CANCEL_QUERY);

        if (
            UI_COMMITTED(EditSig) ||
            UISlotPress(UI_EVENT_ACTION_SLOT_ACCEPT)
        ) {
            IsAccepted = TRUE;

            if (APP_STATE->QueryListCursor.Y > 0)
                Accepted = Rows[APP_STATE->QueryListCursor.Y - 1];
            else if (!IsFilePath && RowsCount)
                Accepted = Rows[0];
        }
    }

    if (
        IsAccepted && 
        IsFilePath && 
        Accepted && 
        Accepted->IconKind == APP_ICON_KIND_FOLDER
    ) {
        Str8 NewInput = ArenaPushStrFmt(
            Scratch.MemPool,
            "%S/",
            PRINT_STR(Accepted->String)
        );

        APP_STATE->QueryStringSize = MIN(
            sizeof(APP_STATE->QueryBuffer), 
            NewInput.Size
        );
        MemCpy(
            APP_STATE->QueryBuffer, 
            NewInput.Str, 
            APP_STATE->QueryStringSize
        );
        APP_STATE->QueryCursor = TxtPt(1, 1 + APP_STATE->QueryStringSize);
        APP_STATE->QueryMark = APP_STATE->QueryCursor;
        APP_STATE->QueryListCursor = {};
        APP_STATE->QueryListMark = {};
        APP_STATE->QueryScroll = {};
        AppRequestFrame();
    } else if (IsAccepted && (Accepted || IsFilePath)) {
        Str8 Value = Accepted 
            ? Accepted->String 
            : StrTrimLastSlash(Input);

        AppCmd(APP_COMMAND_KIND_CANCEL_QUERY);

        AppRegistersScope() {
            CopyRegisters(AppFrameMemPool(), Registers(), QueryRegs);

            if (!CommandName.Size) {
                AppCmd(
                    APP_COMMAND_KIND_RUN_COMMAND,
                    __Registers.CommandName = Value,
                    __Registers.Expression = ""_s8
                );
            } else {
                AppRegistersFillSlotFromStr(
                    Info->Query.Slot, 
                    Expression, 
                    Value
                );

                if (Accepted && Accepted->CfgID)
                    Registers()->Config = Accepted->CfgID;

                if (
                    Accepted &&
                    !IsFilePath &&
                    Info->Query.Slot == APP_REGISTERS_SLOT_FILE_PATH
                ) {
                    Registers()->FilePath = ArenaPushStrCpy(
                        AppFrameMemPool(),
                        Accepted->String
                    );
                }

                Registers()->Expression = ""_s8;
                AppPushCmd(CommandName, Registers());
            }
        }
    }

    ReleaseScratch(Scratch);
}

UISignal 
AppIconButton(AppIconKind Kind, FMRangeList* Matches, Str8 String)
{
    Str8 DisplayString = UIDisplayPartFromKeyStr(String);

    UISetNextChildLayoutAxis(AXIS_2D_X);

    UIBox* Box = UIBuildBoxFromStr(
        (
            UI_BOX_KIND_CLICKABLE |
            UI_BOX_KIND_DRAW_BACKGROUND |
            UI_BOX_KIND_DRAW_HOT_EFFECTS |
            UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
        ),
        String
    );

    UIParent(Box) {
        if (Kind != APP_ICON_KIND_NULL) {
            UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                UITag("weak"_s8) {
                    UILabel(APP_ICON_KIND_TEXT_TABLE[Kind]);
                }
            }
        }

        if (DisplayString.Size) {
            UIPreferredWidth(UI_PERCENT(1.0f, 0.0f)) {
                UIBox* LabelBox = UILabel(DisplayString).Box;

                if (Matches)
                    UIBoxEquipFuzzyMatchRanges(LabelBox, Matches);
            }
        }
    }

    UISignal Result = UISignalFromBox(Box);

    return Result;
}

UISignal 
AppIconButton(AppIconKind Kind, FMRangeList* Matches, char* Fmt, ...)
{
    TempArena Scratch = GetScratch(NULL, 0);
    va_list Args;

    va_start(Args, Fmt);

    Str8 String = ArenaPushStrFmtV(
        Scratch.MemPool,
        Fmt,
        Args
    );
    UISignal Result = AppIconButton(Kind, Matches, String);

    va_end(Args);
    ReleaseScratch(Scratch);

    return Result;
}

UISignal 
AppMenuBarButton(Str8 String)
{
    UIBox* Box = UIBuildBoxFromStr(
        (
            UI_BOX_KIND_DRAW_TEXT |
            UI_BOX_KIND_DRAW_BACKGROUND |
            UI_BOX_KIND_CLICKABLE |
            UI_BOX_KIND_DRAW_HOT_EFFECTS
        ),
        String
    );

    UISignal Result = UISignalFromBox(Box);

    return Result;
}

void
AppCommandBindingButtons(
    Str8 Name,
    Str8 Filter,
    u64 Limit,
    AppCommandBindingButtonFlag Flags
) {
    TempArena Scratch = GetScratch(NULL, 0);
    ConfigInputMapNodePtrList InputMapNodes = ConfigInputMapNodePtrListFromName(
        Scratch.MemPool,
        APP_STATE->KeyMap,
        Name
    );
    u64 InputMapIndex = 0;

    for (
        ConfigInputMapNodePtr* N = InputMapNodes.Head;
        N && InputMapIndex < Limit;
        N = N->Next
    ) {
        ConfigBinding Binding = N->V->Binding;

        if (Binding.Input == INPUT_KIND_NULL)
            continue;

        ++InputMapIndex;
        UISpacer(UI_PX(1.0f, 1.0f));

        b32 RebindingActiveForThisBinding = (
            APP_STATE->BindChangeActive &&
            StrMatch(APP_STATE->BindChangeCommandName, Name, 0) &&
            N->V->ID == APP_STATE->BindChangeBindingID
        );
        b32 IsReserved = (
            !Binding.ModKind &&
            (
                Binding.Input == INPUT_KIND_ESC ||
                Binding.Input == INPUT_KIND_DELETE
            )
        );
        b32 CanEdit = (
            !(Flags & APP_COMMAND_BINDING_BTN_FLAG_NO_EDIT) &&
            !IsReserved
        );
        b32 HasConflicts = FALSE;
        ConfigInputMapNodePtrList NodesWithThisBinding = ConfigInputMapNodePtrListFromBinding(
            Scratch.MemPool,
            APP_STATE->KeyMap,
            Binding
        );

        for (
            ConfigInputMapNodePtr* N2 = NodesWithThisBinding.Head;
            N2;
            N2 = N2->Next
        ) {
            if (!StrMatch(N->V->Name, N2->V->Name, 0)) {
                HasConflicts = TRUE;
                break;
            }
        }

        Str8 KeyBindingString = RebindingActiveForThisBinding
            ? "\xE2\x80\xA6"_s8
            : StrFromInputModifierInput(
                Scratch.MemPool,
                Binding.ModKind,
                Binding.Input
            );
        FMRangeList Matches = {};

        if (Filter.Size)
            Matches = FuzzyFind(Scratch.MemPool, Filter, KeyBindingString);

        UISetNextTag(
            RebindingActiveForThisBinding
                ? "pop"_s8
                : HasConflicts
                    ? "bad"_s8
                    : "alt"_s8
        );
        UISetNextTextAlignment(UI_TEXT_ALIGN_CENTRE);
        UISetNextGroupKey(EMPTY_UI_KEY_VALUE);
        UISetNextFastpathCodepoint(0);
        UISetNextPreferredWidth(UI_TEXT_DIM(0.0f, 1.0f));

        UIBox* Box = UIBuildBoxFromStrFmt(
            (
                UI_BOX_KIND_DRAW_TEXT |
                UI_BOX_KIND_DRAW_BACKGROUND |
                UI_BOX_KIND_FOCUS_NAV_SKIP |
                (
                    CanEdit * (
                        UI_BOX_KIND_MOUSE_CLICKABLE |
                        UI_BOX_KIND_DRAW_HOT_EFFECTS |
                        UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
                    )
                )
            ),
            "%S###bind_btn_%S_%x_%x",
            PRINT_STR(KeyBindingString),
            PRINT_STR(Name),
            Binding.Input,
            Binding.ModKind
        );

        if (Filter.Size)
            UIBoxEquipFuzzyMatchRanges(Box, &Matches);

        UISignal Sig = UISignalFromBox(Box);

        if (!APP_STATE->BindChangeActive && UI_CLICKED(Sig)) {
            ArenaClear(APP_STATE->BindChangeMemPool);
            APP_STATE->BindChangeActive = TRUE;
            APP_STATE->BindChangeCommandName = ArenaPushStrCpy(
                APP_STATE->BindChangeMemPool,
                Name
            );
            APP_STATE->BindChangeBindingID = N->V->ID;
            APP_STATE->BindChangeUIKey = Box->Key;
        } else if (APP_STATE->BindChangeActive && UI_CLICKED(Sig)) {
            APP_STATE->BindChangeActive = FALSE;
        }

        if (UI_HOVERING(Sig) && HasConflicts) {
            UITooltip() {
                UILabel("Other commands that have this key:"_s8);

                for (
                    ConfigInputMapNodePtr* N2 = NodesWithThisBinding.Head;
                    N2;
                    N2 = N2->Next
                ) {
                    if (!StrMatch(N2->V->Name, N->V->Name, 0)) {
                        UILabel(
                            AppVocabInfoFromCodeName(
                                N2->V->Name
                            )->DisplayName
                        );
                    }
                }
            }
        }

        if (RebindingActiveForThisBinding) {
            UISetNextTag("bad"_s8);
            UISetNextTextAlignment(UI_TEXT_ALIGN_CENTRE);
            UISetNextGroupKey(EMPTY_UI_KEY_VALUE);
            UISetNextFastpathCodepoint(0);
            UISetNextPreferredWidth(UI_PX(3.0f, 1.0f));

            UIBox* RemoveBox = UIBuildBoxFromStrFmt(
                (
                    UI_BOX_KIND_DRAW_TEXT |
                    UI_BOX_KIND_DRAW_BACKGROUND |
                    UI_BOX_KIND_FOCUS_NAV_SKIP |
                    UI_BOX_KIND_MOUSE_CLICKABLE |
                    UI_BOX_KIND_DRAW_HOT_EFFECTS |
                    UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
                ),
                "%S###remove_binding_%S",
                PRINT_STR(APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_X]),
                PRINT_STR(Name)
            );

            if (UI_PRESSED(UISignalFromBox(RemoveBox))) {
                ConfigNodeRelease(
                    APP_STATE->Config,
                    ConfigNodeFromID(APP_STATE->BindChangeBindingID)
                );
                APP_STATE->BindChangeActive = FALSE;
            }
        }
    }

    if (Flags & APP_COMMAND_BINDING_BTN_FLAG_ADD_NEW) {
        b32 AddingNewBinding = (
            APP_STATE->BindChangeActive &&
            StrMatch(APP_STATE->BindChangeCommandName, Name, 0) &&
            !APP_STATE->BindChangeBindingID
        );

        UISpacer(UI_PX(1.0f, 1.0f));
        UISetNextTag(AddingNewBinding ? "pop"_s8 : "alt"_s8);
        UISetNextTextAlignment(UI_TEXT_ALIGN_CENTRE);
        UISetNextGroupKey(EMPTY_UI_KEY_VALUE);
        UISetNextFastpathCodepoint(0);
        UISetNextPreferredWidth(UI_PX(3.0f, 1.0f));

        Str8 BindingString = (AddingNewBinding) 
            ? "\xE2\x80\xA6"_s8 
            : APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_ADD];
        UIBox* Box = UIBuildBoxFromStrFmt(
            (
                UI_BOX_KIND_DRAW_TEXT |
                UI_BOX_KIND_DRAW_BACKGROUND |
                UI_BOX_KIND_FOCUS_NAV_SKIP |
                UI_BOX_KIND_MOUSE_CLICKABLE |
                UI_BOX_KIND_DRAW_HOT_EFFECTS |
                UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
            ),
            "%S###add_binding_%S",
            PRINT_STR(BindingString),
            PRINT_STR(Name)
        );

        UISignal Sig = UISignalFromBox(Box);

        if (!AddingNewBinding && UI_CLICKED(Sig)) {
            ArenaClear(APP_STATE->BindChangeMemPool);
            APP_STATE->BindChangeActive = TRUE;
            APP_STATE->BindChangeCommandName = ArenaPushStrCpy(
                APP_STATE->BindChangeMemPool,
                Name
            );
            APP_STATE->BindChangeBindingID = 0;
            APP_STATE->BindChangeUIKey = Box->Key;
        } else if (AddingNewBinding && UI_CLICKED(Sig)) {
            APP_STATE->BindChangeActive = FALSE;
        }
    }

    ReleaseScratch(Scratch);
}

UISignal 
AppCommandSpecButton(Str8 Name)
{
    TempArena Scratch = GetScratch(NULL, 0);
    AppCommandKindInfo* Info = AppCommandKindInfoFromStr(Name);
    ConfigInputMapNodePtrList InputMapNodes = ConfigInputMapNodePtrListFromName(
        Scratch.MemPool,
        APP_STATE->KeyMap,
        Name
    );

    UISetNextChildLayoutAxis(AXIS_2D_X);

    UIBox* Box = UIBuildBoxFromStrFmt(
        (
            UI_BOX_KIND_DRAW_BACKGROUND |
            UI_BOX_KIND_DRAW_HOT_EFFECTS |
            UI_BOX_KIND_DRAW_ACTIVE_EFFECTS |
            UI_BOX_KIND_CLICKABLE
        ),
        "###cmd_%p",
        Info
    );

    UIParent(Box) {
        UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
            UIFlags(UI_BOX_KIND_DRAW_TEXT_FASTPATH_CODEPOINT) {
                UIFastpathCodepoint(Box->FastpathCodepoint) {
                    UILabel(
                        AppVocabInfoFromCodeName(
                            Info->String
                        )->DisplayName
                    );
                }
            }

            UISpacer(UI_PERCENT(1.0f, 0.0f));
            UISetNextFlags(
                UI_BOX_KIND_MOUSE_CLICKABLE | UI_BOX_KIND_FOCUS_NAV_SKIP
            );
            UISetNextGroupKey(EMPTY_UI_KEY_VALUE);

            UIPreferredWidth(UI_SUM_OF_CHILDREN(1.0f)) {
                UINamedRow("###bindings"_s8) {
                    AppCommandBindingButtons(
                        Name,
                        ""_s8,
                        1,
                        InputMapNodes.Head
                            ? 0
                            : APP_COMMAND_BINDING_BTN_FLAG_ADD_NEW
                    );
                }
            }
        }
    }

    UISignal Sig = UISignalFromBox(Box);

    ReleaseScratch(Scratch);

    return Sig;
}

void 
AppCommandListMenuButtons(
    Str8* CommandNames, 
    u64 CommandNamesCount, 
    u32* FastPointCodePoints
) {
    UISetNextPreferredHeight(UI_SUM_OF_CHILDREN(1.0f));

    UIColumn() {
        for (u64 Index = 0; Index < CommandNamesCount; ++Index) {
            if (!CommandNames[Index].Size) {
                UIDivider(UI_PX(1.0f, 1.0f));
            } else {
                AppCommandKind Kind = AppCommandKindFromStr(CommandNames[Index]);
                b32 IsDisabled = (
                    Kind >= APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD &&
                    APP_COMMAND_KIND_INFO_TABLE[Kind].Flags & APP_COMMAND_KIND_FLAG_LIST_IN_FILE &&
                    !Registers()->FilePath.Size
                );

                if (IsDisabled)
                    UISetNextFlags(UI_BOX_KIND_DISABLED);

                UISetNextFastpathCodepoint(FastPointCodePoints[Index]);

                UISignal Sig = AppCommandSpecButton(CommandNames[Index]);

                if (UI_CLICKED(Sig) && !IsDisabled) {
                    AppCmd(
                        APP_COMMAND_KIND_RUN_COMMAND,
                        __Registers.CommandName = CommandNames[Index]
                    );
                    UIContextMenuClose();
                    APP_STATE->MenuBarFocused = FALSE;
                }
            }
        }
    }
}

void 
AppInit(CommandLine* CLI)
{
    EMPTY_APP_VIEW_STATE_VALUE->HashNext = EMPTY_APP_VIEW_STATE_VALUE;
    EMPTY_APP_VIEW_STATE_VALUE->HashPrev = EMPTY_APP_VIEW_STATE_VALUE;
    __EMPTY_APP_VOCAB_INFO_VALUE = {};

    Arena* MemPool = ArenaAlloc();

    APP_STATE = ArenaPushArrayZero(MemPool, AppState, 1);
    APP_STATE->MemPool = MemPool;
    APP_STATE->SessionPathMemPool = ArenaAlloc();
    APP_STATE->ThemePathMemPool = ArenaAlloc();
    APP_STATE->SessionConfigStringKey = CKeyAlloc(
        CRootAlloc(),
        CIDAlloc(0, 0)
    );
    APP_STATE->TransientConfigStringKey = CKeyAlloc(
        CRootAlloc(),
        CIDAlloc(0, 0)
    );

    for (
        u64 Index = 0; 
        Index < ARRAY_COUNT(APP_STATE->FrameMemPools); 
        ++Index
    ) {
        APP_STATE->FrameMemPools[Index] = ArenaAlloc();
    }

    APP_STATE->NumFramesRequested = 2;
    APP_STATE->ECache = EvalCacheAlloc();

    for (
        u64 Index = 0; 
        Index < ARRAY_COUNT(APP_STATE->CommandMemPools); 
        ++Index
    ) {
        APP_STATE->CommandMemPools[Index] = ArenaAlloc();
    }

    APP_STATE->CommandOutputMemPool = ArenaAlloc();
    APP_STATE->PopUpMemPool = ArenaAlloc();
    APP_STATE->ContextMenuKey = UIKeyFromStr(
        EMPTY_UI_KEY_VALUE, 
        "top_level_ctx_menu"_s8
    );
    APP_STATE->DropCompletionKey = UIKeyFromStr(
        EMPTY_UI_KEY_VALUE,
        "drop_completion_ctx_menu"_s8
    );
    APP_STATE->QueryMemPool = ArenaAlloc();
    APP_STATE->AutoCompleteMemPool = ArenaAlloc();
    APP_STATE->DragDropMemPool = ArenaAlloc();
    APP_STATE->DragDropRegisters = ArenaPushArrayZero(
        APP_STATE->DragDropMemPool,
        AppRegisters,
        1
    );
    APP_STATE->BindChangeMemPool = ArenaAlloc();
    APP_STATE->HeadRegisters = &APP_STATE->BaseRegisters;
    APP_STATE->UI = UIStateAlloc();

    {
        APP_STATE->CfgSchemaTable = ArenaPushArrayZero(
            APP_STATE->MemPool,
            ConfigSchemaTable,
            1
        );
        APP_STATE->CfgSchemaTable->SlotsCount = 4096;
        APP_STATE->CfgSchemaTable->Slots = ArenaPushArrayZero(
            APP_STATE->MemPool,
            ConfigSchemaNode*,
            APP_STATE->CfgSchemaTable->SlotsCount
        );

        for (
            u64 Index = 0;
            Index < ARRAY_COUNT(APP_NAME_SCHEMA_INFO_TABLE);
            ++Index
        ) {
            MDNode* Schema = MDTreeFromStr(
                APP_STATE->MemPool,
                APP_NAME_SCHEMA_INFO_TABLE[Index].Schema
            )->Head;

            ConfigSchemaTableInsert(
                APP_STATE->MemPool,
                APP_STATE->CfgSchemaTable,
                APP_NAME_SCHEMA_INFO_TABLE[Index].Name,
                Schema
            );
        }
    }

    {
        for (
            AppThemePreset ThemePreset = 0;
            ThemePreset < APP_THEME_PRESET_COUNT;
            ++ThemePreset
        ) {
            APP_STATE->ThemePresetTrees[ThemePreset] = MDTreeFromStr(
                APP_STATE->MemPool,
                APP_THEME_PRESET_CONFIG_STR_TABLE[ThemePreset]
            )->Head;
        }
    }

    APP_STATE->VocabInfoMap.SingleSlotsCount = 1024;
    APP_STATE->VocabInfoMap.SingleSlots = ArenaPushArrayZero(
        APP_STATE->MemPool,
        AppVocabInfoMapSlot,
        APP_STATE->VocabInfoMap.SingleSlotsCount
    );
    APP_STATE->VocabInfoMap.PluralSlotsCount = 1024;
    APP_STATE->VocabInfoMap.PluralSlots = ArenaPushArrayZero(
        APP_STATE->MemPool,
        AppVocabInfoMapSlot,
        APP_STATE->VocabInfoMap.PluralSlotsCount
    );

    for (
        u64 Index = 0;
        Index < ARRAY_COUNT(APP_VOCAB_INFO_TABLE);
        ++Index
    ) {
        AppVocabInfoMapNode* N = ArenaPushArrayZero(
            APP_STATE->MemPool,
            AppVocabInfoMapNode,
            1
        );

        MemCpy(
            &N->V,
            &APP_VOCAB_INFO_TABLE[Index],
            sizeof(N->V)
        );

        u64 SingleHash = Hash(N->V.CodeName);
        u64 PluralHash = Hash(N->V.CodeNamePlural);

        if (N->V.CodeName.Size) {
            SLL_QUEUE_PUSH_EX(
                APP_STATE->VocabInfoMap.SingleSlots[
                    SingleHash % APP_STATE->VocabInfoMap.SingleSlotsCount
                ].Head,
                APP_STATE->VocabInfoMap.SingleSlots[
                    SingleHash % APP_STATE->VocabInfoMap.SingleSlotsCount
                ].Tail,
                N,
                SingleNext
            );
        }

        if (N->V.CodeNamePlural.Size) {
            SLL_QUEUE_PUSH_EX(
                APP_STATE->VocabInfoMap.PluralSlots[
                    PluralHash % APP_STATE->VocabInfoMap.PluralSlotsCount
                ].Head,
                APP_STATE->VocabInfoMap.PluralSlots[
                    PluralHash % APP_STATE->VocabInfoMap.PluralSlotsCount
                ].Tail,
                N,
                PluralNext
            );
        }
    }

    APP_STATE->ConfigIDSlotsCount = 1024;
    APP_STATE->ConfigIDSlots = ArenaPushArrayZero(
        MemPool,
        ConfigNodePtrSlot,
        APP_STATE->ConfigIDSlotsCount
    );
    APP_STATE->Config = ConfigStateAlloc();
    ConfigContextSelect(ConfigContextFromState(APP_STATE->Config));
    
    ConfigNode* Session = ConfigNodeNew(
        APP_STATE->Config, 
        ConfigNodeRoot(), 
        "session"_s8
    );

    ConfigNodeNew(APP_STATE->Config, ConfigNodeRoot(), "command_line"_s8);
    ConfigNodeNew(APP_STATE->Config, ConfigNodeRoot(), "transient"_s8);
    APP_STATE->ViewStateSlotsCount = 1024;
    APP_STATE->ViewStateSlots = ArenaPushArrayZero(
        MemPool,
        AppViewStateSlot,
        APP_STATE->ViewStateSlotsCount
    );

    {
        struct {
            Str8 Name;
            Str8 Value;
        } Settings[] = {
            {
                Str8Lit("focus_menu_bar_with_alt"),
                Str8Lit("1")
            },
            {
                Str8Lit("animations"),
                Str8Lit("1")
            },
            {
                Str8Lit("scrolling_animations"),
                Str8Lit("1")
            },
            {
                Str8Lit("autocompletion_lister"),
                Str8Lit("1")
            }
        };
        ConfigNode* SettingsRoot = ConfigNodeNew(
            APP_STATE->Config, 
            Session, 
            "settings"_s8
        );

        for (u64 Index = 0; Index < ARRAY_COUNT(Settings); ++Index) {
            ConfigNode* Setting = ConfigNodeNew(
                APP_STATE->Config,
                SettingsRoot,
                Settings[Index].Name
            );

            ConfigNodeNew(APP_STATE->Config, Setting, Settings[Index].Value);
        }
    }

    {
        TempArena Scratch = GetScratch(NULL, 0);
        Str8 SessionPath = CommandLineString(CLI, "session"_s8);

        if (SessionPath.Size) {
            SessionPath = PathAbsoluteDstFromRelativeDstSrc(
                Scratch.MemPool,
                SessionPath,
                GetProcessProperties()->InitialPath
            );
        }

        Str8 SessionProgramDataPath = GetProcessProperties()->UserProgramDataPath;
        Str8 SessionDataPath = ArenaPushStrFmt(
            Scratch.MemPool,
            "%S/%s",
            PRINT_STR(SessionProgramDataPath),
            NC_DATA_DIR
        );

        SystemCreateDirectory(SessionDataPath);

        if (!SessionPath.Size) {
            SessionPath = ArenaPushStrFmt(
                Scratch.MemPool,
                "%S/default.%s_session",
                PRINT_STR(SessionDataPath),
                NC_DATA_DIR
            );
        }

        if (SessionPath.Size) {
            ArenaClear(APP_STATE->SessionPathMemPool);
            APP_STATE->SessionPath = ArenaPushStrCpy(
                APP_STATE->SessionPathMemPool,
                SessionPath
            );
        }

        AppCmd(
            APP_COMMAND_KIND_OPEN_SESSION,
            __Registers.FilePath = SessionPath
        );
        ReleaseScratch(Scratch);
    }
}

ConfigNode* 
AppImmediateConfigFromKey(Str8 String)
{
    ConfigNode* Transient = ConfigNodeChildFromStr(
        ConfigNodeRoot(),
        "transient"_s8
    );
    ConfigNode* Immediate = EMPTY_CFG_NODE_VALUE;
    ConfigNode* Result = EMPTY_CFG_NODE_VALUE;

    for (
        ConfigNode* Child = Transient->Head;
        Child != EMPTY_CFG_NODE_VALUE;
        Child = Child->Next
    ) {
        if (StrMatch(Child->String, "immediate"_s8, 0)) {
            Result = ConfigNodeChildFromStr(Child, String);

            if (Result != EMPTY_CFG_NODE_VALUE) {
                Immediate = Child;
                break;
            }
        }
    }

    if (Result == EMPTY_CFG_NODE_VALUE) {
        Immediate = ConfigNodeNew(
            APP_STATE->Config,
            Transient,
            "immediate"_s8
        );
        Result = ConfigNodeNew(APP_STATE->Config, Immediate, String);
    }

    ConfigNodeChildFromStrOrAlloc(APP_STATE->Config, Immediate, "hot"_s8);

    return Result;
}

ConfigNode* 
AppImmediateConfigFromKey(char* Fmt, ...)
{
    TempArena Scratch = GetScratch(NULL, 0);
    va_list Args;

    va_start(Args, Fmt);

    Str8 Key = ArenaPushStrFmtV(Scratch.MemPool, Fmt, Args);
    ConfigNode* Result = AppImmediateConfigFromKey(Key);

    va_end(Args);
    ReleaseScratch(Scratch);

    return Result;
}

INTERNAL Str8
AppSessionListNameFromPath(Str8 Path)
{
    Str8 Ext = StrSkipLastDot(Path);
    Str8 Result = StrMatch(Ext, "flv"_s8, STR_MATCH_ALL_CASES)
        ? "view"_s8
        : "file"_s8;

    return Result;
}

INTERNAL b32
AppSessionPathIsRecorded(Str8 ListName, Str8 Path)
{
    ConfigNode* Session = ConfigNodeChildFromStr(
        ConfigNodeRoot(),
        "session"_s8
    );
    b32 Result = FALSE;
    b32 IsFile = StrMatch(ListName, "file"_s8, 0);

    if (IsFile)
        Path = StrChopLastDot(Path);

    for (
        ConfigNode* Child = Session->Head;
        Child != EMPTY_CFG_NODE_VALUE;
        Child = Child->Next
    ) {
        Str8 Recorded = Child->Head->String;

        if (IsFile)
            Recorded = StrChopLastDot(Recorded);

        if (
            StrMatch(Child->String, ListName, 0) &&
            PathMatchNormalised(Recorded, Path)
        ) {
            Result = TRUE;
            break;
        }
    }

    return Result;
}

INTERNAL b32
AppSessionRecordPath(Str8 Path)
{
    Str8 ListName = AppSessionListNameFromPath(Path);
    b32 IsNew = !AppSessionPathIsRecorded(ListName, Path);

    if (IsNew) {
        ConfigNode* Session = ConfigNodeChildFromStr(
            ConfigNodeRoot(),
            "session"_s8
        );
        ConfigNode* Node = ConfigNodeNew(
            APP_STATE->Config,
            Session,
            ListName
        );

        ConfigNodeNew(APP_STATE->Config, Node, Path);
    }

    return IsNew;
}

INTERNAL b32
AppPathIsLog(Str8 Path)
{
    Str8 Ext = StrSkipLastDot(Path);
    b32 Result = (
        StrMatch(Ext, "fli"_s8, STR_MATCH_ALL_CASES) ||
        StrMatch(Ext, "bin"_s8, STR_MATCH_ALL_CASES)
    );

    return Result;
}

INTERNAL void
AppPushLogPathsFromDir(Arena* MemPool, Str8List* Paths, Str8 Dir)
{
    Str8List Dirs = {};
    Str8List FLIs = {};

    ListPush(MemPool, &Dirs, Dir);

    for (Str8Node* DirNode = Dirs.Head; DirNode; DirNode = DirNode->Next) {
        FileIter* Iter = SystemFileIterBegin(MemPool, DirNode->String, 0);
        FileInfo Info = {};

        while (SystemFileIterNext(MemPool, Iter, &Info)) {
            Str8 FullPath = ArenaPushStrFmt(
                MemPool,
                "%S/%S",
                PRINT_STR(DirNode->String),
                PRINT_STR(Info.Name)
            );

            if (Info.Properties.Kind & SYS_FILE_IS_DIR) {
                ListPush(MemPool, &Dirs, FullPath);
            } else if (
                StrMatch(StrSkipLastDot(FullPath), "bin"_s8, STR_MATCH_ALL_CASES)
            ) {
                ListPush(MemPool, Paths, FullPath);
            } else if (AppPathIsLog(FullPath)) {
                ListPush(MemPool, &FLIs, FullPath);
            }
        }

        SystemFileIterEnd(Iter);
    }

    for (Str8Node* Node = FLIs.Head; Node; Node = Node->Next) {
        Str8 Stem = StrChopLastDot(Node->String);
        b32 HasBIN = FALSE;

        for (
            Str8Node* Log = Paths->Head; 
            Log && !HasBIN; 
            Log = Log->Next
        ) {
            HasBIN = PathMatchNormalised(StrChopLastDot(Log->String), Stem);
        }

        if (!HasBIN)
            ListPush(MemPool, Paths, Node->String);
    }
}

INTERNAL void
AppWriteSessionData(void)
{
    TempArena Scratch = GetScratch(NULL, 0);
    Str8 DstPath = APP_STATE->SessionPath;
    Str8 BucketName = "session"_s8;

    if (DstPath.Size) {
        b32 DstExists = !!(SystemGetFileProperties(DstPath).Created);
        Str8 TempPath = ArenaPushStrFmt(
            Scratch.MemPool,
            "%S.temp",
            PRINT_STR(DstPath)
        );
        Str8 OverwrittenPath = ArenaPushStrFmt(
            Scratch.MemPool,
            "%S.old",
            PRINT_STR(DstPath)
        );

        ConfigNode* TreeRoot = ConfigNodeChildFromStr(
            ConfigNodeRoot(),
            BucketName
        );
        Str8List Strings = {};

        ListPush(
            Scratch.MemPool,
            &Strings,
            ArenaPushStrFmt(
                Scratch.MemPool,
                "// %s %S file\n\n",
                NC_DATA_DIR,
                PRINT_STR(BucketName)
            )
        );

        for (
            ConfigNode* Child = TreeRoot->Head;
            Child != EMPTY_CFG_NODE_VALUE;
            Child = Child->Next
        ) {
            ListPush(
                Scratch.MemPool,
                &Strings,
                ConfigStrFromTree(
                    Scratch.MemPool,
                    APP_STATE->CfgSchemaTable,
                    StrChopLastSlash(DstPath),
                    Child
                )
            );
        }

        b32 TempWriteOkay = WriteFileContents(
            TempPath,
            Strings
        );
        b32 OldMoveOkay = (
            TempWriteOkay &&
            (!DstExists || SystemMoveFile(OverwrittenPath, DstPath))
        );
        b32 NewMoveOkay = (OldMoveOkay && SystemMoveFile(DstPath, TempPath));

        if (NewMoveOkay && DstExists)
            SystemDeleteFile(OverwrittenPath);
        else if (!NewMoveOkay && OldMoveOkay && DstExists)
            SystemMoveFile(DstPath, OverwrittenPath);
    }

    ReleaseScratch(Scratch);
}

void 
AppFrame(void)
{
    TempArena Scratch = GetScratch(NULL, 0);

    ++APP_STATE->FrameDepth;
    ArenaClear(AppFrameMemPool());
    APP_STATE->HeadRegisters = &APP_STATE->BaseRegisters;
    CopyRegisters(
        AppFrameMemPool(),
        &APP_STATE->HeadRegisters->V,
        &APP_STATE->HeadRegisters->V
    );

    if (APP_STATE->NextHoverRegisters) {
        APP_STATE->HoverRegisters = CopyRegisters(
            AppFrameMemPool(),
            APP_STATE->NextHoverRegisters
        );
        APP_STATE->HoverRegistersSlot = APP_STATE->NextHoverRegistersSlot;
        APP_STATE->NextHoverRegisters = NULL;
    } else {
        APP_STATE->HoverRegisters = ArenaPushArrayZero(
            AppFrameMemPool(),
            AppRegisters,
            1
        );
        APP_STATE->HoverRegistersSlot = APP_REGISTERS_SLOT_NULL;
    }

    b32 AllowTextHotkeys = !APP_STATE->TextEditMode;

    APP_STATE->TextEditMode = FALSE;

    if (APP_STATE->FrameDepth == 1) {
        ArenaClear(APP_STATE->CommandOutputMemPool);
        MemSet(
            &APP_STATE->CommandOutputs, 
            0, 
            sizeof(APP_STATE->CommandOutputs)
        );
    }

    if (APP_STATE->FrameDepth == 1) {
        TempArena Scratch = GetScratch(NULL, 0);
        ConfigNode* Window = ConfigNodeFromID(Registers()->Window);
        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
            Scratch.MemPool,
            Window
        );

        for (
            ConfigPanelNode* P = PanelTree.Root;
            P != EMPTY_CFG_PANEL_NODE_VALUE;
            P = ConfigPanelNodeRecDepthFirstPre(PanelTree.Root, P).Next
        ) {
            for (
                ConfigNodePtrNode* Node = P->Tabs.Head;
                Node;
                Node = Node->Next
            ) {
                AppViewStateFromConfig(Node->V);
            }

            if (
                P->SelectedTab == EMPTY_CFG_NODE_VALUE &&
                P->Tabs.Head
            ) {
                AppCmd(
                    APP_COMMAND_KIND_FOCUS_TAB,
                    __Registers.Panel = P->Config->ID,
                    __Registers.Tab = P->Tabs.Head->V->ID
                );
            }
        }

        ReleaseScratch(Scratch);
    }

    if (APP_STATE->FrameDepth == 1) {
        ConfigNode* Transient = ConfigNodeChildFromStr(
            ConfigNodeRoot(), 
            "transient"_s8
        );

        for (
            ConfigNode* TLN = Transient->Head, *Next = EMPTY_CFG_NODE_VALUE;
            TLN != EMPTY_CFG_NODE_VALUE;
            TLN = Next
        ) {
            Next = TLN->Next;

            if (StrMatch(TLN->String, "immediate"_s8, 0)) {
                if (ConfigNodeChildFromStr(TLN, "hot"_s8) == EMPTY_CFG_NODE_VALUE)
                    ConfigNodeRelease(APP_STATE->Config, TLN);
            }
        }

        for (
            ConfigNode* TLN = Transient->Head; 
            TLN != EMPTY_CFG_NODE_VALUE; 
            TLN = TLN->Next
        ) {
            if (StrMatch(TLN->String, "immediate"_s8, 0)) {
                for (
                    ConfigNode* Child = TLN->Head, *Next = EMPTY_CFG_NODE_VALUE; 
                    Child != EMPTY_CFG_NODE_VALUE; 
                    Child = Next
                ) {
                    Next = Child->Next;

                    if (StrMatch(Child->String, "hot"_s8, 0))
                        ConfigNodeRelease(APP_STATE->Config, Child);
                }
            }
        }
    }

    if (APP_STATE->FrameDepth == 1) {
        for (
            u64 SlotIndex = 0; 
            SlotIndex < APP_STATE->ViewStateSlotsCount; 
            ++SlotIndex
        ) {
            for (
                AppViewState* VS = APP_STATE->ViewStateSlots[SlotIndex].Head, *Next;
                VS;
                VS = Next
            ) {
                Next = VS->HashNext;

                if (VS->LastFrameIndexTouched + 2 < APP_STATE->FrameIndex) {
                    for (
                        AppArenaExt* Ext = VS->HeadArenaExt;
                        Ext;
                        Ext = Ext->Next
                    ) {
                        ArenaRelease(Ext->MemPool);
                    }

                    ArenaRelease(VS->MemPool);
                    DLL_REMOVE_EX(
                        NULL,
                        APP_STATE->ViewStateSlots[SlotIndex].Head,
                        APP_STATE->ViewStateSlots[SlotIndex].Tail,
                        VS,
                        HashNext,
                        HashPrev
                    );
                    SLL_STACK_PUSH_EX(
                        APP_STATE->FreeViewState,
                        VS,
                        HashNext
                    );

                    if (APP_STATE->ViewStateLastAccessed == VS) {
                        APP_STATE->ViewStateLastAccessed = EMPTY_APP_VIEW_STATE_VALUE;
                        APP_STATE->ViewStateLastAccessedID = 0;
                    }
                }
            }
        }
    }

    if (APP_STATE->FrameDepth == 1) {
        f32 SlowRate = 1.0f - PowF32(2.0f, (-10.0f * APP_STATE->FrameDeltaTime));

        for (
            u64 SlotIndex = 0;
            SlotIndex < APP_STATE->ViewStateSlotsCount;
            ++SlotIndex
        ) {
            for (
                AppViewState* VS = APP_STATE->ViewStateSlots[SlotIndex].Head;
                VS;
                VS = VS->HashNext
            ) {
                f32 ScrollXDiff = (-VS->ScrollPosition.X.Offset);
                f32 ScrollYDiff = (-VS->ScrollPosition.Y.Offset);
                f32 LoadingTDiff = (VS->LoadingTTarget - VS->LoadingT);

                VS->ScrollPosition.X.Offset += ScrollXDiff * APP_STATE->ScrollingAnimationRate;
                VS->ScrollPosition.Y.Offset += ScrollYDiff * APP_STATE->ScrollingAnimationRate;
                VS->LoadingT += LoadingTDiff * SlowRate;

                if (
                    AbsF32(LoadingTDiff) > 0.01f ||
                    AbsF32(ScrollXDiff) > 0.01f ||
                    AbsF32(ScrollYDiff) > 0.01f
                ) {
                    AppRequestFrame();
                }

                if (AbsF32(ScrollXDiff) <= 0.01f)
                    VS->ScrollPosition.X.Offset = 0.0f;

                if (AbsF32(ScrollYDiff) <= 0.01f)
                    VS->ScrollPosition.Y.Offset = 0.0f;

                ConfigNode* VConfig = ConfigNodeFromID(VS->CfgID);

                if (
                    ConfigNodeChildFromStr(
                        VConfig,
                        "selected"_s8
                    ) != EMPTY_CFG_NODE_VALUE
                ) {
                    if (VS->LoadingTTarget > 0.5f)
                        AppRequestFrame();

                    VS->LoadingTTarget = 0.0f;
                }
            }
        }
    }

    InputEventList Events = {};

    if (APP_STATE->FrameDepth == 1)
        Events = GetEvents(Scratch.MemPool, !APP_STATE->NumFramesRequested);

    Access* FrameAccessRestore = APP_STATE->FrameAccess;

    APP_STATE->FrameAccess = AccessOpen();

    f32 TargetHz = GetTerminalProperties()->DefaultRefreshRate;

    if (TargetHz < 1.0f)
        TargetHz = 60.0f;

    u64 FrameTimeTargetCapacityUSecs = (u64) (MILLION(1) / TargetHz);

    APP_STATE->FrameDeltaTime = 1.0f / TargetHz;

    u64 StartTimeUSecs = TimeNow();

    if (!APP_STATE->PopUpActive && APP_STATE->BindChangeActive) {
        if (InputPress(&Events, 0, INPUT_KIND_ESC)) {
            AppRequestFrame();
            APP_STATE->BindChangeActive = FALSE;
        }

        if (
            APP_STATE->BindChangeActive &&
            InputPress(&Events, 0, INPUT_KIND_DELETE)
        ) {
            AppRequestFrame();
            ConfigNodeRelease(
                APP_STATE->Config,
                ConfigNodeFromID(APP_STATE->BindChangeBindingID)
            );
            APP_STATE->BindChangeActive = FALSE;
        }

        for (
            InputEvent* Event = Events.Head, *Next = NULL;
            Event && APP_STATE->BindChangeActive;
            Event = Next
        ) {
            Next = Event->Next;

            if (
                Event->Kind == EVENT_KIND_PRESS &&
                Event->Input != INPUT_KIND_ESC &&
                Event->Input != INPUT_KIND_RETURN &&
                Event->Input != INPUT_KIND_BACKSPACE &&
                Event->Input != INPUT_KIND_DELETE &&
                Event->Input != INPUT_KIND_LEFT_MOUSE_BTN &&
                Event->Input != INPUT_KIND_RIGHT_MOUSE_BTN &&
                Event->Input != INPUT_KIND_MIDDLE_MOUSE_BTN &&
                Event->Input != INPUT_KIND_CTRL &&
                Event->Input != INPUT_KIND_ALT &&
                Event->Input != INPUT_KIND_SHIFT
            ) {
                APP_STATE->BindChangeActive = FALSE;

                ConfigNode* Binding = ConfigNodeFromID(
                    APP_STATE->BindChangeBindingID
                );

                if (Binding == EMPTY_CFG_NODE_VALUE) {
                    ConfigNode* Session = ConfigNodeChildFromStr(
                        ConfigNodeRoot(),
                        "session"_s8
                    );
                    ConfigNode* InputBindings = ConfigNodeChildFromStrOrAlloc(
                        APP_STATE->Config,
                        Session,
                        "keybindings"_s8
                    );

                    Binding = ConfigNodeNew(
                        APP_STATE->Config,
                        InputBindings,
                        ""_s8
                    );
                }

                ConfigNodeReleaseAllChildren(APP_STATE->Config, Binding);
                ConfigNodeNew(
                    APP_STATE->Config,
                    Binding,
                    APP_STATE->BindChangeCommandName
                );
                ConfigNodeNew(
                    APP_STATE->Config,
                    Binding,
                    INPUT_DISPLAY_STR_TABLE[Event->Input]
                );

                if (Event->Modifier & INPUT_MOD_KIND_CTRL)
                    ConfigNodeNew(APP_STATE->Config, Binding, "ctrl"_s8);

                if (Event->Modifier & INPUT_MOD_KIND_SHIFT)
                    ConfigNodeNew(APP_STATE->Config, Binding, "shift"_s8);

                if (Event->Modifier & INPUT_MOD_KIND_ALT)
                    ConfigNodeNew(APP_STATE->Config, Binding, "alt"_s8);

                u32 CodePoint = CodePointFromInput(
                    Event->Modifier, 
                    Event->Input
                );

                Text(&Events, CodePoint);
                ConsumeEvent(&Events, Event);
                AppRequestFrame();
                break;
            }
        }
    }

    APP_STATE->KeyMap = ConfigInputMapFromConfig(AppFrameMemPool());

    for (
        InputEvent* Event = Events.Head, *Next = NULL;
        Event;
        Event = Next
    ) {
        AppRegistersScope() {
            Next = Event->Next;

            b32 Take = FALSE;

            if (
                AppDragIsActive() &&
                Event->Kind == EVENT_KIND_RELEASE &&
                Event->Input == INPUT_KIND_LEFT_MOUSE_BTN
            ) {
                APP_STATE->DragDropState = APP_DRAG_DROP_STATE_DROPPING;
            }

            if (!Take && Event->Kind == EVENT_KIND_CONSOLE_CLOSE) {
                Take = TRUE;
                AppCmd(APP_COMMAND_KIND_EXIT);
            }

            if (Event->Kind == EVENT_KIND_CONSOLE_LOSE_FOCUS) {
                APP_STATE->MenuBarKeyHeld = FALSE;
                APP_STATE->MenuBarFocusPressStarted = FALSE;
            }

            // if (
            //     Event->Kind == EVENT_KIND_PRESS &&
            //     APP_STATE->ErrorStrSize &&
            //     APP_STATE->ErrorFrameIndex < APP_STATE->FrameIndex
            // ) {
            //     APP_STATE->ErrorStrSize = 0;
            //     AppRequestFrame();
            // }

            if (APP_STATE->AltMenuBarEnabled) {
                if (
                    !Take &&
                    Event->Kind == EVENT_KIND_PRESS &&
                    Event->Input == INPUT_KIND_ALT &&
                    !Event->Modifier &&
                    !Event->IsRepeat
                ) {
                    Take = TRUE;
                    AppRequestFrame();
                    APP_STATE->MenuBarFocusedOnPress = APP_STATE->MenuBarFocused;
                    APP_STATE->MenuBarKeyHeld = TRUE;
                    APP_STATE->MenuBarFocusPressStarted = TRUE;
                }

                if (
                    APP_STATE->MenuBarFocused &&
                    Event->Kind == EVENT_KIND_PRESS &&
                    Event->Input == INPUT_KIND_ALT &&
                    !Event->Modifier &&
                    !Event->IsRepeat
                ) {
                    Take = TRUE;
                    AppRequestFrame();
                    APP_STATE->MenuBarFocused = FALSE;
                } else if (
                    APP_STATE->MenuBarFocusPressStarted &&
                    !APP_STATE->MenuBarFocused &&
                    Event->Kind == EVENT_KIND_RELEASE &&
                    !Event->Modifier &&
                    Event->Input == INPUT_KIND_ALT &&
                    !Event->IsRepeat
                ) {
                    Take = TRUE;
                    AppRequestFrame();
                    APP_STATE->MenuBarFocused = !APP_STATE->MenuBarFocusedOnPress;
                    APP_STATE->MenuBarFocusPressStarted = FALSE;
                } else if (
                    Event->Kind == EVENT_KIND_PRESS &&
                    Event->Input == INPUT_KIND_ESC &&
                    APP_STATE->MenuBarFocused &&
                    !UIAnyContextMenuIsOpen()
                ) {
                    Take = FALSE;
                    AppRequestFrame();
                    APP_STATE->MenuBarFocused = FALSE;
                }

                if (
                    Event->Kind == EVENT_KIND_RELEASE &&
                    Event->Input == INPUT_KIND_ALT
                ) {
                    APP_STATE->MenuBarKeyHeld = FALSE;
                }
            }

            if (!Take && Event->Kind == EVENT_KIND_PRESS) {
                ConfigBinding Binding = {};

                Binding.Input = Event->Input;
                Binding.ModKind = Event->Modifier;

                ConfigInputMapNodePtrList KeyMapNodes = ConfigInputMapNodePtrListFromBinding(
                    Scratch.MemPool,
                    APP_STATE->KeyMap,
                    Binding
                );

                if (KeyMapNodes.Head) {
                    u32 HitCharacter = CodePointFromInput(
                        Event->Modifier, 
                        Event->Input
                    );
                    b32 IsPrintable = (
                        HitCharacter >= 32 && 
                        HitCharacter < 127
                    );

                    if (!HitCharacter || AllowTextHotkeys) {
                        AppCmd(
                            APP_COMMAND_KIND_RUN_COMMAND,
                            __Registers.CommandName = KeyMapNodes.Head->V->Name
                        );

                        if (AllowTextHotkeys && IsPrintable) {
                            Text(&Events, HitCharacter);
                            Next = Event->Next;
                        }

                        Take = TRUE;

                        if (Event->Modifier & INPUT_MOD_KIND_ALT)
                            APP_STATE->MenuBarFocusPressStarted = FALSE;
                    }
                } else if (
                    Event->Input >= INPUT_KIND_FUNC_1 && 
                    Event->Input <= INPUT_KIND_FUNC_9
                ) {
                    APP_STATE->MenuBarFocusPressStarted = FALSE;
                }

                AppRequestFrame();
            }

            if (!Take && Event->Kind == EVENT_KIND_TEXT) {
                u8 TextBytes[4] = {};
                Str8 Insertion = Str(
                    TextBytes,
                    UTF8Encode(TextBytes, Event->Character)
                );

                AppCmd(
                    APP_COMMAND_KIND_INSERT_TEXT, 
                    __Registers.String = Insertion
                );
                AppRequestFrame();
                Take = TRUE;

                if (Event->Modifier & INPUT_MOD_KIND_ALT)
                    APP_STATE->MenuBarFocusPressStarted = FALSE;
            }

            if (!Take) {
                Take = TRUE;
                AppCmd(APP_COMMAND_KIND_OS_EVENT, __Registers.OSEvent = Event);
            }

            if (Take)
                ConsumeEvent(&Events, Event);
        }
    }

    EvalCacheSelect(APP_STATE->ECache);

    EvalBaseContext* EvalBaseCtx = ArenaPushArrayZero(
        Scratch.MemPool,
        EvalBaseContext,
        1
    );

    {
        EvalBaseContext* Ctx = EvalBaseCtx;

        Ctx->SpaceGenFunc = AppEvalSpaceGen;
        Ctx->SpaceReadFunc = AppEvalSpaceRead;
        Ctx->SpaceWriteFunc = AppEvalSpaceWrite;
    }

    EvalBaseContextSelect(EvalBaseCtx);

    EvalStr2ExprMap* MacroMap = ArenaPushArrayZero(
        Scratch.MemPool,
        EvalStr2ExprMap,
        1
    );

    *MacroMap = EvalStr2ExprMapAlloc(Scratch.MemPool, 512);
    APP_STATE->MetaName2TypeMap = ArenaPushArrayZero(
        AppFrameMemPool(),
        EvalStr2TypeKeyMap,
        1
    );
    APP_STATE->MetaName2TypeMap[0] = EvalStr2TypeKeyMapAlloc(
        AppFrameMemPool(),
        256
    );

    EVExpandRuleTable* ExpandRuleTable = ArenaPushArrayZero(
        Scratch.MemPool,
        EVExpandRuleTable,
        1
    );

    APP_STATE->ViewUIRuleMap = AppViewUIRuleMapAlloc(
        Scratch.MemPool,
        512
    );

    PROFILE(BuildExtraTypesAndMapsTimer) {
        {
            Str8 Names[] = {
                "commands"_s8,
                "tab_commands"_s8
            };

            for (u64 Index = 0; Index < ARRAY_COUNT(Names); ++Index) {
                Str8 Name = Names[Index];
                EvalTypeKey TypeKey = EvalTypeKeyConstruct(
                    __Params.Kind = EVAL_TYPE_KIND_SET,
                    __Params.Flags = EVAL_TYPE_FLAG_STUB_SINGLE_LINE_EXPANSION,
                    __Params.Name = Name,
                    __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(Commands),
                    __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(Commands),
                    __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Commands)
                );
                EvalExpr* Expr = EvalPushExpr(
                    Scratch.MemPool,
                    EVAL_EXPR_KIND_LEAF_OFFSET,
                    Rng((u64) 0, (u64) 0)
                );

                Expr->TypeKey = TypeKey;
                Expr->Space = EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_QUERY);
                MapInsert(Scratch.MemPool, MacroMap, Name, Expr);
            }
        }

        {
            Str8 Names[] = {
                "themes"_s8
            };

            for (u64 Index = 0; Index < ARRAY_COUNT(Names); ++Index) {
                Str8 Name = Names[Index];
                EvalTypeKey TypeKey = EvalTypeKeyConstruct(
                    __Params.Kind = EVAL_TYPE_KIND_SET,
                    __Params.Flags = EVAL_TYPE_FLAG_STUB_SINGLE_LINE_EXPANSION,
                    __Params.Name = Name,
                    __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(Themes),
                    __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(Themes),
                    __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Themes)
                );
                EvalExpr* Expr = EvalPushExpr(
                    Scratch.MemPool,
                    EVAL_EXPR_KIND_LEAF_OFFSET,
                    Rng((u64) 0, (u64) 0)
                );

                Expr->TypeKey = TypeKey;
                Expr->Space = EvalSpaceAlloc(
                    APP_EVAL_SPACE_KIND_META_QUERY
                );
                MapInsert(Scratch.MemPool, MacroMap, Name, Expr);
            }
        }

        {
            Str8 Names[] = {
                "views"_s8
            };

            for (u64 Index = 0; Index < ARRAY_COUNT(Names); ++Index) {
                Str8 Name = Names[Index];
                EvalTypeKey TypeKey = EvalTypeKeyConstruct(
                    __Params.Kind = EVAL_TYPE_KIND_SET,
                    __Params.Flags = EVAL_TYPE_FLAG_STUB_SINGLE_LINE_EXPANSION,
                    __Params.Name = Name,
                    __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(Views),
                    __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(Views),
                    __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Views)
                );
                EvalExpr* Expr = EvalPushExpr(
                    Scratch.MemPool,
                    EVAL_EXPR_KIND_LEAF_OFFSET,
                    Rng((u64) 0, (u64) 0)
                );

                Expr->TypeKey = TypeKey;
                Expr->Space = EvalSpaceAlloc(
                    APP_EVAL_SPACE_KIND_META_QUERY
                );
                MapInsert(Scratch.MemPool, MacroMap, Name, Expr);
            }
        }

        for (
            u64 Index = 0; 
            Index < ARRAY_COUNT(APP_NAME_SCHEMA_INFO_TABLE); 
            ++Index
        ) {
            Str8 Name = APP_NAME_SCHEMA_INFO_TABLE[Index].Name;
            EvalTypeKey TypeKey = EvalTypeKeyConstruct(
                __Params.Name = Name,
                __Params.Kind = EVAL_TYPE_KIND_SET,
                __Params.IRExtFunc = EVAL_TYPE_IR_EXT_FUNC_NAME(Schema),
                __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(Schema),
                __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(Schema),
                __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Schema)
            );

            MapInsert(
                AppFrameMemPool(),
                APP_STATE->MetaName2TypeMap,
                Name,
                TypeKey
            );
        }

        {
            Str8 Name = "config"_s8;
            EvalTypeKey TypeKey = EvalTypeKeyConstruct(
                __Params.Name = Name,
                __Params.Kind = EVAL_TYPE_KIND_SET,
                __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(Configs)
            );
            EvalExpr* Expr = EvalPushExpr(
                Scratch.MemPool,
                EVAL_EXPR_KIND_LEAF_OFFSET,
                Rng((u64) 0, (u64) 0)
            );

            Expr->TypeKey = TypeKey;
            Expr->Space = EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_QUERY);
            MapInsert(Scratch.MemPool, MacroMap, Name, Expr);
            MapInsert(
                AppFrameMemPool(), 
                APP_STATE->MetaName2TypeMap, 
                Name, 
                TypeKey
            );
        }

        Str8 EvalCfgNames[] = {
            "recent_session"_s8
        };

        for (u64 Index = 0; Index < ARRAY_COUNT(EvalCfgNames); ++Index) {
            Str8 CfgName = EvalCfgNames[Index];
            Str8 CollectionName = AppPluralFromCodeName(CfgName);
            EvalTypeKey CollectionTypeKey = EvalTypeKeyConstruct(
                __Params.Kind = EVAL_TYPE_KIND_SET,
                __Params.IRExtFunc = EVAL_TYPE_IR_EXT_FUNC_NAME(ConfigsSlice),
                __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(ConfigsSlice),
                __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(ConfigsSlice),
                __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(ConfigsSlice),
                __Params.ExpandRule.ExpandIDFromNumFunc = EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(ConfigsSlice),
                __Params.ExpandRule.ExpandNumFromIDFunc = EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(ConfigsSlice)
            );
            EvalExpr* Expr = EvalPushExpr(
                Scratch.MemPool, 
                EVAL_EXPR_KIND_LEAF_OFFSET, 
                Rng((u64) 0, (u64) 0)
            );

            Expr->TypeKey = CollectionTypeKey;
            Expr->Space = EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_QUERY);
            MapInsert(Scratch.MemPool, MacroMap, CollectionName, Expr);
            MapInsert(
                AppFrameMemPool(), 
                APP_STATE->MetaName2TypeMap, 
                CollectionName, 
                CollectionTypeKey
            );
        }

        for (
            u64 Index = 0; 
            Index < ARRAY_COUNT(APP_NAME_SCHEMA_INFO_TABLE); 
            ++Index
        ) {
            Str8 Name = APP_NAME_SCHEMA_INFO_TABLE[Index].Name;
            MDNodePtrList Schemas = ConfigSchemasFromName(
                Scratch.MemPool,
                APP_STATE->CfgSchemaTable,
                Name
            );
            b32 IsIndividuallyEvallable = FALSE;

            for (
                MDNodePtrNode* N = Schemas.Head;
                N;
                N = N->Next
            ) {
                if (
                    MDNodeHasChild(N->V, "label"_s8, 0) ||
                    MDNodeHasChild(N->V, "executable"_s8, 0)
                ) {
                    IsIndividuallyEvallable = TRUE;
                    break;
                }
            }

            if (IsIndividuallyEvallable) {
                EvalTypeKey TypeKey = EvalStr2TypeKeyMapLookup(
                    APP_STATE->MetaName2TypeMap, 
                    Name
                );
                ConfigNodePtrList Cfgs = ConfigNodeTopLevelListFromStr(
                    Scratch.MemPool,
                    Name
                );

                for (
                    ConfigNodePtrNode* N = Cfgs.Head;
                    N;
                    N = N->Next
                ) {
                    ConfigNode* Cfg = N->V;
                    Str8 Label = AppLabelFromConfig(Cfg);

                    if (Label.Size) {
                        EvalSpace Space = AppEvalSpaceFromConfig(Cfg);
                        EvalExpr* Expr = EvalPushExpr(
                            Scratch.MemPool,
                            EVAL_EXPR_KIND_LEAF_OFFSET,
                            Rng((u64) 0, (u64) 0)
                        );

                        Expr->Space = Space;
                        Expr->Mode = EVAL_MODE_OFFSET;
                        Expr->TypeKey = TypeKey;
                        MapInsert(
                            Scratch.MemPool,
                            MacroMap,
                            Label,
                            Expr
                        );
                    }
                }
            }
        }


        {
            ConfigNodePtrList Windows = ConfigNodeTopLevelListFromStr(
                Scratch.MemPool,
                "window"_s8
            );

            for (
                ConfigNodePtrNode* N = Windows.Head;
                N;
                N = N->Next
            ) {
                ConfigNode* Window = N->V;

                {
                    EvalTypeKey TypeKey = EvalStr2TypeKeyMapLookup(
                        APP_STATE->MetaName2TypeMap,
                        Window->String
                    );
                    EvalSpace Space = AppEvalSpaceFromConfig(Window);
                    EvalExpr* Expr = EvalPushExpr(
                        Scratch.MemPool,
                        EVAL_EXPR_KIND_LEAF_OFFSET,
                        Rng((u64) 0, (u64) 0)
                    );

                    Expr->Space = Space;
                    Expr->Mode = EVAL_MODE_OFFSET;
                    Expr->TypeKey = TypeKey;
                    MapInsert(
                        Scratch.MemPool,
                        MacroMap,
                        ArenaPushStrFmt(
                            Scratch.MemPool,
                            "query:config.$%llx",
                            Window->ID
                        ),
                        Expr
                    );
                }

                ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                    Scratch.MemPool,
                    Window
                );

                for (
                    ConfigPanelNode* P = PanelTree.Root;
                    P != EMPTY_CFG_PANEL_NODE_VALUE;
                    P = ConfigPanelNodeRecDepthFirstPre(PanelTree.Root, P).Next
                ) {
                    for (
                        ConfigNodePtrNode* TabNode = P->Tabs.Head;
                        TabNode;
                        TabNode = TabNode->Next
                    ) {
                        ConfigNode* Tab = TabNode->V;
                        EvalTypeKey TypeKey = EvalStr2TypeKeyMapLookup(
                            APP_STATE->MetaName2TypeMap,
                            Tab->String
                        );
                        EvalSpace Space = AppEvalSpaceFromConfig(Tab);
                        EvalExpr* Expr = EvalPushExpr(
                            Scratch.MemPool,
                            EVAL_EXPR_KIND_LEAF_OFFSET,
                            Rng((u64) 0, (u64) 0)
                        );

                        Expr->Space = Space;
                        Expr->Mode = EVAL_MODE_OFFSET;
                        Expr->TypeKey = TypeKey;
                        MapInsert(
                            Scratch.MemPool,
                            MacroMap,
                            ArenaPushStrFmt(
                                Scratch.MemPool,
                                "query:config.$%llx",
                                Tab->ID
                            ),
                            Expr
                        );

                    }
                }
            }
        }

        {
            EvalTypeKey TypeKey = EvalStr2TypeKeyMapLookup(
                APP_STATE->MetaName2TypeMap,
                "session"_s8
            );
            EvalSpace Space = AppEvalSpaceFromConfig(
                ConfigNodeChildFromStr(ConfigNodeRoot(), "session"_s8)
            );
            EvalExpr* Expr = EvalPushExpr(
                Scratch.MemPool,
                EVAL_EXPR_KIND_LEAF_OFFSET,
                Rng((u64) 0, (u64) 0)
            );

            Expr->Space = Space;
            Expr->Mode = EVAL_MODE_OFFSET;
            Expr->TypeKey = TypeKey;
            MapInsert(
                Scratch.MemPool,
                MacroMap,
                "session_settings"_s8,
                Expr
            );
        }

        {
            MapInsert(
                AppFrameMemPool(),
                APP_STATE->MetaName2TypeMap,
                "theme_colours"_s8,
                EvalTypeKeyConstruct(
                    __Params.Kind = EVAL_TYPE_KIND_SET,
                    __Params.Flags = EVAL_TYPE_FLAG_STUB_SINGLE_LINE_EXPANSION,
                    __Params.Name = "theme_colours"_s8,
                    __Params.IRExtFunc = EVAL_TYPE_IR_EXT_FUNC_NAME(ConfigsSlice),
                    __Params.AccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(ConfigsSlice),
                    __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(ConfigsQuery),
                    __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(ConfigsSlice),
                    __Params.ExpandRule.ExpandIDFromNumFunc = EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(ConfigsSlice),
                    __Params.ExpandRule.ExpandNumFromIDFunc = EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(ConfigsSlice)
                )
            );
        }

        struct {
            Str8                          Name;
            b32                           InheritedByMembers;
            b32                           InheritedByElements;
            b32                           ArrayLike;
            EvalTypeIRExtFuncType*        IRExtFunc;
            EvalTypeAccessFuncType*       AccessFunc;
            EvalTypeExpandRule            ExpandRule;
            AppViewUIFuncType*            ViewUIFunc;
            EVExpandRuleInfoHookFuncType* ExpandInfoHookFunc;
        } LensTable[] = {
            {
                "raw"_s8,
                FALSE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "bin"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "oct"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "dec"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "hex"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "digits"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "no_string"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "no_char"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "no_addr"_s8,
                TRUE,
                TRUE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "columns"_s8,
                FALSE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "flatten"_s8,
                FALSE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                {}
            },
            {
                "range1"_s8,
                FALSE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                {}
            }
        };

        {
            for (u64 Index = 0; Index < ARRAY_COUNT(LensTable); ++Index) {
                if (LensTable[Index].ViewUIFunc) {
                    MapInsert(
                        Scratch.MemPool,
                        APP_STATE->ViewUIRuleMap,
                        LensTable[Index].Name,
                        LensTable[Index].ViewUIFunc
                    );
                }

                if (LensTable[Index].ExpandInfoHookFunc) {
                    TablePushNew(
                        Scratch.MemPool,
                        ExpandRuleTable,
                        __Rule.String = LensTable[Index].Name,
                        __Rule.InfoHookFunc = LensTable[Index].ExpandInfoHookFunc
                    );
                }
            }
        }

        for (u64 Index = 0; Index < ARRAY_COUNT(LensTable); ++Index) {
            EvalTypeFlag TypeFlags = 0;

            if (LensTable[Index].InheritedByMembers)
                TypeFlags |= EVAL_TYPE_FLAG_INHERITED_BY_MEMBERS;

            if (LensTable[Index].InheritedByElements)
                TypeFlags |= EVAL_TYPE_FLAG_INHERITED_BY_ELEMENTS;

            if (LensTable[Index].ArrayLike)
                TypeFlags |= EVAL_TYPE_FLAG_ARRAY_LIKE_EXPANSION;

            EvalExpr* Expr = EvalPushExpr(
                Scratch.MemPool,
                EVAL_EXPR_KIND_LEAF_OFFSET,
                Rng((u64) 0, (u64) 0)
            );

            Expr->TypeKey = EvalTypeKeyConstruct(
                __Params.Kind = EVAL_TYPE_KIND_LENS_SPEC,
                __Params.Flags = TypeFlags,
                __Params.Name = LensTable[Index].Name,
                __Params.IRExtFunc = LensTable[Index].IRExtFunc,
                __Params.AccessFunc = LensTable[Index].AccessFunc,
                __Params.ExpandRule = LensTable[Index].ExpandRule
            );
            MapInsert(
                Scratch.MemPool,
                MacroMap,
                LensTable[Index].Name,
                Expr
            );
        }
    }

    EVSelectExpandRuleTable(ExpandRuleTable);

    EvalIRContext* IRCtx = ArenaPushArrayZero(
        Scratch.MemPool,
        EvalIRContext,
        1
    );

    IRCtx->MacroMap = MacroMap;
    EvalIRContextSelect(IRCtx);

    EvalInterpretContext* InterpCtx = ArenaPushArrayZero(
        Scratch.MemPool,
        EvalInterpretContext,
        1
    );

    InterpCtx->PrimarySpace = EvalSpaceAlloc(APP_EVAL_SPACE_KIND_META_VIEW);
    EvalInterpContextSelect(InterpCtx);
    APP_STATE->AltMenuBarEnabled = AppSettingFromNameB32(
        "focus_menu_bar_with_alt"_s8
    );

    AppCommand* Command = NULL;

    if (APP_STATE->FrameDepth == 1) {
        for (; AppNextCommand(&Command);) {
            AppRegistersScope() {
                AppCommandKind Kind = AppCommandKindFromStr(Command->Name);

                CopyRegisters(
                    AppFrameMemPool(), 
                    Registers(), 
                    Command->Registers
                );
                AppRequestFrame();

                u64 PanelSiblingOffset = 0;
                u64 PanelChildOffset = 0;
                v2i32 PanelChangeDirection = {};
                UIEvent Event = {};

                switch (Kind) {
                    default: {
                        if (
                            Kind >= APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD
                        ) {
                            u64 FastPathIndex = (
                                Kind - APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD
                            );
                            Str8 ViewName = APP_TAB_FAST_PATH_VIEW_NAME_TABLE[FastPathIndex];
                            b32 NeedsFile = !!(
                                APP_COMMAND_KIND_INFO_TABLE[Kind].Flags &
                                APP_COMMAND_KIND_FLAG_LIST_IN_FILE
                            );

                            if (NeedsFile && !Registers()->FilePath.Size) {
                                AppCmd(APP_COMMAND_KIND_OPEN_FILES);
                            } else {
                                AppCmd(
                                    APP_COMMAND_KIND_BUILD_TAB,
                                    __Registers.String = ViewName,
                                    __Registers.FilePath = NeedsFile 
                                        ? Registers()->FilePath 
                                        : ""_s8
                                );
                            }
                        }
                    } break;

                    case APP_COMMAND_KIND_EXIT: {
                        AppWriteSessionData();
                        APP_STATE->ShouldQuit = TRUE;
                    } break;

                    case APP_COMMAND_KIND_OPEN_PALETTE: {
                        AppCmd(
                            APP_COMMAND_KIND_PUSH_QUERY,
                            __Registers.CommandName = ""_s8,
                            __Registers.Expression = "commands"_s8
                        );
                    } break;

                    case APP_COMMAND_KIND_RUN_COMMAND:
                    case APP_COMMAND_KIND_OPEN_TAB: {
                        AppCommandKindInfo* Info = AppCommandKindInfoFromStr(
                            Command->Registers->CommandName
                        );

                        if (!(Info->Query.Kind & APP_QUERY_KIND_REQUIRED)) {
                            AppRegistersScope(__Registers.CommandName = ""_s8) {
                                AppPushCmd(
                                    Command->Registers->CommandName, 
                                    Registers()
                                );
                            }
                        } else {
                            AppCmd(
                                APP_COMMAND_KIND_PUSH_QUERY,
                                __Registers.Expression = Info->Query.Expression
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_OS_EVENT: {
                        InputEvent* OSEvent = Registers()->OSEvent;

                        if (!OSEvent)
                            break;

                        UIEventKind EventKind = UI_EVENT_KIND_NULL;

                        switch (OSEvent->Kind) {
                            default: {} break;

                            case EVENT_KIND_PRESS: {
                                EventKind = UI_EVENT_KIND_PRESS;
                            } break;

                            case EVENT_KIND_RELEASE: {
                                EventKind = UI_EVENT_KIND_RELEASE;
                            } break;

                            case EVENT_KIND_MOUSE_MOVE: {
                                EventKind = UI_EVENT_KIND_MOUSE_MOVE;
                            } break;

                            case EVENT_KIND_TEXT: {
                                EventKind = UI_EVENT_KIND_TEXT;
                            } break;

                            case EVENT_KIND_SCROLL: {
                                EventKind = UI_EVENT_KIND_SCROLL;
                            } break;

                            case EVENT_KIND_FILE_DROP: {
                                EventKind = UI_EVENT_KIND_FILE_DROP;
                            } break;
                        }

                        if (EventKind != UI_EVENT_KIND_NULL) {
                            Event.Kind = EventKind;
                            Event.Input = OSEvent->Input;
                            Event.Modifiers = OSEvent->Modifier;

                            if (OSEvent->Kind == EVENT_KIND_TEXT) {
                                u8* TextBytes = ArenaPushArrayZero(
                                    AppFrameMemPool(), 
                                    u8, 
                                    4
                                );

                                Event.String = Str(
                                    TextBytes,
                                    UTF8Encode(TextBytes, OSEvent->Character)
                                );
                            }

                            Event.Paths = OSEvent->Strings;
                            Event.Position = OSEvent->Position;
                            Event.DeltaF32 = OSEvent->PositionDelta;
                            Event.TimestampUSecs = OSEvent->TimeStampUSecs;
                        }
                    } break;

                    case APP_COMMAND_KIND_POP_UP_ACCEPT: {
                        APP_STATE->PopUpActive = FALSE;
                        APP_STATE->PopUpKey = {};

                        for (
                            AppCommandNode* N = APP_STATE->PopUpCommands.Head;
                            N;
                            N = N->Next
                        ) {
                            AppPushCmd(N->Command.Name, N->Command.Registers);
                        }
                    } break;

                    case APP_COMMAND_KIND_POP_UP_CANCEL: {
                        APP_STATE->PopUpActive = FALSE;
                        APP_STATE->PopUpKey = {};
                    } break;

                    case APP_COMMAND_KIND_RESET_TO_DEFAULT_BINDINGS: {
                        ConfigNode* Session = ConfigNodeChildFromStr(
                            ConfigNodeRoot(),
                            "session"_s8
                        );
                        ConfigNodePtrList AllInputBindings = ConfigNodeChildListFromStr(
                            Scratch.MemPool,
                            Session,
                            "keybindings"_s8
                        );

                        for (
                            ConfigNodePtrNode* N = AllInputBindings.Head;
                            N;
                            N = N->Next
                        ) {
                            ConfigNodeRelease(APP_STATE->Config, N->V);
                        }

                        ConfigNode* InputBindings = ConfigNodeNew(
                            APP_STATE->Config,
                            Session,
                            "keybindings"_s8
                        );

                        for (
                            u32 Index = 0;
                            Index < ARRAY_COUNT(APP_DEFAULT_BINDING_TABLE);
                            ++Index
                        ) {
                            Str8 Name = APP_DEFAULT_BINDING_TABLE[Index].String;
                            ConfigBinding Binding = APP_DEFAULT_BINDING_TABLE[Index].Binding;
                            ConfigNode* BindingRoot = ConfigNodeNew(
                                APP_STATE->Config,
                                InputBindings,
                                ""_s8
                            );

                            ConfigNodeNew(
                                APP_STATE->Config,
                                BindingRoot,
                                Name
                            );
                            ConfigNodeNew(
                                APP_STATE->Config,
                                BindingRoot,
                                INPUT_DISPLAY_STR_TABLE[Binding.Input]
                            );

                            if (Binding.ModKind & INPUT_MOD_KIND_CTRL) {
                                ConfigNodeNew(
                                    APP_STATE->Config,
                                    BindingRoot,
                                    "ctrl"_s8
                                );
                            }

                            if (Binding.ModKind & INPUT_MOD_KIND_SHIFT) {
                                ConfigNodeNew(
                                    APP_STATE->Config,
                                    BindingRoot,
                                    "shift"_s8
                                );
                            }

                            if (Binding.ModKind & INPUT_MOD_KIND_ALT) {
                                ConfigNodeNew(
                                    APP_STATE->Config,
                                    BindingRoot,
                                    "alt"_s8
                                );
                            }
                        }
                    } break;

                    case APP_COMMAND_KIND_NEW_PANEL_LEFT:
                    case APP_COMMAND_KIND_NEW_PANEL_UP:
                    case APP_COMMAND_KIND_NEW_PANEL_RIGHT:
                    case APP_COMMAND_KIND_NEW_PANEL_DOWN:
                    case APP_COMMAND_KIND_SPLIT_PANEL: {
                        Direction2D SplitDir = DIR_2D_INVALID;
                        ConfigNode* SplitPanel = ConfigNodeFromID(
                            Registers()->Panel
                        );

                        if (Kind == APP_COMMAND_KIND_NEW_PANEL_LEFT) {
                            SplitDir = DIR_2D_LEFT;
                        } else if (Kind == APP_COMMAND_KIND_NEW_PANEL_UP) {
                            SplitDir = DIR_2D_UP;
                        } else if (Kind == APP_COMMAND_KIND_NEW_PANEL_RIGHT) {
                            SplitDir = DIR_2D_RIGHT;
                        } else if (Kind == APP_COMMAND_KIND_NEW_PANEL_DOWN) {
                            SplitDir = DIR_2D_DOWN;
                        } else {
                            SplitDir = Registers()->Direction;
                            SplitPanel = ConfigNodeFromID(Registers()->DstPanel);
                        }

                        if (
                            SplitDir != DIR_2D_INVALID &&
                            SplitPanel != EMPTY_CFG_NODE_VALUE
                        ) {
                            Axis2D SplitAxis = Axis2DFromDir2D(SplitDir);
                            SideKind SplitSide = SideFromDir2D(SplitDir);
                            ConfigNode* NewPanelConfig = EMPTY_CFG_NODE_VALUE;
                            ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                                Scratch.MemPool,
                                SplitPanel
                            );
                            ConfigPanelNode* Panel = ConfigPanelNodeFromConfigTree(
                                PanelTree.Root,
                                SplitPanel
                            );
                            ConfigPanelNode* Parent = Panel->Parent;

                            if (
                                Parent != EMPTY_CFG_PANEL_NODE_VALUE &&
                                Parent->SplitAxis == SplitAxis
                            ) {
                                NewPanelConfig = ConfigNodeAlloc(APP_STATE->Config);
                                ConfigNodeInsertChild(
                                    APP_STATE->Config,
                                    Parent->Config,
                                    (SplitSide == SIDE_MAX)
                                        ? SplitPanel
                                        : SplitPanel->Prev,
                                    NewPanelConfig
                                );
                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    NewPanelConfig,
                                    "%f",
                                    1.0f / (Parent->ChildCount + 1)
                                );

                                for (
                                    ConfigPanelNode* Child = Parent->Head;
                                    Child != EMPTY_CFG_PANEL_NODE_VALUE;
                                    Child = Child->Next
                                ) {
                                    f32 OldPercent = Child->PercentOfParent;
                                    f32 NewPercent = OldPercent * ((f32) Parent->ChildCount / (Parent->ChildCount + 1));

                                    ConfigNodeEquipStr(
                                        APP_STATE->Config,
                                        Child->Config,
                                        "%f",
                                        NewPercent
                                    );
                                }
                            } else {
                                ConfigNode* SplitPanelPrev = Panel->Prev->Config;
                                ConfigNode* NewParent = ConfigNodeAlloc(APP_STATE->Config);
                                ConfigNode* NewSibling = ConfigNodeAlloc(APP_STATE->Config);

                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    NewParent,
                                    SplitPanel->String
                                );
                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    SplitPanel,
                                    "0.5"_s8
                                );
                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    NewSibling,
                                    "0.5"_s8
                                );

                                if (Parent->Config != EMPTY_CFG_NODE_VALUE) {
                                    ConfigNodeUnhook(
                                        APP_STATE->Config,
                                        Parent->Config,
                                        SplitPanel
                                    );
                                    ConfigNodeInsertChild(
                                        APP_STATE->Config,
                                        Parent->Config,
                                        SplitPanelPrev,
                                        NewParent
                                    );
                                } else {
                                    ConfigNode* Window = ConfigWindowFromConfig(
                                        SplitPanel
                                    );

                                    ConfigNodeEquipStr(
                                        APP_STATE->Config,
                                        NewParent,
                                        "panels"_s8
                                    );
                                    ConfigNodeInsertChild(
                                        APP_STATE->Config,
                                        Window,
                                        Window->Tail,
                                        NewParent
                                    );

                                    if (SplitAxis == AXIS_2D_X) {
                                        ConfigNodeChildFromStrOrAlloc(
                                            APP_STATE->Config,
                                            Window,
                                            "split_x"_s8
                                        );
                                    } else {
                                        ConfigNodeRelease(
                                            APP_STATE->Config,
                                            ConfigNodeChildFromStr(
                                                Window,
                                                "split_x"_s8
                                            )
                                        );
                                    }
                                }

                                ConfigNode* Min = SplitPanel;
                                ConfigNode* Max = NewSibling;

                                if (SplitSide == SIDE_MIN)
                                    SWAP(ConfigNode*, Min, Max);

                                ConfigNodeInsertChild(
                                    APP_STATE->Config,
                                    NewParent,
                                    NewParent->Tail,
                                    Min
                                );
                                ConfigNodeInsertChild(
                                    APP_STATE->Config,
                                    NewParent,
                                    NewParent->Tail,
                                    Max
                                );
                                NewPanelConfig = NewSibling;
                            }

                            ConfigNode* DragTab = ConfigNodeFromID(Registers()->View);
                            ConfigNode* OriginPanel = DragTab->Parent;

                            if (
                                Kind == APP_COMMAND_KIND_SPLIT_PANEL &&
                                DragTab != EMPTY_CFG_NODE_VALUE &&
                                OriginPanel != EMPTY_CFG_NODE_VALUE
                            ) {
                                ConfigNodeUnhook(
                                    APP_STATE->Config,
                                    OriginPanel,
                                    DragTab
                                );
                                ConfigNodeInsertChild(
                                    APP_STATE->Config,
                                    NewPanelConfig,
                                    NewPanelConfig->Tail,
                                    DragTab
                                );
                                AppCmd(
                                    APP_COMMAND_KIND_FOCUS_TAB,
                                    __Registers.Tab = DragTab->ID
                                );
                            }

                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_PANEL,
                                __Registers.Panel = NewPanelConfig->ID
                            );

                            if (
                                Kind == APP_COMMAND_KIND_SPLIT_PANEL &&
                                DragTab != EMPTY_CFG_NODE_VALUE &&
                                OriginPanel != EMPTY_CFG_NODE_VALUE
                            ) {
                                ConfigPanelTree OriginPanelTree = ConfigPanelTreeFromConfig(
                                    Scratch.MemPool,
                                    OriginPanel
                                );
                                ConfigPanelNode* OriginPanelNode = ConfigPanelNodeFromConfigTree(
                                    OriginPanelTree.Root,
                                    OriginPanel
                                );

                                if (
                                    OriginPanelNode->SelectedTab == EMPTY_CFG_NODE_VALUE &&
                                    OriginPanelNode->Tabs.Head
                                ) {
                                    AppCmd(
                                        APP_COMMAND_KIND_FOCUS_TAB,
                                        __Registers.Tab = OriginPanelNode->Tabs.Head->V->ID
                                    );
                                }

                                if (
                                    OriginPanel != SplitPanel &&
                                    !OriginPanelNode->Tabs.Count
                                ) {
                                    AppCmd(
                                        APP_COMMAND_KIND_CLOSE_PANEL,
                                        __Registers.Panel = OriginPanel->ID
                                    );
                                }
                            }
                        }
                    } break;

                    case APP_COMMAND_KIND_NEXT_PANEL:
                    case APP_COMMAND_KIND_PREV_PANEL: {
                        if (Kind == APP_COMMAND_KIND_NEXT_PANEL) {
                            PanelSiblingOffset = OFFSETOF(ConfigPanelNode, Next);
                            PanelChildOffset = OFFSETOF(ConfigPanelNode, Head);
                        } else {
                            PanelSiblingOffset = OFFSETOF(ConfigPanelNode, Prev);
                            PanelChildOffset = OFFSETOF(ConfigPanelNode, Tail);
                        }

                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool,
                            ConfigNodeFromID(Registers()->Window)
                        );
                        ConfigPanelNode* NextFocused = EMPTY_CFG_PANEL_NODE_VALUE;

                        for (
                            ConfigPanelNode* P = PanelTree.Focused;
                            P != EMPTY_CFG_PANEL_NODE_VALUE;
                            P = ConfigPanelNodeRecDepthFirst(
                                PanelTree.Root,
                                P,
                                PanelSiblingOffset,
                                PanelChildOffset
                            ).Next
                        ) {
                            if (
                                P != PanelTree.Focused &&
                                P->Head == EMPTY_CFG_PANEL_NODE_VALUE
                            ) {
                                NextFocused = P;
                                break;
                            }
                        }

                        if (NextFocused == EMPTY_CFG_PANEL_NODE_VALUE) {
                            for (
                                ConfigPanelNode* P = PanelTree.Root;
                                P != EMPTY_CFG_PANEL_NODE_VALUE;
                                P = ConfigPanelNodeRecDepthFirst(
                                    PanelTree.Root,
                                    P,
                                    PanelSiblingOffset,
                                    PanelChildOffset
                                ).Next
                            ) {
                                if (
                                    P != PanelTree.Focused &&
                                    P->Head == EMPTY_CFG_PANEL_NODE_VALUE
                                ) {
                                    NextFocused = P;
                                    break;
                                }
                            }
                        }

                        if (NextFocused != EMPTY_CFG_PANEL_NODE_VALUE) {
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_PANEL,
                                __Registers.Panel = NextFocused->Config->ID
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_FOCUS_PANEL: {
                        ConfigNode* Panel = ConfigNodeFromID(Registers()->Panel);
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool,
                            Panel
                        );
                        ConfigNode* SelectionConfig = EMPTY_CFG_NODE_VALUE;

                        for (
                            ConfigPanelNode* P = PanelTree.Root;
                            P != EMPTY_CFG_PANEL_NODE_VALUE;
                            P = ConfigPanelNodeRecDepthFirstPre(PanelTree.Root, P).Next
                        ) {
                            ConfigNode* PConfig = P->Config;
                            ConfigNode* PSelection = ConfigNodeChildFromStr(
                                PConfig,
                                "selected"_s8
                            );

                            if (SelectionConfig == EMPTY_CFG_NODE_VALUE) {
                                SelectionConfig = PSelection;
                            } else {
                                for (
                                    ConfigNode* S = PSelection;
                                    S != EMPTY_CFG_NODE_VALUE;
                                    S = ConfigNodeChildFromStr(
                                        PConfig,
                                        "selected"_s8
                                    )
                                ) {
                                    ConfigNodeRelease(APP_STATE->Config, S);
                                }
                            }
                        }

                        if (Panel != EMPTY_CFG_NODE_VALUE) {
                            if (SelectionConfig == EMPTY_CFG_NODE_VALUE) {
                                SelectionConfig = ConfigNodeAlloc(APP_STATE->Config);
                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    SelectionConfig,
                                    "selected"_s8
                                );
                            }

                            ConfigNodeInsertChild(
                                APP_STATE->Config,
                                Panel,
                                EMPTY_CFG_NODE_VALUE,
                                SelectionConfig
                            );
                            APP_STATE->MenuBarFocused = FALSE;
                        }
                    } break;

                    case APP_COMMAND_KIND_FOCUS_PANEL_RIGHT:
                    case APP_COMMAND_KIND_FOCUS_PANEL_LEFT:
                    case APP_COMMAND_KIND_FOCUS_PANEL_UP:
                    case APP_COMMAND_KIND_FOCUS_PANEL_DOWN: {
                        if (Kind == APP_COMMAND_KIND_FOCUS_PANEL_RIGHT)
                            PanelChangeDirection = Vec(1, 0);
                        else if (Kind == APP_COMMAND_KIND_FOCUS_PANEL_LEFT)
                            PanelChangeDirection = Vec(-1, 0);
                        else if (Kind == APP_COMMAND_KIND_FOCUS_PANEL_UP)
                            PanelChangeDirection = Vec(0, -1);
                        else
                            PanelChangeDirection = Vec(0, 1);

                        ConfigNode* Window = ConfigNodeFromID(
                            Registers()->Window
                        );
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(Scratch.MemPool, Window);
                        ConfigPanelNode* SourcePanel = PanelTree.Focused;
                        r2f32 SourcePanelRect = ConfigTargetRectFromPanelNode(
                            Rng(
                                Vec(0.0f, 0.0f),
                                Vec(1000.0f, 1000.0f)
                            ),
                            PanelTree.Root,
                            SourcePanel
                        );
                        v2f32 SourcePanelCentre = Centre(SourcePanelRect);
                        v2f32 SourcePanelHalfDim = Length(SourcePanelRect) * 0.5f;
                        v2f32 TravelDim = SourcePanelHalfDim + Vec(10.0f, 10.0f);
                        v2f32 TravelDistance = (
                            SourcePanelCentre +
                            (
                                TravelDim * Vec(
                                    (f32) PanelChangeDirection.X,
                                    (f32) PanelChangeDirection.Y
                                )
                            )
                        );
                        ConfigPanelNode* DestinationRoot = EMPTY_CFG_PANEL_NODE_VALUE;

                        for (
                            ConfigPanelNode* P = PanelTree.Root;
                            P != EMPTY_CFG_PANEL_NODE_VALUE;
                            P = ConfigPanelNodeRecDepthFirstPre(PanelTree.Root, P).Next
                        ) {
                            if (P == SourcePanel || P->Head != EMPTY_CFG_PANEL_NODE_VALUE)
                                continue;

                            r2f32 PRect = ConfigTargetRectFromPanelNode(
                                Rng(
                                    Vec(0.0f, 0.0f),
                                    Vec(1000.0f, 1000.0f)
                                ),
                                PanelTree.Root,
                                P
                            );

                            if (InRange(PRect, TravelDistance)) {
                                DestinationRoot = P;
                                break;
                            }
                        }

                        if (DestinationRoot != EMPTY_CFG_PANEL_NODE_VALUE) {
                            ConfigPanelNode* DestinationPanel = EMPTY_CFG_PANEL_NODE_VALUE;

                            for (
                                ConfigPanelNode* P = DestinationRoot;
                                P != EMPTY_CFG_PANEL_NODE_VALUE;
                                P = ConfigPanelNodeRecDepthFirstPre(DestinationRoot, P).Next
                            ) {
                                if (P->Head == EMPTY_CFG_PANEL_NODE_VALUE && P != SourcePanel) {
                                    DestinationPanel = P;
                                    break;
                                }
                            }

                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_PANEL,
                                __Registers.Panel = DestinationPanel->Config->ID
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_CLOSE_PANEL: {
                        ConfigNode* Window = ConfigNodeFromID(Registers()->Window);
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool,
                            Window
                        );
                        ConfigPanelNode* Panel = ConfigPanelNodeFromConfigTree(
                            PanelTree.Root,
                            ConfigNodeFromID(Registers()->Panel)
                        );
                        ConfigPanelNode* Parent = Panel->Parent;

                        if (Parent != EMPTY_CFG_PANEL_NODE_VALUE) {
                            ConfigPanelNode* NewFocused = EMPTY_CFG_PANEL_NODE_VALUE;

                            if (Parent->ChildCount == 2) {
                                ConfigPanelNode* KeepChild = (Panel == Parent->Head)
                                    ? Parent->Tail
                                    : Parent->Head;
                                ConfigPanelNode* Grandparent = Parent->Parent;
                                ConfigPanelNode* ParentPrev = Parent->Prev;
                                f32 PercentOfParent = Parent->PercentOfParent;

                                ConfigNodeUnhook(
                                    APP_STATE->Config,
                                    Parent->Config,
                                    KeepChild->Config
                                );
                                ConfigNodeUnhook(
                                    APP_STATE->Config,
                                    (Grandparent != EMPTY_CFG_PANEL_NODE_VALUE)
                                        ? Grandparent->Config
                                        : Window,
                                    Parent->Config
                                );
                                ConfigNodeRelease(
                                    APP_STATE->Config,
                                    Parent->Config
                                );

                                if (Grandparent == EMPTY_CFG_PANEL_NODE_VALUE) {
                                    if (KeepChild->SplitAxis == AXIS_2D_X) {
                                        ConfigNodeChildFromStrOrAlloc(
                                            APP_STATE->Config,
                                            Window,
                                            "split_x"_s8
                                        );
                                    } else {
                                        ConfigNodeRelease(
                                            APP_STATE->Config,
                                            ConfigNodeChildFromStr(
                                                Window,
                                                "split_x"_s8
                                            )
                                        );
                                    }

                                    ConfigNodeEquipStr(
                                        APP_STATE->Config,
                                        KeepChild->Config,
                                        "panels"_s8
                                    );
                                    ConfigNodeInsertChild(
                                        APP_STATE->Config,
                                        Window,
                                        Window->Tail,
                                        KeepChild->Config
                                    );
                                } else {
                                    ConfigNodeInsertChild(
                                        APP_STATE->Config,
                                        Grandparent->Config,
                                        ParentPrev->Config,
                                        KeepChild->Config
                                    );
                                    ConfigNodeEquipStr(
                                        APP_STATE->Config,
                                        KeepChild->Config,
                                        "%f",
                                        PercentOfParent
                                    );
                                }

                                if (
                                    Grandparent != EMPTY_CFG_PANEL_NODE_VALUE &&
                                    Grandparent->SplitAxis == KeepChild->SplitAxis &&
                                    KeepChild->Head != EMPTY_CFG_PANEL_NODE_VALUE
                                ) {
                                    ConfigNodeUnhook(
                                        APP_STATE->Config,
                                        Grandparent->Config,
                                        KeepChild->Config
                                    );

                                    ConfigNode* Prev = ParentPrev->Config;

                                    for (
                                        ConfigPanelNode* Child = KeepChild->Head;
                                        Child != EMPTY_CFG_PANEL_NODE_VALUE;
                                        Child = Child->Next
                                    ) {
                                        ConfigNodeUnhook(
                                            APP_STATE->Config,
                                            KeepChild->Config,
                                            Child->Config
                                        );
                                        ConfigNodeInsertChild(
                                            APP_STATE->Config,
                                            Grandparent->Config,
                                            Prev,
                                            Child->Config
                                        );
                                        ConfigNodeEquipStr(
                                            APP_STATE->Config,
                                            Child->Config,
                                            "%f",
                                            Child->PercentOfParent * PercentOfParent
                                        );
                                        Prev = Child->Config;
                                    }

                                    ConfigNodeRelease(
                                        APP_STATE->Config,
                                        KeepChild->Config
                                    );
                                }

                                NewFocused = KeepChild;
                            } else {
                                f32 RemovedSizePercent = Panel->PercentOfParent;

                                NewFocused = (Panel->Prev != EMPTY_CFG_PANEL_NODE_VALUE)
                                    ? Panel->Prev
                                    : Panel->Next;
                                ConfigNodeUnhook(
                                    APP_STATE->Config,
                                    Parent->Config,
                                    Panel->Config
                                );
                                ConfigNodeRelease(
                                    APP_STATE->Config,
                                    Panel->Config
                                );

                                for (
                                    ConfigPanelNode* Child = Parent->Head;
                                    Child != EMPTY_CFG_PANEL_NODE_VALUE;
                                    Child = Child->Next
                                ) {
                                    if (Child == Panel)
                                        continue;

                                    ConfigNodeEquipStr(
                                        APP_STATE->Config,
                                        Child->Config,
                                        "%f",
                                        Child->PercentOfParent / (1.0f - RemovedSizePercent)
                                    );
                                }
                            }

                            if (PanelTree.Focused == Panel) {
                                for (
                                    ConfigPanelNode* Child = NewFocused->Head;
                                    Child != EMPTY_CFG_PANEL_NODE_VALUE;
                                    Child = Child->Head
                                ) {
                                    NewFocused = Child;
                                }

                                AppCmd(
                                    APP_COMMAND_KIND_FOCUS_PANEL,
                                    __Registers.Panel = NewFocused->Config->ID
                                );
                            }
                        }
                    } break;

                    case APP_COMMAND_KIND_FOCUS_TAB: {
                        ConfigNode* Tab = ConfigNodeFromID(Registers()->Tab);
                        ConfigNode* Panel = Tab->Parent;
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool,
                            Panel
                        );
                        ConfigPanelNode* PanelNode = ConfigPanelNodeFromConfigTree(
                            PanelTree.Root,
                            Panel
                        );
                        ConfigNode* SelectionConfig = EMPTY_CFG_NODE_VALUE;

                        for (
                            ConfigNodePtrNode* N = PanelNode->Tabs.Head;
                            N;
                            N = N->Next
                        ) {
                            ConfigNode* TabSelectionConfig = ConfigNodeChildFromStr(
                                N->V,
                                "selected"_s8
                            );

                            if (SelectionConfig == EMPTY_CFG_NODE_VALUE) {
                                SelectionConfig = TabSelectionConfig;
                            } else {
                                for (
                                    ConfigNode* S = TabSelectionConfig;
                                    S != EMPTY_CFG_NODE_VALUE;
                                    S = ConfigNodeChildFromStr(N->V, "selected"_s8)
                                ) {
                                    ConfigNodeRelease(APP_STATE->Config, S);
                                }
                            }
                        }

                        if (Tab != EMPTY_CFG_NODE_VALUE) {
                            if (SelectionConfig == EMPTY_CFG_NODE_VALUE) {
                                SelectionConfig = ConfigNodeAlloc(APP_STATE->Config);
                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    SelectionConfig,
                                    "selected"_s8
                                );
                            }

                            ConfigNodeInsertChild(
                                APP_STATE->Config,
                                Tab,
                                EMPTY_CFG_NODE_VALUE,
                                SelectionConfig
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_NEXT_TAB: {
                        ConfigNode* Window = ConfigNodeFromID(Registers()->Window);
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool,
                            Window
                        );
                        ConfigPanelNode* Focused = PanelTree.Focused;
                        ConfigNodePtrNode* SelectedTabNode = NULL;

                        for (
                            ConfigNodePtrNode* N = Focused->Tabs.Head;
                            N;
                            N = N->Next
                        ) {
                            if (N->V == Focused->SelectedTab) {
                                SelectedTabNode = N;
                                break;
                            }
                        }

                        ConfigNode* NextSelectedTab = EMPTY_CFG_NODE_VALUE;
                        u64 Index = 0;

                        for (
                            ConfigNodePtrNode* TabNode = SelectedTabNode;
                            TabNode && (
                                TabNode != SelectedTabNode || 
                                !Index
                            );
                            (
                                (!TabNode->Next)
                                ? TabNode = Focused->Tabs.Head
                                : TabNode = TabNode->Next
                            ), ++Index
                        ) {
                            if (TabNode != SelectedTabNode) {
                                NextSelectedTab = TabNode->V;
                                break;
                            }
                        }

                        if (NextSelectedTab != EMPTY_CFG_NODE_VALUE) {
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_TAB, 
                                __Registers.Tab = NextSelectedTab->ID
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_PREV_TAB: {
                        ConfigNode* Window = ConfigNodeFromID(
                            Registers()->Window
                        );
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool, 
                            Window
                        );
                        ConfigPanelNode* Focused = PanelTree.Focused;
                        ConfigNodePtrNode* SelectedTabNode = NULL;

                        for (
                            ConfigNodePtrNode* N = Focused->Tabs.Tail; 
                            N; 
                            N = N->Prev
                        ) {
                            if (N->V == Focused->SelectedTab) {
                                SelectedTabNode = N;
                                break;
                            }
                        }

                        ConfigNode* NextSelectedTab = EMPTY_CFG_NODE_VALUE;
                        u64 Index = 0;

                        for (
                            ConfigNodePtrNode* TabNode = SelectedTabNode;
                            TabNode && (TabNode != SelectedTabNode || !Index);
                            (
                                (!TabNode->Prev)
                                ? TabNode = Focused->Tabs.Tail
                                : TabNode = TabNode->Prev
                            ), ++Index
                        ) {
                            if (TabNode != SelectedTabNode) {
                                NextSelectedTab = TabNode->V;
                                break;
                            }
                        }

                        if (NextSelectedTab != EMPTY_CFG_NODE_VALUE) {
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_TAB, 
                                __Registers.Tab = NextSelectedTab->ID
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_MOVE_TAB_RIGHT:
                    case APP_COMMAND_KIND_MOVE_TAB_LEFT: {
                        ConfigNode* Tab = ConfigNodeFromID(Registers()->Tab);
                        ConfigNode* PanelConfig = Tab->Parent;
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool, 
                            PanelConfig
                        );
                        ConfigPanelNode* Panel = ConfigPanelNodeFromConfigTree(
                            PanelTree.Root, 
                            PanelConfig
                        );
                        ConfigNode* TabPrev2 = EMPTY_CFG_NODE_VALUE;
                        ConfigNode* TabPrev = EMPTY_CFG_NODE_VALUE;
                        ConfigNode* TabNext = EMPTY_CFG_NODE_VALUE;
                        ConfigNode* Prev2 = EMPTY_CFG_NODE_VALUE;
                        ConfigNode* Prev = EMPTY_CFG_NODE_VALUE;
                        ConfigNode* Next = EMPTY_CFG_NODE_VALUE;

                        for (
                            ConfigNodePtrNode* N = Panel->Tabs.Head;
                            N;
                            (
                                Prev2 = Prev,
                                Prev = N->V,
                                N = N->Next
                            )
                        ) {
                            Next = (N->Next) ? N->Next->V : EMPTY_CFG_NODE_VALUE;

                            if (N->V == Tab) {
                                TabPrev2 = Prev2;
                                TabPrev = Prev;
                                TabNext = Next;
                                break;
                            }
                        }

                        ConfigNode* NewPrev = (Kind == APP_COMMAND_KIND_MOVE_TAB_RIGHT)
                            ? TabNext
                            : TabPrev2;

                        if (NewPrev == TabPrev && Panel->Tabs.Tail)
                            NewPrev = Panel->Tabs.Tail->V;

                        if (Tab != EMPTY_CFG_NODE_VALUE && NewPrev != Tab) {
                            ConfigNodeUnhook(APP_STATE->Config, PanelConfig, Tab);
                            ConfigNodeInsertChild(
                                APP_STATE->Config,
                                PanelConfig,
                                NewPrev,
                                Tab
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_BUILD_TAB: {
                        ConfigNode* Panel = ConfigNodeFromID(Registers()->Panel);
                        ConfigNode* Tab = EMPTY_CFG_NODE_VALUE;

                        for (
                            ConfigNode* Child = Panel->Head;
                            Child != EMPTY_CFG_NODE_VALUE;
                            Child = Child->Next
                        ) {
                            if (
                                StrMatch(Child->String, Registers()->String, 0) &&
                                StrMatch(
                                    ConfigNodeChildFromStr(
                                        Child, 
                                        "file"_s8
                                    )->Head->String,
                                    Registers()->FilePath,
                                    0
                                )
                            ) {
                                Tab = Child;
                                break;
                            }
                        }

                        if (
                            Tab == EMPTY_CFG_NODE_VALUE && 
                            Panel != EMPTY_CFG_NODE_VALUE
                        ) {
                            Tab = ConfigNodeNew(
                                APP_STATE->Config,
                                Panel,
                                Registers()->String
                            );

                            if (Registers()->FilePath.Size) {
                                ConfigNode* File = ConfigNodeNew(
                                    APP_STATE->Config,
                                    Tab,
                                    "file"_s8
                                );

                                ConfigNodeNew(
                                    APP_STATE->Config,
                                    File,
                                    Registers()->FilePath
                                );
                            }
                        }

                        if (Tab != EMPTY_CFG_NODE_VALUE) {
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_TAB, 
                                __Registers.Tab = Tab->ID
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_DUPLICATE_TAB: {
                        ConfigNode* Source = ConfigNodeFromID(Registers()->Tab);

                        if (Source != EMPTY_CFG_NODE_VALUE) {
                            ConfigNode* Destination = ConfigNodeCpy(
                                APP_STATE->Config,
                                Source
                            );

                            ConfigNodeInsertChild(
                                APP_STATE->Config,
                                Source->Parent,
                                Source,
                                Destination
                            );
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_TAB,
                                __Registers.Tab = Destination->ID
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_MOVE_VIEW: {
                        ConfigNode* View = ConfigNodeFromID(Registers()->View);
                        ConfigNode* PrevTab = ConfigNodeFromID(Registers()->PrevTab);
                        ConfigNode* SrcPanel = View->Parent;
                        ConfigNode* DstPanel = ConfigNodeFromID(Registers()->DstPanel);

                        if (
                            View != EMPTY_CFG_NODE_VALUE &&
                            DstPanel != EMPTY_CFG_NODE_VALUE &&
                            PrevTab != View
                        ) {
                            ConfigNodeUnhook(APP_STATE->Config, SrcPanel, View);
                            ConfigNodeInsertChild(
                                APP_STATE->Config,
                                DstPanel,
                                PrevTab,
                                View
                            );
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_TAB,
                                __Registers.Tab = View->ID
                            );
                            AppCmd(
                                APP_COMMAND_KIND_FOCUS_PANEL,
                                __Registers.Panel = DstPanel->ID
                            );

                            if (SrcPanel != DstPanel) {
                                ConfigPanelTree SrcPanelTree = ConfigPanelTreeFromConfig(
                                    Scratch.MemPool,
                                    SrcPanel
                                );
                                ConfigPanelNode* SrcPanelNode = ConfigPanelNodeFromConfigTree(
                                    SrcPanelTree.Root,
                                    SrcPanel
                                );

                                if (!SrcPanelNode->Tabs.Count) {
                                    AppCmd(
                                        APP_COMMAND_KIND_CLOSE_PANEL,
                                        __Registers.Panel = SrcPanel->ID
                                    );
                                } else if (SrcPanelNode->SelectedTab == EMPTY_CFG_NODE_VALUE) {
                                    AppCmd(
                                        APP_COMMAND_KIND_FOCUS_TAB,
                                        __Registers.Tab = SrcPanelNode->Tabs.Head->V->ID
                                    );
                                }
                            }
                        }
                    } break;

                    case APP_COMMAND_KIND_CLOSE_TAB: {
                        ConfigNode* Tab = ConfigNodeFromID(Registers()->Tab);
                        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
                            Scratch.MemPool, 
                            Tab
                        );
                        ConfigPanelNode* Panel = ConfigPanelNodeFromConfigTree(
                            PanelTree.Root, 
                            Tab->Parent
                        );

                        if (Panel->SelectedTab == Tab) {
                            b32 FoundSelected = FALSE;
                            ConfigNode* NextSelectedTab = EMPTY_CFG_NODE_VALUE;

                            for (
                                ConfigNodePtrNode* N = Panel->Tabs.Head;
                                N;
                                N = N->Next
                            ) {
                                if (N->V == Panel->SelectedTab) {
                                    FoundSelected = TRUE;
                                } else {
                                    NextSelectedTab = N->V;

                                    if (FoundSelected)
                                        break;
                                }
                            }

                            if (NextSelectedTab != EMPTY_CFG_NODE_VALUE) {
                                AppCmd(
                                    APP_COMMAND_KIND_FOCUS_TAB, 
                                    __Registers.Tab = NextSelectedTab->ID
                                );
                            }
                        }

                        if (Tab != EMPTY_CFG_NODE_VALUE)
                            ConfigNodeRelease(APP_STATE->Config, Tab);
                    } break;

                    case APP_COMMAND_KIND_SET_TAB_VIEW: {
                        ConfigNode* Tab = ConfigNodeFromID(Registers()->Tab);

                        if (
                            Tab != EMPTY_CFG_NODE_VALUE &&
                            Registers()->String.Size &&
                            !StrMatch(Tab->String, Registers()->String, 0)
                        ) {
                            AppViewState* VS = AppViewStateFromConfig(Tab);

                            ConfigNodeEquipStr(
                                APP_STATE->Config,
                                Tab,
                                Registers()->String
                            );

                            for (
                                AppArenaExt* Ext = VS->HeadArenaExt;
                                Ext;
                                Ext = Ext->Next
                            ) {
                                ArenaRelease(Ext->MemPool);
                            }

                            ArenaPopTo(VS->MemPool, VS->MemPoolResetPosition);
                            VS->UserData = NULL;
                            VS->HeadArenaExt = NULL;
                            VS->TailArenaExt = NULL;
                            VS->ScrollPosition = {};
                            VS->QueryIsOpen = FALSE;
                        }
                    } break;

                    case APP_COMMAND_KIND_SET_TAB_FILE: {
                        ConfigNode* Tab = ConfigNodeFromID(Registers()->Tab);
                        ConfigNode* TabFile = ConfigNodeChildFromStr(
                            Tab, 
                            "file"_s8
                        );
                        Str8 Path = StrTrimLastSlash(
                            PathAbsoluteDstFromRelativeDstSrc(
                                Scratch.MemPool,
                                Registers()->FilePath,
                                GetCurrentPath(Scratch.MemPool)
                            )
                        );
                        FileProperties Props = SystemGetFileProperties(Path);
                        b32 IsFolder = !!(Props.Kind & SYS_FILE_IS_DIR);

                        if (!Props.Created || IsFolder) {
                            Str8 Error = IsFolder
                                ? ArenaPushStrFmt(
                                    Scratch.MemPool,
                                    "FLAP cannot show \"%S\" in a tab. It is a folder.",
                                    PRINT_STR(Path)
                                )
                                : ArenaPushStrFmt(
                                    Scratch.MemPool,
                                    "FLAP cannot open \"%S\". The file does not exist.",
                                    PRINT_STR(Path)
                                );

                            APP_STATE->ErrorStrSize = MIN(
                                sizeof(APP_STATE->ErrorBuffer),
                                Error.Size
                            );
                            MemCpy(
                                APP_STATE->ErrorBuffer,
                                Error.Str,
                                APP_STATE->ErrorStrSize
                            );
                            APP_STATE->ErrorFrameIndex = APP_STATE->FrameIndex;
                        } else if (TabFile == EMPTY_CFG_NODE_VALUE) {
                            AppCmd(
                                APP_COMMAND_KIND_OPEN,
                                __Registers.FilePath = Path
                            );
                        } else if (
                            !PathMatchNormalised(
                                TabFile->Head->String, 
                                Path
                            )
                        ) {
                            AppViewState* VS = AppViewStateFromConfig(Tab);

                            ConfigNodeNewReplace(
                                APP_STATE->Config, 
                                TabFile, 
                                Path
                            );

                            if (AppSessionRecordPath(Path))
                                AppFileRecorded(Path);

                            for (
                                AppArenaExt* Ext = VS->HeadArenaExt;
                                Ext;
                                Ext = Ext->Next
                            ) {
                                ArenaRelease(Ext->MemPool);
                            }

                            ArenaPopTo(VS->MemPool, VS->MemPoolResetPosition);
                            VS->UserData = NULL;
                            VS->HeadArenaExt = NULL;
                            VS->TailArenaExt = NULL;
                            VS->ScrollPosition = {};
                            AppCmd(
                                APP_COMMAND_KIND_SET_CURRENT_PATH,
                                __Registers.FilePath = StrChopLastSlash(Path)
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_SET_CURRENT_PATH: {
                        ConfigNode* Session = ConfigNodeChildFromStr(
                            ConfigNodeRoot(), 
                            "session"_s8
                        );
                        ConfigNode* CurrentPath = ConfigNodeChildFromStrOrAlloc(
                            APP_STATE->Config,
                            Session,
                            "current_path"_s8
                        );

                        ConfigNodeNewReplace(
                            APP_STATE->Config,
                            CurrentPath,
                            Registers()->FilePath
                        );
                    } break;

                    case APP_COMMAND_KIND_OPEN: {
                        Str8 Path = StrTrimLastSlash(
                            PathAbsoluteDstFromRelativeDstSrc(
                                Scratch.MemPool,
                                Registers()->FilePath,
                                GetCurrentPath(Scratch.MemPool)
                            )
                        );
                        FileProperties Props = SystemGetFileProperties(Path);
                        b32 IsFolder = !!(Props.Kind & SYS_FILE_IS_DIR);
                        Str8List Paths = {};

                        if (IsFolder) {
                            AppPushLogPathsFromDir(
                                Scratch.MemPool, 
                                &Paths, 
                                Path
                            );
                        } else if (Props.Created) {
                            ListPush(Scratch.MemPool, &Paths, Path);
                        }

                        for (
                            Str8Node* Node = Paths.Head; 
                            Node; 
                            Node = Node->Next
                        ) {
                            if (AppSessionRecordPath(Node->String))
                                AppFileRecorded(Node->String);
                        }

                        if (!Paths.Count) {
                            Str8 Error = {};

                            if (IsFolder) {
                                Error = ArenaPushStrFmt(
                                    Scratch.MemPool,
                                    "FLAP found no .fli or .BIN files in \"%S\".",
                                    PRINT_STR(Path)
                                );
                            } else {
                                Error = ArenaPushStrFmt(
                                    Scratch.MemPool,
                                    "FLAP cannot open \"%S\". The file does not exist.",
                                    PRINT_STR(Path)
                                );
                            }

                            APP_STATE->ErrorStrSize = MIN(
                                sizeof(APP_STATE->ErrorBuffer),
                                Error.Size
                            );
                            MemCpy(
                                APP_STATE->ErrorBuffer,
                                Error.Str,
                                APP_STATE->ErrorStrSize
                            );
                            APP_STATE->ErrorFrameIndex = APP_STATE->FrameIndex;
                        } else {
                            Str8 ListName = AppSessionListNameFromPath(
                                Paths.Head->String
                            );

                            if (StrMatch(ListName, "view"_s8, 0)) {
                                AppCmd(APP_COMMAND_KIND_OPEN_VIEWS);
                            } else {
                                AppCmd(APP_COMMAND_KIND_OPEN_FILES);
                            }

                            AppCmd(
                                APP_COMMAND_KIND_SET_CURRENT_PATH,
                                __Registers.FilePath = IsFolder
                                    ? Path
                                    : StrChopLastSlash(Path)
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_COPY_TAB_FULL_PATH: {
                        ConfigNode* Tab = ConfigNodeFromID(Registers()->Tab);
                        Str8 Path = ConfigNodeChildFromStr(
                            Tab, 
                            "file"_s8
                        )->Head->String;

                        if (Path.Size)
                            SetClipboardText(Path);
                    } break;

                    case APP_COMMAND_KIND_RECORD_FILE_IN_SESSION: {
                        Str8 Path = Registers()->FilePath;

                        if (Path.Size && AppSessionRecordPath(Path))
                            AppFileRecorded(Path);
                    } break;

                    case APP_COMMAND_KIND_NEW_SESSION:
                    case APP_COMMAND_KIND_SAVE_SESSION: {
                        Str8 NewPath = Registers()->FilePath;
                        b32 FileWillBeOverWritten = !!(
                            SystemGetFileProperties(NewPath).Created
                        );
                        UIKey Key = UIKeyFromStr(
                            EMPTY_UI_KEY_VALUE, 
                            "new_config_overwrite_confirm"_s8
                        );

                        if (!NewPath.Size)
                            break;

                        if (
                            FileWillBeOverWritten &&
                            !Registers()->ForceConfirm &&
                            APP_STATE->PopUpKey != Key
                        ) {
                            APP_STATE->PopUpKey = Key;
                            APP_STATE->PopUpActive = TRUE;
                            ArenaClear(APP_STATE->PopUpMemPool);
                            MemSet(
                                &APP_STATE->PopUpCommands, 
                                0, 
                                sizeof(APP_STATE->PopUpCommands)
                            );
                            APP_STATE->PopUpTitle = ArenaPushStrCpy(
                                APP_STATE->PopUpMemPool,
                                "Are you sure you want to save to this path?"_s8
                            );
                            APP_STATE->PopUpDescription = ArenaPushStrFmt(
                                APP_STATE->PopUpMemPool,
                                "The existing file at '%S' will be overwritten.",
                                PRINT_STR(NewPath)
                            );

                            AppRegisters* Regs = CopyRegisters(
                                AppFrameMemPool(), 
                                Registers()
                            );

                            Regs->ForceConfirm = TRUE;
                            ListPush(
                                APP_STATE->PopUpMemPool,
                                &APP_STATE->PopUpCommands,
                                APP_COMMAND_KIND_INFO_TABLE[Kind].String,
                                Regs
                            );
                        } else if (Kind == APP_COMMAND_KIND_NEW_SESSION) {
                            SystemDeleteFile(NewPath);
                            AppCmd(
                                APP_COMMAND_KIND_OPEN_SESSION, 
                                __Registers.FilePath = NewPath
                            );
                        } else {
                            ArenaClear(APP_STATE->SessionPathMemPool);
                            APP_STATE->SessionPath = ArenaPushStrCpy(
                                APP_STATE->SessionPathMemPool, 
                                NewPath
                            );
                            AppCmd(APP_COMMAND_KIND_WRITE_SESSION_DATA);
                            AppCmd(
                                APP_COMMAND_KIND_RECORD_SESSION_AS_LAST_OPENED,
                                __Registers.FilePath = NewPath
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_OPEN_SESSION: {
                        ConfigNode* FileRoot = ConfigNodeChildFromStr(
                            ConfigNodeRoot(), 
                            "session"_s8
                        );
                        Str8 FilePath = Registers()->FilePath;
                        Str8 FileContents = ReadFileContents(Scratch.MemPool, FilePath);
                        FileProperties FileProps = SystemGetFileProperties(FilePath);
                        Str8 StoredPath = APP_STATE->SessionPath;
                        Str8 FileExpectedHeader = ArenaPushStrFmt(
                            Scratch.MemPool,
                            "// %s",
                            NC_DATA_DIR
                        );
                        b32 FileIsOkay = (
                            (!FileProps.Size && !FileProps.Created) ||
                            StrMatch(FilePath, StoredPath, 0) ||
                            !StoredPath.Size ||
                            StrMatch(
                                StrPrefix(FileContents, FileExpectedHeader.Size),
                                FileExpectedHeader,
                                0
                            )
                        );

                        if (!FileIsOkay) {
                            Str8 Error = ArenaPushStrFmt(
                                Scratch.MemPool,
                                "FLAP cannot open \"%S\". It is not a session file.",
                                PRINT_STR(FilePath)
                            );

                            APP_STATE->ErrorStrSize = MIN(
                                sizeof(APP_STATE->ErrorBuffer),
                                Error.Size
                            );
                            MemCpy(
                                APP_STATE->ErrorBuffer,
                                Error.Str,
                                APP_STATE->ErrorStrSize
                            );
                            APP_STATE->ErrorFrameIndex = APP_STATE->FrameIndex;
                            break;
                        }

                        // NOTE(nc): a file with no contents (the first run) keeps the
                        //         : defaults that AppInit made
                        if (FileContents.Size) {
                            ConfigNodeReleaseAllChildren(
                                APP_STATE->Config, 
                                FileRoot
                            );

                            ConfigNodePtrList FileConfigs = ConfigNodePtrListFromStr(
                                Scratch.MemPool,
                                APP_STATE->Config,
                                APP_STATE->CfgSchemaTable,
                                StrChopLastSlash(FilePath),
                                FileContents
                            );

                            for (
                                ConfigNodePtrNode* N = FileConfigs.Head;
                                N;
                                N = N->Next
                            ) {
                                ConfigNodeInsertChild(
                                    APP_STATE->Config,
                                    FileRoot,
                                    FileRoot->Tail,
                                    N->V
                                );
                            }
                        }

                        ArenaClear(APP_STATE->SessionPathMemPool);
                        APP_STATE->SessionPath = ArenaPushStrCpy(
                            APP_STATE->SessionPathMemPool, 
                            FilePath
                        );

                        ConfigNode* Window = ConfigNodeChildFromStr(
                            FileRoot, 
                            "window"_s8
                        );

                        if (Window == EMPTY_CFG_NODE_VALUE) {
                            Window = ConfigNodeNew(
                                APP_STATE->Config, 
                                FileRoot, 
                                "window"_s8
                            );

                            ConfigNode* Panels = ConfigNodeNew(
                                APP_STATE->Config, 
                                Window, 
                                "panels"_s8
                            );

                            ConfigNodeNew(
                                APP_STATE->Config, 
                                Panels, 
                                "selected"_s8
                            );
                            AppCmd(
                                APP_COMMAND_KIND_OPEN_FILES,
                                __Registers.Window = Window->ID,
                                __Registers.Panel = Panels->ID
                            );
                        }

                        BaseRegisters()->Window = Window->ID;

                        if (
                            ConfigNodeChildFromStr(
                                FileRoot, 
                                "keybindings"_s8
                            ) == EMPTY_CFG_NODE_VALUE
                        ) {
                            AppCmd(APP_COMMAND_KIND_RESET_TO_DEFAULT_BINDINGS);
                        }

                        for (
                            ConfigNode* Child = FileRoot->Head;
                            Child != EMPTY_CFG_NODE_VALUE;
                            Child = Child->Next
                        ) {
                            if (
                                StrMatch(Child->String, "file"_s8, 0) ||
                                StrMatch(Child->String, "view"_s8, 0)
                            ) {
                                AppFileRecorded(Child->Head->String);
                            }
                        }

                        AppCmd(
                            APP_COMMAND_KIND_RECORD_SESSION_AS_LAST_OPENED,
                            __Registers.FilePath = FilePath
                        );
                    } break;

                    case APP_COMMAND_KIND_OPEN_RECENT_SESSION: {
                        ConfigNode* Config = ConfigNodeFromID(
                            Registers()->Config
                        );
                        ConfigNode* Path = ConfigNodeChildFromStr(
                            Config, 
                            "path"_s8
                        );

                        if (
                            StrMatch(
                                Config->String, 
                                "recent_session"_s8, 
                                0
                            ) &&
                            Path->Head->String.Size
                        ) {
                            AppCmd(
                                APP_COMMAND_KIND_OPEN_SESSION,
                                __Registers.FilePath = Path->Head->String
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_WRITE_SESSION_DATA: {
                        AppWriteSessionData();
                    } break;

                    case APP_COMMAND_KIND_RECORD_SESSION_AS_LAST_OPENED: {
                        Str8 FilePath = Registers()->FilePath;
                        ConfigNode* Session = ConfigNodeChildFromStr(
                            ConfigNodeRoot(), 
                            "session"_s8
                        );
                        ConfigNodePtrList RecentSessions = ConfigNodeChildListFromStr(
                            Scratch.MemPool,
                            Session,
                            "recent_session"_s8
                        );
                        ConfigNode* RecentSession = EMPTY_CFG_NODE_VALUE;

                        for (
                            ConfigNodePtrNode* N = RecentSessions.Head;
                            N;
                            N = N->Next
                        ) {
                            if (
                                PathMatchNormalised(
                                    PathFromConfig(N->V), 
                                    FilePath
                                )
                            ) {
                                RecentSession = N->V;
                                break;
                            }
                        }

                        if (RecentSession == EMPTY_CFG_NODE_VALUE) {
                            RecentSession = ConfigNodeNew(
                                APP_STATE->Config,
                                Session,
                                "recent_session"_s8
                            );

                            ConfigNode* PathRoot = ConfigNodeNew(
                                APP_STATE->Config,
                                RecentSession,
                                "path"_s8
                            );

                            ConfigNodeNew(
                                APP_STATE->Config, 
                                PathRoot, 
                                FilePath
                            );
                        }

                        ConfigNodeUnhook(
                            APP_STATE->Config, 
                            Session, 
                            RecentSession
                        );
                        ConfigNodeInsertChild(
                            APP_STATE->Config,
                            Session,
                            EMPTY_CFG_NODE_VALUE,
                            RecentSession
                        );
                        RecentSessions = ConfigNodeChildListFromStr(
                            Scratch.MemPool,
                            Session,
                            "recent_session"_s8
                        );

                        if (RecentSessions.Count > 32) {
                            ConfigNodeRelease(
                                APP_STATE->Config,
                                RecentSessions.Tail->V
                            );
                        }
                    } break;


                    case APP_COMMAND_KIND_EDIT: {
                        Event.Kind = UI_EVENT_KIND_PRESS;
                        Event.Slot = UI_EVENT_ACTION_SLOT_EDIT;
                    } break;

                    case APP_COMMAND_KIND_ACCEPT: {
                        Event.Kind = UI_EVENT_KIND_PRESS;
                        Event.Slot = UI_EVENT_ACTION_SLOT_ACCEPT;
                    } break;

                    case APP_COMMAND_KIND_CANCEL: {
                        Event.Kind = UI_EVENT_KIND_PRESS;
                        Event.Slot = UI_EVENT_ACTION_SLOT_CANCEL;
                    } break;

                    case APP_COMMAND_KIND_MOVE_LEFT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_PICK_SELECT_SIDE | 
                            UI_EVENT_FLAG_ZERO_DELTA_ON_SELECT | 
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_RIGHT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_PICK_SELECT_SIDE | 
                            UI_EVENT_FLAG_ZERO_DELTA_ON_SELECT | 
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL | 
                            UI_EVENT_FLAG_SECONDARY
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL | 
                            UI_EVENT_FLAG_SECONDARY
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_LEFT_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_SECONDARY | 
                            UI_EVENT_FLAG_KEEP_MARK
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_RIGHT_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_SECONDARY | 
                            UI_EVENT_FLAG_KEEP_MARK
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL | 
                            UI_EVENT_FLAG_SECONDARY | 
                            UI_EVENT_FLAG_KEEP_MARK
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL | 
                            UI_EVENT_FLAG_SECONDARY | 
                            UI_EVENT_FLAG_KEEP_MARK
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_LEFT_CHUNK: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_RIGHT_CHUNK: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_CHUNK: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL | 
                            UI_EVENT_FLAG_SECONDARY
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_CHUNK: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL | 
                            UI_EVENT_FLAG_SECONDARY
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_LEFT_CHUNK_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_KEEP_MARK | 
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_RIGHT_CHUNK_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_KEEP_MARK | 
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_CHUNK_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_KEEP_MARK | 
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_CHUNK_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = (
                            UI_EVENT_FLAG_KEEP_MARK | 
                            UI_EVENT_FLAG_EXPLICIT_DIRECTIONAL
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_PAGE: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_PAGE;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_PAGE: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_PAGE;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_WHOLE: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WHOLE;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_WHOLE: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WHOLE;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_PAGE_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_PAGE;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_PAGE_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_PAGE;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_UP_WHOLE_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WHOLE;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_DOWN_WHOLE_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WHOLE;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_HOME: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_LINE;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_END: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_LINE;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_HOME_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_LINE;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_MOVE_END_SELECT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_LINE;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_SELECT_ALL: {
                        UIEvent MoveEvent = {};

                        MoveEvent.Kind = UI_EVENT_KIND_NAVIGATE;
                        MoveEvent.DeltaStride = UI_EVENT_DELTA_STRIDE_WHOLE;
                        MoveEvent.DeltaI32 = Vec(-1, 0);
                        ListPush(
                            Scratch.MemPool, 
                            &APP_STATE->UIEvents, 
                            &MoveEvent
                        );
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_KEEP_MARK;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WHOLE;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_DELETE_SINGLE: {
                        Event.Kind = UI_EVENT_KIND_EDIT;
                        Event.Flags = (
                            UI_EVENT_FLAG_DELETE | 
                            UI_EVENT_FLAG_ZERO_DELTA_ON_SELECT
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_DELETE_CHUNK: {
                        Event.Kind = UI_EVENT_KIND_EDIT;
                        Event.Flags = (
                            UI_EVENT_FLAG_DELETE | 
                            UI_EVENT_FLAG_ZERO_DELTA_ON_SELECT
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(1, 0);
                    } break;

                    case APP_COMMAND_KIND_BACKSPACE_SINGLE: {
                        Event.Kind = UI_EVENT_KIND_EDIT;
                        Event.Flags = (
                            UI_EVENT_FLAG_DELETE | 
                            UI_EVENT_FLAG_ZERO_DELTA_ON_SELECT
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_BACKSPACE_CHUNK: {
                        Event.Kind = UI_EVENT_KIND_EDIT;
                        Event.Flags = (
                            UI_EVENT_FLAG_DELETE | 
                            UI_EVENT_FLAG_ZERO_DELTA_ON_SELECT
                        );
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_WORD;
                        Event.DeltaI32 = Vec(-1, 0);
                    } break;

                    case APP_COMMAND_KIND_COPY: {
                        Event.Kind = UI_EVENT_KIND_EDIT;
                        Event.Flags = (
                            UI_EVENT_FLAG_COPY | 
                            UI_EVENT_FLAG_KEEP_MARK
                        );
                    } break;

                    case APP_COMMAND_KIND_CUT: {
                        Event.Kind = UI_EVENT_KIND_EDIT;
                        Event.Flags = (
                            UI_EVENT_FLAG_COPY | 
                            UI_EVENT_FLAG_DELETE
                        );
                    } break;

                    case APP_COMMAND_KIND_PASTE: {
                        Event.Kind = UI_EVENT_KIND_TEXT;
                        Event.String = GetClipboardText(Scratch.MemPool);
                    } break;

                    case APP_COMMAND_KIND_INSERT_TEXT: {
                        Event.Kind = UI_EVENT_KIND_TEXT;
                        Event.String = Registers()->String;
                    } break;

                    case APP_COMMAND_KIND_MOVE_NEXT: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_SECONDARY;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(0, 1);
                    } break;

                    case APP_COMMAND_KIND_MOVE_PREV: {
                        Event.Kind = UI_EVENT_KIND_NAVIGATE;
                        Event.Flags = UI_EVENT_FLAG_SECONDARY;
                        Event.DeltaStride = UI_EVENT_DELTA_STRIDE_CHAR;
                        Event.DeltaI32 = Vec(0, -1);
                    } break;

                    // TODO(nc): implement these
                    case APP_COMMAND_KIND_UNDO:
                    case APP_COMMAND_KIND_REDO:
                    case APP_COMMAND_KIND_CENTRE_CURSOR:
                    case APP_COMMAND_KIND_CONTAIN_CURSOR:
                    case APP_COMMAND_KIND_PICK_FILE:
                    case APP_COMMAND_KIND_PICK_FOLDER:
                    case APP_COMMAND_KIND_PICK_FILE_OR_FOLDER:
                    case APP_COMMAND_KIND_GO_BACK:
                    case APP_COMMAND_KIND_GO_FORWARD:
                    case APP_COMMAND_KIND_GOTO_TIME:
                    case APP_COMMAND_KIND_SEARCH:
                    case APP_COMMAND_KIND_SEARCH_BACKWARDS:
                    case APP_COMMAND_KIND_FIND_NEXT:
                    case APP_COMMAND_KIND_FIND_PREV:
                    case APP_COMMAND_KIND_FILTER:
                    case APP_COMMAND_KIND_SWITCH:
                    case APP_COMMAND_KIND_SHOW_FILE_IN_EXPLORER:
                    case APP_COMMAND_KIND_SESSION_SETTINGS:
                    case APP_COMMAND_KIND_EDIT_SESSION_THEME:
                    case APP_COMMAND_KIND_RECORD_RECENT_SESSION: {} break;

                    case APP_COMMAND_KIND_SELECT_CONFIG: {
                        ConfigNode* Cfg = ConfigNodeFromID(Registers()->Config);
                        ConfigNodePtrList AllOfTheSameKind = ConfigNodeTopLevelListFromStr(
                            Scratch.MemPool,
                            Cfg->String
                        );

                        for (
                            ConfigNodePtrNode* N = AllOfTheSameKind.Head;
                            N;
                            N = N->Next
                        ) {
                            ConfigNodeRelease(
                                APP_STATE->Config,
                                ConfigNodeChildFromStr(N->V, "enabled"_s8)
                            );
                        }

                        ConfigNode* EnabledRoot = ConfigNodeChildFromStrOrAlloc(
                            APP_STATE->Config,
                            Cfg,
                            "enabled"_s8
                        );

                        ConfigNodeNewReplace(
                            APP_STATE->Config, 
                            EnabledRoot, 
                            "1"_s8
                        );
                    } break;

                    case APP_COMMAND_KIND_ENABLE_CONFIG:
                    case APP_COMMAND_KIND_DISABLE_CONFIG:
                    case APP_COMMAND_KIND_DESELECT_CONFIG: {
                        ConfigNode* Cfg = ConfigNodeFromID(Registers()->Config);
                        ConfigNode* EnabledRoot = ConfigNodeChildFromStrOrAlloc(
                            APP_STATE->Config,
                            Cfg,
                            "enabled"_s8
                        );

                        ConfigNodeNewReplace(
                            APP_STATE->Config,
                            EnabledRoot,
                            (Kind == APP_COMMAND_KIND_ENABLE_CONFIG) 
                                ? "1"_s8 
                                : "0"_s8
                        );
                    } break;

                    case APP_COMMAND_KIND_REMOVE_CONFIG: {
                        ConfigNode* Cfg = ConfigNodeFromID(Registers()->Config);

                        ConfigNodeRelease(APP_STATE->Config, Cfg);
                    } break;

                    case APP_COMMAND_KIND_NAME_CONFIG: {
                        ConfigNode* Cfg = ConfigNodeFromID(Registers()->Config);

                        if (Registers()->String.Size) {
                            ConfigNode* Label = ConfigNodeChildFromStrOrAlloc(
                                APP_STATE->Config,
                                Cfg,
                                "label"_s8
                            );

                            ConfigNodeNewReplace(
                                APP_STATE->Config,
                                Label,
                                Registers()->String
                            );
                        } else {
                            ConfigNodeRelease(
                                APP_STATE->Config,
                                ConfigNodeChildFromStr(Cfg, "label"_s8)
                            );
                        }
                    } break;

                    case APP_COMMAND_KIND_DUPLICATE_CONFIG: {
                        ConfigNode* Source = ConfigNodeFromID(
                            Registers()->Config
                        );
                        ConfigNode* Destination = ConfigNodeCpy(
                            APP_STATE->Config, 
                            Source
                        );

                        ConfigNodeInsertChild(
                            APP_STATE->Config,
                            Source->Parent,
                            Source,
                            Destination
                        );
                    } break;

                    case APP_COMMAND_KIND_SAVE_TO_SESSION: {
                        ConfigNode* Cfg = ConfigNodeFromID(
                            Registers()->Config
                        );
                        ConfigNode* Session = ConfigNodeChildFromStr(
                            ConfigNodeRoot(),
                            "session"_s8
                        );

                        ConfigNodeUnhook(APP_STATE->Config, Cfg->Parent, Cfg);
                        ConfigNodeInsertChild(
                            APP_STATE->Config,
                            Session,
                            Session->Tail,
                            Cfg
                        );
                    } break;

                    case APP_COMMAND_KIND_ADD_THEME_COLOUR: {
                        ConfigNode* Parent = ConfigNodeFromID(
                            Registers()->Config
                        );

                        // NOTE(nc): the theme colours are in the session if no config is given
                        if (Parent == EMPTY_CFG_NODE_VALUE) {
                            Parent = ConfigNodeChildFromStr(
                                ConfigNodeRoot(), 
                                "session"_s8
                            );
                        }

                        ConfigNode* Colour = ConfigNodeNew(
                            APP_STATE->Config,
                            Parent,
                            "theme_colour"_s8
                        );

                        ConfigNodeNew(APP_STATE->Config, Colour, "tags"_s8);

                        ConfigNode* Value = ConfigNodeNew(
                            APP_STATE->Config,
                            Colour,
                            "value"_s8
                        );

                        ConfigNodeNew(
                            APP_STATE->Config, 
                            Value, 
                            "0xFFFFFFFF"_s8
                        );
                    } break;

                    case APP_COMMAND_KIND_FORK_THEME: {
                        Access* Acc = AccessOpen();
                        ConfigNode* Parent = ConfigNodeFromID(
                            Registers()->Config
                        );

                        if (Parent == EMPTY_CFG_NODE_VALUE) {
                            Parent = ConfigNodeChildFromStr(
                                ConfigNodeRoot(), 
                                "session"_s8
                            );
                        }

                        ConfigNodePtrList Colours = ConfigNodeChildListFromStr(
                            Scratch.MemPool,
                            Parent,
                            "theme_colour"_s8
                        );

                        for (
                            ConfigNodePtrNode* N = Colours.Head;
                            N;
                            N = N->Next
                        ) {
                            ConfigNodeRelease(APP_STATE->Config, N->V);
                        }

                        ConfigNode* ThemeConfig = ConfigNodeChildFromStr(
                            Parent, 
                            "theme"_s8
                        );
                        MDNode* ThemeTree = AppThemeTreeFromName(
                            Scratch.MemPool,
                            Acc,
                            ThemeConfig->Head->String
                        );

                        for (
                            MDNode* N = ThemeTree;
                            !MDNodeIsEmpty(N);
                            N = MDNodeRecordDepthFirstPreOrder(N, ThemeTree).Next
                        ) {
                            if (StrMatch(N->String, "theme_colour"_s8, 0)) {
                                ConfigNode* Colour = ConfigNodeNew(
                                    APP_STATE->Config,
                                    Parent,
                                    "theme_colour"_s8
                                );
                                ConfigNode* Tags = ConfigNodeNew(
                                    APP_STATE->Config,
                                    Colour,
                                    "tags"_s8
                                );
                                ConfigNode* Value = ConfigNodeNew(
                                    APP_STATE->Config,
                                    Colour,
                                    "value"_s8
                                );

                                ConfigNodeNew(
                                    APP_STATE->Config,
                                    Tags,
                                    MDChildFromStr(
                                        N, 
                                        "tags"_s8, 
                                        0
                                    )->Head->String
                                );
                                ConfigNodeNew(
                                    APP_STATE->Config,
                                    Value,
                                    MDChildFromStr(
                                        N, 
                                        "value"_s8, 
                                        0
                                    )->Head->String
                                );
                            }
                        }

                        ConfigNodeRelease(APP_STATE->Config, ThemeConfig);
                        AccessClose(Acc);
                    } break;

                    case APP_COMMAND_KIND_SAVE_THEME:
                    case APP_COMMAND_KIND_SAVE_AND_SET_THEME: {
                        Str8 Name = StrSkipLastSlash(Registers()->String);
                        Str8 ThemesFolder = ArenaPushStrFmt(
                            Scratch.MemPool,
                            "%S/%s/themes",
                            PRINT_STR(GetProcessProperties()->UserProgramDataPath),
                            NC_DATA_DIR
                        );

                        if (Name.Size && SystemCreateDirectory(ThemesFolder)) {
                            Str8 DstPath = ArenaPushStrFmt(
                                Scratch.MemPool,
                                "%S/%S",
                                PRINT_STR(ThemesFolder),
                                PRINT_STR(Name)
                            );
                            ConfigNode* Parent = ConfigNodeFromID(
                                Registers()->Config
                            );

                            if (Parent == EMPTY_CFG_NODE_VALUE) {
                                Parent = ConfigNodeChildFromStr(
                                    ConfigNodeRoot(), 
                                    "session"_s8
                                );
                            }

                            ConfigNodePtrList Colours = ConfigNodeChildListFromStr(
                                Scratch.MemPool,
                                Parent,
                                "theme_colour"_s8
                            );
                            Str8List Strings = {};

                            ListPush(Scratch.MemPool, &Strings, "theme:\n{\n"_s8);

                            for (
                                ConfigNodePtrNode* N = Colours.Head;
                                N;
                                N = N->Next
                            ) {
                                ListPush(
                                    Scratch.MemPool,
                                    &Strings,
                                    ConfigStrFromTree(
                                        Scratch.MemPool,
                                        APP_STATE->CfgSchemaTable,
                                        ThemesFolder,
                                        N->V
                                    )
                                );
                            }

                            ListPush(Scratch.MemPool, &Strings, "}\n"_s8);

                            if (
                                WriteFileContents(DstPath, Strings) &&
                                Kind == APP_COMMAND_KIND_SAVE_AND_SET_THEME
                            ) {
                                ConfigNode* Theme = ConfigNodeChildFromStrOrAlloc(
                                    APP_STATE->Config,
                                    Parent,
                                    "theme"_s8
                                );

                                ConfigNodeNewReplace(APP_STATE->Config, Theme, Name);
                            }
                        }
                    } break;


                    case APP_COMMAND_KIND_SET_THEME: {
                        ConfigNode* Session = ConfigNodeChildFromStr(
                            ConfigNodeRoot(), 
                            "session"_s8
                        );
                        ConfigNode* Theme = ConfigNodeChildFromStrOrAlloc(
                            APP_STATE->Config,
                            Session,
                            "theme"_s8
                        );

                        ConfigNodeNewReplace(
                            APP_STATE->Config,
                            Theme,
                            Registers()->String
                        );
                    } break;

                    case APP_COMMAND_KIND_TOGGLE_DEV_MENU: {
                        APP_STATE->DevMenuIsOpen ^= TRUE;
                    } break;

                    case APP_COMMAND_KIND_PUSH_QUERY: {
                        Str8 CommandName = Registers()->CommandName;
                        AppCommandKindInfo* Info = AppCommandKindInfoFromStr(
                            CommandName
                        );
                        b32 IsFloating = (
                            !CommandName.Size || 
                            Info->Query.Kind & APP_QUERY_KIND_FLOATING
                        );
                        b32 IsFilePath = (
                            AppQueryExpressionKindFromStr(
                                Registers()->Expression
                            ) == APP_QUERY_EXPR_KIND_FILE_PATH
                        );

                        UIContextMenuClose();
                        APP_STATE->MenuBarFocused = FALSE;

                        if (IsFloating) {
                            Str8 InitialInput = ""_s8;

                            if (IsFilePath) {
                                ConfigNode* Session = ConfigNodeChildFromStr(
                                    ConfigNodeRoot(), 
                                    "session"_s8
                                );
                                ConfigNode* CurrentPath = ConfigNodeChildFromStr(
                                    Session, 
                                    "current_path"_s8
                                );
                                Str8 CurrentPathString = CurrentPath->Head->String;

                                if (!CurrentPathString.Size) {
                                    CurrentPathString = PathNormalisedFromStr(
                                        Scratch.MemPool, 
                                        GetCurrentPath(Scratch.MemPool)
                                    );
                                }

                                InitialInput = ArenaPushStrFmt(
                                    Scratch.MemPool, 
                                    "%S/", 
                                    PRINT_STR(CurrentPathString)
                                );
                            }

                            APP_STATE->QueryIsActive = TRUE;
                            ArenaClear(APP_STATE->QueryMemPool);
                            APP_STATE->QueryRegs = CopyRegisters(
                                APP_STATE->QueryMemPool, 
                                Registers()
                            );
                            APP_STATE->QueryStringSize = MIN(
                                sizeof(APP_STATE->QueryBuffer), 
                                InitialInput.Size
                            );
                            MemCpy(
                                APP_STATE->QueryBuffer, 
                                InitialInput.Str, 
                                APP_STATE->QueryStringSize
                            );
                            APP_STATE->QueryCursor = TxtPt(
                                1, 
                                1 + APP_STATE->QueryStringSize
                            );
                            APP_STATE->QueryMark = APP_STATE->QueryCursor;
                            APP_STATE->QueryScroll = {};
                            APP_STATE->QueryListCursor = {};
                            APP_STATE->QueryListCursor.Y = IsFilePath;
                            APP_STATE->QueryListMark = APP_STATE->QueryListCursor;
                        } else if (
                            ConfigNodeFromID(
                                Registers()->View
                            ) != EMPTY_CFG_NODE_VALUE
                        ) {
                            ConfigNode* View = ConfigNodeFromID(Registers()->View);
                            ConfigNode* Query = ConfigNodeChildFromStrOrAlloc(
                                APP_STATE->Config,
                                View,
                                "query"_s8
                            );
                            ConfigNode* Cmd = ConfigNodeChildFromStrOrAlloc(
                                APP_STATE->Config,
                                Query,
                                "cmd"_s8
                            );
                            ConfigNode* Input = ConfigNodeChildFromStrOrAlloc(
                                APP_STATE->Config,
                                Query,
                                "input"_s8
                            );
                            Str8 InitialInput = ""_s8;

                            if (Info->Query.Kind & APP_QUERY_KIND_KEEP_OLD_INPUT)
                                InitialInput = Input->Head->String;

                            Str8 CurrentQueryCommandName = ArenaPushStrCpy(
                                Scratch.MemPool, 
                                Cmd->Head->String
                            );

                            ConfigNodeNewReplace(
                                APP_STATE->Config,
                                Input,
                                InitialInput
                            );
                            ConfigNodeNewReplace(
                                APP_STATE->Config,
                                Cmd,
                                CommandName
                            );

                            AppViewState* VS = AppViewStateFromConfig(View);

                            if (
                                !VS->QueryIsOpen && 
                                Info->Query.Kind & APP_QUERY_KIND_SELECT_OLD_INPUT
                            ) {
                                VS->QueryCursor = TxtPt(1, 1 + Input->Head->String.Size);
                                VS->QueryMark = TxtPt(1, 1);
                            } else {
                                VS->QueryCursor = TxtPt(1, 1 + Input->Head->String.Size);
                                VS->QueryMark = VS->QueryCursor;
                            }

                            if (
                                !StrMatch(CurrentQueryCommandName, CommandName, 0) ||
                                VS->ContentsAreFocused
                            ) {
                                VS->QueryIsOpen = TRUE;
                            } else {
                                VS->QueryIsOpen ^= TRUE;
                            }

                            VS->ContentsAreFocused = FALSE;
                        }
                    } break;

                    case APP_COMMAND_KIND_QUERY:
                    case APP_COMMAND_KIND_COMPLETE_QUERY: {
                        ConfigNode* View = ConfigNodeFromID(Registers()->View);
                        Str8 CommandName = AppViewQueryCommand();

                        if (CommandName.Size)
                            AppPushCmd(CommandName, Registers());

                        AppCommandKindInfo* Info = AppCommandKindInfoFromStr(
                            CommandName
                        );

                        if (!(Info->Query.Kind & APP_QUERY_KIND_KEEP_OLD_INPUT)) {
                            AppViewState* VS = AppViewStateFromConfig(View);

                            VS->QueryIsOpen = FALSE;
                            VS->QueryStringSize = 0;
                        }
                    } break;

                    case APP_COMMAND_KIND_CANCEL_QUERY: {
                        APP_STATE->QueryIsActive = FALSE;
                        ArenaClear(APP_STATE->QueryMemPool);
                        APP_STATE->QueryRegs = NULL;
                    } break;

                    case APP_COMMAND_KIND_UPDATE_QUERY: {
                        ConfigNode* View = ConfigNodeFromID(Registers()->View);
                        ConfigNode* Query = ConfigNodeChildFromStrOrAlloc(
                            APP_STATE->Config,
                            View,
                            "query"_s8
                        );
                        ConfigNode* Input = ConfigNodeChildFromStrOrAlloc(
                            APP_STATE->Config,
                            Query,
                            "input"_s8
                        );

                        ConfigNodeNewReplace(
                            APP_STATE->Config,
                            Input,
                            Registers()->String
                        );

                        AppViewState* VS = AppViewStateFromConfig(View);

                        VS->QueryStringSize = MIN(
                            sizeof(VS->QueryBuffer), 
                            Registers()->String.Size
                        );
                        VS->QueryCursor = TxtPt(1, 1 + VS->QueryStringSize);
                        VS->QueryMark = VS->QueryCursor;
                        MemCpy(
                            VS->QueryBuffer, 
                            Registers()->String.Str, 
                            VS->QueryStringSize
                        );
                    } break;
                }

                if (Event.Kind != UI_EVENT_KIND_NULL)
                    ListPush(Scratch.MemPool, &APP_STATE->UIEvents, &Event);
            }
        }
    }

    if (APP_STATE->FrameDepth == 1) {
        Arena* HeadMemPool = APP_STATE->CommandMemPools[0];
        AppCommandList HeadCommands = APP_STATE->Commands[0];

        MemCpy(
            APP_STATE->CommandMemPools,
            APP_STATE->CommandMemPools + 1,
            sizeof(APP_STATE->CommandMemPools[0]) * (ARRAY_COUNT(APP_STATE->CommandMemPools) - 1)
        );
        MemCpy(
            APP_STATE->Commands,
            APP_STATE->Commands + 1,
            sizeof(APP_STATE->Commands[0]) * (ARRAY_COUNT(APP_STATE->Commands) - 1)
        );
        APP_STATE->CommandMemPools[ARRAY_COUNT(APP_STATE->CommandMemPools) - 1] = HeadMemPool;
        APP_STATE->Commands[ARRAY_COUNT(APP_STATE->Commands) - 1] = HeadCommands;
        ArenaClear(APP_STATE->CommandMemPools[0]);
        MemZero(&APP_STATE->Commands[0], sizeof(APP_STATE->Commands[0]));
        ++APP_STATE->CommandsGeneration;
    }

    f32 MasterAnimationsF = (f32) !!AppSettingFromNameB32("animations"_s8);
    f32 ScrollingAnimationsF = (f32) !!AppSettingFromNameB32("scrolling_animations"_s8);

    APP_STATE->CatchAllAnimationRate = 1.0f - MasterAnimationsF * PowF32(2.0f, (-60.0f * APP_STATE->FrameDeltaTime));
    APP_STATE->ScrollingAnimationRate = 1.0f - MasterAnimationsF * ScrollingAnimationsF * PowF32(2.0f, (-60.0f * APP_STATE->FrameDeltaTime));

    AppPushRegisters();

    APP_STATE->TabFilePaths = {};

    {
        ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
            Scratch.MemPool,
            ConfigNodeFromID(BaseRegisters()->Window)
        );

        for (
            ConfigPanelNode* P = PanelTree.Root;
            P != EMPTY_CFG_PANEL_NODE_VALUE;
        ) {
            for (
                ConfigNodePtrNode* N = P->Tabs.Head;
                N;
                N = N->Next
            ) {
                Str8 Path = ConfigNodeChildFromStr(N->V, "file"_s8)->Head->String;

                if (Path.Size) {
                    ListPush(
                        AppFrameMemPool(),
                        &APP_STATE->TabFilePaths,
                        ArenaPushStrCpy(AppFrameMemPool(), Path)
                    );
                }
            }

            if (P->Head != EMPTY_CFG_PANEL_NODE_VALUE) {
                P = P->Next;
            } else {
                for (
                    ;
                    (
                        P != EMPTY_CFG_PANEL_NODE_VALUE && 
                        P->Next == EMPTY_CFG_PANEL_NODE_VALUE
                    );
                    P = P->Parent
                ) {
                    if (P != EMPTY_CFG_PANEL_NODE_VALUE)
                        P = P->Next;
                }
            }
        }
    }

    AppWindowFrame();
    MemSet(&APP_STATE->UIEvents, 0, sizeof(APP_STATE->UIEvents));

    if (APP_STATE->DragDropState == APP_DRAG_DROP_STATE_DROPPING)
        APP_STATE->DragDropState = APP_DRAG_DROP_STATE_NULL;

    AppRegisters* WindowRegs = AppPopRegisters();

    MemCpy(Registers(), WindowRegs, sizeof(*WindowRegs));

    if (APP_STATE->NumFramesRequested)
        --APP_STATE->NumFramesRequested;

    AccessClose(APP_STATE->FrameAccess);
    APP_STATE->FrameAccess = FrameAccessRestore;

    u64 EndTimeUSecs = TimeNow();
    u64 FrameTimeUSecs = EndTimeUSecs - StartTimeUSecs;

    APP_STATE->FrameTimeUSecsHistory[
        APP_STATE->FrameIndex % ARRAY_COUNT(APP_STATE->FrameTimeUSecsHistory)
    ] = FrameTimeUSecs;

    if (
        APP_STATE->NumFramesRequested &&
        FrameTimeUSecs < FrameTimeTargetCapacityUSecs
    ) {
        SleepMSecs((u32) ((FrameTimeTargetCapacityUSecs - FrameTimeUSecs) / THOUSAND(1)));
    }

    ++APP_STATE->FrameIndex;
    APP_STATE->TimeSecs += APP_STATE->FrameDeltaTime;
    APP_STATE->TimeUSecs += FrameTimeUSecs;

    if (APP_STATE->FrameDepth == 1)
        ++APP_STATE->CommandsGeneration;

    --APP_STATE->FrameDepth;
    ReleaseScratch(Scratch);
}

void 
AppWindowFrame(void)
{
    TempArena Scratch = GetScratch(NULL, 0);
    ConfigNode* Window = ConfigNodeFromID(Registers()->Window);
    ConfigPanelTree PanelTree = ConfigPanelTreeFromConfig(
        Scratch.MemPool, 
        Window
    );
    b32 PopUpIsOpen = APP_STATE->PopUpActive;
    b32 QueryIsOpen = APP_STATE->QueryIsActive;

    if (PopUpIsOpen)
        APP_STATE->MenuBarKeyHeld = FALSE;

    UISelectState(APP_STATE->UI);
    Registers()->Panel = PanelTree.Focused->Config->ID;
    Registers()->Tab = PanelTree.Focused->SelectedTab->ID;
    Registers()->View = PanelTree.Focused->SelectedTab->ID;

    {
        Access* Acc = AccessOpen();
        ConfigNode* Session = ConfigNodeChildFromStr(
            ConfigNodeRoot(), 
            "session"_s8
        );
        ConfigNode* ThemeConfig = ConfigNodeChildFromStr(
            Session, 
            "theme"_s8
        );
        Str8 ThemeName = (
            ThemeConfig != EMPTY_CFG_NODE_VALUE && 
            ThemeConfig->Head != EMPTY_CFG_NODE_VALUE
        ) ? ThemeConfig->Head->String
          : APP_THEME_PRESET_DISPLAY_STR_TABLE[APP_THEME_PRESET_AYU_DARK];
        MDNode* ThemeTree = AppThemeTreeFromName(
            Scratch.MemPool, 
            Acc, 
            ThemeName
        );

        struct ThemePatternNode {
            ThemePatternNode* Next;
            UIThemePattern    Pattern;
        };

        ThemePatternNode* HeadPattern = NULL;
        ThemePatternNode* TailPattern = NULL;
        u64 PatternCount = 0;

        for (
            MDNode* N = ThemeTree;
            !MDNodeIsEmpty(N);
            N = MDNodeRecordDepthFirstPreOrder(N, ThemeTree).Next
        ) {
            if (StrMatch(N->String, "theme_colour"_s8, 0)) {
                MDNode* TagsChild = MDChildFromStr(N, "tags"_s8, 0);
                MDNode* ValueChild = MDChildFromStr(N, "value"_s8, 0);
                u8 SplitChar = ' ';
                Str8List Tags = StrSplit(
                    Scratch.MemPool,
                    TagsChild->Head->String,
                    &SplitChar,
                    1,
                    FALSE
                );
                u32 ColourU32 = U32FromStr(ValueChild->Head->String, 16);
                ThemePatternNode* Node = ArenaPushArrayZero(
                    Scratch.MemPool,
                    ThemePatternNode,
                    1
                );

                Node->Pattern.Tags = StrArrayFromList(
                    AppFrameMemPool(), 
                    &Tags
                );
                Node->Pattern.Linear = RGBAFromU32(ColourU32);
                SLL_QUEUE_PUSH(HeadPattern, TailPattern, Node);
                ++PatternCount;
            }
        }

        APP_STATE->Theme = ArenaPushArrayZero(AppFrameMemPool(), UITheme, 1);
        APP_STATE->Theme->PatternsCount = PatternCount;
        APP_STATE->Theme->Patterns = ArenaPushArrayZero(
            AppFrameMemPool(),
            UIThemePattern,
            APP_STATE->Theme->PatternsCount
        );

        u64 Index = 0;

        for (ThemePatternNode* N = HeadPattern; N; N = N->Next, ++Index)
            APP_STATE->Theme->Patterns[Index] = N->Pattern;

        AccessClose(Acc);
    }

    UIIconInfo IconInfo = {};

    IconInfo.IconKindTextMap[UI_ICON_KIND_RIGHT_ARROW] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_RIGHT_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_DOWN_ARROW] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_DOWN_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_LEFT_ARROW] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_LEFT_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_UP_ARROW] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_UP_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_RIGHT_CARET] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_RIGHT_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_DOWN_CARET] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_DOWN_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_LEFT_CARET] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_LEFT_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_UP_CARET] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_UP_CARET];
    IconInfo.IconKindTextMap[UI_ICON_KIND_CHECK_HOLLOW] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_CHECK_HOLLOW];
    IconInfo.IconKindTextMap[UI_ICON_KIND_CHECK_FILLED] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_CHECK_FILLED];
    IconInfo.IconKindTextMap[UI_ICON_KIND_RADIO_HOLLOW] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_RADIO_HOLLOW];
    IconInfo.IconKindTextMap[UI_ICON_KIND_RADIO_FILLED] = APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_RADIO_FILLED];

    UIAnimationInfo AnimationInfo = {};

    AnimationInfo.HotAnimationRate = APP_STATE->CatchAllAnimationRate;
    AnimationInfo.ActiveAnimationRate = APP_STATE->CatchAllAnimationRate;
    AnimationInfo.FocusAnimationRate = 1.0f;
    AnimationInfo.ScrollAnimationRate = APP_STATE->ScrollingAnimationRate;
    UIBeginBuild(
        &APP_STATE->UIEvents,
        &IconInfo,
        APP_STATE->Theme,
        &AnimationInfo,
        APP_STATE->FrameDeltaTime,
        APP_STATE->FrameDeltaTime
    );
    UIPushTextPadding(1.0f);
    UIPushPreferredWidth(UI_PX(20.0f, 1.0f));
    UIPushPreferredHeight(UI_PX(1.0f, 1.0f));

    r2f32 WindowRect = GetConsoleRect();
    r2f32 TopBarRect = Rng(
        WindowRect.X0,
        WindowRect.Y0,
        WindowRect.X1,
        WindowRect.Y0 + 1.0f
    );
    r2f32 BottomBarRect = Rng(
        WindowRect.X0,
        WindowRect.Y1 - 1.0f,
        WindowRect.X1,
        WindowRect.Y1
    );
    r2f32 ContentRect = Rng(
        WindowRect.X0,
        TopBarRect.Y1,
        WindowRect.X1,
        BottomBarRect.Y0
    );

    if (
        AppDragIsActive() &&
        APP_STATE->DragDropRegistersSlot == APP_REGISTERS_SLOT_VIEW
    ) {
        ConfigNode* DragTab = ConfigNodeFromID(
            APP_STATE->DragDropRegisters->View
        );

        if (
            DragTab == EMPTY_CFG_NODE_VALUE ||
            UISlotPress(UI_EVENT_ACTION_SLOT_CANCEL)
        ) {
            AppDragKill();
            UIKillAction();
        } else {
            UITooltip() {
                FancyStrList Title = AppTitleFStrFromConfig(
                    Scratch.MemPool,
                    DragTab,
                    FALSE
                );
                UIBox* Box = UIBuildBoxFromKey(UI_BOX_KIND_DRAW_TEXT, {});

                UIBoxEquipDisplayFancyStrs(Box, &Title);
            }
        }
    }

    if (UIStringHoverActive()) {
        UITooltip() {
            TempArena Scratch = GetScratch(NULL, 0);
            FancyStrList FancyStrings = UIStringHoverFancyStrings(Scratch.MemPool);
            UIBox* Box = UIBuildBoxFromKey(UI_BOX_KIND_DRAW_TEXT, {});

            UIBoxEquipDisplayFancyStrs(Box, &FancyStrings);
            ReleaseScratch(Scratch);
        }
    }

    if (APP_STATE->DevMenuIsOpen) {
        u64 FrameTimeSumUSecs = 0;
        u64 FrameTimeCount = MIN(
            ARRAY_COUNT(APP_STATE->FrameTimeUSecsHistory), 
            APP_STATE->FrameIndex
        );

        for (u64 Index = 0; Index < FrameTimeCount; ++Index)
            FrameTimeSumUSecs += APP_STATE->FrameTimeUSecsHistory[Index];

        UITag("floating"_s8) {
            UIPane(
                Rng(
                    WindowRect.X1 - 46.0f,
                    WindowRect.Y0 + 2.0f,
                    WindowRect.X1 - 2.0f,
                    WindowRect.Y0 + 13.0f
                ),
                "###dev_ctx_menu"_s8
            ) {
                UIWidthFill() {
                    UILabel("Frame: %llu", APP_STATE->FrameIndex);
                    // UILabel("Frames requested: %llu", APP_STATE->NumFramesRequested);
                    // UILabel("Frame time: %llu us", FrameTimeCount ? FrameTimeSumUSecs / FrameTimeCount : 0);
                    UILabel("Frame bytes: %llu", REND_STATE->PrevFrameBytes);
                    // UILabel("UI is animating: %d", (i32) UIIsAnimatingFromState(APP_STATE->UI));
                    UILabel("UI box count: %llu", APP_STATE->UI->LastBuildBoxCount);
                    // UILabel("UI build index: %llu", APP_STATE->UI->BuildIndex);
                    UILabel("UI table: %llu", APP_STATE->UI->TableBoxCount);
                    UILabel("UI free: %llu", APP_STATE->UI->FreeBoxCount);
                    UILabel("UI pruned: %llu", APP_STATE->UI->PrunedBoxCount);
                    UILabel("UI memory: %llu", ArenaGetPosition(APP_STATE->UI->MemPool));
                    UILabel(
                        "UI build memory: %llu, %llu",
                        ArenaGetPosition(APP_STATE->UI->BuildMemPools[0]),
                        ArenaGetPosition(APP_STATE->UI->BuildMemPools[1])
                    );
                    UILabel(
                        "Frame memory: %llu, %llu",
                        ArenaGetPosition(APP_STATE->FrameMemPools[0]),
                        ArenaGetPosition(APP_STATE->FrameMemPools[1])
                    );
                }
            }
        }
    }

    if (APP_STATE->PopUpActive) {
        UIFocus(UI_FOCUS_KIND_ROOT) {
            v2f32 WindowLength = Length(WindowRect);
            f32 PopUpWidth = MIN(60.0f, WindowLength.X);
            r2f32 PopUpRect = Rng(
                WindowRect.X0 + FloorF32((WindowLength.X - PopUpWidth) / 2.0f),
                WindowRect.Y0 + FloorF32(WindowLength.Y / 2.0f) - 4.0f,
                WindowRect.X0 + FloorF32((WindowLength.X - PopUpWidth) / 2.0f) + PopUpWidth,
                WindowRect.Y0 + FloorF32(WindowLength.Y / 2.0f) + 3.0f
            );
            UIBox* BackgroundBox = EMPTY_UI_BOX_VALUE;

            UIContextMenuClose();

            UITag("floating"_s8) {
                UISetNextFlags(
                    UI_BOX_KIND_DEFAULT_FOCUS_NAV |
                    UI_BOX_KIND_DISABLE_FOCUS_OVERLAY |
                    UI_BOX_KIND_DRAW_DROP_SHADOW
                );

                UIPane(PopUpRect, "###popup"_s8) {
                    UIBox* PopUpBox = UIHeadParent();

                    UIWidthFill() {
                        UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                            UILabel(APP_STATE->PopUpTitle);

                            UITag("weak"_s8) {
                                UILabel(APP_STATE->PopUpDescription);
                            }

                            UISpacer(UI_PX(1.0f, 1.0f));

                            UIRow() {
                                UIPadding(UI_PERCENT(1.0f, 0.0f)) {
                                    UIPreferredWidth(UI_PX(12.0f, 1.0f)) {
                                        UITag("pop"_s8) {
                                            if (
                                                UI_CLICKED(UIButton("OK"_s8)) ||
                                                (
                                                    (PopUpBox->DefaultNavFocusHotKey == EMPTY_UI_KEY_VALUE) &&
                                                    UISlotPress(UI_EVENT_ACTION_SLOT_ACCEPT)
                                                )
                                            ) {
                                                AppCmd(APP_COMMAND_KIND_POP_UP_ACCEPT);
                                            }

                                            UISpacer(UI_PX(2.0f, 1.0f));

                                            if (UI_CLICKED(UIButton("Cancel"_s8)) || UISlotPress(UI_EVENT_ACTION_SLOT_CANCEL)) {
                                                AppCmd(APP_COMMAND_KIND_POP_UP_CANCEL);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            UIRect(WindowRect) {
                BackgroundBox = UIBuildBoxFromStr(
                    (
                        UI_BOX_KIND_FLOATING |
                        UI_BOX_KIND_CLICKABLE |
                        UI_BOX_KIND_SCROLL |
                        UI_BOX_KIND_DRAW_OVERLAY
                    ),
                    "###popup_background"_s8
                );
            }

            UISignalFromBox(BackgroundBox);
        }
    }

    if (QueryIsOpen && !PopUpIsOpen)
        AppQueryViewUI(ContentRect);

    UIKey FileMenuKey = UIKeyFromStr({}, "_file_menu_key_"_s8);
    UIKey ViewMenuKey = UIKeyFromStr({}, "_view_menu_key_"_s8);
    UIKey GoMenuKey = UIKeyFromStr({}, "_go_menu_key_"_s8);
    UIKey TabMenuKey = UIKeyFromStr({}, "_tab_menu_key_"_s8);
    UIKey PanelMenuKey = UIKeyFromStr({}, "_panel_menu_key_"_s8);
    UIKey HelpMenuKey = UIKeyFromStr({}, "_help_menu_key_"_s8);
    UIKey TabCtxMenuKey = UIKeyFromStr({}, "_tab_ctx_menu_key_"_s8);

    UIContextMenu(FileMenuKey) {
        UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
            UITag("floating"_s8) {
                Str8 Commands[] = {
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_PALETTE].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_EXIT].String
                };
                u32 CodePoints[] = {
                    'o',
                    'p',
                    0,
                    'x'
                };

                STATIC_ASSERT(
                    ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands), 
                    FileMenuCommandsSizeCheck
                );
                AppCommandListMenuButtons(
                    Commands, 
                    ARRAY_COUNT(Commands), 
                    CodePoints
                );
            }
        }
    }

    UIContextMenu(ViewMenuKey) {
        UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
            UITag("floating"_s8) {
                Str8 Commands[] = {
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_FILES].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_VIEWS].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_SET_THEME].String
                };
                u32 CodePoints[] = {
                    'f',
                    'v',
                    0,
                    't'
                };

                STATIC_ASSERT(
                    ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands), 
                    ViewMenuCommandsSizeCheck
                );
                AppCommandListMenuButtons(
                    Commands, 
                    ARRAY_COUNT(Commands), 
                    CodePoints
                );
            }
        }
    }

    UIContextMenu(GoMenuKey) {
        UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
            UITag("floating"_s8) {
                Str8 Commands[] = {
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_GOTO_TIME].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_SEARCH].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_SEARCH_BACKWARDS].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FIND_NEXT].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FIND_PREV].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FILTER].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_GO_BACK].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_GO_FORWARD].String
                };
                u32 CodePoints[] = {
                    't',
                    0,
                    's',
                    'w',
                    'n',
                    'p',
                    'f',
                    0,
                    'b',
                    'r'
                };

                STATIC_ASSERT(
                    ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands), 
                    GoMenuCommandsSizeCheck
                );
                AppCommandListMenuButtons(
                    Commands, 
                    ARRAY_COUNT(Commands), 
                    CodePoints
                );
            }
        }
    }

    UIContextMenu(TabMenuKey) {
        UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
            UITag("floating"_s8) {
                Str8 Commands[] = {
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_TAB].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_DUPLICATE_TAB].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_SET_TAB_FILE].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_CLOSE_TAB].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_MOVE_TAB_LEFT].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_MOVE_TAB_RIGHT].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_NEXT_TAB].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_PREV_TAB].String
                };
                u32 CodePoints[] = {
                    'o',
                    'd',
                    'f',
                    'c',
                    0,
                    'l',
                    'r',
                    0,
                    'n',
                    'p'
                };

                STATIC_ASSERT(
                    ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands), 
                    TabMenuCommandsSizeCheck
                );
                AppCommandListMenuButtons(
                    Commands, 
                    ARRAY_COUNT(Commands), 
                    CodePoints
                );
            }
        }
    }

    UIContextMenu(PanelMenuKey) {
        UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
            UITag("floating"_s8) {
                Str8 Commands[] = {
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_NEW_PANEL_RIGHT].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_NEW_PANEL_DOWN].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_NEW_PANEL_LEFT].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_NEW_PANEL_UP].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_CLOSE_PANEL].String,
                    {},
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_NEXT_PANEL].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_PREV_PANEL].String
                };
                u32 CodePoints[] = {
                    'r',
                    'd',
                    'l',
                    'u',
                    0,
                    'c',
                    0,
                    'n',
                    'p'
                };

                STATIC_ASSERT(
                    ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands),
                    PanelMenuCommandsSizeCheck
                );
                AppCommandListMenuButtons(
                    Commands,
                    ARRAY_COUNT(Commands),
                    CodePoints
                );
            }
        }
    }

    UIContextMenu(HelpMenuKey) {
        UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
            UITag("floating"_s8) {
                Str8 Commands[] = {
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_PALETTE].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_RESET_TO_DEFAULT_BINDINGS].String,
                    APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_TOGGLE_DEV_MENU].String
                };
                u32 CodePoints[] = {
                    'p',
                    'k',
                    'd'
                };

                STATIC_ASSERT(
                    ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands), 
                    HelpMenuCommandsSizeCheck
                );
                AppCommandListMenuButtons(
                    Commands, 
                    ARRAY_COUNT(Commands), 
                    CodePoints
                );
            }
        }
    }

    UIContextMenu(TabCtxMenuKey) {
        ConfigNode* MenuTab = ConfigNodeFromID(APP_STATE->TabCtxMenuTab);

        if (MenuTab == EMPTY_CFG_NODE_VALUE) {
            UIContextMenuClose();
        } else {
            UIPreferredWidth(UI_PX(48.0f, 1.0f)) {
                UITag("floating"_s8) {
                    AppRegistersScope(
                        __Registers.Panel = MenuTab->Parent->ID,
                        __Registers.Tab = MenuTab->ID,
                        __Registers.View = MenuTab->ID
                    ) {
                        Str8 TabFilePath = AppViewFilePath();

                        if (TabFilePath.Size) {
                            UISetNextPreferredHeight(UI_SUM_OF_CHILDREN(1.0f));

                            UIColumn() {
                                for (
                                    u64 Index = 0;
                                    Index < ARRAY_COUNT(APP_TAB_FAST_PATH_VIEW_NAME_TABLE);
                                    ++Index
                                ) {
                                    AppCommandKindInfo* ViewInfo = &APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD + Index];
                                    Str8 ViewName = APP_TAB_FAST_PATH_VIEW_NAME_TABLE[Index];

                                    if (!(ViewInfo->Flags & APP_COMMAND_KIND_FLAG_LIST_IN_FILE))
                                        continue;

                                    UISignal Sig = AppIconButton(
                                        StrMatch(MenuTab->String, ViewName, 0)
                                            ? APP_ICON_KIND_RADIO_FILLED
                                            : APP_ICON_KIND_RADIO_HOLLOW,
                                        NULL,
                                        "%S###tab_view_%S",
                                        PRINT_STR(
                                            AppVocabInfoFromCodeName(
                                                ViewInfo->String
                                            )->DisplayName
                                        ),
                                        PRINT_STR(ViewName)
                                    );

                                    if (UI_CLICKED(Sig)) {
                                        AppCmd(
                                            APP_COMMAND_KIND_SET_TAB_VIEW,
                                            __Registers.String = ViewName
                                        );
                                        UIContextMenuClose();
                                    }
                                }

                                UIDivider(UI_PX(1.0f, 1.0f));
                                UISetNextFocusHot(UI_FOCUS_KIND_ON);
                                UISetNextFocusActive(UI_FOCUS_KIND_ON);

                                UINamedRow("###tab_file_row"_s8) {
                                    UIKey FileEditKey = UIKeyFromStr(
                                        UIHeadParent()->Key,
                                        "###tab_file"_s8
                                    );

                                    if (UIIsKeyAutoFocusActive(FileEditKey))
                                        APP_STATE->TextEditMode = TRUE;

                                    UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                                        UITag("weak"_s8) {
                                            UILabel(APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_FILE]);
                                        }
                                    }

                                    UIPreferredWidth(UI_PERCENT(1.0f, 0.0f)) {
                                        UITag("alt"_s8) {
                                            UISignal EditSig = UILineEdit(
                                                &APP_STATE->TabCtxMenuCursor,
                                                &APP_STATE->TabCtxMenuMark,
                                                APP_STATE->TabCtxMenuFileBuffer,
                                                sizeof(APP_STATE->TabCtxMenuFileBuffer),
                                                &APP_STATE->TabCtxMenuFileSize,
                                                TabFilePath,
                                                "###tab_file"_s8
                                            );

                                            if (UI_COMMITTED(EditSig)) {
                                                AppCmd(
                                                    APP_COMMAND_KIND_SET_TAB_FILE,
                                                    __Registers.FilePath = Str(
                                                        APP_STATE->TabCtxMenuFileBuffer,
                                                        APP_STATE->TabCtxMenuFileSize
                                                    )
                                                );
                                                UIContextMenuClose();
                                            }
                                        }
                                    }
                                }

                                UIDivider(UI_PX(1.0f, 1.0f));
                            }
                        }

                        Str8 Commands[] = {
                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_DUPLICATE_TAB].String,
                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_CLOSE_TAB].String,
                            {},
                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_MOVE_TAB_LEFT].String,
                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_MOVE_TAB_RIGHT].String
                        };
                        u32 CodePoints[] = {
                            0,
                            0,
                            0,
                            0,
                            0
                        };

                        STATIC_ASSERT(
                            ARRAY_COUNT(CodePoints) == ARRAY_COUNT(Commands),
                            TabCtxMenuCommandsSizeCheck
                        );
                        AppCommandListMenuButtons(
                            Commands,
                            ARRAY_COUNT(Commands),
                            CodePoints
                        );
                    }
                }
            }
        }
    }

    UIFocus(
        (APP_STATE->MenuBarFocused && !UIAnyContextMenuIsOpen())
        ? UI_FOCUS_KIND_ON
        : UI_FOCUS_KIND_NULL
    ) {
        UITag("menu_bar"_s8) {
            UIBox* TopBarBox = EMPTY_UI_BOX_VALUE;

            UIRect(TopBarRect) {
                UISetNextChildLayoutAxis(AXIS_2D_X);
                UISetNextFlags(
                    UI_BOX_KIND_DEFAULT_FOCUS_NAV | UI_BOX_KIND_DISABLE_FOCUS_OVERLAY
                );
                TopBarBox = UIBuildBoxFromStr(
                    UI_BOX_KIND_CLICKABLE | UI_BOX_KIND_DRAW_BACKGROUND,
                    "###top_bar"_s8
                );
            }

            UIParent(TopBarBox) {
                UIFocus(UI_FOCUS_KIND_NULL) {
                    UIKey MenuBarGroupKey = UIKeyFromStr(
                        EMPTY_UI_KEY_VALUE, 
                        "###top_bar_group"_s8
                    );

                    UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                        UIGroupKey(MenuBarGroupKey) {
                            UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                                struct {
                                    Str8      Name;
                                    u32       CodePoint;
                                    InputKind Input;
                                    UIKey     MenuKey;
                                } Items[] = {
                                    {
                                        Str8Lit("File"),
                                        'f',
                                        INPUT_KIND_F,
                                        FileMenuKey
                                    },
                                    {
                                        Str8Lit("View"),
                                        'v',
                                        INPUT_KIND_V,
                                        ViewMenuKey
                                    },
                                    {
                                        Str8Lit("Go"),
                                        'g',
                                        INPUT_KIND_G,
                                        GoMenuKey
                                    },
                                    {
                                        Str8Lit("Tab"),
                                        't',
                                        INPUT_KIND_T,
                                        TabMenuKey
                                    },
                                    {
                                        Str8Lit("Panel"),
                                        'p',
                                        INPUT_KIND_P,
                                        PanelMenuKey
                                    },
                                    {
                                        Str8Lit("Help"),
                                        'h',
                                        INPUT_KIND_H,
                                        HelpMenuKey
                                    }
                                };

                                b32 MenuOpen = FALSE;
                                u64 OpenMenuIndex = 0;

                                for (u64 I = 0; I < ARRAY_COUNT(Items); ++I) {
                                    if (UIContextMenuIsOpen(Items[I].MenuKey)) {
                                        MenuOpen = TRUE;
                                        OpenMenuIndex = I;
                                        break;
                                    }
                                }

                                u64 OpenMenuIndexPrime = OpenMenuIndex;

                                if (MenuOpen && APP_STATE->MenuBarFocused) {
                                    for (UIEvent* Evt = NULL; UINextEvent(&Evt); ) {
                                        b32 Taken = FALSE;

                                        if (Evt->DeltaI32.X > 0) {
                                            Taken = TRUE;
                                            ++OpenMenuIndexPrime;
                                            OpenMenuIndexPrime = OpenMenuIndexPrime % ARRAY_COUNT(Items);
                                        }

                                        if (Evt->DeltaI32.X < 0) {
                                            Taken = TRUE;
                                            OpenMenuIndexPrime = OpenMenuIndexPrime > 0 
                                                ? OpenMenuIndexPrime - 1 
                                                : (ARRAY_COUNT(Items) - 1);
                                        }

                                        if (Taken)
                                            UIEatEvent(Evt);
                                    }
                                }

                                for (u64 I = 0; I < ARRAY_COUNT(Items); ++I) {
                                    UISetNextFastpathCodepoint(Items[I].CodePoint);

                                    b32 AltFastpathKey = FALSE;

                                    if (
                                        APP_STATE->AltMenuBarEnabled && 
                                        UIKeyPress(INPUT_MOD_KIND_ALT, Items[I].Input)
                                    ) {
                                        AltFastpathKey = TRUE;
                                    }

                                    if (
                                        (
                                            APP_STATE->MenuBarKeyHeld || 
                                            APP_STATE->MenuBarFocused
                                        ) && 
                                        !UIAnyContextMenuIsOpen()
                                    ) {
                                        UISetNextFlags(UI_BOX_KIND_DRAW_TEXT_FASTPATH_CODEPOINT);
                                    }

                                    UISignal Sig = AppMenuBarButton(Items[I].Name);

                                    if (MenuOpen) {
                                        if (
                                            (UI_HOVERING(Sig) && !UIContextMenuIsOpen(Items[I].MenuKey)) ||
                                            (OpenMenuIndexPrime == I && OpenMenuIndexPrime != OpenMenuIndex)
                                        ) {
                                            UIContextMenuOpen(
                                                Items[I].MenuKey, 
                                                Sig.Box->Key, 
                                                Vec(0.0f, Sig.Box->Rect.Y1 - Sig.Box->Rect.Y0)
                                            );
                                        }
                                    } else if (UI_PRESSED(Sig) || AltFastpathKey) {
                                        if (UIContextMenuIsOpen(Items[I].MenuKey)) {
                                            UIContextMenuClose();
                                        } else {
                                            UIContextMenuOpen(
                                                Items[I].MenuKey, 
                                                Sig.Box->Key, 
                                                Vec(0.0f, Sig.Box->Rect.Y1 - Sig.Box->Rect.Y0)
                                            );
                                        }
                                    }
                                }
                            }
                        }

                        UISpacer(UI_PERCENT(1.0f, 0.0f));

                        UITag("weak"_s8) {
                            UILabel("FLAP"_s8);
                        }
                    }
                }
            }

            UISignalFromBox(TopBarBox);
        }
    }

    UITag("menu_bar"_s8) {
        UIBox* BottomBarBox = EMPTY_UI_BOX_VALUE;

        UIRect(BottomBarRect) {
            UISetNextChildLayoutAxis(AXIS_2D_X);
            BottomBarBox = UIBuildBoxFromStr(
                UI_BOX_KIND_DRAW_BACKGROUND,
                "###bottom_bar"_s8
            );
        }

        UIParent(BottomBarBox) {
            UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                if (APP_STATE->ErrorStrSize) {
                    UITag("bad"_s8) {
                        UILabel(Str(APP_STATE->ErrorBuffer, APP_STATE->ErrorStrSize));
                    }
                } else if (Registers()->FilePath.Size) {
                    UILabel(Registers()->FilePath);
                } else {
                    UILabel("No file"_s8);
                }

                UISpacer(UI_PERCENT(1.0f, 0.0f));

                AppCommandKind BarCommands[] = {
                    APP_COMMAND_KIND_OPEN_PALETTE,
                    APP_COMMAND_KIND_OPEN,
                    APP_COMMAND_KIND_EXIT
                };

                for (u64 Index = 0; Index < ARRAY_COUNT(BarCommands); ++Index) {
                    AppCommandKindInfo* BarInfo = &APP_COMMAND_KIND_INFO_TABLE[BarCommands[Index]];
                    ConfigInputMapNodePtrList BarKeys = ConfigInputMapNodePtrListFromName(
                        Scratch.MemPool,
                        APP_STATE->KeyMap,
                        BarInfo->String
                    );
                    UISignal Sig = UIButton(
                        "%S###bar_cmd_%S",
                        PRINT_STR(
                            AppVocabInfoFromCodeName(
                                BarInfo->String
                            )->DisplayName
                        ),
                        PRINT_STR(BarInfo->String)
                    );

                    if (UI_CLICKED(Sig)) {
                        AppCmd(
                            APP_COMMAND_KIND_RUN_COMMAND,
                            __Registers.CommandName = BarInfo->String
                        );
                    }

                    AppCommandBindingButtons(
                        BarInfo->String,
                        ""_s8,
                        1,
                        BarKeys.Head
                            ? 0
                            : APP_COMMAND_BINDING_BTN_FLAG_ADD_NEW
                    );
                    UISpacer(UI_PX(1.0f, 1.0f));
                }
            }
        }
    }

    if (ContentRect.X1 > ContentRect.X0 && ContentRect.Y1 > ContentRect.Y0) {
        for (
            ConfigPanelNode* Panel = PanelTree.Root;
            Panel != EMPTY_CFG_PANEL_NODE_VALUE;
            Panel = ConfigPanelNodeRecDepthFirstPre(PanelTree.Root, Panel).Next
        ) {
            if (
                Panel->Head == EMPTY_CFG_PANEL_NODE_VALUE ||
                Panel->SplitAxis != AXIS_2D_X
            ) {
                continue;
            }

            r2f32 ParentRect = ConfigTargetRectFromPanelNode(
                ContentRect,
                PanelTree.Root,
                Panel
            );
            f32 TotalSize = Length(ParentRect).X;

            for (
                ConfigPanelNode* MinChild = Panel->Head;
                MinChild->Next != EMPTY_CFG_PANEL_NODE_VALUE;
                MinChild = MinChild->Next
            ) {
                ConfigPanelNode* MaxChild = MinChild->Next;
                r2f32 MinChildRect = ConfigTargetRectFromPanelNodeChild(
                    ParentRect,
                    Panel,
                    MinChild
                );
                r2f32 BoundaryRect = Rng(
                    MinChildRect.X1 - 1.0f,
                    ParentRect.Y0,
                    MinChildRect.X1,
                    ParentRect.Y1
                );
                UIBox* BoundaryBox = EMPTY_UI_BOX_VALUE;

                UIRect(BoundaryRect) {
                    UITag("menu_bar"_s8) {
                        UISetNextHoverCursor(CURSOR_KIND_LEFT_RIGHT);
                        BoundaryBox = UIBuildBoxFromStrFmt(
                            (
                                UI_BOX_KIND_MOUSE_CLICKABLE |
                                UI_BOX_KIND_DRAW_BACKGROUND |
                                UI_BOX_KIND_DRAW_HOT_EFFECTS |
                                UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
                            ),
                            "###panel_boundary_%p_%p",
                            MinChild->Config,
                            MaxChild->Config
                        );
                    }
                }

                UISignal Sig = UISignalFromBox(BoundaryBox);

                if (UI_DOUBLE_CLICKED(Sig)) {
                    f32 HalfPercent = 0.5f * (MinChild->PercentOfParent + MaxChild->PercentOfParent);

                    UIKillAction();
                    MinChild->PercentOfParent = HalfPercent;
                    MaxChild->PercentOfParent = HalfPercent;
                    ConfigNodeEquipStr(
                        APP_STATE->Config,
                        MinChild->Config,
                        "%f",
                        HalfPercent
                    );
                    ConfigNodeEquipStr(
                        APP_STATE->Config,
                        MaxChild->Config,
                        "%f",
                        HalfPercent
                    );
                } else if (UI_PRESSED(Sig)) {
                    v2f32 Percents = Vec(
                        MinChild->PercentOfParent,
                        MaxChild->PercentOfParent
                    );

                    UIStoreDragStruct(&Percents);
                } else if (UI_DRAGGING(Sig)) {
                    v2f32 Percents = *UIGetDragStruct(v2f32);
                    f32 BothSize = (Percents.X + Percents.Y) * TotalSize;

                    if (BothSize >= 20.0f) {
                        f32 MinSize = CLAMP(
                            10.0f,
                            Percents.X * TotalSize + UIDragDelta().X,
                            BothSize - 10.0f
                        );

                        MinChild->PercentOfParent = MinSize / TotalSize;
                        MaxChild->PercentOfParent = (BothSize - MinSize) / TotalSize;
                        ConfigNodeEquipStr(
                            APP_STATE->Config,
                            MinChild->Config,
                            "%f",
                            MinChild->PercentOfParent
                        );
                        ConfigNodeEquipStr(
                            APP_STATE->Config,
                            MaxChild->Config,
                            "%f",
                            MaxChild->PercentOfParent
                        );
                    }
                }
            }
        }

        for (
            ConfigPanelNode* Panel = PanelTree.Root;
            Panel != EMPTY_CFG_PANEL_NODE_VALUE;
            Panel = ConfigPanelNodeRecDepthFirstPre(PanelTree.Root, Panel).Next
        ) {
            if (Panel->Head != EMPTY_CFG_PANEL_NODE_VALUE)
                continue;

            b32 PanelIsFocused = (
                !APP_STATE->MenuBarFocused &&
                !QueryIsOpen &&
                !PopUpIsOpen &&
                !UIAnyContextMenuIsOpen() &&
                PanelTree.Focused == Panel
            );
            ConfigNode* SelectedTab = Panel->SelectedTab;
            AppViewState* SelectedTabViewState = AppViewStateFromConfig(SelectedTab);

            UIFocus((PanelIsFocused) ? UI_FOCUS_KIND_NULL : UI_FOCUS_KIND_OFF) {
                r2f32 PanelRect = ConfigTargetRectFromPanelNode(ContentRect, PanelTree.Root, Panel);

                if (PanelRect.X1 < ContentRect.X1)
                    PanelRect.X1 -= 1.0f;

                r2f32 TabBarRect = Rng(
                    PanelRect.X0,
                    PanelRect.Y0,
                    PanelRect.X1,
                    PanelRect.Y0 + 1.0f
                );
                r2f32 PanelContentRect = Rng(
                    PanelRect.X0,
                    PanelRect.Y0 + 1.0f,
                    PanelRect.X1,
                    PanelRect.Y1
                );

                if (Panel->TabSide == SIDE_MAX) {
                    TabBarRect.Y0 = PanelRect.Y1 - 1.0f;
                    TabBarRect.Y1 = PanelRect.Y1;
                    PanelContentRect.Y0 = PanelRect.Y0;
                    PanelContentRect.Y1 = PanelRect.Y1 - 1.0f;
                }

                b32 BuildPanel = (
                    PanelContentRect.X1 > PanelContentRect.X0 &&
                    PanelContentRect.Y1 > PanelContentRect.Y0
                );

                if (
                    BuildPanel &&
                    AppDragIsActive() &&
                    APP_STATE->DragDropRegistersSlot == APP_REGISTERS_SLOT_VIEW
                ) {
                    ConfigNode* DragTab = ConfigNodeFromID(
                        APP_STATE->DragDropRegisters->View
                    );
                    b32 DragIsFromThisPanel = (DragTab->Parent == Panel->Config);
                    f32 SideX = FloorF32(Length(PanelContentRect).X / 4.0f);
                    f32 SideY = FloorF32(Length(PanelContentRect).Y / 4.0f);

                    struct {
                        Direction2D Direction;
                        r2f32       Rect;
                    } Sites[] = {
                        {
                            DIR_2D_LEFT,
                            Rng(
                                PanelContentRect.X0,
                                PanelContentRect.Y0,
                                PanelContentRect.X0 + SideX,
                                PanelContentRect.Y1
                            )
                        },
                        {
                            DIR_2D_RIGHT,
                            Rng(
                                PanelContentRect.X1 - SideX,
                                PanelContentRect.Y0,
                                PanelContentRect.X1,
                                PanelContentRect.Y1
                            )
                        },
                        {
                            DIR_2D_UP,
                            Rng(
                                PanelContentRect.X0 + SideX,
                                PanelContentRect.Y0,
                                PanelContentRect.X1 - SideX,
                                PanelContentRect.Y0 + SideY
                            )
                        },
                        {
                            DIR_2D_DOWN,
                            Rng(
                                PanelContentRect.X0 + SideX,
                                PanelContentRect.Y1 - SideY,
                                PanelContentRect.X1 - SideX,
                                PanelContentRect.Y1
                            )
                        },
                        {
                            DIR_2D_INVALID,
                            Rng(
                                PanelContentRect.X0 + SideX,
                                PanelContentRect.Y0 + SideY,
                                PanelContentRect.X1 - SideX,
                                PanelContentRect.Y1 - SideY
                            )
                        }
                    };

                    for (u64 Index = 0; Index < ARRAY_COUNT(Sites); ++Index) {
                        Direction2D Direction = Sites[Index].Direction;

                        if (
                            DragIsFromThisPanel &&
                            (
                                Direction == DIR_2D_INVALID ||
                                Panel->Tabs.Count < 2
                            )
                        ) {
                            continue;
                        }

                        UIBox* SiteBox = EMPTY_UI_BOX_VALUE;

                        UIRect(Sites[Index].Rect) {
                            SiteBox = UIBuildBoxFromStrFmt(
                                UI_BOX_KIND_DROP_SITE,
                                "###drop_site_%d_%p",
                                (i32) Direction,
                                Panel->Config
                            );
                        }

                        UISignalFromBox(SiteBox);

                        if (SiteBox->Key != UIDropHotKey())
                            continue;

                        r2f32 FutureRect = PanelRect;

                        if (Direction != DIR_2D_INVALID) {
                            Axis2D SplitAxis = Axis2DFromDir2D(Direction);
                            SideKind SplitSide = SideFromDir2D(Direction);

                            FutureRect.V[FLIP_SIDE(SplitSide)].V[SplitAxis] = FloorF32(
                                Centre(PanelRect).V[SplitAxis]
                            );
                        }

                        UIRect(FutureRect) {
                            UITag("drop_site"_s8) {
                                UIBuildBoxFromKey(
                                    UI_BOX_KIND_DRAW_BACKGROUND,
                                    EMPTY_UI_KEY_VALUE
                                );
                            }
                        }

                        if (AppDragDrop()) {
                            if (Direction != DIR_2D_INVALID) {
                                AppCmd(
                                    APP_COMMAND_KIND_SPLIT_PANEL,
                                    __Registers.DstPanel = Panel->Config->ID,
                                    __Registers.Panel = APP_STATE->DragDropRegisters->Panel,
                                    __Registers.Tab = APP_STATE->DragDropRegisters->View,
                                    __Registers.View = APP_STATE->DragDropRegisters->View,
                                    __Registers.Direction = Direction
                                );
                            } else {
                                AppCmd(
                                    APP_COMMAND_KIND_MOVE_VIEW,
                                    __Registers.DstPanel = Panel->Config->ID,
                                    __Registers.Panel = APP_STATE->DragDropRegisters->Panel,
                                    __Registers.Tab = APP_STATE->DragDropRegisters->View,
                                    __Registers.View = APP_STATE->DragDropRegisters->View,
                                    __Registers.PrevTab = ConfigTailNode(&Panel->Tabs)->ID
                                );
                            }
                        }
                    }
                }

                UIBox* PanelBox = EMPTY_UI_BOX_VALUE;

                if (BuildPanel) {
                    UIRect(PanelContentRect) {
                        UIChildLayoutAxis(AXIS_2D_Y) {
                            UIFocus(UI_FOCUS_KIND_ON) {
                                UIKey PanelKey = UIKeyFromStrFmt({}, "panel_box_%p", Panel->Config);

                                PanelBox = UIBuildBoxFromKey(
                                    (
                                        UI_BOX_KIND_MOUSE_CLICKABLE |
                                        UI_BOX_KIND_CLIP |
                                        UI_BOX_KIND_DISABLE_FOCUS_EFFECTS
                                    ),
                                    PanelKey
                                );
                            }
                        }
                    }
                }

                UIBox* LoadingOverlayContainer = EMPTY_UI_BOX_VALUE;

                if (BuildPanel) {
                    UIParent(PanelBox) {
                        UIWidthFill() {
                            UIHeightFill() {
                                LoadingOverlayContainer = UIBuildBoxFromKey(
                                    UI_BOX_KIND_FLOATING, 
                                    EMPTY_UI_KEY_VALUE
                                );
                            }
                        }
                    }
                }

                if (BuildPanel) {
                    UIParent(PanelBox) {
                        UIFocus(
                            (PanelIsFocused) 
                                ? UI_FOCUS_KIND_NULL 
                                : UI_FOCUS_KIND_OFF
                        ) {
                            UIWidthFill() {
                                AppPushRegisters(
                                    __Registers.Panel = Panel->Config->ID,
                                    __Registers.Tab = SelectedTab->ID,
                                    __Registers.View = SelectedTab->ID
                                );

                                {
                                    Str8 ViewFilePath = AppViewFilePath();

                                    if (ViewFilePath.Size)
                                        Registers()->FilePath = ViewFilePath;
                                }

                                UIBox* ViewContainerBox = EMPTY_UI_BOX_VALUE;

                                UIFixedWidth(Length(PanelContentRect).X) {
                                    UIFixedHeight(Length(PanelContentRect).Y) {
                                        UIChildLayoutAxis(AXIS_2D_Y) {
                                            ViewContainerBox = UIBuildBoxFromKey(
                                                0, 
                                                EMPTY_UI_KEY_VALUE
                                            );
                                        }
                                    }
                                }

                                UIParent(ViewContainerBox) {
                                    if (SelectedTab != EMPTY_CFG_NODE_VALUE) {
                                        AppViewUI(PanelContentRect);
                                    } else {
                                        Str8 Commands[] = {
                                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN].String,
                                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_FILES].String,
                                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_TAB].String,
                                            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_CLOSE_PANEL].String
                                        };
                                        u64 CommandsCount = ARRAY_COUNT(Commands);

                                        if (Panel->Parent == EMPTY_CFG_PANEL_NODE_VALUE)
                                            --CommandsCount;

                                        UISpacer(UI_PERCENT(1.0f, 0.0f));

                                        for (u64 Index = 0; Index < CommandsCount; ++Index) {
                                            UIRow() {
                                                UISpacer(UI_PERCENT(1.0f, 0.0f));

                                                UIPreferredWidth(UI_PX(40.0f, 1.0f)) {
                                                    UISignal Sig = AppCommandSpecButton(
                                                        Commands[Index]
                                                    );

                                                    if (UI_CLICKED(Sig)) {
                                                        AppCmd(
                                                            APP_COMMAND_KIND_RUN_COMMAND,
                                                            __Registers.CommandName = Commands[Index]
                                                        );
                                                    }
                                                }

                                                UISpacer(UI_PERCENT(1.0f, 0.0f));
                                            }
                                        }

                                        UISpacer(UI_PERCENT(1.0f, 0.0f));
                                    }
                                }

                                AppRegisters* ViewRegs = AppPopRegisters();

                                if (PanelIsFocused)
                                    MemCpy(Registers(), ViewRegs, sizeof(AppRegisters));
                            }
                        }
                    }
                }

                if (BuildPanel) {
                    f32 SelectedTabLoadingT = SelectedTabViewState->LoadingT;

                    if (SelectedTabLoadingT > 0.01f) {
                        UIParent(LoadingOverlayContainer) {
                            UILoadingOverlay(
                                PanelContentRect,
                                SelectedTabLoadingT,
                                SelectedTabViewState->LoadingProgressV,
                                SelectedTabViewState->LoadingProgressVTarget
                            );
                        }
                    }
                }

                if (BuildPanel) {
                    UISignal PanelSig = UISignalFromBox(PanelBox);

                    if (UI_PRESSED(PanelSig)) {
                        AppCmd(
                            APP_COMMAND_KIND_FOCUS_PANEL, 
                            __Registers.Panel = Panel->Config->ID
                        );
                    }
                }

                ConfigPanelNode* ResizeMaxChild = EMPTY_CFG_PANEL_NODE_VALUE;

                for (
                    ConfigPanelNode* P = Panel;
                    Panel->TabSide == SIDE_MIN && P->Parent != EMPTY_CFG_PANEL_NODE_VALUE;
                    P = P->Parent
                ) {
                    if (
                        P->Parent->SplitAxis == AXIS_2D_Y &&
                        P->Prev != EMPTY_CFG_PANEL_NODE_VALUE
                    ) {
                        ResizeMaxChild = P;
                        break;
                    }
                }

                UIBox* TabBarBox = EMPTY_UI_BOX_VALUE;

                if (BuildPanel) {
                    UITag("tab"_s8) {
                        UITag("inactive"_s8) {
                            UIRect(TabBarRect) {

                                if (ResizeMaxChild != EMPTY_CFG_PANEL_NODE_VALUE)
                                    UISetNextHoverCursor(CURSOR_KIND_UP_DOWN);

                                UISetNextChildLayoutAxis(AXIS_2D_X);
                                TabBarBox = UIBuildBoxFromStrFmt(
                                    (
                                        UI_BOX_KIND_CLIP |
                                        UI_BOX_KIND_ALLOW_OVERFLOW_X |
                                        UI_BOX_KIND_VIEW_CLAMP_X |
                                        UI_BOX_KIND_VIEW_SCROLL_X |
                                        UI_BOX_KIND_DRAW_BACKGROUND |
                                        UI_BOX_KIND_CLICKABLE
                                    ),
                                    "tab_bar_%p",
                                    Panel->Config
                                );
                            }
                        }
                    }
                }

                ConfigNode* TabDropPrev = EMPTY_CFG_NODE_VALUE;
                b32 TabDropIsActive = FALSE;

                if (
                    BuildPanel &&
                    AppDragIsActive() &&
                    APP_STATE->DragDropRegistersSlot == APP_REGISTERS_SLOT_VIEW
                ) {
                    ConfigNode* DragTab = ConfigNodeFromID(
                        APP_STATE->DragDropRegisters->View
                    );
                    UIBox* TabDropSiteBox = EMPTY_UI_BOX_VALUE;

                    UIRect(TabBarRect) {
                        TabDropSiteBox = UIBuildBoxFromStrFmt(
                            UI_BOX_KIND_DROP_SITE,
                            "###tab_drop_site_%p",
                            Panel->Config
                        );
                    }

                    UISignalFromBox(TabDropSiteBox);
                    TabDropIsActive = (TabDropSiteBox->Key == UIDropHotKey());

                    for (
                        ConfigNodePtrNode* N = Panel->Tabs.Head;
                        N;
                        N = N->Next
                    ) {
                        UIBox* LastTabBox = UIBoxFromKey(
                            UIKeyFromStrFmt(TabBarBox->Key, "tab_%p", N->V)
                        );

                        if (
                            !IsUIBoxEmpty(LastTabBox) &&
                            Centre(LastTabBox->Rect).X < UIMouse().X
                        ) {
                            TabDropPrev = N->V;
                        }
                    }

                    for (
                        ConfigNodePtrNode* N = Panel->Tabs.Head;
                        N;
                        N = N->Next
                    ) {
                        if (
                            N->V == DragTab &&
                            (
                                TabDropPrev == DragTab ||
                                TabDropPrev == (
                                    N->Prev 
                                        ? N->Prev->V 
                                        : EMPTY_CFG_NODE_VALUE
                                )
                            )
                        ) {
                            TabDropIsActive = FALSE;
                        }
                    }
                }

                if (BuildPanel) {
                    UIFocus(UI_FOCUS_KIND_OFF) {
                        UIParent(TabBarBox) {
                            UITag("tab"_s8) {
                                if (
                                    TabDropIsActive &&
                                    TabDropPrev == EMPTY_CFG_NODE_VALUE
                                ) {
                                    UISetNextPreferredWidth(UI_PX(1.0f, 1.0f));
                                    UISetNextBackgroundColour(UIColourFromName("hover"_s8));
                                    UIBuildBoxFromKey(
                                        UI_BOX_KIND_DRAW_BACKGROUND,
                                        EMPTY_UI_KEY_VALUE
                                    );
                                }

                                for (
                                    ConfigNodePtrNode* N = Panel->Tabs.Head;
                                    N;
                                    N = N->Next
                                ) {
                                    ConfigNode* Tab = N->V;

                                    AppRegistersScope(
                                        __Registers.Panel = Panel->Config->ID,
                                        __Registers.View = Tab->ID,
                                        __Registers.Tab = Tab->ID
                                    ) {
                                        b32 TabIsSelected = (Tab == Panel->SelectedTab);

                                        UITag(
                                            (!TabIsSelected) 
                                                ? "inactive"_s8 
                                                : (PanelTree.Focused != Panel)
                                                    ? "unfocused"_s8
                                                    : ""_s8
                                        ) {
                                            FancyStrList String = AppTitleFStrFromConfig(
                                                Scratch.MemPool, 
                                                Tab,
                                                FALSE
                                            );
                                            f32 TabWidth = DDimensionsFromFancyStrList(
                                                UIHeadTabSize(), 
                                                &String
                                            ).X + 2.0f;

                                            UISetNextChildLayoutAxis(AXIS_2D_X);
                                            UISetNextPreferredWidth(UI_SUM_OF_CHILDREN(1.0f));

                                            UIBox* TabBox = UIBuildBoxFromStrFmt(
                                                (
                                                    UI_BOX_KIND_DRAW_HOT_EFFECTS |
                                                    UI_BOX_KIND_DRAW_BACKGROUND |
                                                    UI_BOX_KIND_CLICKABLE
                                                ),
                                                "tab_%p",
                                                Tab
                                            );

                                            UIParent(TabBox) {
                                                UIPreferredWidth(UI_PX(TabWidth, 1.0f)) {
                                                    UIBox* NameBox = UIBuildBoxFromKey(
                                                        UI_BOX_KIND_DRAW_TEXT, 
                                                        EMPTY_UI_KEY_VALUE
                                                    );

                                                    UIBoxEquipDisplayFancyStrs(NameBox, &String);
                                                }

                                                if (TabIsSelected) {
                                                    UIPreferredWidth(UI_PX(3.0f, 1.0f)) {
                                                        UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                                                            UIBox* CloseBox = UIBuildBoxFromStrFmt(
                                                                (
                                                                    UI_BOX_KIND_CLICKABLE |
                                                                    UI_BOX_KIND_DRAW_TEXT |
                                                                    UI_BOX_KIND_DRAW_HOT_EFFECTS |
                                                                    UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
                                                                ),
                                                                "%S###close_view_%p",
                                                                PRINT_STR(APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_X]),
                                                                Tab
                                                            );
                                                            UISignal Sig = UISignalFromBox(CloseBox);

                                                            if (UI_CLICKED(Sig) || UI_MIDDLE_CLICKED(Sig))
                                                                AppCmd(APP_COMMAND_KIND_CLOSE_TAB);
                                                        }
                                                    }
                                                }
                                            }

                                            {
                                                UISignal Sig = UISignalFromBox(TabBox);

                                                if (UI_PRESSED(Sig)) {
                                                    AppCmd(APP_COMMAND_KIND_FOCUS_TAB);
                                                    AppCmd(APP_COMMAND_KIND_FOCUS_PANEL);
                                                } else if (
                                                    UI_DRAGGING(Sig) &&
                                                    !AppDragIsActive() &&
                                                    Length(UIDragDelta()) > 1.0f
                                                ) {
                                                    AppDragBegin(APP_REGISTERS_SLOT_VIEW);
                                                } else if (UI_RIGHT_CLICKED(Sig)) {
                                                    Str8 TabFilePath = AppViewFilePath();

                                                    APP_STATE->TabCtxMenuTab = Tab->ID;
                                                    APP_STATE->TabCtxMenuFileSize = MIN(
                                                        sizeof(APP_STATE->TabCtxMenuFileBuffer),
                                                        TabFilePath.Size
                                                    );
                                                    MemCpy(
                                                        APP_STATE->TabCtxMenuFileBuffer,
                                                        TabFilePath.Str,
                                                        APP_STATE->TabCtxMenuFileSize
                                                    );
                                                    APP_STATE->TabCtxMenuCursor = TxtPt(
                                                        1,
                                                        1 + APP_STATE->TabCtxMenuFileSize
                                                    );
                                                    APP_STATE->TabCtxMenuMark = APP_STATE->TabCtxMenuCursor;
                                                    UIContextMenuOpen(
                                                        TabCtxMenuKey,
                                                        TabBox->Key,
                                                        Vec(0.0f, 1.0f)
                                                    );
                                                } else if (UI_MIDDLE_CLICKED(Sig)) {
                                                    AppCmd(APP_COMMAND_KIND_CLOSE_TAB);
                                                }
                                            }

                                            if (TabDropIsActive && Tab == TabDropPrev) {
                                                UISetNextPreferredWidth(UI_PX(1.0f, 1.0f));
                                                UISetNextBackgroundColour(UIColourFromName("hover"_s8));
                                                UIBuildBoxFromKey(
                                                    UI_BOX_KIND_DRAW_BACKGROUND,
                                                    EMPTY_UI_KEY_VALUE
                                                );
                                            }
                                        }
                                    }
                                }

                                UITag("inactive"_s8) {
                                    UIPreferredWidth(UI_PX(3.0f, 1.0f)) {
                                        UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                                            UIBox* AddNewBox = UIBuildBoxFromStrFmt(
                                                (
                                                    UI_BOX_KIND_DRAW_TEXT |
                                                    UI_BOX_KIND_DRAW_HOT_EFFECTS |
                                                    UI_BOX_KIND_DRAW_ACTIVE_EFFECTS |
                                                    UI_BOX_KIND_CLICKABLE |
                                                    UI_BOX_KIND_DISABLE_TEXT_TRUNC
                                                ),
                                                "%S###add_new_tab_button_%p",
                                                PRINT_STR(APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_ADD]),
                                                Panel->Config
                                            );
                                            UISignal Sig = UISignalFromBox(AddNewBox);

                                            if (UI_PRESSED(Sig)) {
                                                AppCmd(APP_COMMAND_KIND_FOCUS_PANEL, __Registers.Panel = Panel->Config->ID);

                                                if (APP_STATE->QueryIsActive) {
                                                    AppCmd(APP_COMMAND_KIND_CANCEL_QUERY);
                                                } else {
                                                    AppCmd(
                                                        APP_COMMAND_KIND_RUN_COMMAND,
                                                        __Registers.Panel = Panel->Config->ID,
                                                        __Registers.CommandName = APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN_TAB].String
                                                    );
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    UISignal TabBarSig = UISignalFromBox(TabBarBox);

                    if (ResizeMaxChild != EMPTY_CFG_PANEL_NODE_VALUE) {
                        ConfigPanelNode* ResizeMinChild = ResizeMaxChild->Prev;
                        f32 TotalSize = Length(
                            ConfigTargetRectFromPanelNode(
                                ContentRect,
                                PanelTree.Root,
                                ResizeMaxChild->Parent
                            )
                        ).Y;

                        if (UI_DOUBLE_CLICKED(TabBarSig)) {
                            f32 HalfPercent = 0.5f * (ResizeMinChild->PercentOfParent + ResizeMaxChild->PercentOfParent);

                            UIKillAction();
                            ConfigNodeEquipStr(
                                APP_STATE->Config,
                                ResizeMinChild->Config,
                                "%f",
                                HalfPercent
                            );
                            ConfigNodeEquipStr(
                                APP_STATE->Config,
                                ResizeMaxChild->Config,
                                "%f",
                                HalfPercent
                            );
                        } else if (UI_PRESSED(TabBarSig)) {
                            v2f32 Percents = Vec(
                                ResizeMinChild->PercentOfParent,
                                ResizeMaxChild->PercentOfParent
                            );

                            UIStoreDragStruct(&Percents);
                        } else if (UI_DRAGGING(TabBarSig)) {
                            v2f32 Percents = *UIGetDragStruct(v2f32);
                            f32 BothSize = (Percents.X + Percents.Y) * TotalSize;

                            if (BothSize >= 6.0f) {
                                f32 MinSize = CLAMP(
                                    3.0f,
                                    Percents.X * TotalSize + UIDragDelta().Y,
                                    BothSize - 3.0f
                                );

                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    ResizeMinChild->Config,
                                    "%f",
                                    MinSize / TotalSize
                                );
                                ConfigNodeEquipStr(
                                    APP_STATE->Config,
                                    ResizeMaxChild->Config,
                                    "%f",
                                    (BothSize - MinSize) / TotalSize
                                );
                            }
                        }
                    }

                    if (TabDropIsActive && AppDragDrop()) {
                        AppCmd(
                            APP_COMMAND_KIND_MOVE_VIEW,
                            __Registers.DstPanel = Panel->Config->ID,
                            __Registers.Panel = APP_STATE->DragDropRegisters->Panel,
                            __Registers.Tab = APP_STATE->DragDropRegisters->View,
                            __Registers.View = APP_STATE->DragDropRegisters->View,
                            __Registers.PrevTab = TabDropPrev->ID
                        );
                    }
                }

                {
                    for (UIEvent* Evt = NULL; UINextEvent(&Evt); ) {
                        if (
                            Evt->Kind == UI_EVENT_KIND_FILE_DROP && 
                            InRange(PanelRect, Evt->Position)
                        ) {
                            for (
                                Str8Node* N = Evt->Paths.Head; 
                                N; 
                                N = N->Next
                            ) {
                                AppCmd(
                                    APP_COMMAND_KIND_OPEN,
                                    __Registers.FilePath = N->String,
                                    __Registers.Panel = Panel->Config->ID
                                );
                            }

                            UIEatEvent(Evt);
                        }
                    }
                }
            }
        }
    }

    UIPopPreferredHeight();
    UIPopPreferredWidth();
    UIPopTextPadding();
    UIEndBuild();

    if (
        APP_STATE->BindChangeActive &&
        (
            IsUIBoxEmpty(UIBoxFromKey(APP_STATE->BindChangeUIKey)) ||
            (
                UIActiveKey(UI_MOUSE_BTN_KIND_LEFT) != EMPTY_UI_KEY_VALUE &&
                UIActiveKey(UI_MOUSE_BTN_KIND_LEFT) != APP_STATE->BindChangeUIKey
            )
        )
    ) {
        APP_STATE->BindChangeActive = FALSE;
    }

    if (UIIsAnimatingFromState(APP_STATE->UI))
        AppRequestFrame();

    DBeginFrame(
        AppFrameMemPool(),
        UIColourFromName("text"_s8),
        UIColourFromName("background"_s8)
    );
    UIDrawRoot(UIRootFromState(APP_STATE->UI));
    DEndFrame();
    ++APP_STATE->FramesAlive;
    ReleaseScratch(Scratch);
}
