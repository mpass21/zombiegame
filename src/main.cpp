#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <ctime>
#include <cstring>
#include <map>
#include <tuple>
#include <cstdlib>
#include <vector>
#include <string>

const int WIDTH = 960, HEIGHT = 640;
const int TILE_SIZE = 32;
const int MAP_WIDTH = 30;
const int MAP_HEIGHT = 20;
const int VISIBLE_WIDTH = WIDTH / TILE_SIZE; // collumns visible on the screen
const int VISIBLE_HEIGHT = HEIGHT / TILE_SIZE;  // 15 rows visible on screen
const char* GITHUB_URL = "https://github.com/mpass21/saving_sean";
const int ZOMBIE_MAX_HEALTH = 3;
const Uint32 ATTACK_COOLDOWN_MS = 250;
std::map<std::tuple<int,int>,Uint64> flames;

// Difficulty-tunable settings (defaults = medium)
float playerSpeed = 10.0f;
int zombieCount = 8;
float zombieDelay = 4.0f; // seconds between spawns
int zombieSpeed = 100;    // move delay ticks - LOWER is faster

// Custom difficulty settings, editable from the custom screen
int customPlayerSpeed = 10;
int customZombieCount = 8;
int customSpawnDelay = 4;
int customZombieMoveDelay = 100;

#if defined(__APPLE__)
const char* FONT_PATH = "/System/Library/Fonts/Supplemental/Arial.ttf";
#elif defined(_WIN32)
const char* FONT_PATH = "C:\\Windows\\Fonts\\arial.ttf";
#else
const char* FONT_PATH = "assets/font.ttf";
#endif

enum GameState { STATE_MENU, STATE_DIFFICULTY, STATE_CUSTOM, STATE_HOW_TO_PLAY, STATE_PLAYING, STATE_PAUSED, STATE_DEAD, STATE_WIN };
enum Difficulty { DIFF_EASY, DIFF_MEDIUM, DIFF_HARD, DIFF_CUSTOM };

void applyDifficulty(Difficulty d) {
    switch (d) {
        case DIFF_EASY:
            playerSpeed = 13.0f; zombieCount = 5; zombieDelay = 6.0f; zombieSpeed = 140;
            break;
        case DIFF_MEDIUM:
            playerSpeed = 10.0f; zombieCount = 8; zombieDelay = 4.0f; zombieSpeed = 100;
            break;
        case DIFF_HARD:
            playerSpeed = 8.0f; zombieCount = 12; zombieDelay = 2.5f; zombieSpeed = 70;
            break;
        case DIFF_CUSTOM:
            playerSpeed = (float)customPlayerSpeed;
            zombieCount = customZombieCount;
            zombieDelay = (float)customSpawnDelay;
            zombieSpeed = customZombieMoveDelay;
            break;
    }
}

struct Character {
    int x, y;
    int moveDelay;
    int counter;


    // Move character if enough time has passed
    void move(int targetX, int targetY) {
        if (counter >= moveDelay) {
            // Logic to move towards target
            if (x < targetX) x++;
            else if (x > targetX) x--;

            if (y < targetY) y++;
            else if (y > targetY) y--;

            counter = 0; // Reset counter after moving
        }
        counter++;
    }
};

struct Zombie : public Character {
    int health = ZOMBIE_MAX_HEALTH;

    Zombie() : Character() {}
    Zombie(int startX, int startY, int delay, int startHealth) {
        x = startX;
        y = startY;
        moveDelay = delay;
        counter = 0;
        health = startHealth;
    }
    // Move character if enough time has passed
    void move(int targetX, int targetY, int tilemap[][MAP_HEIGHT]) {
        int xChange = 0;
        int yChange = 0;

        if (counter >= moveDelay) {
            // Logic to move towards target
            if (x < targetX) xChange++;
            else if (x > targetX) xChange--;

            if (y < targetY) yChange++;
            else if (y > targetY) yChange--;

            // Making sure there is no barricade in the way and checking bounds
            int newX = x + xChange;
            int newY = y + yChange;

            if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT) {
                if (tilemap[newX][newY] != 1) { // Assuming '1' is a barricade
                    x = newX;
                    y = newY;
                }
            }

            counter = 0; // Reset counter after moving
        }
        counter++;
    }
};
//zombie holding datastructure
std::vector<Zombie> zombies;

