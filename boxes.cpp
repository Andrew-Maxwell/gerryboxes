#include <raylib.h>
#include <list>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#define NUMCOLORS 8
#define WIDTH 800
#define SIDEBAR 200
#define BOARDWIDTH (WIDTH - SIDEBAR)
#define HEIGHT 600
#define MINGRID 16
#define DEFAULTROWS 24
#define DEFAULTCOLS 24
#define DEFAULTSYMBOLS 4
#define GAMENAME "GerryBoxes"
#define BACKGROUND WHITE
#define FOREGROUND BLACK
//#define BACKGROUND (Color){0xd0, 0xd0, 0xd0, 0xff}
//#define FOREGROUND (Color){0x00, 0x11, 0x08, 0xff}
#define HIGHLIGHT (Color){0, 0, 0, 127}
#define MAXSYMBOLS 16
#define BUTTONHEIGHT 24
#define BUTTONMARGIN 8

using namespace std;

struct V2 {
    int x, y;

    V2(int newX, int newY) : x(newX), y(newY) {}
    V2() {x = y = 0;}

    bool operator==(const V2& other) {
        return x == other.x && y == other.y;
    }

    bool operator!=(const V2& other) {
        return x != other.x || y != other.y;
    }

    V2 operator+(const V2& other) {
        return V2(x + other.x, y + other.y);
    }

    V2 operator-(const V2& other) {
        return V2(x - other.x, y - other.y);
    }
};

struct box {
    V2 pos;
    V2 dim;
    V2 opp; //location of corner opposite pos
    char color = 0;
    bool path[MAXSYMBOLS] = {false};
    set<box*> children;
    int distance;

    box(V2 newPos, V2 newDim, char newColor) : pos(newPos), dim(newDim), color(newColor) {
        opp = pos + dim - V2(1, 1);
    }

    void write(vector<vector<char>>& colors) {
        if (dim == V2(1, 1)) {
            colors[pos.y][pos.x] = color;
        }
        else {
            for (box* b : children) {
                b -> write(colors);
            }
        }
    }

    void deleteChildren() {
        for (box* b : children) {
            b -> deleteChildren();
            delete b;
        }
    }
};

struct symbol {
    unsigned char c;
    unsigned char color;
    V2 start, end;
};

class boardType {

    char colorSelect = 0;
    V2* symbolToChange = NULL;

    vector<Color> boxColors =
    {
        {24, 24, 24, 0xff},
        {0xd4, 0x19, 0x29, 0xff},
        {0xe3, 0x62, 0x27, 0xff},
        {0xfa, 0xb2, 0x23, 0xff},
        {0x1d, 0x47, 0x2f, 0xff},
        {0x40, 0x32, 0x70, 0xff},
        {0x8A, 0x1C, 0x5F, 0xff},
        {0xd0, 0xd0, 0xd0, 0xd0}
    };

    vector<symbol> symbols;
    vector<vector<box*>> boxes;
    int rows = 0, cols = 0, numSymbols = 0;
    string caption;
    int grid, space;
    V2 p1, p2, low, high;

    public:

    void init(int newRows, int newCols, int newNumSymbols, string newCaption) {
        rows = newRows;
        cols = newCols;
        numSymbols = newNumSymbols;
        caption = newCaption;
        boxes = vector<vector<box*>>(rows, vector<box*>(cols, NULL));
        symbols = vector<symbol>(numSymbols, symbol());
        for (int i = 0; i < symbols.size(); i++) {
            symbols[i].c = i;
        }
        space = min(BOARDWIDTH / (cols * 8 + 1), HEIGHT / (rows * 8 + 1));
        grid = 8 * space;
    }

    void generate(int newRows, int newCols, int newNumSymbols, string newCaption) {
        init(newRows, newCols, newNumSymbols, newCaption);
        //level generation
        for (int x = 0; x < cols; x++) {
            for (int y = 0; y < rows; y++) {
                put(x, y, new box(V2(x, y), V2(1, 1), rand() % (NUMCOLORS - 1) + 1));
            }
        }
        for (symbol& s : symbols) {
            for (V2* v : {&s.start, &s.end}) {
                bool ok = false;
                while (!ok) {
                    ok = true;
                    *v = V2(rand() % cols, rand() % rows);
                    for (int k = 0; k < numSymbols; k++) {
                        if (at(*v) -> path[k]) {
                            ok = false;
                            break;
                        }
                    }
                }
                at(*v) -> path[s.c] = true;
            }
            s.color = at(s.end) -> color = at(s.start) -> color = rand() % (NUMCOLORS - 2) + 1;
        }
        updatePath();
    }

