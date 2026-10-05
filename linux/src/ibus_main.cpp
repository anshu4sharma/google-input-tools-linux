#include <gio/gio.h>
#include <glib-object.h>
#include "engine.hpp"

// Standard Keysym Constants (X11 / IBus standard)
#define IBUS_KEY_BackSpace 0xff08
#define IBUS_KEY_Tab       0xff09
#define IBUS_KEY_Return    0xff0d
#define IBUS_KEY_Escape    0xff1b
#define IBUS_KEY_space     0x0020
#define IBUS_KEY_Page_Up   0xff55
#define IBUS_KEY_Page_Down 0xff56
#define IBUS_KEY_Up        0xff52
#define IBUS_KEY_Down      0xff54
#define IBUS_KEY_KP_Enter  0xff8d
#define IBUS_KEY_KP_1      0xffb1
#define IBUS_KEY_KP_5      0xffb5
#define IBUS_KEY_1         0x0031
#define IBUS_KEY_5         0x0035

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <cctype>

extern "C" {
    void ibus_init(void);
    void ibus_main(void);
    void ibus_quit(void);
    GType ibus_engine_get_type(void);
    gpointer ibus_bus_new(void);
    gboolean ibus_bus_is_connected(gpointer bus);
    GDBusConnection* ibus_bus_get_connection(gpointer bus);
    gpointer ibus_factory_new(GDBusConnection* connection);
    void ibus_factory_add_engine(gpointer factory, const gchar* engine_name, GType engine_type);
    guint ibus_bus_request_name(gpointer bus, const gchar* name, guint flags);

    gpointer ibus_text_new_from_string(const gchar* str);
    void ibus_text_set_attributes(gpointer text, gpointer attrs);
    gpointer ibus_attr_list_new(void);
    gpointer ibus_attr_underline_new(guint underline_type, guint start_index, guint end_index);
    void ibus_attr_list_append(gpointer attr_list, gpointer attr);

    gpointer ibus_lookup_table_new(guint page_size, guint cursor_pos, gboolean cursor_visible, gboolean round);
    void ibus_lookup_table_append_candidate(gpointer table, gpointer text);
    void ibus_lookup_table_append_label(gpointer table, gpointer text);
    void ibus_lookup_table_set_cursor_pos(gpointer table, guint cursor_pos);
    void ibus_lookup_table_set_orientation(gpointer table, gint orientation);
    void ibus_engine_commit_text(gpointer engine, gpointer text);

    void ibus_engine_delete_surrounding_text(gpointer engine, gint offset, guint nchars);
    void ibus_engine_update_preedit_text(gpointer engine, gpointer text, guint cursor_pos, gboolean visible);
    void ibus_engine_hide_preedit_text(gpointer engine);
    void ibus_engine_update_lookup_table(gpointer engine, gpointer table, gboolean visible);
    void ibus_engine_show_lookup_table(gpointer engine);
    void ibus_engine_hide_lookup_table(gpointer engine);
}

using namespace google_input_tools;

struct EngineContext {
    std::string preedit;
    std::vector<std::string> candidates;
    size_t cursor_pos{0};
    std::unique_ptr<TransliterationEngine> engine;
    std::string last_committed_roman;
    std::string last_committed_hindi;
    bool just_committed{false};
    bool committed_with_space{false};
};

// Global map from IBusEngine instance pointer to EngineContext
static GHashTable* g_contexts = nullptr;

static EngineContext* get_context(gpointer engine_instance) {
    if (!engine_instance) return nullptr;
    if (!g_contexts) {
        g_contexts = g_hash_table_new_full(g_direct_hash, g_direct_equal, nullptr, +[](gpointer d) {
            delete static_cast<EngineContext*>(d);
        });
    }
    EngineContext* ctx = static_cast<EngineContext*>(g_hash_table_lookup(g_contexts, engine_instance));
    if (!ctx) {
        ctx = new EngineContext();
        ctx->engine = std::make_unique<TransliterationEngine>("hi");
        ctx->cursor_pos = 0;
        g_hash_table_insert(g_contexts, engine_instance, ctx);
    }
    return ctx;
}

