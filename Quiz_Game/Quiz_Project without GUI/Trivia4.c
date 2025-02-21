#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 512
#define MAX_CHOICES 4
#define MAX_QUESTIONS 5000
#define MAX_CATEGORIES 100
#define MAX_MISTAKES 5
#define POINTS_PER_QUESTION 10
#define TIME_LIMIT 10
#define MAX_PLAYERS 100
#define SHOW_LEADERBOARD 5
 //?new
void help();
void menu();
void saveScore(const char *filename, const char *username, int score);
void loadScores(const char *filename);
int compareScores(const void *a, const void *b);
void showLeaderboard(const char *filename);
void clearScores(const char *filename);
typedef struct {
    char category[100];
    char question[256];
    char choices[MAX_CHOICES][100];
    char correctAnswer;
    int isBoolean;
} Question;
                     //?new
typedef struct {    
    char name[100];
    int score;
} Player;

Question questions[MAX_QUESTIONS];
char categories[MAX_CATEGORIES][100];
int questionCount = 0;
int categoryCount = 0;
char userAnswer;
int score = 0;
int mistakes = 0;
int answerReceived = 0;
int difficulty = 1;
                                 //?new
char username[100];
Player players[MAX_PLAYERS];
int playerCount = 0;

void addCategory(char *category) {
    for (int i = 0; i < categoryCount; i++) {
        if (strcmp(categories[i], category) == 0) return;
    }
    strcpy(categories[categoryCount++], category);
}

void trimAndLowercase(char *str) {
    char *end;
    while (isspace((unsigned char) *str)) str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end)) end--;
    end[1] = '\0';
    for (char *p = str; *p; p++) *p = tolower(*p);
}

void parseLine(char *line) {
    char *token;
    Question *q = &questions[questionCount];
    
    token = strtok(line, ","); // Type
    if (token == NULL) return;
    trimAndLowercase(token);
    q->isBoolean = (strcmp(token, "boolean") == 0);
    
    token = strtok(NULL, ","); // Difficulty (not used)
    
    token = strtok(NULL, ","); // Category
    if (token != NULL) {
        strcpy(q->category, token);
        addCategory(token);
    }
    
    token = strtok(NULL, ","); // Question
    if (token != NULL) strcpy(q->question, token);
    
    if (q->isBoolean) {
        strcpy(q->choices[0], "True");
        strcpy(q->choices[1], "False");
        q->choices[2][0] = '\0';
        q->choices[3][0] = '\0';
        
        token = strtok(NULL, ","); // Correct answer
        if (token != NULL) {
            trimAndLowercase(token);
            q->correctAnswer = (strcmp(token, "true") == 0) ? 'A' : 'B';
        }
    }
    questionCount++;
}

void loadQuestions(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file!\n");
        exit(1);
    }
    char line[MAX_LINE_LENGTH];
    fgets(line, sizeof(line), file); // Skip header
    while (fgets(line, sizeof(line), file)) {
        parseLine(line);
    }
    fclose(file);
}

void showCategories() {
    printf("Available Categories:\n");
    for (int i = 0; i < categoryCount; i++) {
        printf("%d) %s\n", i + 1, categories[i]);
    }
}

void shuffleQuestions() {
    srand(time(NULL));
    for (int i = questionCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Question temp = questions[i];
        questions[i] = questions[j];
        questions[j] = temp;
    }
}

void *getInput(void *arg) {
    scanf(" %c", &userAnswer);
    userAnswer = toupper(userAnswer);
    answerReceived = 1;
    return NULL;
}

void startQuiz(const char *selectedCategory) {
    shuffleQuestions();
    for (int i = 0; i < questionCount; i++) {
        if (strcmp(questions[i].category, selectedCategory) == 0) {
            printf("\n%s\n", questions[i].question);
            for (int j = 0; j < MAX_CHOICES; j++) {
                if (questions[i].choices[j][0] != '\0') {
                    printf("%c) %s\n", 'A' + j, questions[i].choices[j]);
                }
            }
            answerReceived = 0;
            pthread_t inputThread;
            printf("Enter your choice (A, B, C, D): ");
            pthread_create(&inputThread, NULL, getInput, NULL);
            time_t startTime = time(NULL);
            while (difftime(time(NULL), startTime) < TIME_LIMIT) {
                if (answerReceived) break;
            }
            pthread_cancel(inputThread);
            if (!answerReceived) {
                printf("Time's up! The correct answer was %c.\n\n", questions[i].correctAnswer);
                mistakes++;
            } else if (userAnswer == questions[i].correctAnswer) {
                printf("Correct!\n\n");
                score += POINTS_PER_QUESTION;
            } else {
                printf("Wrong! The correct answer was %c.\n\n", questions[i].correctAnswer);
                mistakes++;
            }
            printf("Current Score: %d\n", score);
            if (mistakes >= MAX_MISTAKES) {
                printf("Game over! You made %d mistakes.\n", mistakes);
                printf("Final Score: %d\n", score);
                break;
            }
        }
    }
    printf("\nQuiz finished! Your final score: %d\n", score);
    saveScore("E:\\Quiz_Game\\Quiz_Project without GUI\\leaderboard.csv", username, score);
    char outing;
    printf("\n Press : M to go to menu\n B to leaderboard\n Any key to exit\n");
    scanf(" %c",&outing);
    if(outing=='M'||outing=='m'){menu();}
    else if(outing=='B'||outing=='b'){showLeaderboard("E:\\Quiz_Game\\Quiz_Project without GUI\\leaderboard.csv");}
    else{exit(0);}
}