    //Read from file constructor
    void read(string fileName) {
        ifstream in;
        in.open("resources/" + fileName);
        if (!in) {
            cerr << "Could not open level file " << fileName << endl;
            exit(EXIT_FAILURE);
        }
        in >> rows >> cols >> numSymbols;
        getline(in, caption);
        init(rows, cols, numSymbols, caption);
        char c;
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                in >> c;
                put(x, y, new box(V2(x, y), V2(1, 1), c - '0'));
            }
        }
        for (symbol& s : symbols) {
            in >> c >> s.start.x >> s.start.y >> s.end.x >> s.end.y;
            s.c = c - 'A';
            s.color = at(s.start) -> color;
        }
        updatePath();
    }

    ~boardType() {
        set<box*> toDelete;
        for (int x = 0; x < cols; x++) {
            for (int y = 0; y < rows; y++) {
                toDelete.insert(at(x, y));
            }
        }
        toDelete.erase(NULL);
        for (box* b : toDelete) {
            b -> deleteChildren();
            delete b;
        }
    }

    //Accessors allow access with V2 object, and avoid confusion with boxes[y][x] vs boxes[x][y]
    box* at(V2 v) {
        return boxes[v.y][v.x];
    }

    box* at(int x, int y) {
        return boxes[y][x];
    }

    void put(V2 v, box* b) {
        boxes[v.y][v.x] = b;
    }

    void put(int x, int y, box* b) {
        boxes[y][x] = b;
    }

    V2 mouse() {
        V2 toReturn;
        toReturn.x = max(0, min(cols - 1, (int)GetMousePosition().x / grid));
        toReturn.y = max(0, min(rows - 1, (int)GetMousePosition().y / grid));
        return toReturn;
    }

    set<box*> adj(box* b) {
        set<box*> toReturn;
        if (b -> pos.y > 0) {
            for (int x = b -> pos.x; x <= b -> opp.x; x++) {
                toReturn.insert(at(x, b -> pos.y - 1));
            }
        }
        if (b -> opp.y < rows - 1) {
            for (int x = b -> pos.x; x <= b -> opp.x; x++) {
                toReturn.insert(at(x, b -> opp.y + 1));
            }
        }
        if (b -> pos.x > 0) {
            for (int y = b -> pos.y; y <= b -> opp.y; y++) {
                toReturn.insert(at(b -> pos.x - 1, y));
            }
        }
        if (b -> opp.x < cols - 1) {
            for (int y = b -> pos.y; y <= b -> opp.y; y++) {
                toReturn.insert(at(b -> opp.x + 1, y));
            }
        }
        return toReturn;
    }

    void visit(box* thisBox, set<box*>& visitedBoxes) {
        set<box*> adjBoxes = adj(thisBox);
        for (box* otherBox : adjBoxes) {
            if (visitedBoxes.count(otherBox) == 0 && thisBox -> color == otherBox -> color) {
                for (int k = 0; k < symbols.size(); k++) {
                    otherBox -> path[k] = thisBox -> path[k];
                }
                visitedBoxes.insert(otherBox);
                visit(otherBox, visitedBoxes);
            }
        }
    }

    //Check win conditions / propogate path between symbols
    //Return true if won
    bool updatePath() {
        for (int x = 0; x < cols; x++) {
            for (int y = 0; y < rows; y++) {
                for (int k = 0; k < symbols.size(); k++) {
                    at(x, y) -> path[k] = false;
                }
            }
        }
        bool won = true;
        for (symbol& s : symbols) {
            if (at(s.start) -> color == s.color) {
                at(s.start) -> path[s.c] = true;
                set<box*> visitedBoxes;
                visitedBoxes.insert(at(s.start));
                visit(at(s.start), visitedBoxes);
            }
            if (!at(s.end) -> path[s.c]) {
                won = false;
            }
            if (at(s.end) -> color == s.color) {
                at(s.end) -> path[s.c] = true;
                set<box*> visitedBoxes;
                visitedBoxes.insert(at(s.end));
                visit(at(s.end), visitedBoxes);
            }
        }
        return won;
    }

    char resultColor(V2 low, V2 high) {
        int colorCounts[NUMCOLORS] = {};
        for (int x = low.x; x <= high.x; x++) {
            for (int y = low.y; y <= high.y; y++) {
                if (at(x, y) -> color == 0) {
                    return 0;   //If there is a black tile, the result is black
                }
                else if (at(x, y) -> color == NUMCOLORS - 1) {
                    colorCounts[NUMCOLORS - 1] = 1; //Result is only white if no other colors present.
                }
                else {
                    colorCounts[at(x, y) -> color]++;
                }
            }
        }
        int maxColor = 0;
        for (int i = 0; i < NUMCOLORS; i++) {
            if (colorCounts[i] > colorCounts[maxColor]) {
                maxColor = i;
            }
        }
        return maxColor;
    }

    bool combine(V2 low, V2 high) {
        bool ok = true;
        //Verify that there is more than one box in the selection
        if (at(low) -> pos == at(high) -> pos) {
            ok = false;
        }
        //Verify that boxes are the same size and fit inside selection
        for (int x = low.x; x <= high.x && ok; x++) {
            for (int y = low.y; y <= high.y && ok; y++) {
                if (at(x, y) -> dim != at(low) -> dim ||
                    at(x, y) -> pos.x < low.x || at(x, y) -> pos.y < low.y ||
                    at(x, y) -> opp.y > high.y || at(x, y) -> opp.x  > high.x) {
                    ok = false;
                    break;
                }
            }
        }
        if (ok) {
            box* newBox = new box(low, high - low + V2(1, 1), resultColor(low, high));
            for (int x = low.x; x <= high.x; x++) {
                for (int y = low.y; y <= high.y; y++) {
                    newBox -> children.insert(at(x, y));
                    put(x, y, newBox);
                }
            }
        }
        return ok;
    }

    void split(V2 toSplit) {
        box* b = at(toSplit);
        if (b -> children.size() > 0) {
            for (box* child : b -> children) {
                for (int x = child -> pos.x; x <= child -> opp.x; x++) {
                    for (int y = child -> pos.y; y <= child -> opp.y; y++) {
                        put(x, y, child);
                    }
                }
            }
            delete b;
        }
    }

