#include <string>
#include <vector>
#include <span>
#include <print>
#include <iostream>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct Editor {
    std::vector<std::string> lines = {""};
    int line = 0;
    int column = 0;
    int line_offset = 0;
    bool running = true;

    auto new_line() -> void {
        ++line;
        column = 0;

        lines.insert(lines.begin() + line, "");
    }

    auto delete_line() -> void {
        if (lines.size() > 0) {
            lines.erase(lines.begin() + line);
            column = 0;

            if (line >= static_cast<int>(lines.size()))
                --line;
        }
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
        default:
            if (std::string{"BFNPAECVQ"}.contains(c))
                move(c);
            else
                insert(c);
            break;
        }
    }

    auto adjust_offset(int height) -> bool {
        int line_count = line + 1;

        if (line_count - line_offset > height) {
            line_offset = line_count - height;
            return true;
        } else if (line - line_offset < 0) {
            line_offset = line;
            return true;
        }

        return false;
    }
};

struct Tui {
    struct termios term;

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

        for (auto& line: std::span(lines).subspan(offset, count)) {
            std::println("{}", line);
        }
    }

    auto clear() -> void {
        std::string blank{};
        blank.insert(0, width(), ' ');

        move_cursor(1, 1);

        for (int i = 0; i < height(); ++i) {
            std::println("{}", blank);
        }
    }
};

auto main() -> int {
    Editor editor;
    Tui tui;

    std::streambuf *buf = std::cin.rdbuf();
    tui.clear();
    tui.display(editor.lines, editor.line_offset);
    tui.move_cursor(editor.column + 1, editor.line - editor.line_offset + 1);

    while (editor.running) {
        char input = buf->sbumpc();

        editor.input(input);

        // 1-index based
        int visual_line = editor.line - editor.line_offset + 1;
        int visual_column = editor.column + 1;

        // Clear screen in case of large movement
        if (std::string{"NPCVK\n"}.contains(input)) {
            if (editor.adjust_offset(tui.height()) || input == 'K' || input == '\n') {
                tui.clear();

                // Recalculate visual cursor position
                visual_line = editor.line - editor.line_offset + 1;
                visual_column = editor.column + 1;
            }
        } else if (input == '\b' || input == 127) { // Clear last character in case of backspace
            tui.move_cursor(editor.lines[editor.line].size() + 1, visual_line);
            std::print(" ");
        }

        tui.display(editor.lines, editor.line_offset);

        tui.move_cursor(visual_column, visual_line);
    }

    return 0;
}