struct Player : public Character{
    int flameDuration = 3;

    Player() : Character(), flameDuration(3) {}
    Player( int startX, int startY, int delay, int startHealth) {
        x = startX;
        y = startY;
        moveDelay = delay;
        counter = 0;
    }

    void flame(int tilemap[][MAP_HEIGHT]) {
        if (x > 2 && tilemap[x][y] ==3){
            tilemap[x-2][y] = 1;
            flames[std::make_tuple(x-2,y)] = SDL_GetTicks64();
        }
    }
    void removeFlame(int tilemap[][MAP_HEIGHT], std::tuple<int,int> coords) {
        int removeX =std::get<0>(coords);
        int removeY =std::get<1>(coords);

        tilemap[removeX][removeY] = 3;
    }


    void checkFlame(int tilemap[][MAP_HEIGHT]) {
        Uint64 currentTime = SDL_GetTicks64();

        for (auto i = flames.begin(); i != flames.end(); ) {
            if (currentTime - i->second >= 4000) {
                removeFlame(tilemap, i->first);
                i = flames.erase(i);
            } else {
                i++;
            }
        }
    }

};

void pushZombie() {

    int zombieSpawn = rand() % 49;
    Zombie z;

    if (zombieSpawn > 19) {
        z = {zombieSpawn-19, 0, zombieSpeed, ZOMBIE_MAX_HEALTH};
    } else {
        z = {0, zombieSpawn, zombieSpeed, ZOMBIE_MAX_HEALTH};
    }

    zombies.push_back(z);
}

void openURL(const std::string& url) {
#if defined(__APPLE__)
    std::string cmd = "open \"" + url + "\"";
#elif defined(_WIN32)
    std::string cmd = "start \"\" \"" + url + "\"";
#else
    std::string cmd = "xdg-open \"" + url + "\"";
#endif
    system(cmd.c_str());
}

// Renders text to a brand new texture. Caller owns the returned texture.
SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Color color, int& w, int& h) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) {
        w = 0; h = 0;
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    w = surface->w;
    h = surface->h;
    SDL_FreeSurface(surface);
    return texture;
}

struct Button {
    SDL_Rect rect;
    SDL_Texture* textTexture = nullptr;
    int textW = 0, textH = 0;
};

Button makeButton(SDL_Renderer* renderer, TTF_Font* font, const std::string& label, int x, int y, int w, int h) {
    Button b;
    b.rect = {x, y, w, h};
    b.textTexture = createTextTexture(renderer, font, label, {255, 255, 255, 255}, b.textW, b.textH);
    return b;
}