static void reset_engine_state(gpointer engine, EngineContext* ctx) {
    if (!ctx) return;
    ctx->preedit.clear();
    ctx->candidates.clear();
    ctx->cursor_pos = 0;
    if (engine) {
        ibus_engine_hide_preedit_text(engine);
        ibus_engine_hide_lookup_table(engine);
    }
}

static void commit_candidate(gpointer engine, EngineContext* ctx, const std::string& text, const char* suffix = "") {
    if (!engine || !ctx) return;
    std::string full = text + (suffix ? suffix : "");
    gpointer ibus_txt = ibus_text_new_from_string(full.c_str());
    ibus_engine_commit_text(engine, ibus_txt);
    reset_engine_state(engine, ctx);
}

static void commit_top(gpointer engine, EngineContext* ctx, const char* suffix = "") {
    if (!engine || !ctx || ctx->preedit.empty()) return;
    if (ctx->cursor_pos < ctx->candidates.size()) {
        commit_candidate(engine, ctx, ctx->candidates[ctx->cursor_pos], suffix);
    } else if (!ctx->candidates.empty()) {
        commit_candidate(engine, ctx, ctx->candidates[0], suffix);
    } else {
        commit_candidate(engine, ctx, ctx->preedit, suffix);
    }
}

static void update_lookup_table_view(gpointer engine, EngineContext* ctx) {
    if (!engine || !ctx) return;
    if (ctx->candidates.empty()) {
        ibus_engine_hide_lookup_table(engine);
        return;
    }

    // Create fresh lookup table for this D-Bus update
    // Floating reference is consumed and freed by ibus_engine_update_lookup_table
    gpointer table = ibus_lookup_table_new(5, static_cast<guint>(ctx->cursor_pos), TRUE, TRUE);
    ibus_lookup_table_set_orientation(table, 1); // 1 = Vertical

    for (size_t i = 0; i < ctx->candidates.size(); ++i) {
        std::string lbl = std::to_string(i + 1) + ". ";
        gpointer lbl_txt = ibus_text_new_from_string(lbl.c_str());
        ibus_lookup_table_append_label(table, lbl_txt);

        gpointer cand_txt = ibus_text_new_from_string(ctx->candidates[i].c_str());
        ibus_lookup_table_append_candidate(table, cand_txt);
    }

    ibus_lookup_table_set_cursor_pos(table, static_cast<guint>(ctx->cursor_pos));
    ibus_engine_update_lookup_table(engine, table, TRUE);
    ibus_engine_show_lookup_table(engine);
}

static void update_candidates_instant(gpointer engine, EngineContext* ctx) {
    if (!engine || !ctx) return;
    if (ctx->preedit.empty()) {
        reset_engine_state(engine, ctx);
        return;
    }

    // Microsecond C++ candidate lookup (54 nanoseconds)
    ctx->candidates = ctx->engine->get_candidates(ctx->preedit, 5);
    ctx->cursor_pos = 0;

    if (!ctx->candidates.empty()) {
        const std::string& top = ctx->candidates[0];

        // Preedit with underline
        gpointer ibus_txt = ibus_text_new_from_string(top.c_str());
        gpointer attr_list = ibus_attr_list_new();
        gpointer underline = ibus_attr_underline_new(1, 0, g_utf8_strlen(top.c_str(), -1));
        ibus_attr_list_append(attr_list, underline);
        ibus_text_set_attributes(ibus_txt, attr_list);
        ibus_engine_update_preedit_text(engine, ibus_txt, g_utf8_strlen(top.c_str(), -1), TRUE);

        // Update lookup table
        update_lookup_table_view(engine, ctx);
    } else {
        ibus_engine_hide_preedit_text(engine);
        ibus_engine_hide_lookup_table(engine);
    }
}

