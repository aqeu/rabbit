#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define CLEAR "cls"
    #define SLEEP(ms) Sleep(ms)
    int kbhit_impl() { return _kbhit(); }
    int getch_impl() { return _getch(); }
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #define CLEAR "clear"
    #define SLEEP(ms) usleep((ms) * 1000)
    static struct termios g_oldt;
    static int g_termSaved = 0;
    void saveTerminal() {
        if (!g_termSaved) {
            tcgetattr(STDIN_FILENO, &g_oldt);
            g_termSaved = 1;
        }
    }
    
    int kbhit_impl() {
        saveTerminal();
        struct termios newt = g_oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
        int ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &g_oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);
        if (ch != EOF) {
            ungetc(ch, stdin);
            return 1;
        }
        return 0;
    }
    
    int getch_impl() {
        saveTerminal();
        struct termios newt = g_oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        int ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &g_oldt);
        return ch;
    }
#endif
#define GRID_WIDTH 30
#define GRID_HEIGHT 20
#define MAX_POWERUPS 8
#define MAX_OBSTACLES 20
#define MAX_CARROTS 15
#define MAX_ENEMIES 5
#define MAX_TOP_SCORES 10
#define GAME_TIMEOUT 180
#define MAX_LEVELS 10
#define MAX_PARTICLES 50
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
typedef struct { int x, y; } Position;
typedef enum { POWERUP_SPEED, POWERUP_SHIELD, POWERUP_FREEZE, POWERUP_MAGNET, POWERUP_DOUBLE } PowerUpType;
typedef struct { int x, y, type, active; int duration; } PowerUp;
typedef enum { OBSTACLE_ROCK, OBSTACLE_PUDDLE, OBSTACLE_SPIKE, OBSTACLE_MOVING } ObstacleType;
typedef struct { int x, y, type, active; int dx, dy; int moveTimer; } Obstacle;
typedef struct { int x, y, collected; int value; } Carrot;
typedef enum { ENEMY_HORIZONTAL, ENEMY_VERTICAL, ENEMY_CHASER } EnemyType;
typedef struct { int x, y, type, active; int dx, dy; int moveTimer; } Enemy;
typedef struct { int x, y, life; char symbol; } Particle;
typedef struct {
    char name[32];
    int score;
    int level;
    time_t date;
} HighScore;
int score = 0;
int level = 1;
int timeRemaining = GAME_TIMEOUT;
int rabbitX = 1, rabbitY = 1;
int hasSuperSpeed = 0;
int hasInvincibility = 0;
int hasFreeze = 0;
int hasMagnet = 0;
int hasDouble = 0;
int speedTimer = 0;
int invincibilityTimer = 0;
int freezeTimer = 0;
int magnetTimer = 0;
int doubleTimer = 0;
int gameActive = 1;
int gamePaused = 0;
int totalCarrotsCollected = 0;
int totalMoves = 0;
int totalDeaths = 0;
int comboCount = 0;
int comboTimer = 0;
int lives = 3;
int highScoreCount = 0;
HighScore topScores[MAX_TOP_SCORES];
int maze[GRID_HEIGHT][GRID_WIDTH];
int explored[GRID_HEIGHT][GRID_WIDTH];
PowerUp powerups[MAX_POWERUPS];
Obstacle obstacles[MAX_OBSTACLES];
Carrot carrots[MAX_CARROTS];
Enemy enemies[MAX_ENEMIES];
Particle particles[MAX_PARTICLES];
int particleCount = 0;
int powerupCount = 0;
int obstacleCount = 0;
int carrotCount = 0;
int enemyCount = 0;
time_t startTime;
time_t lastEnemyMove;
int gameRunning = 1;
void clearScreen() { system(CLEAR); }
void setColor(const char* color) { printf("%s", color); }
void hideCursor() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(console, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(console, &cursorInfo);
#else
    printf("\033[?25l");
#endif
}

void showCursor() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(console, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(console, &cursorInfo);
#else
    printf("\033[?25h");
#endif
}

void gotoxy(int x, int y) {
#ifdef _WIN32
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
#else
    printf("\033[%d;%dH", y + 1, x + 1);
#endif
}

int kbhit() { return kbhit_impl(); }
int getch() { return getch_impl(); }
void spawnParticles(int x, int y, int count, char symbol) {
    for (int i = 0; i < count && particleCount < MAX_PARTICLES; i++) {
        particles[particleCount].x = x;
        particles[particleCount].y = y;
        particles[particleCount].life = 3 + rand() % 5;
        particles[particleCount].symbol = symbol;
        particleCount++;
    }
}

void updateParticles() {
    int writeIdx = 0;
    for (int i = 0; i < particleCount; i++) {
        if (particles[i].life > 0) {
            particles[i].life--;
            particles[i].y += (rand() % 3) - 1;
            particles[i].x += (rand() % 3) - 1;
            if (particles[i].y < 0) particles[i].y = 0;
            if (particles[i].y >= GRID_HEIGHT) particles[i].y = GRID_HEIGHT - 1;
            if (particles[i].x < 0) particles[i].x = 0;
            if (particles[i].x >= GRID_WIDTH) particles[i].x = GRID_WIDTH - 1;
            particles[writeIdx++] = particles[i];
        }
    }
    particleCount = writeIdx;
}

