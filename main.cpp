#include "misc.h"
#include "point.h"

#include "curses.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

using azbyn::Point;
using azbyn::string_format;

#define LEN(x) (sizeof(x) / sizeof(*x))

// clang-format off
enum Piece { TP_EMPTY = -1, TP_I, TP_L, TP_J, TP_O, TP_S, TP_T, TP_Z, TP_LEN };
constexpr const char PieceRotationsStr[TP_LEN][16 * 4 + 1] = {
    // I
    "...." "..x." "...." ".x.."
    "xxxx" "..x." "...." ".x.."
    "...." "..x." "xxxx" ".x.."
    "...." "..x." "...." ".x..",
    // L
    "x..." ".xx." "...." ".x.."
    "xxx." ".x.." "xxx." ".x.."
    "...." ".x.." "..x." "xx.."
    "...." "...." "...." "....",
    // J
    "..x." ".x.." "...." "xx.."
    "xxx." ".x.." "xxx." ".x.."
    "...." ".xx." "x..." ".x.."
    "...." "...." "...." "....",
    // O
    ".xx." ".xx." ".xx." ".xx."
    ".xx." ".xx." ".xx." ".xx."
    "...." "...." "...." "...."
    "...." "...." "...." "....",
    // S
    ".xx." ".x.." "...." "x..."
    "xx.." ".xx." ".xx." "xx.."
    "...." "..x." "xx.." ".x.."
    "...." "...." "...." "....",
    // T
    ".x.." ".x.." "...." ".x.."
    "xxx." ".xx." "xxx." "xx.."
    "...." ".x.." ".x.." ".x.."
    "...." "...." "...." "....",
    // Z
    "xx.." "..x." "...." ".x.."
    ".xx." ".xx." "xx.." "xx.."
    "...." ".x.." ".xx." "x..."
    "...." "...." "...." "....",
};
// clang-format on
void generateTable() {
    return;
    constexpr char indent[] = "    ";
    for (int p = 0; p < TP_LEN; ++p) {
        printf("%s{{{", indent);
        for (int r = 0; r < 4; ++r) {
            if (r != 0) printf("%s {{", indent);
            int i = 0;
            for (int x = 0; x < 4; ++x) {
                for (int y = 0; y < 4; ++y) {
                    if (PieceRotationsStr[p][(16 * y) + (r * 4) + x] == 'x')
                        printf("{%d, %d}%s", x, 3 - y, ++i < 4 ? ", " : "");
                }
            }
            printf(r == 3 ? "}}},\n" : "}},\n");
        }
    }
    exit(0);
}

//generated using generateTable
using PiecePoints = std::array<Point, 4>;
constexpr PiecePoints PieceRotations[TP_LEN][4] = {
    {{{{0, 2}, {1, 2}, {2, 2}, {3, 2}}},
     {{{2, 3}, {2, 2}, {2, 1}, {2, 0}}},
     {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}},
     {{{1, 3}, {1, 2}, {1, 1}, {1, 0}}}},
    {{{{0, 3}, {0, 2}, {1, 2}, {2, 2}}},
     {{{1, 3}, {1, 2}, {1, 1}, {2, 3}}},
     {{{0, 2}, {1, 2}, {2, 2}, {2, 1}}},
     {{{0, 1}, {1, 3}, {1, 2}, {1, 1}}}},
    {{{{0, 2}, {1, 2}, {2, 3}, {2, 2}}},
     {{{1, 3}, {1, 2}, {1, 1}, {2, 1}}},
     {{{0, 2}, {0, 1}, {1, 2}, {2, 2}}},
     {{{0, 3}, {1, 3}, {1, 2}, {1, 1}}}},
    {{{{1, 3}, {1, 2}, {2, 3}, {2, 2}}},
     {{{1, 3}, {1, 2}, {2, 3}, {2, 2}}},
     {{{1, 3}, {1, 2}, {2, 3}, {2, 2}}},
     {{{1, 3}, {1, 2}, {2, 3}, {2, 2}}}},
    {{{{0, 2}, {1, 3}, {1, 2}, {2, 3}}},
     {{{1, 3}, {1, 2}, {2, 2}, {2, 1}}},
     {{{0, 1}, {1, 2}, {1, 1}, {2, 2}}},
     {{{0, 3}, {0, 2}, {1, 2}, {1, 1}}}},
    {{{{0, 2}, {1, 3}, {1, 2}, {2, 2}}},
     {{{1, 3}, {1, 2}, {1, 1}, {2, 2}}},
     {{{0, 2}, {1, 2}, {1, 1}, {2, 2}}},
     {{{0, 2}, {1, 3}, {1, 2}, {1, 1}}}},
    {{{{0, 3}, {1, 3}, {1, 2}, {2, 2}}},
     {{{1, 2}, {1, 1}, {2, 3}, {2, 2}}},
     {{{0, 2}, {1, 2}, {1, 1}, {2, 1}}},
     {{{0, 2}, {0, 1}, {1, 3}, {1, 2}}}},
};
constexpr float _speeds[] = {
    1.0,
    0.793,
    0.618,
    0.473,
    0.355,
    0.262,
    0.190,
    0.135,
    0.094,
    0.064,
    0.043,
    0.028,
    0.018,
    0.011,
    0.007,
};
constexpr float speed(int level) {
    return level > 14 ? .007 : _speeds[level];
}
constexpr int Width = 10;
constexpr int Height = 20;
constexpr int MatrixSizeY = 30;
constexpr int NextPiecesLen = 3;