#if not defined(PLATFORM_WEB)
    void write() {
        ofstream out;
        out.open("generated_level", ofstream::trunc);
        if (!out) {
            cout << "Error: level file not opened for write.\n";
        }
        else {
            out << rows << " " << cols << " " << numSymbols << " " << "caption" << endl;
            vector<vector<char>> colors(rows, vector<char>(cols, ' '));
            for (int y = 0; y < rows; y++) {
                for (int x = 0; x < cols; x++) {
                    at(x, y) -> write(colors);
                }
            }
            for (int y = 0; y < rows; y++) {
                for (int x = 0; x < cols; x++) {
                    out << char('0' + colors[y][x]) << " ";
                }
                out << endl;
            }
            for (symbol& s : symbols) {
                out << char('A' + s.c) << " " << s.start.x << " " << s.start.y
                    << " " << s.end.x << " " << s.end.y << endl;
            }
        }
    }
#endif

    void draw() {
        set<box*> drawn;
        for (int x = 0; x < cols; x++) {
            for (int y = 0; y < rows; y++) {
                //Draw boxes
                box* b = at(x, y);
                if (!drawn.count(b)) {
                    drawn.insert(b);
                    //Draw black background (border) for boxes which are connected to a symbol
                    for (int k = 0; k < symbols.size(); k++) {
                        if (b -> path[k]) {
                            DrawRectangle(b -> pos.x * grid, b -> pos.y * grid,
                                          b -> dim.x * grid + space, b -> dim.y * grid + space,
                                          FOREGROUND);
                            break;
                        }
                    }
                    DrawRectangle(b -> pos.x * grid + space, b -> pos.y * grid + space,
                                  b -> dim.x * grid - space, b -> dim.y * grid - space,
                                  boxColors[b -> color]);
                }
            }
        }
        //Draw connectors/borders between adjacent boxes of same color
        for (int x = 0; x < cols; x++) {
            for (int y = 0; y < rows; y++) {
                box* b = at(x, y);
                box* bx = (x == cols - 1) ? NULL : at(x + 1, y);
                box* by = (y == rows - 1) ? NULL : at(x, y + 1);
                if (b != bx && bx != NULL) {
                    if (b -> color == bx -> color) {
                        DrawRectangle((x + 1) * grid, y * grid + 3 * space,
                                      space, grid - 5 * space, boxColors[b -> color]);
                    }
                }
                if (b != by && by != NULL) {
                    if (b -> color == by -> color ) {
                        DrawRectangle(x * grid + 3 * space, (y + 1) * grid,
                                      grid - 5 * space, space, boxColors[b -> color]);
                    }
                }
            }
        }
        //Draw starting and ending points
        char blocked[] = "!";
        for (symbol& s : symbols) {
            char symbol[] = {'A' + s.c, '\0'};
            for (V2 v : {s.start, s.end}) {
                if (at(v) -> color == s.color) {
                    DrawText(symbol, v.x * grid + 2 * space, v.y * grid + space,
                             grid, FOREGROUND);
                }
                else {
                    DrawText(blocked, v.x * grid + 2 * space, v.y * grid + space,
                             grid, BACKGROUND);
                }
            }
        }
        //Draw caption
        Rectangle textRec = {BOARDWIDTH, 5 * BUTTONHEIGHT, SIDEBAR, HEIGHT - 5 * BUTTONHEIGHT};
        DrawTextRec(GetFontDefault(), caption.c_str(), textRec, BUTTONHEIGHT, 3, true, FOREGROUND);
//        DrawText(display.c_str(), 0, rows * grid + space, 16, FOREGROUND);
    }

    bool update() {
        //Return true if won
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            p1 = mouse();
        }
        p2 = mouse();
        low.x = min(p1.x, p2.x);
        low.y = min(p1.y, p2.y);
        high.x = max(p1.x, p2.x);
        high.y = max(p1.y, p2.y);
        low = at(low) -> pos;
        high = at(high) -> opp;

        bool mustUpdatePath = false;
        //Allow for editing/saving levels (desktop only)
    #if not defined(PLATFORM_WEB)
        for (int i = 0; i < NUMCOLORS; i++) {
            if (IsKeyPressed(KEY_ZERO + i)) {
                colorSelect = i;
            }
            if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
                if (at(mouse()) -> dim == V2(1, 1)) {
                    at(mouse()) -> color = colorSelect;
                    mustUpdatePath = true;
                }
            }
        }
        if (IsKeyPressed(KEY_ENTER)) {
            write();
        }
        if (IsKeyPressed(KEY_SPACE)) {
            if (symbolToChange == NULL) {
                for (symbol& s : symbols) {
                    if (s.start == mouse()) {
                        symbolToChange = &s.start;
                        break;
                    }
                    else if (s.end == mouse()) {
                        symbolToChange = &s.end;
                        break;
                    }
                }
            }
            else {
                symbolToChange = NULL;
            }
            mustUpdatePath = true;
        }
        if (symbolToChange != NULL) {
                *symbolToChange = mouse();
            mustUpdatePath = true;
        }
    #endif

        //Draw selection box
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            V2 dim = high - low + V2(1, 1);
            DrawRectangle(low.x * grid + space, low.y * grid + space,
                          dim.x * grid - space, dim.y * grid - space, HIGHLIGHT);
        }
        if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
            mustUpdatePath = combine(low, high);
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && GetMousePosition().x < BOARDWIDTH) {
            split(mouse());
            mustUpdatePath = true;
        }
        if (mustUpdatePath) {
            return updatePath();
        }
        return false;
    }
};

