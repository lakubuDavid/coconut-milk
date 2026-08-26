/// Linux (GTK3) native dialog implementations.
///
/// Uses GtkMessageDialog for message boxes and GtkFileChooserNative
/// for open/save file dialogs. Both work under X11 forwarding (Lima)
/// and headless (xvfb in CI).
///
/// Error boundaries: each entry point catches all exceptions, releases any
/// live GTK resources, and returns std::unexpected(Error) — exceptions
/// never escape toward the dispatcher or Lua bindings.

#include "dialog.h"
#include "../../debug.h"

#include <gtk/gtk.h>

#include <exception>
#include <format>
#include <string>
#include <vector>

namespace coconut::dialog {
  namespace {

    /// Build a DialogError with the given message.
    Error dialogError(std::string message, std::string details = {}) {
      return Error{
          .code    = ErrorCode::DialogError,
          .message = std::move(message),
          .details = std::move(details)
      };
    }

    // ── Helper: ensure GTK is initialised ────────────────────────────────────

    /// GTK must be initialized before any dialog calls, even in headless mode.
    /// On Linux, the App's main() / webview_create() will do this, but we
    /// guard here for test environments where no webview exists.
    ///
    /// \returns error if GTK could not be initialised (e.g. no display).
    std::optional<Error> ensureGtkInit() {
      static bool inited = false;
      if (!inited) {
        // gtk_init_check() is safe to call even if GTK is already initialized.
        if (!gtk_init_check(nullptr, nullptr)) {
          return dialogError(
              "GTK initialisation failed", "no display available and gtk_init_check returned false"
          );
        }
        inited = true;
      }
      return std::nullopt;
    }

    // ── Helper: run a GtkDialog synchronously in a nested main loop ──────────

    /// Run a GtkDialog modally and return the response ID.
    /// Uses gtk_dialog_run() which runs a recursive main loop.
    int runDialogSync(GtkDialog* dialog) {
      auto err = ensureGtkInit();
      if (err) {
        debug::error(std::format("dialog: {}", err->message));
        return GTK_RESPONSE_NONE;
      }
      return gtk_dialog_run(dialog);
    }

  }  // namespace

  // ── Message box (GtkMessageDialog) ───────────────────────────────────────

  std::expected<Result, Error> platformMessageBox(
      const std::string& title, const std::string& message, const std::string& kind
  ) {
    Result result{};

    if (auto err = ensureGtkInit()) {
      return std::unexpected(*err);
    }

    GtkMessageType msgType = GTK_MESSAGE_INFO;
    GtkButtonsType buttons = GTK_BUTTONS_OK;
    const char*    btn1    = nullptr;
    const char*    btn2    = nullptr;

    if (kind == "error") {
      msgType = GTK_MESSAGE_ERROR;
    } else if (kind == "warn") {
      msgType = GTK_MESSAGE_WARNING;
    } else if (kind == "question") {
      msgType = GTK_MESSAGE_QUESTION;
      buttons = GTK_BUTTONS_NONE;
      btn1    = "Yes";
      btn2    = "No";
    }

    GtkWidget* dialog = nullptr;
    try {
      dialog = gtk_message_dialog_new(
          nullptr,           // parent (none)
          GTK_DIALOG_MODAL,  // flags
          msgType,           // type
          buttons,           // buttons
          "%s",              // message format
          message.c_str()
      );
      if (!dialog) {
        return std::unexpected(dialogError("gtk_message_dialog_new returned null"));
      }
      gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());

      if (kind == "question") {
        gtk_dialog_add_button(GTK_DIALOG(dialog), btn1, GTK_RESPONSE_YES);
        gtk_dialog_add_button(GTK_DIALOG(dialog), btn2, GTK_RESPONSE_NO);
      }

      int response = runDialogSync(GTK_DIALOG(dialog));
      result.confirmed =
          (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_YES ||
           response == GTK_RESPONSE_ACCEPT);

      gtk_widget_destroy(dialog);
      dialog = nullptr;

      // Drain pending GTK events so the dialog is fully cleaned up.
      while (gtk_events_pending()) {
        gtk_main_iteration();
      }
    } catch (const std::exception& e) {
      if (dialog)
        gtk_widget_destroy(dialog);
      return std::unexpected(dialogError("messageBox failed", e.what()));
    } catch (...) {
      if (dialog)
        gtk_widget_destroy(dialog);
      return std::unexpected(dialogError("messageBox failed with unknown exception"));
    }

