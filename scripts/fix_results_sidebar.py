import sys

with open('src/window.c', 'r') as f:
    content = f.read()

# 1. Remove refresh_results_tags from refresh_results
old_refresh_results_end = """  }
  refresh_results_tags (state);
  refresh_stats (state);
}"""
new_refresh_results_end = """  }
  refresh_stats (state);
}"""
if old_refresh_results_end in content:
    content = content.replace(old_refresh_results_end, new_refresh_results_end)

# 2. Add refresh_results_tags to the end of refresh_tags
old_refresh_tags_end = """    if (state->tag_tree_store) {
        gtk_tree_store_clear(state->tag_tree_store);
        populate_tag_store(&root, state->tag_tree_store, NULL);
        gtk_tree_view_expand_all(GTK_TREE_VIEW(state->tag_tree_view));
    }

    g_list_free_full (root.children, (GDestroyNotify) tag_node_free);
    refresh_stats (state);
}"""

new_refresh_tags_end = """    if (state->tag_tree_store) {
        gtk_tree_store_clear(state->tag_tree_store);
        populate_tag_store(&root, state->tag_tree_store, NULL);
        gtk_tree_view_expand_all(GTK_TREE_VIEW(state->tag_tree_view));
    }

    g_list_free_full (root.children, (GDestroyNotify) tag_node_free);
    refresh_stats (state);
    refresh_results_tags (state);
}"""
if old_refresh_tags_end in content:
    content = content.replace(old_refresh_tags_end, new_refresh_tags_end)


# 3. Modify refresh_results_tags to block signal and restore selection
old_refresh_results_tags_start = """static void
refresh_results_tags (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->results_tag_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->results_tag_list), child);"""

new_refresh_results_tags_start = """static void
refresh_results_tags (CualiAppState *state)
{
  g_signal_handlers_block_by_func(state->results_tag_list, on_results_tag_selected, state);
  
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->results_tag_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->results_tag_list), child);"""
if old_refresh_results_tags_start in content:
    content = content.replace(old_refresh_results_tags_start, new_refresh_results_tags_start)


old_refresh_results_tags_end = """      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      g_object_set_data(G_OBJECT(row), "tag-id", GINT_TO_POINTER(tag_id));
      g_object_set_data_full (G_OBJECT (row), "tag_path", g_strdup (path), g_free);
      gtk_list_box_append (GTK_LIST_BOX (state->results_tag_list), row);
    }
    sqlite3_finalize (stmt);
  }
}"""

new_refresh_results_tags_end = """      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      g_object_set_data(G_OBJECT(row), "tag-id", GINT_TO_POINTER(tag_id));
      g_object_set_data_full (G_OBJECT (row), "tag_path", g_strdup (path), g_free);
      gtk_list_box_append (GTK_LIST_BOX (state->results_tag_list), row);
      
      if (state->selected_result_tag && g_strcmp0(path, state->selected_result_tag) == 0) {
          gtk_list_box_select_row(GTK_LIST_BOX(state->results_tag_list), GTK_LIST_BOX_ROW(row));
      }
    }
    sqlite3_finalize (stmt);
  }
  
  g_signal_handlers_unblock_by_func(state->results_tag_list, on_results_tag_selected, state);
}"""

if old_refresh_results_tags_end in content:
    content = content.replace(old_refresh_results_tags_end, new_refresh_results_tags_end)

with open('src/window.c', 'w') as f:
    f.write(content)

print("Fixed")