int getParticleAt(int x, int y, char* outSymbol) {
    for (int i = 0; i < particleCount; i++) {
        if (particles[i].x == x && particles[i].y == y && particles[i].life > 0) {
            *outSymbol = particles[i].symbol;
            return 1;
        }
    }
    return 0;
}

void initHighScores() {
    for (int i = 0; i < MAX_TOP_SCORES; i++) {
        strcpy(topScores[i].name, "---");
        topScores[i].score = 0;
        topScores[i].level = 0;
        topScores[i].date = 0;
    }
    highScoreCount = 0;
}

void saveHighScores() {
    FILE *f = fopen("rabbit_scores.dat", "wb");
    if (f) {
        fwrite(&highScoreCount, sizeof(int), 1, f);
        fwrite(topScores, sizeof(HighScore), MAX_TOP_SCORES, f);
        fclose(f);
    }
}

void loadHighScores() {
    FILE *f = fopen("rabbit_scores.dat", "rb");
    if (f) {
        if (fread(&highScoreCount, sizeof(int), 1, f) != 1) highScoreCount = 0;
        if (highScoreCount < 0 || highScoreCount > MAX_TOP_SCORES) highScoreCount = 0;
        if (fread(topScores, sizeof(HighScore), MAX_TOP_SCORES, f) != MAX_TOP_SCORES) {
            initHighScores();
        }
        fclose(f);
    } else {
        initHighScores();
    }
}

void addHighScore(const char* name, int sc, int lvl) {
    HighScore newScore;
    strncpy(newScore.name, name, 31);
    newScore.name[31] = '\0';
    newScore.score = sc;
    newScore.level = lvl;
    newScore.date = time(NULL);
    int pos = MAX_TOP_SCORES;
    for (int i = 0; i < MAX_TOP_SCORES; i++) {
        if (sc > topScores[i].score) {
            pos = i;
            break;
        }
    }
    
    if (pos >= MAX_TOP_SCORES) {
        if (highScoreCount < MAX_TOP_SCORES) {
            pos = highScoreCount;
        } else {
            return;
        }
    }
    
    int last = (highScoreCount < MAX_TOP_SCORES) ? highScoreCount : MAX_TOP_SCORES - 1;
    for (int j = last; j > pos; j--) {
        topScores[j] = topScores[j - 1];
    }
    topScores[pos] = newScore;
    if (highScoreCount < MAX_TOP_SCORES) highScoreCount++;
    saveHighScores();
}

void displayHighScores() {
    clearScreen();
    setColor(COLOR_CYAN);
    printf("\n");
    printf("     +==========================================================+\n");
    printf("     |                    TOP 10 HIGH SCORES                    |\n");
    printf("     +==========================================================+\n");
    setColor(COLOR_RESET);
    if (highScoreCount == 0) {
        printf("     |              No high scores yet. Play a game!            |\n");
    } else {
        printf("     |  Rank  Name              Score    Level   Date           |\n");
        printf("     +----------------------------------------------------------+\n");
        for (int i = 0; i < MAX_TOP_SCORES && i < highScoreCount; i++) {
            char dateStr[16];
            time_t d = topScores[i].date;
            struct tm *tm_info = localtime(&d);
            if (tm_info) {
                strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", tm_info);
            } else {
                strcpy(dateStr, "----------");
            }
            const char* medalColor = COLOR_RESET;
            if (i == 0) medalColor = COLOR_YELLOW;
            else if (i == 1) medalColor = COLOR_WHITE;
            else if (i == 2) medalColor = COLOR_MAGENTA;
            setColor(medalColor);
            printf("     |  %2d.   %-16s  %6d    %2d     %s   |\n", 
                   i + 1, topScores[i].name, topScores[i].score, 
                   topScores[i].level, dateStr);
            setColor(COLOR_RESET);
        }
    }
    
    setColor(COLOR_CYAN);
    printf("     +==========================================================+\n");
    setColor(COLOR_RESET);
    printf("\n     Press any key to continue...\n");
    getch();
}

void initMaze() {
    for (int i = 0; i < GRID_HEIGHT; i++) {
        for (int j = 0; j < GRID_WIDTH; j++) {
            explored[i][j] = 0;
            if (i == 0 || i == GRID_HEIGHT - 1 || j == 0 || j == GRID_WIDTH - 1) {
                maze[i][j] = 1;
            } else {
                maze[i][j] = 0;
            }
        }
    }
}

void generateMaze() {
    for (int i = 0; i < GRID_HEIGHT; i++) {
        for (int j = 0; j < GRID_WIDTH; j++) {
            explored[i][j] = 0;
            if (i == 0 || i == GRID_HEIGHT - 1 || j == 0 || j == GRID_WIDTH - 1) {
                maze[i][j] = 1;
            } else {
                maze[i][j] = 0;
            }
        }
    }
    
    int wallDensity = 15 + level * 3;
    if (wallDensity > 35) wallDensity = 35;
    for (int i = 1; i < GRID_HEIGHT - 1; i++) {
        for (int j = 1; j < GRID_WIDTH - 1; j++) {
            if (rand() % 100 < wallDensity) {
                maze[i][j] = 1;
            }
        }
    }
    
    for (int i = 1; i <= 2; i++)
        for (int j = 1; j <= 2; j++)
            if (i < GRID_HEIGHT-1 && j < GRID_WIDTH-1) maze[i][j] = 0;
    maze[GRID_HEIGHT - 2][GRID_WIDTH - 2] = 0;
    int x = 1, y = 1;
    while (x < GRID_WIDTH - 2 || y < GRID_HEIGHT - 2) {
        maze[y][x] = 0;
        if (x < GRID_WIDTH - 2 && (y >= GRID_HEIGHT - 2 || rand() % 2)) {
            x++;
        } else if (y < GRID_HEIGHT - 2) {
            y++;
        } else {
            break;
        }
    }
    maze[GRID_HEIGHT - 2][GRID_WIDTH - 2] = 0;
    for (int i = 1; i < GRID_HEIGHT - 1; i++) {
        for (int j = 1; j < GRID_WIDTH - 1; j++) {
            if (maze[i][j] == 1) {
                int openNeighbors = 0;
                if (i > 1 && maze[i-1][j] == 0) openNeighbors++;
                if (i < GRID_HEIGHT-2 && maze[i+1][j] == 0) openNeighbors++;
                if (j > 1 && maze[i][j-1] == 0) openNeighbors++;
                if (j < GRID_WIDTH-2 && maze[i][j+1] == 0) openNeighbors++;
                if (openNeighbors >= 3) maze[i][j] = 0;
            }
        }
    }
}

