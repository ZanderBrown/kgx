/* kgx-terminal.h
 *
 * Copyright 2019 Zander Brown
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

#include "kgx-enums.h"

G_BEGIN_DECLS

/**
 * KgxTheme:
 * @KGX_THEME_AUTO: Matches %KGX_THEME_DAY or %KGX_THEME_NIGHT depending on the
 *     system preferences
 * @KGX_THEME_NIGHT: The default, public, theme
 * @KGX_THEME_DAY: Alternate, light theme
 * @KGX_THEME_HACKER: Little easter egg theme
 *
 * Until [meson#1687](https://github.com/mesonbuild/meson/issues/1687) is
 * resolved this enum must be manually kept in sync with
 * the ‘Theme’ enum in the gschema
 */
typedef enum /*< enum,prefix=KGX >*/
{
  KGX_THEME_AUTO = 0,   /*< nick=auto >*/
  KGX_THEME_NIGHT = 1,  /*< nick=night >*/
  KGX_THEME_DAY = 2,    /*< nick=day >*/
  KGX_THEME_HACKER = 3, /*< nick=hacker >*/
} KgxTheme;

/**
 * KGX_TERMINAL_N_LINK_REGEX:
 * The number of regexs use to search for hyperlinks
 *
 * Stability: Private
 */
#define KGX_TERMINAL_N_LINK_REGEX 5


/**
 * KGX_FONT_SCALE_MIN:
 * The smallest font scale/zoom
 *
 * Stability: Private
 */
#define KGX_FONT_SCALE_MIN 0.5


/**
 * KGX_FONT_SCALE_MAX:
 * The largest font scale/zoom
 *
 * Stability: Private
 */
#define KGX_FONT_SCALE_MAX 4.0


/**
 * KGX_FONT_SCALE_DEFAULT:
 * The standard font scale/zoom
 *
 * Stability: Private
 */
#define KGX_FONT_SCALE_DEFAULT 1.0


#define KGX_TYPE_TERMINAL (kgx_terminal_get_type())

/**
 * KgxTerminal:
 * @theme: the palette to use, see #KgxTerminal:theme
 * @actions: action map for the context menu
 * @current_url: the address under the cursor
 * @match_id: regex ids for finding hyperlinks
 *
 * Stability: Private
 */
struct _KgxTerminal {
  /*< private >*/
  VteTerminal parent_instance;

  /*< public >*/
  KgxTheme    theme;
  GtkWidget  *popup_menu;

  /* Hyperlinks */
  char       *current_url;
  int         match_id[KGX_TERMINAL_N_LINK_REGEX];

  gboolean    popup_is_touch;
};

G_DECLARE_FINAL_TYPE (KgxTerminal, kgx_terminal, KGX, TERMINAL, VteTerminal)

void kgx_terminal_accept_paste (KgxTerminal *self,
                                const char  *text);

G_END_DECLS
