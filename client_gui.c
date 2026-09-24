#include <sys/time.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12345
#define GRID_SIZE 9

static int sock = -1;
static int grid[GRID_SIZE][GRID_SIZE];
static int fixed_cells[GRID_SIZE][GRID_SIZE];
static int selected_row = -1, selected_col = -1;

static GtkWidget *window;
static GtkWidget *cells[GRID_SIZE][GRID_SIZE];
static GtkWidget *status_label;
static GtkWidget *message_label;
static GtkWidget *connect_entry;
static GtkWidget *hidden_entry;

/* ─────────────────────────────────────────
   Réseau
───────────────────────────────────────── */
static void send_command(const char *cmd) {
    if (sock < 0) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    send(sock, buf, strlen(buf), 0);
}

static int recv_response(char *buf, int size) {
    if (sock < 0) return -1;
    memset(buf, 0, size);
    int total = 0;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int n;
    while (total < size - 1) {
        n = recv(sock, buf + total, size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = 0;
    return total;
}

/* ─────────────────────────────────────────
   Affichage grille
───────────────────────────────────────── */
static void update_cell_display(int r, int c) {
    GtkWidget *btn = cells[r][c];
    char label[4] = "";
    if (grid[r][c] != 0)
        snprintf(label, sizeof(label), "%d", grid[r][c]);
    gtk_button_set_label(GTK_BUTTON(btn), label);

    GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
    gtk_style_context_remove_class(ctx, "cell-fixed");
    gtk_style_context_remove_class(ctx, "cell-selected");
    gtk_style_context_remove_class(ctx, "cell-empty");

    if (r == selected_row && c == selected_col)
        gtk_style_context_add_class(ctx, "cell-selected");
    else if (fixed_cells[r][c])
        gtk_style_context_add_class(ctx, "cell-fixed");
    else
        gtk_style_context_add_class(ctx, "cell-empty");
}

static void refresh_grid(void) {
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            update_cell_display(r, c);
}

static void parse_grid(const char *data) {
    const char *p = data;
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') return;
            grid[r][c] = (*p - '0');
            fixed_cells[r][c] = (grid[r][c] != 0);
            p++;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    refresh_grid();
}

/* ─────────────────────────────────────────
   Helpers couleur status
───────────────────────────────────────── */
static void set_status_connected(void) {
    gtk_label_set_markup(GTK_LABEL(status_label),
        "<span color=\"#00aa00\"><b>● Connecté</b></span>");
}

static void set_status_disconnected(void) {
    gtk_label_set_markup(GTK_LABEL(status_label),
        "<span color=\"#dd0000\"><b>● Déconnecté</b></span>");
}

/* ─────────────────────────────────────────
   Callbacks
───────────────────────────────────────── */
static void on_hidden_entry_changed(GtkEditable *editable,
                                     gpointer data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(hidden_entry));
    if (strlen(text) == 0) return;
    char ch = text[0];
    gtk_entry_set_text(GTK_ENTRY(hidden_entry), "");

    if (selected_row < 0 || selected_col < 0) {
        gtk_label_set_text(GTK_LABEL(message_label),
            "Cliquez d'abord sur une case !");
        return;
    }
    if (ch < '1' || ch > '9') return;
    if (fixed_cells[selected_row][selected_col]) {
        gtk_label_set_text(GTK_LABEL(message_label),
            "Case fixe — non modifiable.");
        return;
    }

    int val = ch - '0';
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "PLAY %d %d %d",
             selected_row + 1, selected_col + 1, val);
    send_command(cmd);

    char response[4096] = {0};
    recv_response(response, sizeof(response));
    response[strcspn(response, "\r\n")] = 0;

    if (strncmp(response, "OK", 2) == 0 ||
        strncmp(response, "BRAVO", 5) == 0) {
        grid[selected_row][selected_col] = val;
    }
    gtk_label_set_text(GTK_LABEL(message_label), response);
    refresh_grid();
    gtk_widget_grab_focus(hidden_entry);
}

