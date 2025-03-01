#include <string>
#include <vector>
#include <span>
#include <print>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct Editor {
    const char *output = "out";
    std::vector<std::string> lines = {""};
    int line = 0;
    int column = 0;
    int line_offset = 0;
    bool running = true;

    auto new_line() -> void {
        column = 0;
        lines.insert(lines.begin() + line, "");
    }

    auto delete_line() -> void {
        if (lines.size() == 1)
            return;

        lines.erase(lines.begin() + line);
        column = 0;

        if (line >= static_cast<int>(lines.size()))
            --line;
    }

    auto backspace() -> void {
        if (column == 0)
            return;

        --column;
        lines[line].erase(column, 1);
    }

    auto insert(char c, int count = 1) -> void {
        lines[line].insert(column, count, c);
        column += count;
    }

    auto load() -> void {
        lines.clear();

        std::ifstream f{output};

        std::string file_line;

        while (std::getline(f, file_line))
            lines.push_back(file_line);
    }

    auto save() -> void {
        std::ofstream f{output};
        std::ranges::copy(lines, std::ostream_iterator<std::string>(f, "\n"));
    }

    auto move(char c) -> void {
        switch (c) {
        case 'B':
            column = std::max(0, column - 1);
            break;
        case 'F':
            column = std::min(static_cast<int>(lines[line].size()), column + 1);
            break;
        case 'N':
            line = std::min(static_cast<int>(lines.size() - 1), line + 1);
            column = std::min(static_cast<int>(lines[line].size()), column);
            break;
        case 'P':
            line = std::max(0, line - 1);
            column = std::min(static_cast<int>(lines[line].size()), column);
            break;
        case 'A':
            column = 0;
            break;
        case 'E':
            column = lines[line].size();
            break;
        case 'V':
            line = std::min(static_cast<int>(lines.size() - 1), line + 10);
            column = std::min(static_cast<int>(lines[line].size()), column);
            break;
        case 'C':
            line = std::max(0, line - 10);
            column = std::min(static_cast<int>(lines[line].size()), column);
            break;
        case 'Q':
            running = false;
            break;
        }
    }

    auto input(char c) -> void {
        switch (c) {
        case '\n':
            ++line;
            new_line();
            break;
        case 'O':
            new_line();
            break;
        case '\b':
        case 127:
            backspace();
            break;
        case '\t':
            insert(' ', 4);
            break;
        case 'K':
            delete_line();
            break;
        case 'S':
            save();
            break;
        default:
            if (std::string{"BFNPAECVQ"}.contains(c))
                move(c);
            else
                insert(c);
            break;
        }
    }

    auto adjust_offset(int height) -> void {
        int line_count = line + 1;

        if (line_count - line_offset > height)
            line_offset = line_count - height;
        else if (line - line_offset < 0)
            line_offset = line;
    }
};

struct Tui {
    struct termios term;
    std::vector<std::string> back_buffer;

    Tui() {
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
    }

    ~Tui() {
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag |= (ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
    }

    auto move_cursor(int x, int y) -> void {
        std::print("\033[{};{}H", y, x);
    }

    auto width() -> int {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        return w.ws_col - 1;
    }

    auto height() -> int {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        return w.ws_row - 1;
    }

    auto display(std::vector<std::string> const& lines, int offset = 0) -> void {
        move_cursor(1, 1);

        int count = std::min(height(), static_cast<int>(lines.size() - offset));

        for (int i = 0; i < count; ++i) {
            auto& line = lines[offset + i];

            std::print("{}", line);

            if (i < static_cast<int>(back_buffer.size())) {
                auto& back_buffer_line = back_buffer[i];

                if (line.size() < back_buffer_line.size()) {
                    std::string blank;

                    blank.insert(0, back_buffer_line.size() - line.size(), ' ');

                    std::print("{}", blank);
                }
            }

            std::println("");
        }
    }

    auto setup_back_buffer(std::vector<std::string> const& lines, int offset = 0) -> void {
        back_buffer.clear();

        int count = std::min(height(), static_cast<int>(lines.size() - offset));

        for (auto& line: std::span(lines).subspan(offset, count)) {
            back_buffer.push_back(line);
        }
    }
};

auto main(int argc, char *argv[]) -> int {
    Editor editor;
    Tui tui;

    if (argc > 1) {
        editor.output = argv[1];
        editor.load();
    }

    std::streambuf *buf = std::cin.rdbuf();

    tui.display(editor.lines, editor.line_offset);
    tui.setup_back_buffer(editor.lines);
    tui.move_cursor(editor.column + 1, editor.line - editor.line_offset + 1);

    while (editor.running) {
        char input = buf->sbumpc();

        editor.input(input);

        editor.adjust_offset(tui.height());

        // 1-index based
        int visual_line = editor.line - editor.line_offset + 1;
        int visual_column = editor.column + 1;

        tui.display(editor.lines, editor.line_offset);

        tui.move_cursor(visual_column, visual_line);

        std::cout.flush();

        tui.setup_back_buffer(editor.lines, editor.line_offset);
    }

    return 0;
}
