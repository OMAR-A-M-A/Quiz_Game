#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 512
#define MAX_CHOICES 4
#define MAX_QUESTIONS 5000
#define MAX_CATEGORIES 100
#define MAX_MISTAKES 5
#define POINTS_PER_QUESTION 10
#define TIME_LIMIT 5
#define MAX_PLAYERS 100
#define SHOW_LEADERBOARD 5

// Function to load CSS
void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    GError *error = NULL;
    gtk_css_provider_load_from_path(provider, "style.css", &error);
    if (error) {
        g_printerr("Error loading CSS file: %s\n", error->message);
        g_error_free(error);
    }
    
    g_object_unref(provider);
}

typedef struct {
    char category[100];
    char question[256];
    char choices[MAX_CHOICES][100];
    char correctAnswer;
    int isBoolean;
} Question;

typedef struct {
    char name[100];
    int score;
} Player;

GtkWidget *window;
GtkWidget *timer_label;
GtkWidget *score_label;
GtkWidget *mistakes_label;
GtkWidget *question_label;
GtkWidget *choices_box;
GtkWidget *name_entry;

Question questions[MAX_QUESTIONS];
char categories[MAX_CATEGORIES][100];
int questionCount = 0;
int categoryCount = 0;
int score = 0;
int mistakes = 0;
int time_left = TIME_LIMIT;
int current_question_index = -1;
int difficulty = 1;

char username[100];
Player players[MAX_PLAYERS];
int playerCount = 0;
guint timer_id;

// Function prototypes
void loadQuestions(const char *filename);
void startQuiz(const char *category, int difficulty);
void showLeaderboard();
void help();
void clearLeaderboard();
gboolean updateTimer(gpointer data);
void checkAnswer(GtkWidget *widget, gpointer data);
void nextQuestion();
void endQuiz();
void saveScore();
void loadScores();
int compareScores(const void *a, const void *b);
void showNameEntryDialog();
void showMainMenu();
void setupWelcomeScreen();
void clearContainer(GtkWidget *container);
void showCategoryDialog();
void onCategorySelected(GtkComboBox *combo, gpointer data);

void addCategory(char *category) {
    for (int i = 0; i < categoryCount; i++) {
        if (strcmp(categories[i], category) == 0) return;
    }
    strcpy(categories[categoryCount++], category);
}

void trimAndLowercase(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    for (char *p = str; *p; p++) *p = tolower(*p);
}

void parseLine(char *line) {
    char *token;
    Question *q = &questions[questionCount];
    
    token = strtok(line, ","); // Type
    trimAndLowercase(token);
    q->isBoolean = (strcmp(token, "boolean") == 0);
    
    strtok(NULL, ","); // Difficulty (not used)
    token = strtok(NULL, ","); // Category
    if (token) {
        strcpy(q->category, token);
        addCategory(token);
    }
    
    token = strtok(NULL, ","); // Question
    if (token) strcpy(q->question, token);
    
    if (q->isBoolean) {
        strcpy(q->choices[0], "True");
        strcpy(q->choices[1], "False");
        token = strtok(NULL, ",");
        trimAndLowercase(token);
        q->correctAnswer = (strcmp(token, "true") == 0) ? 'A' : 'B';
    } else {
        for (int i = 0; i < MAX_CHOICES; i++) {
            token = strtok(NULL, ",");
            if (token) strcpy(q->choices[i], token);
        }
        token = strtok(NULL, ",");
        trimAndLowercase(token);
        q->correctAnswer = toupper(token[0]);
    }
    questionCount++;
}

void loadQuestions(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file!\n");
        exit(1);
    }
    
    char line[MAX_LINE_LENGTH];
    fgets(line, sizeof(line), file); // Skip header
    while (fgets(line, sizeof(line), file)) {
        parseLine(line);
    }
    fclose(file);
}

void startQuiz(const char *category, int difficulty) {
    clearContainer(window);
    
    // Create a vertical box to center the content
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);
    
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    timer_label = gtk_label_new(g_strdup_printf("Time left: %d", TIME_LIMIT));
    score_label = gtk_label_new("Score: 0");
    mistakes_label = gtk_label_new("Mistakes: 0/5");
    
    gtk_box_pack_start(GTK_BOX(info_box), timer_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(info_box), score_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(info_box), mistakes_label, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), info_box, 0, 0, 1, 1);
    
    question_label = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), question_label, 0, 1, 1, 1);
    
    choices_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_grid_attach(GTK_GRID(grid), choices_box, 0, 2, 1, 1);
    
    time_left = TIME_LIMIT;
    current_question_index = -1;
    score = 0;
    mistakes = 0;
    
    timer_id = g_timeout_add_seconds(1, updateTimer, NULL);
    nextQuestion();
    
    gtk_widget_show_all(window);
}