static void on_cell_clicked(GtkButton *btn, gpointer data) {
    int idx = GPOINTER_TO_INT(data);
    selected_row = idx / GRID_SIZE;
    selected_col = idx % GRID_SIZE;
    refresh_grid();
    char msg[64];
    snprintf(msg, sizeof(msg),
        "Case [%d,%d] selectionnee. Tapez 1-9.",
        selected_row + 1, selected_col + 1);
    gtk_label_set_text(GTK_LABEL(message_label), msg);
    gtk_widget_grab_focus(hidden_entry);
}

static void on_new_game(GtkButton *btn, gpointer level) {
    if (sock < 0) {
        gtk_label_set_text(GTK_LABEL(message_label),
            "Non connecte au serveur !");
        return;
    }
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "NEW %d", GPOINTER_TO_INT(level));
    send_command(cmd);

    char response[4096] = {0};
    recv_response(response, sizeof(response));
    char *grid_start = strstr(response, "\n");
    if (grid_start) parse_grid(grid_start + 1);

    selected_row = selected_col = -1;
    gtk_label_set_text(GTK_LABEL(message_label),
        "Nouvelle grille ! Cliquez une case puis tapez un chiffre.");
    gtk_widget_grab_focus(hidden_entry);
}

static void on_check(GtkButton *btn, gpointer data) {
    if (sock < 0) return;
    send_command("CHECK");
    char response[512] = {0};
    recv_response(response, sizeof(response));
    response[strcspn(response, "\r\n")] = 0;
    gtk_label_set_text(GTK_LABEL(message_label), response);
    gtk_widget_grab_focus(hidden_entry);
}

static void on_solution(GtkButton *btn, gpointer data) {
    if (sock < 0) return;
    send_command("SOLUTION");
    char response[4096] = {0};
    recv_response(response, sizeof(response));
    char *sol = strstr(response, "\n");
    if (sol) parse_grid(sol + 1);
    gtk_label_set_text(GTK_LABEL(message_label), "Solution affichee.");
    gtk_widget_grab_focus(hidden_entry);
}

static void on_connect(GtkButton *btn, gpointer data) {
    const char *ip = gtk_entry_get_text(GTK_ENTRY(connect_entry));
    struct sockaddr_in addr;
    if (sock >= 0) { close(sock); sock = -1; }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        gtk_label_set_text(GTK_LABEL(message_label),
            "Connexion echouee ! Verifiez que le serveur tourne.");
        set_status_disconnected();
        close(sock); sock = -1;
        return;
    }
    char buf[1024] = {0};
    recv_response(buf, sizeof(buf));
    set_status_connected();
    gtk_label_set_text(GTK_LABEL(message_label),
        "Connecte ! Choisissez Facile / Moyen / Difficile.");
    gtk_widget_grab_focus(hidden_entry);
}

static void on_quit(GtkButton *btn, gpointer data) {
    if (sock >= 0) {
        send_command("QUIT");
        close(sock);
        sock = -1;
    }
    gtk_main_quit();
}

/* ─────────────────────────────────────────
   CSS — sans caractères spéciaux
───────────────────────────────────────── */
static const char *CSS =
    ".cell-fixed {"
    "  background-color: #c0c0c0;"
    "  color: #333333;"
    "  font-weight: bold;"
    "  font-size: 16px;"
    "}"
    ".cell-empty {"
    "  background-color: #ffffff;"
    "  color: #333333;"
    "  font-size: 16px;"
    "}"
    ".cell-selected {"
    "  background-color: #5aabf5;"
    "  color: #002244;"
    "  font-weight: bold;"
    "  font-size: 16px;"
    " border: 2px solid #000000;"
    "}"
    ".grid-button {"
    "  min-width: 44px;"
    "  min-height: 44px;"
    "  border-radius: 6px;"
    "  border: 1px solid #aaaaaa;"
    "}"
    ".thick-right {"
    "  border-right: 3px solid #333333;"
    "}"
    ".thick-bottom {"
    "  border-bottom: 3px solid #333333;"
    "}";