static gboolean on_process_key_event(gpointer engine, guint keyval, guint keycode, guint state) {
    (void)keycode;

    // Ignore key release events
    if (state & (1 << 30)) { // 1 << 30 = Release mask
        return FALSE;
    }

    EngineContext* ctx = get_context(engine);
    if (!ctx) return FALSE;

    // 1. Ctrl+Backspace -> Remove entire preedit word instantly!
    if ((state & (1 << 2)) && keyval == IBUS_KEY_BackSpace) { // (1 << 2) = Control mask
        if (!ctx->preedit.empty()) {
            reset_engine_state(engine, ctx);
            return TRUE;
        }
        return FALSE;
    }

    // 2. Modifiers: Ctrl or Alt held with other keys -> commit and let shortcut pass
    if (state & ((1 << 2) | (1 << 3))) {
        if (!ctx->preedit.empty()) {
            commit_top(engine, ctx);
        }
        return FALSE;
    }

    // 3. Backspace -> Instant character deletion
    if (keyval == IBUS_KEY_BackSpace) {
        if (!ctx->preedit.empty()) {
            ctx->preedit.pop_back();
            if (!ctx->preedit.empty()) {
                update_candidates_instant(engine, ctx);
            } else {
                reset_engine_state(engine, ctx);
            }
            return TRUE;
        }
        return FALSE;
    }

    // 4. Escape -> Cancel preedit
    if (keyval == IBUS_KEY_Escape) {
        if (!ctx->preedit.empty()) {
            reset_engine_state(engine, ctx);
            return TRUE;
        }
        return FALSE;
    }

    // 5. Space / Return / Enter -> Commit
    if (keyval == IBUS_KEY_space || keyval == IBUS_KEY_Return || keyval == IBUS_KEY_KP_Enter) {
        if (!ctx->preedit.empty()) {
            const char* suffix = (keyval == IBUS_KEY_space) ? " " : "";
            commit_top(engine, ctx, suffix);
            return TRUE;
        }
        return FALSE;
    }

    // 6. Number selection (1-5)
    if (!ctx->preedit.empty() && keyval >= IBUS_KEY_1 && keyval <= IBUS_KEY_5) {
        size_t idx = keyval - IBUS_KEY_1;
        if (idx < ctx->candidates.size()) {
            commit_candidate(engine, ctx, ctx->candidates[idx]);
            return TRUE;
        }
    }
    if (!ctx->preedit.empty() && keyval >= IBUS_KEY_KP_1 && keyval <= IBUS_KEY_KP_5) {
        size_t idx = keyval - IBUS_KEY_KP_1;
        if (idx < ctx->candidates.size()) {
            commit_candidate(engine, ctx, ctx->candidates[idx]);
            return TRUE;
        }
    }

    // 7. Arrows: Up / Down / PageUp / PageDown
    if (!ctx->candidates.empty()) {
        if (keyval == IBUS_KEY_Down || keyval == IBUS_KEY_Page_Down) {
            if (ctx->cursor_pos + 1 < ctx->candidates.size()) {
                ctx->cursor_pos++;
                update_lookup_table_view(engine, ctx);
            }
            return TRUE;
        } else if (keyval == IBUS_KEY_Up || keyval == IBUS_KEY_Page_Up) {
            if (ctx->cursor_pos > 0) {
                ctx->cursor_pos--;
                update_lookup_table_view(engine, ctx);
            }
            return TRUE;
        } else if (keyval == IBUS_KEY_Tab) {
            ctx->cursor_pos = (ctx->cursor_pos + 1) % ctx->candidates.size();
            update_lookup_table_view(engine, ctx);
            return TRUE;
        }
    }

    // 8. Printable ASCII keys
    if (keyval >= 32 && keyval < 127) {
        char ch = static_cast<char>(keyval);
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            ctx->preedit += ch;
            update_candidates_instant(engine, ctx);
            return TRUE;
        }

        // Punctuation / symbol: commit current candidate, then pass punctuation
        if (!ctx->preedit.empty()) {
            std::string punc(1, ch);
            commit_top(engine, ctx, punc.c_str());
            return TRUE;
        }
    }

    return FALSE;
}

static void on_candidate_clicked(gpointer engine, guint index, guint, guint) {
    EngineContext* ctx = get_context(engine);
    if (!ctx) return;
    if (index < ctx->candidates.size()) {
        commit_candidate(engine, ctx, ctx->candidates[index]);
    }
}

static void on_engine_reset(gpointer engine, gpointer) {
    EngineContext* ctx = get_context(engine);
    reset_engine_state(engine, ctx);
}