gboolean updateTimer(gpointer data) {
    time_left--;
    gtk_label_set_text(GTK_LABEL(timer_label), g_strdup_printf("Time left: %d", time_left));
    
    if (time_left <= 0) {
        mistakes++;
        gtk_label_set_text(GTK_LABEL(mistakes_label), g_strdup_printf("Mistakes: %d/5", mistakes));
        
        if (mistakes >= MAX_MISTAKES) endQuiz();
        else nextQuestion();
    }
    return G_SOURCE_CONTINUE;
}

void nextQuestion() {
    current_question_index++;
    time_left = TIME_LIMIT;
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(choices_box));
    g_list_foreach(children, (GFunc)gtk_widget_destroy, NULL);
    g_list_free(children);
    
    if (current_question_index >= questionCount) {
        endQuiz();
        return;
    }
    
    Question q = questions[current_question_index];
    gtk_label_set_text(GTK_LABEL(question_label), q.question);
    
    for (int i = 0; i < MAX_CHOICES; i++) {
        if (strlen(q.choices[i]) > 0) {
            GtkWidget *btn = gtk_button_new_with_label(q.choices[i]);
            g_signal_connect(btn, "clicked", G_CALLBACK(checkAnswer), GINT_TO_POINTER('A' + i));
            gtk_container_add(GTK_CONTAINER(choices_box), btn);
        }
    }
    gtk_widget_show_all(choices_box);
}

void checkAnswer(GtkWidget *widget, gpointer data) {

    char answer = GPOINTER_TO_INT(data);
    Question q = questions[current_question_index];
    
    if (answer == q.correctAnswer) {
        score += POINTS_PER_QUESTION;
        gtk_label_set_text(GTK_LABEL(score_label), g_strdup_printf("Score: %d", score));
    } else {
        mistakes++;
        gtk_label_set_text(GTK_LABEL(mistakes_label), g_strdup_printf("Mistakes: %d/5", mistakes));
        
        if (mistakes >= MAX_MISTAKES) {
            endQuiz();
            return;
        }
    }
    
    if (current_question_index < questionCount - 1) nextQuestion();
    else endQuiz();
}

void endQuiz() {
    g_source_remove(timer_id);
    if (strlen(username) == 0) {
    }
    saveScore();
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Final Score: %d", score);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    showMainMenu();
}
void saveScore() {
    if (strlen(username) == 0) {
        return;
    }

    FILE *file = fopen("leaderboard.csv", "a");
    if (file) {
        fprintf(file, "%s,%d\n", username, score);  
        fclose(file);
    } else {
        fprintf(stderr, "Error opening leaderboard file for writing!\n");
    }
}


void loadScores() {
    FILE *file = fopen("leaderboard.csv", "r");
    if (!file) return;
    
    playerCount = 0;
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) && playerCount < MAX_PLAYERS) {
        char *name = strtok(line, ",");
        char *score_str = strtok(NULL, "\n");
        if (name && score_str) {
            strcpy(players[playerCount].name, name);
            players[playerCount].score = atoi(score_str);
            playerCount++;
        }
    }
    fclose(file);
}

int compareScores(const void *a, const void *b) {
    Player *playerA = (Player *)a;
    Player *playerB = (Player *)b;
    return playerB->score - playerA->score;
}

void showLeaderboard() {
    loadScores();
    qsort(players, playerCount, sizeof(Player), compareScores);

    // Create leaderboard window
    GtkWidget *leader_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(leader_window), "Leaderboard");
    gtk_window_set_default_size(GTK_WINDOW(leader_window), 400, 300); // Adjusted size for better fit

    // Create a vertical box to structure content
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(leader_window), vbox);

    // Create a scrollable area for the leaderboard
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 380, 250); // Adjust scroll area size

    // Create list store model (columns: Name, Score)
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    // Create and append columns for Name and Score
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Score", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);

    // Populate the leaderboard with player data
    for (int i = 0; i < playerCount && i < SHOW_LEADERBOARD; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, players[i].name, 1, players[i].score, -1);
    }

    // Pack widgets into the window
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    // Show all widgets
    gtk_widget_show_all(leader_window);
}


void clearLeaderboard() {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Clear all scores?");
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
        FILE *file = fopen("leaderboard.csv", "w");
        if (file) {
            fclose(file);
            GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(window),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "Leaderboard cleared!");
            gtk_dialog_run(GTK_DIALOG(info));
            gtk_widget_destroy(info);
        }
    }
    gtk_widget_destroy(dialog);
}