bool button(int y, string text, int x = 0, int xDivisions = 1, bool highlight = false) {
    int textWidth = MeasureText(text.c_str(), BUTTONHEIGHT);
    Vector2 upperRight = {(x + 1) * WIDTH / (xDivisions + 1) - textWidth / 2, y};
    Vector2 lowerLeft = {(x + 1) * WIDTH / (xDivisions + 1) + textWidth / 2, y + BUTTONHEIGHT};
    Vector2 mouse = GetMousePosition();
    if (highlight ||
        (mouse.x > upperRight.x && mouse.x < lowerLeft.x &&
         mouse.y > upperRight.y && mouse.y < lowerLeft.y)) {
        DrawRectangle(upperRight.x - BUTTONMARGIN, upperRight.y - BUTTONMARGIN,
                      textWidth + 2 * BUTTONMARGIN, BUTTONHEIGHT + 2 * BUTTONMARGIN, FOREGROUND);
        DrawText(text.c_str(), upperRight.x, upperRight.y, BUTTONHEIGHT, BACKGROUND);
        return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    }
    else {
        DrawText(text.c_str(), upperRight.x, upperRight.y, BUTTONHEIGHT, FOREGROUND);
        return false;
    }
}

struct mainData {

    enum states{menu, play};
    int state = menu;

    boardType board;

    bool won = false;

    vector<vector<string>> levels;
    int currentLevel = 0;
    const string tabNames[5] = {"tutorial", "easy", "medium", "hard", "generate"};
    int menuTab = 0;