int main() {
    printf("Enter your name: ");    //?new
    scanf("%s", username);

    loadQuestions("E:\\Quiz_Game\\Quiz_Project without GUI\\quiz_questions.csv");
    
    menu();//?new


    return 0;
}
////////////////////////////////////?new////////////////////////////////

void saveScore(const char *filename, const char *username, int score) {
    FILE *file = fopen(filename, "a");
    if (!file) {
        printf("Error opening score file!\n");
        return;
    }
    fprintf(file, "%s,%d\n", username, score);
    fclose(file);
}

void loadScores(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;
    
    char line[MAX_LINE_LENGTH];
    playerCount = 0;
    
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        if (token) {
            strcpy(players[playerCount].name, token);
            token = strtok(NULL, ",");
            if (token) {
                players[playerCount].score = atoi(token);
                playerCount++;
            }
        }
    }
    fclose(file);
}

int compareScores(const void *a, const void *b) { 
    Player *playerA = (Player *)a;
    Player *playerB = (Player *)b;
    return playerB->score - playerA->score;
}

void showLeaderboard(const char *filename) {
    loadScores(filename);
    qsort(players, playerCount, sizeof(Player), compareScores);

    printf("\n--- Leaderboard ---\n");
    for (int i = 0; i < playerCount && i < SHOW_LEADERBOARD; i++) {
        printf("%d. %s - %d\n", i + 1, players[i].name, players[i].score);
    }
}

void clearScores(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file) {
        fclose(file);
        printf("All scores have been cleared successfully!\n");
    } else {
        printf("Error clearing the scores!\n");
    }
}

void menu(){
    int choice1;
    char confirm;
printf("WELCOME TO THE GAME\n");
printf("are you ready to test your knowledge?\n");
printf("-------------------------------------------\n");
printf("-> Press 1 to start the game\n");
printf("-> Press 2 to show the leaderboard\n");
printf("-> Press 3 to clear the leaderboard\n");
printf("-> Press 4 for help\n");
printf("-> Press 5 to exit the game\n");
scanf("%d", &choice1);
switch(choice1){
    case 1:
help();
printf("\nPress Y to start the game: ");
scanf(" %c", &confirm);
if(confirm == 'Y' || confirm == 'y')
        {
    printf("Select difficulty level:\n1) Easy\n2) Medium\n3) Hard\nEnter your choice: ");
    scanf("%d", &difficulty);
    showCategories();
    int choice;
    printf("Select a category (enter number): ");
    scanf("%d", &choice);
    if (choice < 1 || choice > categoryCount) {
        printf("Invalid choice!\n");
        menu();
    }
    startQuiz(categories[choice - 1]);
    }else{
        printf("You have chosen not to start the game\n");
        menu();
        }
        break;

    case 2:
    showLeaderboard("E:\\Quiz_Game\\Quiz_Project without GUI\\leaderboard.csv");
    printf("\nPress any key to return to the menu:  ");
    scanf(" %c", &confirm);
    if(confirm == 'M' || confirm == 'm'){menu();}else{menu();}
        break;
    case 3:
    clearScores("E:\\Quiz_Game\\Quiz_Project without GUI\\leaderboard.csv");
    printf("\nPress any key to return to the menu:  ");
    scanf(" %c", &confirm);
    if(confirm == 'M' || confirm == 'm'){menu();}else{menu();}
        break;
    case 4:
    help();
    printf("\nPress any key return to the menu:  ");
    scanf(" %c", &confirm);
    if(confirm == 'M' || confirm == 'm'){menu();}else{menu();}
        break;
    case 5:
    exit(0);
        break;
    default:
        printf("Invalid choice. Please try again.\n");
        menu();
    }
}

void help(){
    printf("-------------------------- wlecome to the game --------------------------");
    printf("\n\n Here are the rules of the game: \n\n");
    printf("-------------------------------------------------------------------------");
    printf("\n >> the game is simple and easy to play\n");
    printf("\n >> The game has 3 difficulty levels: Easy, Medium, and Hard");
    printf("\n    You can choose from 10 categories of questions to test your knowledge\n");
    printf("\n >> The rules are simple: try not to make any mistakes.You'll keep getting");
    printf("\n    questions until you reach the maximum number of mistakes, which is 5.");
    printf("\n    Try to get the highest score possible.there is leaderboard to check ur rank\n");
    printf("\n >> Each correct answer earns you 10 points.\n");
    printf("\n >> u will be given 4 options and u have to press A ,B ,C or D\n");
    printf("\n >> no negative marking for wrong answers\n");
    printf("\n\n\t !!!!!!!!!!!!!!!! GOOD LUCK !!!!!!!!!!!!!!!!!!!\n");
}