int isValidPosition(int x, int y) {
    if (x < 1 || x >= GRID_WIDTH - 1 || y < 1 || y >= GRID_HEIGHT - 1) return 0;
    if (maze[y][x] == 1) return 0;
    return 1;
}

int isOccupied(int x, int y, int includeRabbit) {
    if (includeRabbit && x == rabbitX && y == rabbitY) return 1;
    for (int i = 0; i < carrotCount; i++)
        if (!carrots[i].collected && carrots[i].x == x && carrots[i].y == y) return 1;
    for (int i = 0; i < obstacleCount; i++)
        if (obstacles[i].active && obstacles[i].x == x && obstacles[i].y == y) return 1;
    for (int i = 0; i < powerupCount; i++)
        if (powerups[i].active && powerups[i].x == x && powerups[i].y == y) return 1;
    for (int i = 0; i < enemyCount; i++)
        if (enemies[i].active && enemies[i].x == x && enemies[i].y == y) return 1;
    return 0;
}

void generateObstacles() {
    obstacleCount = 4 + (level * 2);
    if (obstacleCount > MAX_OBSTACLES) obstacleCount = MAX_OBSTACLES;
    for (int i = 0; i < obstacleCount; i++) {
        int x = 1, y = 1, attempts = 0;
        do {
            x = rand() % (GRID_WIDTH - 2) + 1;
            y = rand() % (GRID_HEIGHT - 2) + 1;
            attempts++;
        } while ((!isValidPosition(x, y) || isOccupied(x, y, 1) || 
                 (abs(x - 1) < 4 && abs(y - 1) < 4)) && attempts < 100);
        obstacles[i].x = x;
        obstacles[i].y = y;
        obstacles[i].active = 1;
        if (level >= 3 && rand() % 4 == 0) {
            obstacles[i].type = OBSTACLE_MOVING;
            obstacles[i].dx = (rand() % 2) ? 1 : -1;
            obstacles[i].dy = 0;
            obstacles[i].moveTimer = 0;
        } else {
            obstacles[i].type = rand() % 3;
            obstacles[i].dx = 0;
            obstacles[i].dy = 0;
            obstacles[i].moveTimer = 0;
        }
    }
}

void generateCarrots() {
    carrotCount = 6 + (level * 2);
    if (carrotCount > MAX_CARROTS) carrotCount = MAX_CARROTS;
    for (int i = 0; i < carrotCount; i++) {
        int x = 1, y = 1, attempts = 0;
        do {
            x = rand() % (GRID_WIDTH - 2) + 1;
            y = rand() % (GRID_HEIGHT - 2) + 1;
            attempts++;
        } while ((!isValidPosition(x, y) || isOccupied(x, y, 1)) && attempts < 100);
        carrots[i].x = x;
        carrots[i].y = y;
        carrots[i].collected = 0;
        carrots[i].value = 25 + (level * 5);
    }
}

void generatePowerUps() {
    powerupCount = 2 + (level / 2);
    if (powerupCount > MAX_POWERUPS) powerupCount = MAX_POWERUPS;
    for (int i = 0; i < powerupCount; i++) {
        int x = 1, y = 1, attempts = 0;
        do {
            x = rand() % (GRID_WIDTH - 2) + 1;
            y = rand() % (GRID_HEIGHT - 2) + 1;
            attempts++;
        } while ((!isValidPosition(x, y) || isOccupied(x, y, 1)) && attempts < 100);
        powerups[i].x = x;
        powerups[i].y = y;
        powerups[i].active = 1;
        powerups[i].duration = 0;
        powerups[i].type = rand() % 5;
    }
}

void generateEnemies() {
    if (level < 2) {
        enemyCount = 0;
        return;
    }
    enemyCount = 1 + (level / 3);
    if (enemyCount > MAX_ENEMIES) enemyCount = MAX_ENEMIES;
    for (int i = 0; i < enemyCount; i++) {
        int x = 1, y = 1, attempts = 0;
        do {
            x = rand() % (GRID_WIDTH - 2) + 1;
            y = rand() % (GRID_HEIGHT - 2) + 1;
            attempts++;
        } while ((!isValidPosition(x, y) || isOccupied(x, y, 1) || 
                 (abs(x - 1) < 5 && abs(y - 1) < 5)) && attempts < 100);
        enemies[i].x = x;
        enemies[i].y = y;
        enemies[i].active = 1;
        enemies[i].moveTimer = 0;
        if (level >= 5 && rand() % 3 == 0) {
            enemies[i].type = ENEMY_CHASER;
        } else {
            enemies[i].type = rand() % 2;
        }
        
        if (enemies[i].type == ENEMY_HORIZONTAL) {
            enemies[i].dx = (rand() % 2) ? 1 : -1;
            enemies[i].dy = 0;
        } else if (enemies[i].type == ENEMY_VERTICAL) {
            enemies[i].dx = 0;
            enemies[i].dy = (rand() % 2) ? 1 : -1;
        } else {
            enemies[i].dx = 0;
            enemies[i].dy = 0;
        }
    }
}