static void on_engine_enable(gpointer engine, gpointer) {
    EngineContext* ctx = get_context(engine);
    reset_engine_state(engine, ctx);
}

static void on_engine_disable(gpointer engine, gpointer) {
    EngineContext* ctx = get_context(engine);
    reset_engine_state(engine, ctx);
}

static void on_engine_focus_in(gpointer engine, gpointer) {
    EngineContext* ctx = get_context(engine);
    reset_engine_state(engine, ctx);
}

static void on_engine_focus_out(gpointer engine, gpointer) {
    EngineContext* ctx = get_context(engine);
    if (ctx && !ctx->preedit.empty()) {
        commit_top(engine, ctx);
    }
}

// Custom GType for Native C++ IBus Engine
static void native_engine_init(GTypeInstance* instance, gpointer) {
    g_signal_connect(instance, "process-key-event", G_CALLBACK(on_process_key_event), nullptr);
    g_signal_connect(instance, "candidate-clicked", G_CALLBACK(on_candidate_clicked), nullptr);
    g_signal_connect(instance, "reset", G_CALLBACK(on_engine_reset), nullptr);
    g_signal_connect(instance, "enable", G_CALLBACK(on_engine_enable), nullptr);
    g_signal_connect(instance, "disable", G_CALLBACK(on_engine_disable), nullptr);
    g_signal_connect(instance, "focus-in", G_CALLBACK(on_engine_focus_in), nullptr);
    g_signal_connect(instance, "focus-out", G_CALLBACK(on_engine_focus_out), nullptr);

    // Auto cleanup when the engine instance is finalized
    g_object_weak_ref(G_OBJECT(instance), +[](gpointer, GObject* obj) {
        if (g_contexts) {
            g_hash_table_remove(g_contexts, obj);
        }
    }, nullptr);
}

static void native_engine_class_init(gpointer, gpointer) {}

static GType get_native_engine_gtype() {
    static GType type = 0;
    if (!type) {
        GTypeQuery q;
        g_type_query(ibus_engine_get_type(), &q);
        GTypeInfo info = {
            (guint16)q.class_size,
            nullptr, nullptr,
            (GClassInitFunc)native_engine_class_init,
            nullptr, nullptr,
            (guint16)(q.instance_size + 64),
            0,
            (GInstanceInitFunc)native_engine_init,
            nullptr
        };
        type = g_type_register_static(ibus_engine_get_type(), "GoogleInputToolsNativeEngine", &info, (GTypeFlags)0);
    }
    return type;
}

int main(int argc, char* argv[]) {
    // Sanitize environment
    unsetenv("GTK_PATH");
    unsetenv("GTK_EXE_PREFIX");
    unsetenv("GTK_MODULES");
    unsetenv("GTK_IM_MODULE_FILE");
    unsetenv("LOCPATH");
    unsetenv("GSETTINGS_SCHEMA_DIR");

    ibus_init();
    gpointer bus = ibus_bus_new();

    if (!ibus_bus_is_connected(bus)) {
        std::cerr << "Warning: IBus daemon is not running or connection failed." << std::endl;
    }

    // Terminate cleanly when bus disconnects
    g_signal_connect(bus, "disconnected", G_CALLBACK(+[](gpointer, gpointer) {
        ibus_quit();
    }), nullptr);

    const char* component_name = "org.freedesktop.IBus.GoogleInputTools";
    const char* engine_name = "google-input-tools-hindi";

    GDBusConnection* conn = ibus_bus_get_connection(bus);
    gpointer factory = ibus_factory_new(conn);
    ibus_factory_add_engine(factory, engine_name, get_native_engine_gtype());

    bool is_ibus_daemon = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--ibus") == 0) {
            is_ibus_daemon = true;
            break;
        }
    }

    if (is_ibus_daemon) {
        ibus_bus_request_name(bus, component_name, 0);
    } else {
        std::cout << "100% Native C++ Google Input Tools IBus Engine started for " << engine_name << "..." << std::endl;
        std::cout << "Zero-latency typing active. Press Ctrl+C to stop." << std::endl;
    }

    ibus_main();

    return 0;
}