bool pointInRect(int px, int py, const SDL_Rect& r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

void drawButton(SDL_Renderer* renderer, const Button& b, bool hover) {
    SDL_SetRenderDrawColor(renderer, hover ? 80 : 45, hover ? 80 : 45, hover ? 80 : 45, 255);
    SDL_RenderFillRect(renderer, &b.rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &b.rect);
    if (b.textTexture) {
        SDL_Rect textRect = { b.rect.x + (b.rect.w - b.textW) / 2, b.rect.y + (b.rect.h - b.textH) / 2, b.textW, b.textH };
        SDL_RenderCopy(renderer, b.textTexture, nullptr, &textRect);
    }
}

// One row on the custom-difficulty screen: a label, a live value, and -/+ buttons.
struct Adjuster {
    SDL_Texture* labelTex = nullptr;
    int labelW = 0, labelH = 0;
    SDL_Texture* valueTex = nullptr;
    int valueW = 0, valueH = 0;
    Button minusBtn;
    Button plusBtn;
    int* value = nullptr;
    int minV = 0, maxV = 0, step = 1;
    int rowY = 0;
};

int main(int argc, char *argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (TTF_Init() != 0) {
        std::cout << "TTF_Init Error: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create SDL Window
    SDL_Window *window = SDL_CreateWindow("Saving Sean", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create SDL Renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Fonts
    TTF_Font* fontTitle = TTF_OpenFont(FONT_PATH, 56);
    TTF_Font* fontButton = TTF_OpenFont(FONT_PATH, 22);
    TTF_Font* fontText = TTF_OpenFont(FONT_PATH, 20);
    if (!fontTitle || !fontButton || !fontText) {
        std::cout << "TTF_OpenFont Error: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Initialize Map
    SDL_Surface* tile_map_surface = SDL_LoadBMP("./src/images/tiles.bmp");
    if (!tile_map_surface) {
        std::cout << "SDL_LoadBMP Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create textures from surfaces
    SDL_Texture* tile_texture = SDL_CreateTextureFromSurface(renderer, tile_map_surface);
    SDL_FreeSurface(tile_map_surface);

    if (!tile_texture) {
        std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Procedural data structure for the tile map
    int tilemap[MAP_WIDTH][MAP_HEIGHT];

    // Populating the screen with tiles
    SDL_Rect tile[VISIBLE_WIDTH][VISIBLE_HEIGHT];

    for (int x = 0; x < VISIBLE_WIDTH; x++) {
        for (int y = 0; y < VISIBLE_HEIGHT; y++) {
            tile[x][y].x = x * TILE_SIZE;
            tile[x][y].y = y * TILE_SIZE;
            tile[x][y].w = TILE_SIZE;
            tile[x][y].h = TILE_SIZE;
        }
    }

    // The tiles below are the tiles to select from
    SDL_Rect select_tile_1 = {0, 0, TILE_SIZE, TILE_SIZE};
    SDL_Rect select_tile_2 = {TILE_SIZE, 0, TILE_SIZE, TILE_SIZE};
    SDL_Rect select_tile_3 = {0, TILE_SIZE, TILE_SIZE, TILE_SIZE};
    SDL_Rect select_tile_4 = {TILE_SIZE, TILE_SIZE, TILE_SIZE, TILE_SIZE};

    // Pointer to the keys
    const Uint8* pkeys = SDL_GetKeyboardState(NULL);

    //initial palyer position
    float precisePlayerX = VISIBLE_WIDTH / 2;
    float precisePlayerY = VISIBLE_HEIGHT / 2;
    bool playerMoved = false;

    // Time related variables
    Uint32 lastTime = SDL_GetTicks64();
    float deltaTime = 0;

    Uint32 zLastTime = SDL_GetTicks64();
    float zDeltaTime = 0;

    Uint32 lastAttackTime = 0;
    Uint32 pausedAt = 0;

    // Creating characters
    Player sean = {10, VISIBLE_HEIGHT - 2, 500, 0};
    Player player = { VISIBLE_WIDTH/2, VISIBLE_HEIGHT/2, 0, 0 };

    bool gameWon = false;

    // ---- Menu / overlay setup ----
    GameState state = STATE_MENU;

    int titleW, titleH;
    SDL_Texture* titleTexture = createTextTexture(renderer, fontTitle, "SAVING SEAN", {255, 255, 255, 255}, titleW, titleH);

    int deadTitleW, deadTitleH;
    SDL_Texture* deadTitleTexture = createTextTexture(renderer, fontTitle, "SEAN DIED", {220, 40, 40, 255}, deadTitleW, deadTitleH);

    int winTitleW, winTitleH;
    SDL_Texture* winTitleTexture = createTextTexture(renderer, fontTitle, "YOU WIN", {60, 220, 90, 255}, winTitleW, winTitleH);

    int pausedTitleW, pausedTitleH;
    SDL_Texture* pausedTitleTexture = createTextTexture(renderer, fontTitle, "PAUSED", {255, 255, 255, 255}, pausedTitleW, pausedTitleH);

    const int BTN_W = 240, BTN_H = 50;
    Button playBtn   = makeButton(renderer, fontButton, "PLAY",         WIDTH/2 - BTN_W/2, 230, BTN_W, BTN_H);
    Button howToBtn  = makeButton(renderer, fontButton, "HOW TO PLAY",  WIDTH/2 - BTN_W/2, 300, BTN_W, BTN_H);
    Button githubBtn = makeButton(renderer, fontButton, "GITHUB",       WIDTH/2 - BTN_W/2, 370, BTN_W, BTN_H);
    Button exitBtn   = makeButton(renderer, fontButton, "EXIT",         WIDTH/2 - BTN_W/2, 440, BTN_W, BTN_H);

    Button backBtn      = makeButton(renderer, fontButton, "BACK",             WIDTH/2 - BTN_W/2, HEIGHT - 100, BTN_W, BTN_H);
    Button restartBtn   = makeButton(renderer, fontButton, "RESTART",          WIDTH/2 - BTN_W/2, 300, BTN_W, BTN_H);
    Button menuBtn      = makeButton(renderer, fontButton, "RETURN TO MENU",   WIDTH/2 - BTN_W/2, 370, BTN_W, BTN_H);
    Button quitBtn      = makeButton(renderer, fontButton, "QUIT",             WIDTH/2 - BTN_W/2, 440, BTN_W, BTN_H);
    Button resumeBtn     = makeButton(renderer, fontButton, "RESUME",           WIDTH/2 - BTN_W/2, 300, BTN_W, BTN_H);

    Button easyBtn      = makeButton(renderer, fontButton, "EASY",    WIDTH/2 - BTN_W/2, 200, BTN_W, BTN_H);
    Button mediumBtn    = makeButton(renderer, fontButton, "MEDIUM",  WIDTH/2 - BTN_W/2, 270, BTN_W, BTN_H);
    Button hardBtn      = makeButton(renderer, fontButton, "HARD",    WIDTH/2 - BTN_W/2, 340, BTN_W, BTN_H);
    Button customBtn    = makeButton(renderer, fontButton, "CUSTOM",  WIDTH/2 - BTN_W/2, 410, BTN_W, BTN_H);
    Button diffBackBtn  = makeButton(renderer, fontButton, "BACK",    WIDTH/2 - BTN_W/2, 480, BTN_W, BTN_H);

    Button startCustomBtn = makeButton(renderer, fontButton, "START GAME", WIDTH/2 - BTN_W/2, 500, BTN_W, BTN_H);
    Button customBackBtn  = makeButton(renderer, fontButton, "BACK",       WIDTH/2 - BTN_W/2, 565, BTN_W, BTN_H);

    // Custom difficulty adjusters
    std::vector<Adjuster> adjusters;
    {
        std::vector<std::string> labels = { "PLAYER SPEED", "ZOMBIE COUNT", "SPAWN DELAY (s)", "ZOMBIE SPEED (move delay)" };
        int mins[4]  = { 5,  1, 1,  20 };
        int maxs[4]  = { 25, 20, 10, 200 };
        int steps[4] = { 1,  1, 1,  10 };
        int* ptrs[4] = { &customPlayerSpeed, &customZombieCount, &customSpawnDelay, &customZombieMoveDelay };

        for (int i = 0; i < 4; i++) {
            Adjuster a;
            a.rowY = 160 + i * 80;
            a.value = ptrs[i];
            a.minV = mins[i];
            a.maxV = maxs[i];
            a.step = steps[i];
            a.labelTex = createTextTexture(renderer, fontText, labels[i], {230, 230, 230, 255}, a.labelW, a.labelH);
            a.minusBtn = makeButton(renderer, fontButton, "-", 600, a.rowY - 6, 40, 40);
            a.plusBtn = makeButton(renderer, fontButton, "+", 700, a.rowY - 6, 40, 40);
            adjusters.push_back(a);
        }
    }

    auto refreshAdjusterValue = [&](Adjuster& a) {
        if (a.valueTex) SDL_DestroyTexture(a.valueTex);
        a.valueTex = createTextTexture(renderer, fontButton, std::to_string(*a.value), {255, 255, 255, 255}, a.valueW, a.valueH);
    };
    for (auto& a : adjusters) refreshAdjusterValue(a);

    std::vector<std::string> instructionLines = {
        "ARROW KEYS - move your character around the map",
        "Moving for the first time sends Sean walking to the right on his own",
        "Get Sean to the checkered flag on the right edge to win",
        "Zombies spawn in and chase Sean once you start moving",
        "SPACEBAR - drop a flame wall two tiles to your left (on grass)",
        "Use flame walls to block zombies from reaching Sean",
        "Walk up next to a zombie to attack it - a few hits will kill it",
        "Pick EASY, MEDIUM, HARD or build your own CUSTOM difficulty",
        "ESC - pause the game at any time",
        "If a zombie touches Sean, it's game over"
    };
    std::vector<SDL_Texture*> instructionTextures;
    std::vector<int> instructionW, instructionH;
    for (const auto& line : instructionLines) {
        int w, h;
        SDL_Texture* tex = createTextTexture(renderer, fontText, line, {230, 230, 230, 255}, w, h);
        instructionTextures.push_back(tex);
        instructionW.push_back(w);
        instructionH.push_back(h);
    }

    auto resetGame = [&]() {
        zombies.clear();
        flames.clear();
        for (int x = 0; x < MAP_WIDTH; x++)
            for (int y = 0; y < MAP_HEIGHT; y++)
                tilemap[x][y] = 3;

        precisePlayerX = VISIBLE_WIDTH / 2;
        precisePlayerY = VISIBLE_HEIGHT / 2;
        playerMoved = false;
        gameWon = false;

        sean = {10, VISIBLE_HEIGHT - 2, 500, 0};
        player = { VISIBLE_WIDTH/2, VISIBLE_HEIGHT/2, 0, 0 };

        lastTime = SDL_GetTicks64();
        zLastTime = SDL_GetTicks64();
        lastAttackTime = SDL_GetTicks64();
    };
    resetGame();

    // Shifts the timers forward by however long we were paused, so nothing
    // thinks a huge chunk of time just passed (zombie floods, teleports, etc).
    auto resumeGame = [&]() {
        Uint32 pausedDuration = SDL_GetTicks64() - pausedAt;
        lastTime += pausedDuration;
        zLastTime += pausedDuration;
        lastAttackTime += pausedDuration;
        state = STATE_PLAYING;
    };

    // Infinite loop running the game
    bool running = true;
    SDL_Event windowEvent;

    // Main game loop
    while (running) {
        while (SDL_PollEvent(&windowEvent)) {
            if (windowEvent.type == SDL_QUIT) {
                running = false;
            }

            if (windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                if (state == STATE_PLAYING) {
                    state = STATE_PAUSED;
                    pausedAt = SDL_GetTicks64();
                } else if (state == STATE_PAUSED) {
                    resumeGame();
                }
            }

            if (windowEvent.type == SDL_MOUSEBUTTONDOWN && windowEvent.button.button == SDL_BUTTON_LEFT) {
                int mx = windowEvent.button.x, my = windowEvent.button.y;

                if (state == STATE_MENU) {
                    if (pointInRect(mx, my, playBtn.rect)) { state = STATE_DIFFICULTY; }
                    else if (pointInRect(mx, my, howToBtn.rect)) { state = STATE_HOW_TO_PLAY; }
                    else if (pointInRect(mx, my, githubBtn.rect)) { openURL(GITHUB_URL); }
                    else if (pointInRect(mx, my, exitBtn.rect)) { running = false; }
                } else if (state == STATE_DIFFICULTY) {
                    if (pointInRect(mx, my, easyBtn.rect)) { applyDifficulty(DIFF_EASY); resetGame(); state = STATE_PLAYING; }
                    else if (pointInRect(mx, my, mediumBtn.rect)) { applyDifficulty(DIFF_MEDIUM); resetGame(); state = STATE_PLAYING; }
                    else if (pointInRect(mx, my, hardBtn.rect)) { applyDifficulty(DIFF_HARD); resetGame(); state = STATE_PLAYING; }
                    else if (pointInRect(mx, my, customBtn.rect)) { state = STATE_CUSTOM; }
                    else if (pointInRect(mx, my, diffBackBtn.rect)) { state = STATE_MENU; }
                } else if (state == STATE_CUSTOM) {
                    for (auto& a : adjusters) {
                        if (pointInRect(mx, my, a.minusBtn.rect)) {
                            *a.value -= a.step;
                            if (*a.value < a.minV) *a.value = a.minV;
                            refreshAdjusterValue(a);
                        } else if (pointInRect(mx, my, a.plusBtn.rect)) {
                            *a.value += a.step;
                            if (*a.value > a.maxV) *a.value = a.maxV;
                            refreshAdjusterValue(a);
                        }
                    }
                    if (pointInRect(mx, my, startCustomBtn.rect)) { applyDifficulty(DIFF_CUSTOM); resetGame(); state = STATE_PLAYING; }
                    else if (pointInRect(mx, my, customBackBtn.rect)) { state = STATE_DIFFICULTY; }
                } else if (state == STATE_HOW_TO_PLAY) {
                    if (pointInRect(mx, my, backBtn.rect)) { state = STATE_MENU; }
                } else if (state == STATE_DEAD || state == STATE_WIN) {
                    if (pointInRect(mx, my, restartBtn.rect)) { resetGame(); state = STATE_PLAYING; }
                    else if (pointInRect(mx, my, menuBtn.rect)) { state = STATE_MENU; }
                    else if (pointInRect(mx, my, quitBtn.rect)) { running = false; }
                } else if (state == STATE_PAUSED) {
                    if (pointInRect(mx, my, resumeBtn.rect)) { resumeGame(); }
                    else if (pointInRect(mx, my, menuBtn.rect)) { state = STATE_MENU; }
                    else if (pointInRect(mx, my, quitBtn.rect)) { running = false; }
                }
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state == STATE_PLAYING || state == STATE_PAUSED) {
          if (state == STATE_PLAYING) {
            // Checking to see if sean reached the end of the map
            if (sean.x == VISIBLE_WIDTH - 1 && !gameWon) {
                gameWon = true;
                state = STATE_WIN;
            }

            Uint32 zCurrentTime = SDL_GetTicks64();
            zDeltaTime = (zCurrentTime - zLastTime) / 1000.0f;


            // Handling Zombie generation
            if (((int)zombies.size() < zombieCount) && (zDeltaTime >= zombieDelay) && playerMoved) {
                pushZombie();
                zLastTime = zCurrentTime;
            }
            // Move bot to the right if player has moved
            if (playerMoved) {

                sean.move(VISIBLE_WIDTH-1, VISIBLE_HEIGHT/2);

                //Moving the zombies
                for (auto it =zombies.begin(); it != zombies.end();) {
                    it->move(sean.x,sean.y, tilemap);
                    it++;
                }

            }

            // Calculating the time
            Uint32 currentTime = SDL_GetTicks64();
            deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;

            // Checking the map to see if flames need to be removed
            player.checkFlame(tilemap);

            if (pkeys[SDL_SCANCODE_SPACE]) {
                player.flame(tilemap);
            }

            if (pkeys[SDL_SCANCODE_UP]) {
                playerMoved = true;
                precisePlayerY -= playerSpeed * deltaTime;
            }

            if (pkeys[SDL_SCANCODE_DOWN]) {
                playerMoved = true;
                precisePlayerY += playerSpeed * deltaTime;

            }

            if (pkeys[SDL_SCANCODE_RIGHT]) {
                playerMoved = true;
                precisePlayerX += playerSpeed * deltaTime;

            }

            if (pkeys[SDL_SCANCODE_LEFT]) {
                playerMoved = true;
                precisePlayerX -= playerSpeed * deltaTime;
            }

            player.x = static_cast<int>(precisePlayerX);
            player.y = static_cast<int>(precisePlayerY);

            // keeping player in visible area
            if (player.x < 0) {
                player.x = 0;
            } else if (player.x >= VISIBLE_WIDTH-1) {
                player.x = VISIBLE_WIDTH - 1;
            }

            if (player.y < 0) {
                player.y = 0;
            } else if (player.y >= VISIBLE_HEIGHT) {
                player.y = VISIBLE_HEIGHT - 1;
            }

            // Player attacks a nearby/overlapping zombie, gated by a cooldown
            if (currentTime - lastAttackTime >= ATTACK_COOLDOWN_MS) {
                for (auto it = zombies.begin(); it != zombies.end(); ++it) {
                    int dx = std::abs(it->x - player.x);
                    int dy = std::abs(it->y - player.y);
                    if (dx <= 1 && dy <= 1) {
                        it->health -= 1;
                        lastAttackTime = currentTime;
                        if (it->health <= 0) {
                            zombies.erase(it);
                        }
                        break;
                    }
                }
            }
          }

            // Render the tiles based on the offset and tilemap
            for (int x = 0; x < VISIBLE_WIDTH; x++) {
                for (int y = 0; y < VISIBLE_HEIGHT; y++) {
                    if (x == VISIBLE_WIDTH - 1) {
                        // Checkered flag finish line
                        bool white = (y % 2 == 0);
                        if (white) SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                        else SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderFillRect(renderer, &tile[x][y]);
                        continue;
                    }
                    switch (tilemap[x][y]) { // Adjust y-coordinate with offSetY
                        case 1:
                            SDL_RenderCopy(renderer, tile_texture, &select_tile_1, &tile[x][y]);
                            break;
                        case 2:
                            SDL_RenderCopy(renderer, tile_texture, &select_tile_2, &tile[x][y]);
                            break;
                        case 3:
                            SDL_RenderCopy(renderer, tile_texture, &select_tile_3, &tile[x][y]);
                            break;
                        case 4:
                            SDL_RenderCopy(renderer, tile_texture, &select_tile_4, &tile[x][y]);
                            break;
                    }
                }
            }

            if (!gameWon) {
                // Draw the player as a green square
                SDL_Rect playerRect = {player.x * TILE_SIZE, player.y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red color for player
                SDL_RenderFillRect(renderer, &playerRect);



                // Drawing zombies (color fades as they take damage) and checking to see if they have reached sean
                for (auto it =zombies.begin(); it != zombies.end();) {
                    if (it->x == sean.x && it->y == sean.y) state = STATE_DEAD;

                    Uint8 g = (Uint8)(85 * it->health);
                    Uint8 r = (Uint8)(255 - g);
                    SDL_Rect zombieRect = {it->x * TILE_SIZE, it->y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                    SDL_SetRenderDrawColor(renderer, r, g, 0, 255);
                    SDL_RenderFillRect(renderer, &zombieRect);
                    it++;
                }

                // Draw sean
                SDL_Rect seanRect = {sean.x * TILE_SIZE, sean.y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Red color for bot
                SDL_RenderFillRect(renderer, &seanRect);
            }

            if (state == STATE_PAUSED) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
                SDL_Rect overlay = {0, 0, WIDTH, HEIGHT};
                SDL_RenderFillRect(renderer, &overlay);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

                SDL_Rect pausedTitleRect = { WIDTH/2 - pausedTitleW/2, 130, pausedTitleW, pausedTitleH };
                SDL_RenderCopy(renderer, pausedTitleTexture, nullptr, &pausedTitleRect);

                int mx, my;
                SDL_GetMouseState(&mx, &my);
                drawButton(renderer, resumeBtn, pointInRect(mx, my, resumeBtn.rect));
                drawButton(renderer, menuBtn, pointInRect(mx, my, menuBtn.rect));
                drawButton(renderer, quitBtn, pointInRect(mx, my, quitBtn.rect));
            }
        } else if (state == STATE_MENU) {
            SDL_Rect titleRect = { WIDTH/2 - titleW/2, 100, titleW, titleH };
            SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            drawButton(renderer, playBtn, pointInRect(mx, my, playBtn.rect));
            drawButton(renderer, howToBtn, pointInRect(mx, my, howToBtn.rect));
            drawButton(renderer, githubBtn, pointInRect(mx, my, githubBtn.rect));
            drawButton(renderer, exitBtn, pointInRect(mx, my, exitBtn.rect));
        } else if (state == STATE_DIFFICULTY) {
            int tW, tH;
            SDL_Texture* diffTitle = createTextTexture(renderer, fontTitle, "SELECT DIFFICULTY", {255, 255, 255, 255}, tW, tH);
            SDL_Rect diffTitleRect = { WIDTH/2 - tW/2, 90, tW, tH };
            SDL_RenderCopy(renderer, diffTitle, nullptr, &diffTitleRect);
            SDL_DestroyTexture(diffTitle);

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            drawButton(renderer, easyBtn, pointInRect(mx, my, easyBtn.rect));
            drawButton(renderer, mediumBtn, pointInRect(mx, my, mediumBtn.rect));
            drawButton(renderer, hardBtn, pointInRect(mx, my, hardBtn.rect));
            drawButton(renderer, customBtn, pointInRect(mx, my, customBtn.rect));
            drawButton(renderer, diffBackBtn, pointInRect(mx, my, diffBackBtn.rect));
        } else if (state == STATE_CUSTOM) {
            int tW, tH;
            SDL_Texture* customTitle = createTextTexture(renderer, fontTitle, "CUSTOM DIFFICULTY", {255, 255, 255, 255}, tW, tH);
            SDL_Rect customTitleRect = { WIDTH/2 - tW/2, 40, tW, tH };
            SDL_RenderCopy(renderer, customTitle, nullptr, &customTitleRect);
            SDL_DestroyTexture(customTitle);

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            for (auto& a : adjusters) {
                int rowCenterY = a.rowY + 14;
                SDL_Rect labelRect = { 90, rowCenterY - a.labelH/2, a.labelW, a.labelH };
                SDL_RenderCopy(renderer, a.labelTex, nullptr, &labelRect);

                drawButton(renderer, a.minusBtn, pointInRect(mx, my, a.minusBtn.rect));
                drawButton(renderer, a.plusBtn, pointInRect(mx, my, a.plusBtn.rect));

                SDL_Rect valueRect = { 670 - a.valueW/2, rowCenterY - a.valueH/2, a.valueW, a.valueH };
                SDL_RenderCopy(renderer, a.valueTex, nullptr, &valueRect);
            }

            drawButton(renderer, startCustomBtn, pointInRect(mx, my, startCustomBtn.rect));
            drawButton(renderer, customBackBtn, pointInRect(mx, my, customBackBtn.rect));
        } else if (state == STATE_HOW_TO_PLAY) {
            int titleW2, titleH2;
            SDL_Texture* howTitle = createTextTexture(renderer, fontTitle, "HOW TO PLAY", {255, 255, 255, 255}, titleW2, titleH2);
            SDL_Rect howTitleRect = { WIDTH/2 - titleW2/2, 40, titleW2, titleH2 };
            SDL_RenderCopy(renderer, howTitle, nullptr, &howTitleRect);
            SDL_DestroyTexture(howTitle);

            int lineY = 130;
            for (size_t i = 0; i < instructionTextures.size(); i++) {
                SDL_Rect lineRect = { WIDTH/2 - instructionW[i]/2, lineY, instructionW[i], instructionH[i] };
                SDL_RenderCopy(renderer, instructionTextures[i], nullptr, &lineRect);
                lineY += instructionH[i] + 12;
            }

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            drawButton(renderer, backBtn, pointInRect(mx, my, backBtn.rect));
        } else if (state == STATE_DEAD) {
            SDL_Rect deadTitleRect = { WIDTH/2 - deadTitleW/2, 130, deadTitleW, deadTitleH };
            SDL_RenderCopy(renderer, deadTitleTexture, nullptr, &deadTitleRect);

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            drawButton(renderer, restartBtn, pointInRect(mx, my, restartBtn.rect));
            drawButton(renderer, menuBtn, pointInRect(mx, my, menuBtn.rect));
            drawButton(renderer, quitBtn, pointInRect(mx, my, quitBtn.rect));
        } else if (state == STATE_WIN) {
            SDL_Rect winTitleRect = { WIDTH/2 - winTitleW/2, 130, winTitleW, winTitleH };
            SDL_RenderCopy(renderer, winTitleTexture, nullptr, &winTitleRect);

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            drawButton(renderer, restartBtn, pointInRect(mx, my, restartBtn.rect));
            drawButton(renderer, menuBtn, pointInRect(mx, my, menuBtn.rect));
            drawButton(renderer, quitBtn, pointInRect(mx, my, quitBtn.rect));
        }

        // Present the drawn content
        SDL_RenderPresent(renderer);
    }

    // Clean up
    for (auto tex : instructionTextures) SDL_DestroyTexture(tex);
    for (auto& a : adjusters) {
        SDL_DestroyTexture(a.labelTex);
        SDL_DestroyTexture(a.valueTex);
        SDL_DestroyTexture(a.minusBtn.textTexture);
        SDL_DestroyTexture(a.plusBtn.textTexture);
    }
    SDL_DestroyTexture(titleTexture);
    SDL_DestroyTexture(deadTitleTexture);
    SDL_DestroyTexture(winTitleTexture);
    SDL_DestroyTexture(pausedTitleTexture);
    SDL_DestroyTexture(playBtn.textTexture);
    SDL_DestroyTexture(howToBtn.textTexture);
    SDL_DestroyTexture(githubBtn.textTexture);
    SDL_DestroyTexture(exitBtn.textTexture);
    SDL_DestroyTexture(backBtn.textTexture);
    SDL_DestroyTexture(restartBtn.textTexture);
    SDL_DestroyTexture(menuBtn.textTexture);
    SDL_DestroyTexture(quitBtn.textTexture);
    SDL_DestroyTexture(resumeBtn.textTexture);
    SDL_DestroyTexture(easyBtn.textTexture);
    SDL_DestroyTexture(mediumBtn.textTexture);
    SDL_DestroyTexture(hardBtn.textTexture);
    SDL_DestroyTexture(customBtn.textTexture);
    SDL_DestroyTexture(diffBackBtn.textTexture);
    SDL_DestroyTexture(startCustomBtn.textTexture);
    SDL_DestroyTexture(customBackBtn.textTexture);

    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontButton);
    TTF_CloseFont(fontText);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(tile_texture);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