void displayGame() {
    clearScreen();
    setColor(COLOR_CYAN);
    printf("\n");
    printf("  +================================================================+\n");
    printf("  |              RABBIT MAZE ADVENTURE                              |\n");
    printf("  +================================================================+\n");
    printf("  |  Level: %-2d  Score: %-6d  Lives: ", level, score);
    for (int i = 0; i < lives; i++) printf("O ");
    for (int i = lives; i < 5; i++) printf("  ");
    printf("  Time: %3d  |\n", timeRemaining);
    printf("  |  ");
    if (hasSuperSpeed) {
        setColor(COLOR_YELLOW);
        printf("[SPEED %ds] ", speedTimer);
    }
    if (hasInvincibility) {
        setColor(COLOR_CYAN);
        printf("[SHIELD %ds] ", invincibilityTimer);
    }
    if (hasFreeze) {
        setColor(COLOR_BLUE);
        printf("[FREEZE %ds] ", freezeTimer);
    }
    if (hasMagnet) {
        setColor(COLOR_MAGENTA);
        printf("[MAGNET %ds] ", magnetTimer);
    }
    if (hasDouble) {
        setColor(COLOR_GREEN);
        printf("[2xSCORE %ds] ", doubleTimer);
    }
    if (!hasSuperSpeed && !hasInvincibility && !hasFreeze && !hasMagnet && !hasDouble) {
        printf("No active power-ups                              ");
    }
    printf("  |\n");
    
    printf("  |  ");
    if (comboCount > 1) {
        setColor(COLOR_YELLOW);
        printf("COMBO x%d! ", comboCount);
    } else {
        printf("           ");
    }
    printf("Carrots: %d/%d  Moves: %d                       |\n", 
           totalCarrotsCollected, carrotCount, totalMoves);
    
    printf("  +================================================================+\n");
    setColor(COLOR_RESET);
    printf("\n  ");
    for (int j = 0; j < GRID_WIDTH; j++) printf("--");
    printf("\n");
    for (int i = 0; i < GRID_HEIGHT; i++) {
        printf("  |");
        for (int j = 0; j < GRID_WIDTH; j++) {
            int printed = 0;
            char particleSymbol;
            if (getParticleAt(j, i, &particleSymbol)) {
                setColor(COLOR_YELLOW);
                printf("%c ", particleSymbol);
                setColor(COLOR_RESET);
                printed = 1;
            }
            
            if (!printed && i == rabbitY && j == rabbitX) {
                if (hasInvincibility) {
                    setColor(COLOR_CYAN);
                } else if (hasSuperSpeed) {
                    setColor(COLOR_YELLOW);
                } else {
                    setColor(COLOR_WHITE);
                }
                printf("R ");
                setColor(COLOR_RESET);
                printed = 1;
            }
            
            if (!printed) {
                for (int k = 0; k < carrotCount; k++) {
                    if (!carrots[k].collected && 
                        carrots[k].x == j && carrots[k].y == i) {
                        setColor(COLOR_YELLOW);
                        printf("* ");
                        setColor(COLOR_RESET);
                        printed = 1;
                        break;
                    }
                }
            }
            
            if (!printed) {
                for (int k = 0; k < enemyCount; k++) {
                    if (enemies[k].active && enemies[k].x == j && enemies[k].y == i) {
                        setColor(COLOR_RED);
                        printf("E ");
                        setColor(COLOR_RESET);
                        printed = 1;
                        break;
                    }
                }
            }
            
            if (!printed) {
                for (int k = 0; k < obstacleCount; k++) {
                    if (obstacles[k].active && obstacles[k].x == j && obstacles[k].y == i) {
                        switch (obstacles[k].type) {
                            case OBSTACLE_ROCK:
                                setColor(COLOR_WHITE);
                                printf("X ");
                                break;
                            case OBSTACLE_PUDDLE:
                                setColor(COLOR_BLUE);
                                printf("~ ");
                                break;
                            case OBSTACLE_SPIKE:
                                setColor(COLOR_RED);
                                printf("^ ");
                                break;
                            case OBSTACLE_MOVING:
                                setColor(COLOR_MAGENTA);
                                printf("O ");
                                break;
                        }
                        setColor(COLOR_RESET);
                        printed = 1;
                        break;
                    }
                }
            }
            
            if (!printed) {
                for (int k = 0; k < powerupCount; k++) {
                    if (powerups[k].active && powerups[k].x == j && powerups[k].y == i) {
                        switch (powerups[k].type) {
                            case POWERUP_SPEED:
                                setColor(COLOR_YELLOW);
                                printf("S ");
                                break;
                            case POWERUP_SHIELD:
                                setColor(COLOR_CYAN);
                                printf("D ");
                                break;
                            case POWERUP_FREEZE:
                                setColor(COLOR_BLUE);
                                printf("F ");
                                break;
                            case POWERUP_MAGNET:
                                setColor(COLOR_MAGENTA);
                                printf("M ");
                                break;
                            case POWERUP_DOUBLE:
                                setColor(COLOR_GREEN);
                                printf("2 ");
                                break;
                        }
                        setColor(COLOR_RESET);
                        printed = 1;
                        break;
                    }
                }
            }
            
            if (!printed) {
                if (maze[i][j] == 1) {
                    setColor(COLOR_WHITE);
                    printf("# ");
                    setColor(COLOR_RESET);
                } else if (i == GRID_HEIGHT - 2 && j == GRID_WIDTH - 2) {
                    setColor(COLOR_GREEN);
                    printf("G ");
                    setColor(COLOR_RESET);
                } else {
                    printf(". ");
                }
            }
        }
        printf("|\n");
    }
    
    printf("  ");
    for (int j = 0; j < GRID_WIDTH; j++) printf("--");
    printf("\n");
    setColor(COLOR_WHITE);
    printf("\n  R=Rabbit  *=Carrot(%dpts)  E=Enemy  X=Rock  ~=Puddle  ^=Spike  O=Moving\n", 
           25 + (level * 5));
    setColor(COLOR_CYAN);
    printf("  S=Speed  D=Shield  F=Freeze  M=Magnet  2=Double  G=Goal\n");
    setColor(COLOR_YELLOW);
    printf("\n  WASD=Move  J+Dir=Jump  Space=Pause  H=Scores  Q=Quit\n");
    if (gamePaused) {
        setColor(COLOR_RED);
        printf("\n+==================+\n");
        printf("|    PAUSED        |\n");
        printf("|  Press P to      |\n");
        printf("|  continue        |\n");
        printf("+==================+\n");
    }
    setColor(COLOR_RESET);
}

