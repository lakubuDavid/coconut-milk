/// Linux (GTK3) clipboard implementation.
///
/// Uses gtk_clipboard_get() for primary and clipboard selections.
/// The clipboard selection (GDK_SELECTION_CLIPBOARD) is the standard
/// Ctrl+C / Ctrl+V clipboard.

#include "clipboard.h"

#include <gtk/gtk.h>

namespace coconut::clipboard {

std::string platformReadText() {
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!clip) return {};

  gchar* text = gtk_clipboard_wait_for_text(clip);
  if (!text) return {};

  std::string result(text);
  g_free(text);
  return result;
}

bool platformWriteText(const std::string& text) {
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!clip) return false;

  // gtk_clipboard_set_text() copies the string internally.
  gtk_clipboard_set_text(clip, text.c_str(), -1);

  // Flush so the content is available immediately (important for clipboard
  // managers and for correctness in testing).
  gtk_clipboard_store(clip);
  return true;
}

} // namespace coconut::clipboard