void help() {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Game Rules:\n- Answer before time runs out\n- 10 points per correct answer\n- Max 5 mistakes allowed");
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void showCategoryDialog() {
    printf("Username entered: %s\n", username);

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Quiz Settings",
        GTK_WINDOW(window),
        GTK_DIALOG_MODAL,
        "Start",
        GTK_RESPONSE_ACCEPT,
        "Cancel",
        GTK_RESPONSE_REJECT,
        NULL);
		
    gtk_widget_set_size_request(dialog, 600, 400); 
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *name_entry = gtk_entry_new();
    GtkWidget *category_combo = gtk_combo_box_text_new();
    GtkWidget *difficulty_combo = gtk_combo_box_text_new();

    gtk_widget_set_size_request(name_entry, 200, 30); 

    for (int i = 0; i < categoryCount; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(category_combo), categories[i]);
    }
    
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(difficulty_combo), "Easy");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(difficulty_combo), "Medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(difficulty_combo), "Hard");

   
    gtk_container_add(GTK_CONTAINER(content), gtk_label_new("Category:"));
    gtk_container_add(GTK_CONTAINER(content), category_combo);
    gtk_container_add(GTK_CONTAINER(content), gtk_label_new("Difficulty:"));
    gtk_container_add(GTK_CONTAINER(content), difficulty_combo);

    gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(name_entry)); 
        strncpy(username, name, sizeof(username) - 1); 
        username[sizeof(username) - 1] = '\0'; 
    
        gchar *category = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(category_combo));
        difficulty = gtk_combo_box_get_active(GTK_COMBO_BOX(difficulty_combo)) + 1;
    
        startQuiz(category, difficulty);
    
        g_free(category);
    }

    gtk_widget_destroy(dialog);
}


void showMainMenu() {
    clearContainer(window);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);
    
    const char *labels[] = {"Start Quiz", "Leaderboard", "Clear the Leaderboard", "Help", "Exit"};
    for (int i = 0; i < 5; i++) {
        GtkWidget *btn = gtk_button_new_with_label(labels[i]);
        gtk_widget_set_size_request(btn, 200, 50);
        gtk_grid_attach(GTK_GRID(grid), btn, 0, i, 1, 1);
    }
    
    g_signal_connect(gtk_grid_get_child_at(GTK_GRID(grid), 0, 0), "clicked", G_CALLBACK(showNameEntryDialog), NULL);
    g_signal_connect(gtk_grid_get_child_at(GTK_GRID(grid), 0, 1), "clicked", G_CALLBACK(showLeaderboard), NULL);
    g_signal_connect(gtk_grid_get_child_at(GTK_GRID(grid), 0, 2), "clicked", G_CALLBACK(clearLeaderboard), NULL);
    g_signal_connect(gtk_grid_get_child_at(GTK_GRID(grid), 0, 3), "clicked", G_CALLBACK(help), NULL);
    g_signal_connect(gtk_grid_get_child_at(GTK_GRID(grid), 0, 4), "clicked", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_widget_show_all(window);
}
void showNameEntryDialog() {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Enter Your Name",
        GTK_WINDOW(window),
        GTK_DIALOG_MODAL,
        "OK",
        GTK_RESPONSE_ACCEPT,
        "Cancel",
        GTK_RESPONSE_REJECT,
        NULL);

    gtk_widget_set_size_request(dialog, 300, 150);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *name_entry = gtk_entry_new();

    gtk_container_add(GTK_CONTAINER(content), gtk_label_new("Please enter your name:"));
    gtk_container_add(GTK_CONTAINER(content), name_entry);

    gtk_widget_grab_focus(name_entry); 
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(name_entry));

        if (entry_text && *entry_text) { 
            memset(username, 0, sizeof(username)); 
            g_strlcpy(username, entry_text, sizeof(username)); 
            showCategoryDialog();
        } else {
            printf("No name entered!\n");
        }
    }

    gtk_widget_destroy(dialog);
}


void setupWelcomeScreen() {
    clearContainer(window);
    
    // Create a vertical box to center the content
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Create a grid for the widgets
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);
    
    name_entry = gtk_entry_new();
    GtkWidget *start_btn = gtk_button_new_with_label("Continue");
    
    g_signal_connect(start_btn, "clicked", G_CALLBACK(showMainMenu), NULL);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Enter your name:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), start_btn, 0, 2, 1, 1);
    
    gtk_widget_show_all(window);
}

void clearContainer(GtkWidget *container) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(container));
    g_list_foreach(children, (GFunc)gtk_widget_destroy, NULL);
    g_list_free(children);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    loadQuestions("quiz_questions.csv");
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Quiz Game");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    load_css();
    
    showMainMenu();
    gtk_main();
    
    return 0;
}