void updateTimers() {
    if (gamePaused) return;
    if (hasSuperSpeed && speedTimer > 0) {
        speedTimer--;
        if (speedTimer == 0) hasSuperSpeed = 0;
    }
    if (hasInvincibility && invincibilityTimer > 0) {
        invincibilityTimer--;
        if (invincibilityTimer == 0) hasInvincibility = 0;
    }
    if (hasFreeze && freezeTimer > 0) {
        freezeTimer--;
        if (freezeTimer == 0) hasFreeze = 0;
    }
    if (hasMagnet && magnetTimer > 0) {
        magnetTimer--;
        if (magnetTimer == 0) hasMagnet = 0;
    }
    if (hasDouble && doubleTimer > 0) {
        doubleTimer--;
        if (doubleTimer == 0) hasDouble = 0;
    }
    if (comboTimer > 0) {
        comboTimer--;
        if (comboTimer == 0) comboCount = 0;
    }
}

void updateTimer() {
    if (gamePaused) return;
    time_t currentTime = time(NULL);
    int elapsed = (int)(currentTime - startTime);
    timeRemaining = GAME_TIMEOUT - elapsed;
    if (timeRemaining <= 0) {
        timeRemaining = 0;
        lives--;
        totalDeaths++;
        if (lives <= 0) {
            gameActive = 0;
        } else {
            startTime = time(NULL);
            timeRemaining = GAME_TIMEOUT;
        }
    }
}

int canMoveTo(int x, int y) {
    if (x < 1 || x >= GRID_WIDTH - 1 || y < 1 || y >= GRID_HEIGHT - 1) return 0;
    if (maze[y][x] == 1) return 0;
    return 1;
}

int checkObstacleCollision(int x, int y) {
    for (int i = 0; i < obstacleCount; i++) {
        if (obstacles[i].active && obstacles[i].x == x && obstacles[i].y == y) {
            if (obstacles[i].type == OBSTACLE_SPIKE) {
                return 2;
            }
            if (!hasInvincibility) {
                return 1;
            }
        }
    }
    return 0;
}

int checkEnemyCollision(int x, int y) {
    for (int i = 0; i < enemyCount; i++) {
        if (enemies[i].active && enemies[i].x == x && enemies[i].y == y) {
            if (!hasInvincibility) {
                return 1;
            }
        }
    }
    return 0;
}

void collectCarrots(int x, int y) {
    for (int i = 0; i < carrotCount; i++) {
        if (!carrots[i].collected && 
            carrots[i].x == x && carrots[i].y == y) {
            carrots[i].collected = 1;
            totalCarrotsCollected++;
            int points = carrots[i].value;
            if (hasDouble) points *= 2;
            comboCount++;
            comboTimer = 20;
            if (comboCount > 1) {
                points += (comboCount - 1) * 10;
            }
            
            score += points;
            spawnParticles(x, y, 5, '+');
        }
    }
}

void collectPowerUps(int x, int y) {
    for (int i = 0; i < powerupCount; i++) {
        if (powerups[i].active && powerups[i].x == x && powerups[i].y == y) {
            powerups[i].active = 0;
            switch (powerups[i].type) {
                case POWERUP_SPEED:
                    hasSuperSpeed = 1;
                    speedTimer = 15;
                    break;
                case POWERUP_SHIELD:
                    hasInvincibility = 1;
                    invincibilityTimer = 12;
                    break;
                case POWERUP_FREEZE:
                    hasFreeze = 1;
                    freezeTimer = 8;
                    break;
                case POWERUP_MAGNET:
                    hasMagnet = 1;
                    magnetTimer = 10;
                    break;
                case POWERUP_DOUBLE:
                    hasDouble = 1;
                    doubleTimer = 12;
                    break;
            }
            
            score += 50;
            spawnParticles(x, y, 8, '!');
        }
    }
}

