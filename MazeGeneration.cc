#include <iostream>
#include <functional>
#include <cassert>
#include <vector>

using namespace std;


enum DIRECTION { UP = 1, DOWN = 2, LEFT = 4, RIGHT = 8, NONE };

DIRECTION rand_direction(int h, int w, vector<vector<int>>& maze, vector<vector<bool>>& visited) {
    vector<DIRECTION> direction; direction.reserve(4);
   
    if (h - 1 >= 0 && !visited[h - 1][w]) direction.push_back(UP);
    if (h + 1 < (int)maze.size() && !visited[h + 1][w]) direction.push_back(DOWN);
    if (w - 1 >= 0 && !visited[h][w - 1]) direction.push_back(LEFT);
    if (w + 1 < (int)maze[h].size() && !visited[h][w + 1]) direction.push_back(RIGHT);

    if (direction.empty()) return NONE;
    return direction[rand() % direction.size()];
}

int count = 0;

void _gen(int h, int w, vector<vector<int>>& maze, vector<vector<bool>>& visited) {
    //int trace = count++;
    visited[h][w] = true;

    DIRECTION dir = rand_direction(h, w, maze, visited);
    while(dir != NONE) {
        switch (dir) {
            case UP:
                //cout << trace << " UP" << endl;
                maze[h][w] |= UP;
                maze[h - 1][w] |= DOWN;
                _gen(h - 1, w, maze, visited);
                break;
            case DOWN:
                //cout << trace << " DOWN" << endl;
                maze[h][w] |= DOWN;
                maze[h + 1][w] |= UP;
                _gen(h + 1, w, maze, visited);
                break;
            case LEFT:
                //cout << trace << " LEFT" << endl;
                maze[h][w] |= LEFT;
                maze[h][w - 1] |= RIGHT;
                _gen(h, w - 1, maze, visited);
                break;
            case RIGHT:
                //cout << trace << " RIGHT" << endl;
                maze[h][w] |= RIGHT;
                maze[h][w + 1] |= LEFT;
                _gen(h, w + 1, maze, visited);
                break;
            default:
                //cout << trace << " NONE" << endl;
                return;
        }
        dir = rand_direction(h, w, maze, visited);
    }
}

vector<vector<int>> generate(int width, int height) {
    vector<vector<int>> maze(height, vector<int>(width, 0));
    vector<vector<bool>> visited(height, vector<bool>(width, false));
    if (width <= 0 || height <= 0) return maze;

    srand(time(NULL));
    _gen(0, 0, maze, visited);

    return maze;
}

void print(const vector<vector<int>>& maze) {
    for (auto& line : maze) {
        for (auto& cell : line ) {
            if (cell & UP) cout << "UP ";
            if (cell & DOWN) cout << "DOWN ";
            if (cell & LEFT) cout << "LEFT ";
            if (cell & RIGHT) cout << "RIGHT";
            cout << ",  ";
        }
        cout << endl;
    }
}

string gen_svg_open(int height, int width) {

    char buffer[10240];
    sprintf(buffer, "<svg height=\"%d\" width=\"%d\">\n", height, width);
    return buffer;
}

string svg_line(int x1, int y1, int x2, int y2) {
    char buffer[10240];
    sprintf(buffer, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" style=\"stroke:rgb(0,0,0);stroke-width:2\" />\n", x1, y1, x2, y2);
    return buffer;
}

string gen_lines(const vector<vector<int>>& maze, int thick, int height, int width) {
    string lines;
    lines += svg_line(0, 0, width, 0);
    lines += svg_line(0, 0, 0, height);
    lines += svg_line(width, 0, width, height);
    lines += svg_line(0, height, width, height);

    for (int row = 0; row < (int) maze.size(); ++row) {
        for (int col = 0; col < (int) maze[row].size(); ++col) {
            if (!(maze[row][col] & RIGHT)) lines += svg_line(col * thick + thick, row * thick - 1, col * thick + thick, row * thick + thick + 1);
            if (!(maze[row][col] & DOWN)) lines += svg_line(col * thick, row * thick + thick, col * thick + thick, row * thick + thick);
        }
    }

    return lines;
}

string gen_svg(const vector<vector<int>>& maze) {
    const int thick = 10;
    int height = maze.size() * thick;
    int width = maze[0].size() * thick;

    string graph =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n";

    graph += gen_svg_open(height, width);
    graph += gen_lines(maze, thick, height, width);


    graph += "</svg>\n";
    graph += 
        "</body>\n"
        "</body>"; 
    return graph;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "wrong usage: ./prog height width"  << std::endl;
    }

    int height = atoi(argv[1]);
    int width = atoi(argv[2]);
    auto maze = generate(height, width);
    auto svg = gen_svg(maze);
    //print(maze);
    std::cout << svg << std::endl;
    return 0;
}
