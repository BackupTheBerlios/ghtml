#
# emacs like keybindings for HTML Editor
#

binding "html-keys"
{
  bind "Home"               { "cursor_move" (up,   all) }
  bind "End"                { "cursor_move" (down, all) }
  bind "<Alt>less"          { "cursor_move" (up,   all) }
  bind "<Alt>greater"       { "cursor_move" (down, all) }
  bind "<Ctrl>b"            { "cursor_move" (left,  one) }
  bind "<Ctrl>f"            { "cursor_move" (right, one) }
  bind "<Ctrl>Left"         { "cursor_move" (left,  word) }
  bind "<Ctrl>Right"        { "cursor_move" (right, word) }
  bind "<Alt>b"             { "cursor_move" (left,  word) }
  bind "<Alt>f"             { "cursor_move" (right, word) }
  bind "<Ctrl>p"            { "cursor_move" (up,    one) }
  bind "<Ctrl>n"            { "cursor_move" (down,  one) }

  bind "<Alt>v"             { "cursor_move" (up,    page) }
  bind "<Ctrl>v"            { "cursor_move" (down,  page) }

  bind "<Ctrl>d"            { "command" (delete) }
  bind "<Ctrl>g"            { "command" (disable-selection) }
  bind "<Ctrl>m"            { "command" (insert-paragraph) }
  bind "<Ctrl>j"            { "command" (insert-paragraph) }
  bind "<Ctrl>w"            { "command" (cut) }
  bind "<Alt>w"             { "command" (copy) }
  bind "<Ctrl>y"            { "command" (paste) }
  bind "<Ctrl>space"        { "command" (set-mark) }
}

class "GtkHTML" binding "html-keys"