void applyMagnetEffect() {
    if (!hasMagnet) return;
    for (int i = 0; i < carrotCount; i++) {
        if (!carrots[i].collected) {
            int dx = rabbitX - carrots[i].x;
            int dy = rabbitY - carrots[i].y;
            int dist = abs(dx) + abs(dy);
            if (dist <= 3 && dist > 0) {
                int oldX = carrots[i].x;
                int oldY = carrots[i].y;
                if (abs(dx) > abs(dy)) {
                    carrots[i].x += (dx > 0) ? 1 : -1;
                } else {
                    carrots[i].y += (dy > 0) ? 1 : -1;
                }
                
                if (!isValidPosition(carrots[i].x, carrots[i].y)) {
                    carrots[i].x = oldX;
                    carrots[i].y = oldY;
                }
            }
        }
    }
}

void moveRabbit(int dx, int dy) {
    if (gamePaused) return;
    int newX = rabbitX + dx;
    int newY = rabbitY + dy;
    if (canMoveTo(newX, newY)) {
        int obstacleHit = checkObstacleCollision(newX, newY);
        if (obstacleHit == 2) {
            lives--;
            totalDeaths++;
            spawnParticles(newX, newY, 10, 'x');
            if (lives <= 0) {
                gameActive = 0;
                return;
            }
            return;
        } else if (obstacleHit == 1) {
            return;
        }
        
        if (checkEnemyCollision(newX, newY)) {
            if (hasInvincibility) {
                for (int i = 0; i < enemyCount; i++) {
                    if (enemies[i].active && enemies[i].x == newX && enemies[i].y == newY) {
                        enemies[i].active = 0;
                        score += 100;
                        spawnParticles(newX, newY, 10, 'X');
                    }
                }
            } else {
                lives--;
                totalDeaths++;
                spawnParticles(rabbitX, rabbitY, 10, 'x');
                if (lives <= 0) {
                    gameActive = 0;
                    return;
                }
                return;
            }
        }
        
        rabbitX = newX;
        rabbitY = newY;
        totalMoves++;
        collectCarrots(newX, newY);
        collectPowerUps(newX, newY);
        if (!hasSuperSpeed) {
            score += 5;
        }
    }
}

void jump(int dx, int dy) {
    if (gamePaused) return;
    int newX = rabbitX + (dx * 2);
    int newY = rabbitY + (dy * 2);
    int midX = rabbitX + dx;
    int midY = rabbitY + dy;
    if (canMoveTo(midX, midY) && canMoveTo(newX, newY)) {
        if (checkObstacleCollision(midX, midY) == 2 || checkObstacleCollision(newX, newY) == 2) {
            return;
        }
        
        if (checkEnemyCollision(newX, newY) && !hasInvincibility) {
            return;
        }
        rabbitX = newX;
        rabbitY = newY;
        totalMoves++;
        score += 15;
        collectCarrots(newX, newY);
        collectPowerUps(newX, newY);
        spawnParticles(midX, midY, 3, '~');
    }
}

void updateEnemies() {
    if (hasFreeze || gamePaused) return;
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].active) continue;
        enemies[i].moveTimer++;
        int moveSpeed = (level >= 5) ? 8 : 12;
        if (enemies[i].moveTimer >= moveSpeed) {
            enemies[i].moveTimer = 0;
            if (enemies[i].type == ENEMY_CHASER) {
                int dx = 0, dy = 0;
                if (abs(rabbitX - enemies[i].x) > abs(rabbitY - enemies[i].y)) {
                    dx = (rabbitX > enemies[i].x) ? 1 : -1;
                } else {
                    dy = (rabbitY > enemies[i].y) ? 1 : -1;
                }
                
                int newX = enemies[i].x + dx;
                int newY = enemies[i].y + dy;
                if (canMoveTo(newX, newY) && !checkObstacleCollision(newX, newY)) {
                    enemies[i].x = newX;
                    enemies[i].y = newY;
                }
            } else {
                int newX = enemies[i].x + enemies[i].dx;
                int newY = enemies[i].y + enemies[i].dy;
                if (!canMoveTo(newX, newY) || checkObstacleCollision(newX, newY)) {
                    enemies[i].dx = -enemies[i].dx;
                    enemies[i].dy = -enemies[i].dy;
                    newX = enemies[i].x + enemies[i].dx;
                    newY = enemies[i].y + enemies[i].dy;
                    if (canMoveTo(newX, newY) && !checkObstacleCollision(newX, newY)) {
                        enemies[i].x = newX;
                        enemies[i].y = newY;
                    }
                } else {
                    enemies[i].x = newX;
                    enemies[i].y = newY;
                }
            }
            
            if (enemies[i].x == rabbitX && enemies[i].y == rabbitY) {
                if (hasInvincibility) {
                    enemies[i].active = 0;
                    score += 100;
                    spawnParticles(enemies[i].x, enemies[i].y, 10, 'X');
                } else {
                    lives--;
                    totalDeaths++;
                    spawnParticles(rabbitX, rabbitY, 10, 'x');
                    if (lives <= 0) {
                        gameActive = 0;
                    }
                }
            }
        }
    }
}