    debug::log(std::format("dialog::messageBox('{}') → confirmed={}", title, result.confirmed));
    return result;
  }

  // ── Open file / directory (GtkFileChooserNative) ─────────────────────────

  std::expected<Result, Error> platformOpenFile(
      const std::string& title, const std::vector<Filter>& filters, bool multi, bool chooseDir
  ) {
    Result result{};

    if (auto err = ensureGtkInit()) {
      return std::unexpected(*err);
    }

    GtkFileChooserAction action =
        chooseDir ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN;

    GtkFileChooserNative* native = nullptr;
    try {
      native = gtk_file_chooser_native_new(
          title.c_str(),
          nullptr,  // parent
          action,
          "_Open",  // accept label
          "_Cancel"
      );
      if (!native) {
        return std::unexpected(dialogError("gtk_file_chooser_native_new returned null"));
      }

      GtkFileChooser* chooser = GTK_FILE_CHOOSER(native);

      if (multi && !chooseDir) {
        gtk_file_chooser_set_select_multiple(chooser, TRUE);
      }

      // Add file filters (skip for directory pickers — confusing)
      if (!chooseDir && !filters.empty()) {
        for (const auto& filter : filters) {
          GtkFileFilter* ff = gtk_file_filter_new();
          gtk_file_filter_set_name(ff, filter.name.c_str());
          for (const auto& pattern : filter.patterns) {
            gtk_file_filter_add_pattern(ff, pattern.c_str());
          }
          gtk_file_chooser_add_filter(chooser, ff);
        }
      }

      int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));

      if (response == GTK_RESPONSE_ACCEPT) {
        // Collect selected paths
        GSList* files = gtk_file_chooser_get_filenames(chooser);
        for (GSList* iter = files; iter; iter = iter->next) {
          char* path = static_cast<char*>(iter->data);
          if (path) {
            result.paths.push_back(path);
            g_free(path);
          }
        }
        g_slist_free(files);

        if (!result.paths.empty()) {
          result.path = result.paths[0];

          // Check if path is a directory
          GFile* gf = g_file_new_for_path(result.path.c_str());
          result.is_dir =
              g_file_query_file_type(gf, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY;
          g_object_unref(gf);
        }
        result.confirmed = true;
      }

      g_object_unref(native);
      native = nullptr;
    } catch (const std::exception& e) {
      if (native)
        g_object_unref(native);
      return std::unexpected(dialogError("openFile failed", e.what()));
    } catch (...) {
      if (native)
        g_object_unref(native);
      return std::unexpected(dialogError("openFile failed with unknown exception"));
    }

    debug::log(
        std::format(
            "dialog::openFile('{}') → confirmed={}, paths={}, is_dir={}",
            title,
            result.confirmed,
            result.paths.size(),
            result.is_dir
        )
    );
    return result;
  }

  // ── Save file (GtkFileChooserNative) ─────────────────────────────────────

  std::expected<Result, Error> platformSaveFile(
      const std::string& title, const std::string& defaultName, const std::vector<Filter>& filters
  ) {
    Result result{};

    if (auto err = ensureGtkInit()) {
      return std::unexpected(*err);
    }

    GtkFileChooserNative* native = nullptr;
    try {
      native = gtk_file_chooser_native_new(
          title.c_str(), nullptr, GTK_FILE_CHOOSER_ACTION_SAVE, "_Save", "_Cancel"
      );
      if (!native) {
        return std::unexpected(dialogError("gtk_file_chooser_native_new returned null"));
      }

      GtkFileChooser* chooser = GTK_FILE_CHOOSER(native);

      // Set default file name
      if (!defaultName.empty()) {
        gtk_file_chooser_set_current_name(chooser, defaultName.c_str());
      }

      // Add file filters
      if (!filters.empty()) {
        for (const auto& filter : filters) {
          GtkFileFilter* ff = gtk_file_filter_new();
          gtk_file_filter_set_name(ff, filter.name.c_str());
          for (const auto& pattern : filter.patterns) {
            gtk_file_filter_add_pattern(ff, pattern.c_str());
          }
          gtk_file_chooser_add_filter(chooser, ff);
        }

        // Set first filter as default
        GList* filterList = gtk_file_chooser_list_filters(chooser);
        if (filterList && filterList->data) {
          gtk_file_chooser_set_filter(chooser, static_cast<GtkFileFilter*>(filterList->data));
        }
        g_list_free(filterList);
      }

      // Enable confirm overwrite (native dialog handles this)
      gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

      int response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));

      if (response == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(chooser);
        if (path) {
          result.path = path;
          g_free(path);
        }
        result.confirmed = true;
      }

      g_object_unref(native);
      native = nullptr;
    } catch (const std::exception& e) {
      if (native)
        g_object_unref(native);
      return std::unexpected(dialogError("saveFile failed", e.what()));
    } catch (...) {
      if (native)
        g_object_unref(native);
      return std::unexpected(dialogError("saveFile failed with unknown exception"));
    }

    debug::log(
        std::format(
            "dialog::saveFile('{}') → confirmed={}, path='{}'", title, result.confirmed, result.path
        )
    );
    return result;
  }

}  // namespace coconut::dialog