using Callback = void (*)(void);

void waitAFrame() {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
void keyChoice(int a, Callback cbA, int b, Callback cbB) {
    for (;;) {
        auto c = tolower(getch());
        if (c == a) { cbA(); return; }
        if (c == b) { cbB(); return; }
        waitAFrame();
    }
}

class RandomGenerator {
    std::array<Piece, TP_LEN> bags[2] = {};
    int bagIndex = 0;
    std::random_device rd;
    std::mt19937 gen;
    int index = 0;

public:
    RandomGenerator() : rd(), gen(rd()) {
        for (int i = 0; i < TP_LEN; ++i) {
            bags[0][i] = (Piece)i;
            bags[1][i] = (Piece)i;
        }
        Restart();
    }

    Piece operator()() {
        if (index >= TP_LEN) {
            std::shuffle(bags[bagIndex].begin(), bags[bagIndex].end(), gen);
            bagIndex = !bagIndex;
            index = 0;
        }
        return bags[bagIndex][index++];
    }
    Piece NextPiece(int i) const {
        int ix = index + i;
        if (ix >= TP_LEN)
            return bags[!bagIndex][ix - TP_LEN];
        return bags[bagIndex][ix];
    }
    void Restart() {
        bagIndex = 0;
        index = 0;
        std::shuffle(bags[0].begin(), bags[0].end(), gen);
        std::shuffle(bags[1].begin(), bags[1].end(), gen);
    }

} randgen;

class Game {
    int highscore = 0;
    bool hasHighscore = false;
    int score = 0;
    int clearedLinesThisLevel = 0;
    int totalClearedLines = 0;
    int level = 0;
    uint8_t matrix[MatrixSizeY * Width];
    bool running = true;
    Callback pause;
    int RequiredLines() const { return (level + 1) * 5; }

    void ClearLine(int y) {
        ++totalClearedLines;
        if (++clearedLinesThisLevel >= RequiredLines()) {
            ++level;
            clearedLinesThisLevel = 0;
        }
        memcpy(matrix + y * Width,
               matrix + (y + 1) * Width,
               sizeof(*matrix) * Width * (MatrixSizeY - y - 1));
        memset(matrix + (MatrixSizeY - 1) * Width, 0, Width);
    }

public:
    Game() { Restart(); }
    void Restart() {
        score = 0;
        clearedLinesThisLevel = 0;
        totalClearedLines = 0;
        level = 0;
        memset(matrix, 0, sizeof(matrix));
        running = true;
        hasHighscore = false;
    }

    void IncreaseScore(int x) {
        score += x;
        if (highscore < score) {
            highscore = score;
            hasHighscore = true;
        }
    }
    void Init(Callback pause) { this->pause = pause; }
    void End() { running = false; }
    void Pause() { pause(); }

    uint8_t& Matrix(Point p) { return matrix[p.y * Width + p.x]; }
    uint8_t& Matrix(int x, int y) { return matrix[y * Width + x]; }
    float Speed() const { return speed(level); }
    int ClearedLines() const { return totalClearedLines; }
    int GoalLines() const { return RequiredLines() - clearedLinesThisLevel; }
    int Score() const { return score; }
    int Highscore() const { return highscore; }
    void SetHighscore(int i) { highscore = i; }
    bool HasHighscore() const { return hasHighscore; }
    int Level() const { return level + 1; }
    bool Running() const { return running; }
    void CheckClearLines() {
        unsigned y = 0;
        int clearedLines = 0;
        while (y < MatrixSizeY) {
            for (unsigned x = 0; x < Width; ++x) {
                if (!Matrix(x, y))
                    goto nextLine;
            }
            ClearLine(y);
            ++clearedLines;
            continue; //redo line
        nextLine:
            ++y;
        }
        switch (clearedLines) {
        case 1: IncreaseScore(Level() * 40); break;
        case 2: IncreaseScore(Level() * 100); break;
        case 3: IncreaseScore(Level() * 300); break;
        case 4: IncreaseScore(Level() * 1200); break;
        }
    }
} game;

class Player {
    PiecePoints points;
    PiecePoints ghostPoints;
    Point pos;
    int rotation;
    Piece piece;
    std::chrono::time_point<std::chrono::system_clock> lastDrop;
    Piece holdPiece = TP_EMPTY;
    bool lastWasHold = false;

    void UpdatePtsBase(PiecePoints& pts, Point p) const {
        for (int i = 0; i < 4; ++i) {
            pts[i] = PieceRotations[piece][rotation][i] + p;
        }
    }
    void UpdatePoints() { UpdatePtsBase(points, pos); }
    void UpdateGhostPoints() {
        Point p = pos;
        for (;;) {
            for (int i = 0; i < 4; ++i) {
                ghostPoints[i] = PieceRotations[piece][rotation][i] + p;
                if (game.Matrix(ghostPoints[i]) || ghostPoints[i].y < 0) {
                    ++p.y;
                    UpdatePtsBase(ghostPoints, p);
                    return;
                }
            }
            --p.y;
        }
    }
    void Reset(Piece p) {
        if (p == TP_I && game.Matrix(4, Height - 1)) game.End();
        pos = Point(3, Height - 3);
        rotation = 0;
        piece = p;
        UpdatePoints();
        for (auto pt : points) {
            if (game.Matrix(pt)) {
                for (auto p : points) {
                    if (p.y >= Height) {
                        ++pos.y; //just visual
                        UpdatePoints();
                        game.End();
                        return;
                    }
                }
            }
        }
        UpdateGhostPoints();
        ResetDrop();
    }

public:
    Player(Piece p) {
        Reset(p);
    }
    Player() : Player(randgen()) {}
    void Restart() {
        Reset(randgen());
        holdPiece = TP_EMPTY;
        lastWasHold = false;
    }
    const PiecePoints& GetPoints() const {
        return points;
    }
    const PiecePoints& GetGhostPoints() const {
        return ghostPoints;
    }
    Piece GetPiece() const {
        return piece;
    }
    Piece GetHoldPiece() const {
        return holdPiece;
    }
    void Hold() {
        if (holdPiece == TP_EMPTY) {
            holdPiece = piece;
            Reset(randgen());
        }
        else if (!lastWasHold) {
            Piece tmp = piece;
            Reset(holdPiece);
            holdPiece = tmp;
        }
        lastWasHold = true;
    }

    void Move(int x) {
        pos.x += x;
        PiecePoints bk = points;
        UpdatePoints();
        for (auto& pt : points) {
            if (game.Matrix(pt) != 0 || pt.x < 0 || pt.x >= Width) {
                pos.x -= x;
                points = bk;
                return;
            }
        }
        UpdateGhostPoints();
    }
    void Fall() {
        --pos.y;
        UpdatePoints();
        for (auto pt : points) {
            if (game.Matrix(pt) || pt.y < 0) {
                ++pos.y;
                UpdatePoints();
                PlaceOnBoard();
                return;
            }
        }
    }
    void SoftDrop() {
        game.IncreaseScore(1);
        ResetDrop();
        Fall();
    }
    void HardDrop() {
        //if (pos.y == Height - 2) return;
        for (;;) {
            --pos.y;
            UpdatePoints();
            for (auto pt : points) {
                if (game.Matrix(pt) || pt.y < 0) {
                    ++pos.y;
                    UpdatePoints();
                    PlaceOnBoard();
                    return;
                }
            }
            game.IncreaseScore(1);
        }
    }
    void Rotate(int i) {
        auto tmprot = rotation;
        rotation += i;
        if (rotation < 0)
            rotation = 3;
        else if (rotation > 3)
            rotation = 0;
        if (CheckRotation())
            return;
        // wall kick right
        auto tmpx = pos.x;
        ++pos.x;
        if (CheckRotation())
            return;
        // wall kick left
        pos.x = tmpx - 1;
        if (CheckRotation())
            return;
        // special cases for I
        if (piece == TP_I) {
            pos.x = tmpx + 2;
            if (CheckRotation())
                return;
            pos.x = tmpx - 2;
            if (CheckRotation())
                return;
        }
        // can't wall kick
        pos.x = tmpx;
        rotation = tmprot;
        UpdateGhostPoints();
        UpdatePoints();
    }

private:
    bool CheckRotation() {
        UpdatePoints();
        for (auto pt : points) {
            if (game.Matrix(pt) || pt.x < 0 || pt.x >= Width) {
                return false;
            }
        }
        UpdateGhostPoints();
        return true;
    }

public:
    void Input() {
        switch (tolower(getch())) {
            // case ERR: continue;
        case 'z':
        case 'a':
            Rotate(-1);
            break;
        case KEY_UP:
        case 's':
        case 'x':
            Rotate(1);
            break;
        case 'c':
        case 'd':
            Hold();
            break;
        case KEY_F(1):
        case 'q':
        case 27:
        case 'p':
            game.Pause();
            break;
        case ' ':
            HardDrop();
            break;
        case KEY_DOWN:
            SoftDrop();
            break;
        case KEY_LEFT:
            Move(-1);
            break;
        case KEY_RIGHT:
            Move(1);
            break;
        }
    }

    void PlaceOnBoard(Piece pc) {
        for (auto pt : points) {
            /*if (pt.y >= Height) {
                game.End();
                }*/
            game.Matrix(pt) = piece + 1;
        }
        game.CheckClearLines();
        Reset(pc);
        lastWasHold = false;
    }
    void PlaceOnBoard() {
        PlaceOnBoard(randgen());
    }

    void ResetDrop() {
        lastDrop = std::chrono::system_clock::now();
    }
    void Gravity() {
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<float> d = now - lastDrop;
        if (d.count() >= game.Speed()) {
            lastDrop = now;
            Fall();
        }
    }
} player;

class Graphics {
    enum Pairs {
        PAIR_BG = TP_LEN + 1,
        PAIR_BOARD0,
        PAIR_BOARD1,
        PAIR_BORDER,
        PAIR_TEXT,
        PAIR_GHOST_PIECE,
    };
    static void coladdstr(short col, const char* str) {
        attron(COLOR_PAIR(col));
        addstr(str);
    }
    static void mvcoladdstr(int y, int x, short col, const char* str) {
        attron(COLOR_PAIR(col));
        mvaddstr(y, x, str);
    }

    void InitColors() {
        enum Color : short {
            //Color,    //irgb
            Black,      //0000
            DarkBlue,   //0001
            DarkGreen,  //0010
            DarkCyan,   //0011
            DarkRed,    //0100
            DarkMagenta,//0101
            DarkYellow, //0110
            LightGray,  //0111
            DarkGray,   //1000
            Blue,       //1001
            Green,      //1010
            Cyan,       //1011
            Red,        //1100
            Magenta,    //1101
            Yellow,     //1110
            White,      //1111
        };
        start_color();
        constexpr Color bgColor = Black;
        auto addPiece = [](Piece p, Color col) { init_pair(p + 1, col, col); };
        auto addColor = [](int i, Color col) { init_pair(i, col, col); };
        addPiece(TP_I, Cyan);
        addPiece(TP_L, Blue);
        addPiece(TP_J, DarkYellow);
        addPiece(TP_O, Yellow);
        addPiece(TP_S, Green);
        addPiece(TP_T, Magenta);
        addPiece(TP_Z, Red);
        addColor(PAIR_BG, bgColor);
        addColor(PAIR_BOARD0, Black);
        addColor(PAIR_BOARD1, Black);
        addColor(PAIR_GHOST_PIECE, DarkGray);
        addColor(PAIR_BORDER, LightGray);
        init_pair(PAIR_TEXT, White, bgColor);
    }

public:
    Graphics() {
        initscr(); /* initialize the curses library */
        keypad(stdscr, true); /* enable keyboard mapping */
        nonl(); /* tell curses not to do NL->CR/NL on output */
        cbreak(); /* take input chars one at a time, no wait for \n */
        noecho();
        nodelay(stdscr, true);
        meta(stdscr, true);
        curs_set(0);
        //putenv("ESCDELAY=25");
        if (COLS < 2 * Width + 15 + LeftPad || LINES < Height + 1) {
            throw std::runtime_error(
                string_format("terminal too small %dx%d", COLS, LINES));
        }
        if (has_colors())
            InitColors();
        DrawBegin();
    }
    void Restart() {
        clear();
        DrawBegin();
    }

    ~Graphics() {
        endwin();
        //printf("Game Over:\n  Score: %d\n  Level: %d\n  Cleared Lines: %d\n",
        //       game.Score(), game.Level(), game.ClearedLines());
    }
    static constexpr int LeftPad = 10;
    static constexpr int MatrixStartX = LeftPad + 2;
    static constexpr int MatrixEndX = MatrixStartX + 2 * Width;
    void DrawBegin() {
        const std::string singleVertical = std::string(10, ' ');
        attron(COLOR_PAIR(PAIR_TEXT));
        mvaddstr(0, 2, "Hold:");
        mvaddstr(0, MatrixEndX + 4, "Next:");

        attron(COLOR_PAIR(PAIR_BORDER));
        mvaddstr(1, 0, singleVertical.c_str());
        mvaddstr(5, 0, singleVertical.c_str());

        mvaddstr(1, MatrixEndX + 2, singleVertical.c_str());
        mvaddstr(3 * NextPiecesLen + 2, MatrixEndX + 2, singleVertical.c_str());
        for (int y = 1; y < 5; ++y)
            mvaddstr(y, 0, "  ");
        for (int y = 1; y < 3 * NextPiecesLen + 2; ++y)
            mvaddstr(y, MatrixEndX + 10, "  ");

        const std::string verticalBorder = std::string(Width * 2 + 4, ' ');
        mvaddstr(Height, LeftPad, verticalBorder.c_str());
        for (int y = 0; y < Height; ++y) {
            mvaddstr(y, LeftPad, "  ");
            mvaddstr(y, MatrixEndX, "  ");
        }
    }
    void DrawMatrix() {
        move(0, LeftPad);
        for (int y = 0; y < Height; ++y) {
            move(Height - y - 1, MatrixStartX);
            for (int x = 0; x < Width; ++x) {
                auto col = game.Matrix(x, y);
                coladdstr(col ? col : (uint8_t)(x % 2 ? PAIR_BOARD0 : PAIR_BOARD1), "  ");
            }
        }
    }
    void DrawVal(int y, int x, const char* str, int num) {
        mvprintw(y, x, str);
        mvprintw(y + 1, x, "  %d", num);
    }

    void DrawInfo() {
        attron(COLOR_PAIR(PAIR_TEXT));
        constexpr int x = MatrixEndX + 3;
        DrawVal(Height - 4, 1, "Level:", game.Level());
        DrawVal(Height - 2, 1, "Goal:", game.GoalLines());
        DrawVal(Height - 7, x, "Hiscore:", game.Highscore());
        DrawVal(Height - 4, x, "Score:", game.Score());
        DrawVal(Height - 2, x, "Cleared Lines:", game.ClearedLines());
        if (player.GetHoldPiece() != -1)
            DrawPiece(2, 2, player.GetHoldPiece());
        for (int i = 0; i < NextPiecesLen; ++i) {
            DrawPiece(2 + i * 3, MatrixEndX + 2, randgen.NextPiece(i));
        }
    }
    const std::string pieceBgVertical = std::string(8, ' ');
    void DrawPiece(int y, int x, Piece p) {
        attron(COLOR_PAIR(PAIR_BG));
        for (int i = 0; i < 3; ++i) {
            mvaddstr(y + i, x, pieceBgVertical.c_str());
        }
        attron(COLOR_PAIR(p + 1));
        for (int i = 0; i < 4; ++i) {
            auto pt = PieceRotations[p][0][i];
            mvaddstr(3 - pt.y + y, pt.x * 2 + x, "  ");
        }
    }
    void DrawPlayer() {
        attron(COLOR_PAIR(player.GetPiece() + 1));
        for (auto pt : player.GetPoints()) {
            mvaddstr(Height - pt.y - 1, pt.x * 2 + LeftPad + 2, "  ");
        }
    }

    void DrawGhostPiece() {
        attron(COLOR_PAIR(PAIR_GHOST_PIECE));
        for (auto pt : player.GetGhostPoints()) {
            mvaddstr(Height - pt.y - 1, pt.x * 2 + LeftPad + 2, "  ");
        }
    }

public:
    void Draw() {
        DrawInfo();
        DrawMatrix();
        DrawGhostPiece();
        DrawPlayer();
    }
    void DrawPause() {
        DrawScreenBase("Paused", true);
    }

    void DrawEndScreen() {
        DrawScreenBase(game.HasHighscore() ? "HIGH SCORE" : "GAME OVER", false);
    }
private:
    void DrawScreenBase(std::string title, bool isPause) {
        const std::string verticalBar = std::string(Width * 2, ' ');
        DrawAtMiddle(PAIR_BORDER, 3, verticalBar);
        for (int i = 4; i < 10; ++i)
            DrawAtMiddle(PAIR_TEXT, i, verticalBar);

        DrawAtMiddle(PAIR_BORDER, 10, verticalBar);
        DrawAtMiddle(PAIR_TEXT, 5, title);
        DrawAtMiddle(PAIR_TEXT, 7, isPause ?
                     "Quit      Resume" :
                     "Quit      Replay");
        DrawAtMiddle(PAIR_TEXT, 8, "  Q          R  ");
    }

    void DrawAtMiddle(short color, int y, std::string s) {
        mvcoladdstr(y, MatrixStartX + Width - (s.size() / 2), color, s.c_str());
    }
} graphics;

/* TODO:
   - Windows support
 */
const std::string HighscoresFile = "highscores";
void writeHighscore() {
    std::ofstream f(HighscoresFile);
    f << game.Highscore() << "\n";
}
int main() {
    //generateTable();
    std::ifstream f(HighscoresFile);
    int fileHighscore;
    f >> fileHighscore;
    game.SetHighscore(fileHighscore);
    f.close();

    signal(SIGINT, [](int) { game.End(); exit(0); });
    atexit([] { game.End(); });
    game.Init([] {
            graphics.DrawPause();
            keyChoice('q', [] { writeHighscore(); exit(0); },
                      'r', [] { /* don't do anything */ });
        });
    for (;;) {
        while (game.Running()) {
            player.Input();
            player.Gravity();
            graphics.Draw();
            waitAFrame();
        }
        graphics.DrawEndScreen();
        keyChoice('q', [] { writeHighscore(); exit(0); },
                  'r', [] {
                      randgen.Restart();
                      game.Restart();
                      player.Restart();
                      graphics.Restart(); });
    }
    return 0;
}