void updateMovingObstacles() {
    if (gamePaused) return;
    for (int i = 0; i < obstacleCount; i++) {
        if (obstacles[i].active && obstacles[i].type == OBSTACLE_MOVING) {
            obstacles[i].moveTimer++;
            if (obstacles[i].moveTimer >= 15) {
                obstacles[i].moveTimer = 0;
                int newX = obstacles[i].x + obstacles[i].dx;
                int newY = obstacles[i].y + obstacles[i].dy;
                obstacles[i].active = 0;
                int blocked = !canMoveTo(newX, newY) || isOccupied(newX, newY, 1);
                obstacles[i].active = 1;
                if (blocked) {
                    obstacles[i].dx = -obstacles[i].dx;
                    obstacles[i].dy = -obstacles[i].dy;
                    newX = obstacles[i].x + obstacles[i].dx;
                    newY = obstacles[i].y + obstacles[i].dy;
                    obstacles[i].active = 0;
                    blocked = !canMoveTo(newX, newY) || isOccupied(newX, newY, 1);
                    obstacles[i].active = 1;
                }
                
                if (!blocked) {
                    obstacles[i].x = newX;
                    obstacles[i].y = newY;
                }
            }
        }
    }
}

int checkLevelComplete() {
    for (int i = 0; i < carrotCount; i++) {
        if (!carrots[i].collected) {
            return 0;
        }
    }
    return 1;
}

void nextLevel() {
    level++;
    totalCarrotsCollected = 0;
    if (level > MAX_LEVELS) {
        gameActive = 0;
        return;
    }
    generateMaze();
    generateObstacles();
    generateCarrots();
    generatePowerUps();
    generateEnemies();
    rabbitX = 1;
    rabbitY = 1;
    startTime = time(NULL);
    timeRemaining = GAME_TIMEOUT;
    score += 200 * level;
    if (level % 3 == 0 && lives < 5) {
        lives++;
    }
    comboCount = 0;
    comboTimer = 0;
    particleCount = 0;
}

void initGame() {
    score = 0;
    level = 1;
    timeRemaining = GAME_TIMEOUT;
    rabbitX = 1;
    rabbitY = 1;
    hasSuperSpeed = 0;
    hasInvincibility = 0;
    hasFreeze = 0;
    hasMagnet = 0;
    hasDouble = 0;
    speedTimer = 0;
    invincibilityTimer = 0;
    freezeTimer = 0;
    magnetTimer = 0;
    doubleTimer = 0;
    gameActive = 1;
    gamePaused = 0;
    totalCarrotsCollected = 0;
    totalMoves = 0;
    totalDeaths = 0;
    comboCount = 0;
    comboTimer = 0;
    lives = 3;
    particleCount = 0;
    srand((unsigned int)time(NULL));
    generateMaze();
    generateObstacles();
    generateCarrots();
    generatePowerUps();
    generateEnemies();
    startTime = time(NULL);
}

void displayGameOver() {
    clearScreen();
    setColor(COLOR_RED);
    printf("\n\n");
    printf("     +==========================================================+\n");
    printf("     |                      GAME OVER!                          |\n");
    printf("     +==========================================================+\n");
    setColor(COLOR_RESET);
    printf("\n");
    setColor(COLOR_YELLOW);
    printf("     Final Score: %d\n", score);
    printf("     Level Reached: %d\n", level);
    printf("     Carrots Collected: %d\n", totalCarrotsCollected);
    printf("     Total Moves: %d\n", totalMoves);
    setColor(COLOR_RESET);
    if (level > MAX_LEVELS) {
        setColor(COLOR_GREEN);
        printf("\n     *** CONGRATULATIONS! You completed all levels! ***\n");
        setColor(COLOR_RESET);
    }
    
    printf("\n     Enter your name for the high score list (or press Enter to skip): ");
    char name[32] = "";
    int idx = 0;
    int ch;
    while ((ch = getch()) != '\n' && ch != '\r' && idx < 31) {
        if (ch == 127 || ch == 8) {
            if (idx > 0) {
                idx--;
                printf("\b \b");
            }
        } else if (ch >= 32 && ch < 127) {
            name[idx++] = (char)ch;
            printf("%c", ch);
        }
    }
    name[idx] = '\0';
    printf("\n");
    
    if (idx > 0) {
        addHighScore(name, score, level);
    }
}