/* ─────────────────────────────────────────
   Construction UI
───────────────────────────────────────── */
static void build_ui(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Sudoku Client");
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* ── Barre connexion ── */
    GtkWidget *hcon = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    connect_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(connect_entry), "127.0.0.1");
    gtk_entry_set_width_chars(GTK_ENTRY(connect_entry), 14);

    GtkWidget *btn_con = gtk_button_new_with_label("Connecter");
    g_signal_connect(btn_con, "clicked",
                     G_CALLBACK(on_connect), NULL);

    /* Status label avec markup Pango — rouge par defaut */
    status_label = gtk_label_new(NULL);
    set_status_disconnected();

    gtk_box_pack_start(GTK_BOX(hcon), connect_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hcon), btn_con,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hcon), status_label,  FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), hcon, FALSE, FALSE, 0);

    /* ── Champ caché pour le clavier ── */
    hidden_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(hidden_entry), 1);
    gtk_widget_set_size_request(hidden_entry, 1, 1);
    gtk_widget_set_opacity(hidden_entry, 0.0);
    gtk_entry_set_has_frame(GTK_ENTRY(hidden_entry), FALSE);
    g_signal_connect(hidden_entry, "changed",
                     G_CALLBACK(on_hidden_entry_changed), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), hidden_entry,
                       FALSE, FALSE, 0);

    /* ── Grille 9x9 ── */
    GtkWidget *grid_w = gtk_grid_new();
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            GtkWidget *b = gtk_button_new_with_label("");
            gtk_widget_set_size_request(b, 44, 44);
            GtkStyleContext *ctx = gtk_widget_get_style_context(b);
            gtk_style_context_add_class(ctx, "grid-button");
            gtk_style_context_add_class(ctx, "cell-empty");
            if (c == 2 || c == 5)
                gtk_style_context_add_class(ctx, "thick-right");
            if (r == 2 || r == 5)
                gtk_style_context_add_class(ctx, "thick-bottom");
            g_signal_connect(b, "clicked",
                             G_CALLBACK(on_cell_clicked),
                             GINT_TO_POINTER(r * GRID_SIZE + c));
            cells[r][c] = b;
            gtk_grid_attach(GTK_GRID(grid_w), b, c, r, 1, 1);
        }
    }
    gtk_box_pack_start(GTK_BOX(vbox), grid_w, FALSE, FALSE, 4);

    /* ── Boutons jeu ── */
    GtkWidget *hbtn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *lvl_names[] = {"Facile", "Moyen", "Difficile"};
    for (int i = 0; i < 3; i++) {
        GtkWidget *b = gtk_button_new_with_label(lvl_names[i]);
        g_signal_connect(b, "clicked",
                         G_CALLBACK(on_new_game),
                         GINT_TO_POINTER(i + 1));
        gtk_box_pack_start(GTK_BOX(hbtn), b, FALSE, FALSE, 0);
    }
    GtkWidget *bck = gtk_button_new_with_label("Verifier");
    GtkWidget *bsl = gtk_button_new_with_label("Solution");
    GtkWidget *bqt = gtk_button_new_with_label("Quitter");
    g_signal_connect(bck, "clicked", G_CALLBACK(on_check),    NULL);
    g_signal_connect(bsl, "clicked", G_CALLBACK(on_solution), NULL);
    g_signal_connect(bqt, "clicked", G_CALLBACK(on_quit),     NULL);
    gtk_box_pack_start(GTK_BOX(hbtn), bck, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbtn), bsl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbtn), bqt, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbtn, FALSE, FALSE, 0);

    /* ── Message bas ── */
    message_label = gtk_label_new(
        "Entrez l'IP du serveur et cliquez Connecter.");
    gtk_label_set_xalign(GTK_LABEL(message_label), 0.0f);
    gtk_box_pack_start(GTK_BOX(vbox), message_label,
                       FALSE, FALSE, 4);

    gtk_widget_show_all(window);
    gtk_widget_grab_focus(hidden_entry);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    memset(grid, 0, sizeof(grid));
    memset(fixed_cells, 0, sizeof(fixed_cells));
    build_ui();
    gtk_main();
    if (sock >= 0) close(sock);
    return 0;
}