    const string paramNames[4] = {"rows", "columns", "symbols", "seed"};
    unsigned int params[4] = {24, 24, 4, 0};
    const unsigned int maxParams[4] = {HEIGHT / MINGRID, BOARDWIDTH / MINGRID, MAXSYMBOLS, 0xffffffff};
    const unsigned int minParams[4] = {4, 4, 1, 0};
    int paramSelect = 0;

void readLevels() {
    levels = vector<vector<string>>(4, vector<string>());
    fstream list;
    list.open("resources/levels");
    if (!list) {
        cerr << "Couldn't open level list\n";
        exit(EXIT_FAILURE);
    }
    string input;
    int readingDifficulty = 0;
    getline(list, input);    //Clear "tutorial:" line
    while (getline(list, input)) {
        if (input == "easy:" || input == "medium:" || input == "hard:") {
            readingDifficulty++;
        }
        else {
            levels[readingDifficulty].push_back(input);
        }
    }
    list.close();
}

void mainLoop() {
    BeginDrawing();
    ClearBackground(BACKGROUND);

    if (state == menu) {
        for (int i = 0; i < 5; i++) {
            if (button(BUTTONHEIGHT, tabNames[i], i, 5, i == menuTab)) {
                menuTab = i;
            }
        }
        if (menuTab < 4) {
            int levelIndex = 0;
            for (int row = 3 * BUTTONHEIGHT; row < HEIGHT; row += 2 * BUTTONHEIGHT) {
                for (int col = 0; col < 3; col++) {
                    if (levelIndex == levels[menuTab].size()) {
                        break;
                    }
                    string levelName = levels[menuTab][levelIndex];
                    if (button(row, levelName, col, 3, false)) {
                        board = boardType();
                        board.read(levelName);
                        currentLevel = levelIndex;
                        state = play;
                        won = false;
                    }
                    levelIndex++;
                }
                if (levelIndex == levels.size()) {
                    break;
                }
            }
        }
        else {
            string warning = "Warning: some random levels may be impossible.";
            int warningWidth = MeasureText(warning.c_str(), BUTTONHEIGHT);
            DrawText(warning.c_str(), (WIDTH - warningWidth) / 2, 3 * BUTTONHEIGHT,
                     BUTTONHEIGHT, (Color){255, 0, 0, 255});
            for (int x : {0, 1}) {
                for (int y : {0, 1}) {
                    int i = 2 * x + y;
                    bool selected = (i == paramSelect);
                    if (button((5 + 2 * y) * BUTTONHEIGHT,
                        paramNames[i] + ": " + to_string(params[i]) + (selected ? "_" : ""),
                        x, 2, selected)) {
                        params[paramSelect] = min(maxParams[paramSelect], params[paramSelect]);
                        params[paramSelect] = max(minParams[paramSelect], params[paramSelect]);
                        params[2] = min(params[2], params[0] * params[1] / 8);
                        paramSelect = i;
                    }
                }
            }
            for (int i = 0; i < 10; i++) {
                if (IsKeyPressed(KEY_ZERO + i)) {
                    params[paramSelect] = params[paramSelect] * 10 + i;
                }
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                params[paramSelect] /= 10;
            }
            if (button(9 * BUTTONHEIGHT, "Random seed")) {
                srand(time(NULL));
                params[3] = rand();
            }
            if (button(11 * BUTTONHEIGHT, "Done")) {
                string caption = "rows: " + to_string(params[0]) +
                                 "\ncols: " + to_string(params[1]) +
                                 "\nsymbols: " + to_string(params[2]) +
                                 "\nseed: " + to_string(params[3]) + "\n";
                cout << caption;
                srand(params[3]);
                board.generate(params[0], params[1], params[2], caption);
                state = play;
                won = false;
            }
        }
    }
    else if (state == play) {
        board.draw();
        won |= board.update();
        if (button(BUTTONHEIGHT, "menu", 6, 7)) {
            state = menu;
        }
        if (won && button(3 * BUTTONHEIGHT, "continue", 6, 7)) {
            if (menuTab < 4) {
                if (currentLevel < levels[menuTab].size() - 1) {
                    currentLevel++;
                    board = boardType();
                    board.read(levels[menuTab][currentLevel]);
                    won = false;
                }
                else if (menuTab < 3) {
                    menuTab++;
                    currentLevel = 0;
                    board = boardType();
                    board.read(levels[menuTab][currentLevel]);
                    won = false;
                }
                else {
                    state = menu;
                }
            }
            else {
                state = menu;
            }
        }
    }
    //Draw cursor in web mode
#if defined(PLATFORM_WEB)
    Vector2 mouse = GetMousePosition();
    DrawRectangle(mouse.x, mouse.y, 8, 8, BLACK);
#endif
    EndDrawing();
}

};

static mainData everything;
static void mainLoop() {
    everything.mainLoop();
}

int main() {

    InitWindow(WIDTH, HEIGHT, "Boxes");
    everything.readLevels();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(mainLoop, 60, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        everything.mainLoop();
    }
#endif
}