void displayInstructions() {
    clearScreen();
    setColor(COLOR_CYAN);
    printf("\n");
    printf("     +==========================================================+\n");
    printf("     |                    HOW TO PLAY                           |\n");
    printf("     +==========================================================+\n");
    setColor(COLOR_RESET);
    
    printf("\n");
    setColor(COLOR_WHITE);
    printf("     CONTROLS:\n");
    printf("       W/A/S/D    - Move up/left/down/right\n");
    printf("       J + WASD   - Jump 2 spaces in direction\n");
    printf("       Space/P    - Pause game\n");
    printf("       H          - View high scores\n");
    printf("       Q          - Quit current game\n");
    
    printf("\n     OBJECTIVES:\n");
    printf("       * Collect all carrots (*) to advance to the next level\n");
    printf("       * Reach the goal (G) for bonus points\n");
    printf("       * Avoid enemies (E) and obstacles\n");
    printf("       * You have 3 lives - spikes and enemies cost 1 life\n");
    
    printf("\n     POWER-UPS:\n");
    printf("       S - Speed Boost: Move faster, no score penalty\n");
    printf("       D - Shield: Temporary invincibility, destroy enemies on contact\n");
    printf("       F - Freeze: Freeze all enemies for 8 seconds\n");
    printf("       M - Magnet: Attract nearby carrots\n");
    printf("       2 - Double Score: All points doubled for 12 seconds\n");
    
    printf("\n     OBSTACLES:\n");
    printf("       X - Rock: Blocks your path\n");
    printf("       ~ - Puddle: Slows you down\n");
    printf("       ^ - Spike: Damages you on contact (always harmful!)\n");
    printf("       O - Moving obstacle: Moves back and forth\n");
    
    printf("\n     SCORING:\n");
    printf("       Carrot: 25+ points (more at higher levels)\n");
    printf("       Combo: Collect carrots quickly for bonus points\n");
    printf("       Power-up: 50 points\n");
    printf("       Enemy (with shield): 100 points\n");
    printf("       Level complete: 200 x level bonus\n");
    setColor(COLOR_RESET);
    
    printf("\n     Press any key to return...\n");
    getch();
}

int mainMenu() {
    int menuActive = 1;
    int selection = 0;
    const char* menuItems[] = {
        "Start New Game",
        "View High Scores",
        "How to Play",
        "Quit Game"
    };
    int numItems = 4;
    while (menuActive) {
        clearScreen();
        setColor(COLOR_CYAN);
        printf("\n\n");
        printf("     +==========================================================+\n");
        printf("     |                                                          |\n");
        printf("     |              RABBIT MAZE ADVENTURE                       |\n");
        printf("     |                                                          |\n");
        printf("     |                  A Maze Adventure Game                   |\n");
        printf("     |                                                          |\n");
        printf("     +==========================================================+\n");
        setColor(COLOR_RESET);
        for (int i = 0; i < numItems; i++) {
            if (i == selection) {
                setColor(COLOR_YELLOW);
                printf("     |  > %-52s  |\n", menuItems[i]);
                setColor(COLOR_RESET);
            } else {
                printf("     |    %-52s  |\n", menuItems[i]);
            }
        }
        
        setColor(COLOR_CYAN);
        printf("     |                                                          |\n");
        printf("     +==========================================================+\n");
        setColor(COLOR_RESET);
        printf("\n     Use W/S or arrow keys, Enter to select\n");
        int ch = getch();
        if (ch == 224 || ch == 0) {
            ch = getch();
            if (ch == 72) {
                selection = (selection - 1 + numItems) % numItems;
            } else if (ch == 80) {
                selection = (selection + 1) % numItems;
            }
        } else if (ch == 'w' || ch == 'W') {
            selection = (selection - 1 + numItems) % numItems;
        } else if (ch == 's' || ch == 'S') {
            selection = (selection + 1) % numItems;
        } else if (ch == '\n' || ch == '\r' || ch == ' ') {
            switch (selection) {
                case 0:
                    initGame();
                    return 0;
                case 1:
                    displayHighScores();
                    break;
                case 2:
                    displayInstructions();
                    break;
                case 3:
                    printf("\n     Thanks for playing!\n\n");
                    return 1;
            }
        } else if (ch == '1') {
            initGame();
            return 0;
        } else if (ch == '2') {
            displayHighScores();
        } else if (ch == '3') {
            displayInstructions();
        } else if (ch == '4' || ch == 'q' || ch == 'Q') {
            printf("\n     Thanks for playing!\n\n");
            return 1;
        }
    }
    return 1;
}

void playGame() {
    while (gameActive) {
        displayGame();
        if (!gamePaused) {
            updateTimers();
            updateTimer();
            updateEnemies();
            updateMovingObstacles();
            applyMagnetEffect();
        }
        updateParticles();
        if (checkLevelComplete()) {
            nextLevel();
            if (!gameActive) break;
            continue;
        }
        
        if (kbhit()) {
            int input = getch();
            if (input == 224 || input == 0) {
                input = getch();
                switch (input) {
                    case 72: moveRabbit(0, -1); break;
                    case 80: moveRabbit(0, 1); break;
                    case 75: moveRabbit(-1, 0); break;
                    case 77: moveRabbit(1, 0); break;
                }
            } else {
                switch (input) {
                    case 'w': case 'W': moveRabbit(0, -1); break;
                    case 's': case 'S': moveRabbit(0, 1); break;
                    case 'a': case 'A': moveRabbit(-1, 0); break;
                    case 'd': case 'D': moveRabbit(1, 0); break;
                    case 'j': case 'J': {
                        int dir = getch();
                        switch (dir) {
                            case 'w': case 'W': jump(0, -1); break;
                            case 's': case 'S': jump(0, 1); break;
                            case 'a': case 'A': jump(-1, 0); break;
                            case 'd': case 'D': jump(1, 0); break;
                        }
                        break;
                    }
                    case ' ': gamePaused = !gamePaused; break;
                    case 'p': case 'P': gamePaused = !gamePaused; break;
                    case 'h': case 'H': displayHighScores(); break;
                    case 'q': case 'Q': 
                        gameActive = 0;
                        break;
                }
            }
        }
        
        SLEEP(50);
    }
    
    displayGameOver();
}

int main() {
    hideCursor();
    loadHighScores();
    while (1) {
        int result = mainMenu();
        if (result == 0) {
            playGame();
        } else {
            break;
        }
    }
    
    showCursor();
    return 0;
}
