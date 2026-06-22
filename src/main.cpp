#include <ftxui/component/component.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/dom/elements.hpp>

int main(int argc, char* argv[]) {
  auto screen = ftxui::Screen::Create(
    ftxui::Dimension::Full(),
    ftxui::Dimension::Fixed(10)
  );

  auto& cell = screen.CellAt(10, 5);

  cell.character = "X";
  cell.foreground_color = ftxui::Color::Red;
  cell.background_color = ftxui::Color::RGB(0, 255, 0);
  cell.bold = true;

  screen.Print();

  return 0;
}
