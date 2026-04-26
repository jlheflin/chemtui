#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/terminal.hpp>
#include <chemfiles.hpp>
#include <map>

using namespace ftxui;

std::map<std::string, float> covalent_radii = {
  {"H",  0.31f}, {"C",  0.76f}, {"N",  0.71f}, {"O",  0.66f},
  {"F",  0.57f}, {"P",  1.07f}, {"S",  1.05f}, {"Cl", 1.02f},
  {"Br", 1.20f}, {"I",  1.39f},
};

struct Atom {
  std::string element;
  glm::vec3 position;

  friend std::ostream& operator<<(std::ostream& os, const Atom& a) {
    os << a.element << " ("
       << a.position.x << ", "
       << a.position.y << ", "
       << a.position.z << ")";

    return os;
  }
};

struct Bond {
  size_t a, b;
};

struct Molecule {
  std::vector<Atom> atoms;
  std::vector<Bond> bonds;

  friend std::ostream& operator<<(std::ostream& os, const Molecule& m) {
    os << "Molecule with " << m.atoms.size() << " atoms, "
       << m.bonds.size() << " bonds:\n";
    for (const auto& atom: m.atoms) {
      os << "  " << atom << "\n";
    }
    return os;
  }

  glm::vec3 centroid() const {
    if (atoms.empty()) {
      return glm::vec3(0.0f);
    }

    glm::vec3 c(0.0f);

    for (const auto& atom : atoms) {
      c += atom.position;
    }
    return c / static_cast<float>(atoms.size());
  }

  void center() {
    const glm::vec3 c = centroid();
    for (auto& atom : atoms) {
      atom.position -= c; 
    }
  }

  void rotate_in_place(float pitch, float yaw, float roll) {
    const glm::vec3 c = centroid();
    const glm::quat q = glm::quat(glm::vec3(pitch, yaw, roll));
    for (auto& atom : atoms) {
      atom.position = q * (atom.position - c) + c;
    }
  }
  void detect_bonds(float tolerance = 1.2f) {
  bonds.clear();
  for (size_t i = 0; i < atoms.size(); ++i) {
    auto it_i = covalent_radii.find(atoms[i].element);
    if (it_i == covalent_radii.end()) continue;

    for (size_t j = i + 1; j < atoms.size(); ++j) {
      auto it_j = covalent_radii.find(atoms[j].element);
      if (it_j == covalent_radii.end()) continue;

      float max_dist = (it_i->second + it_j->second) * tolerance;
      float dist = glm::distance(atoms[i].position, atoms[j].position);
      if (dist <= max_dist) {
        bonds.push_back({i, j});
      }
    }
  }
}
};

std::map<std::string, glm::vec3> atom_rgb = {
  {"H", {255, 255, 255}},
  {"C", { 80, 200,  80}},
  {"O", {220,  50,  50}},
  {"N", { 60, 100, 230}}
};


int main(int argc, char* argv[]) {
  auto file_logger = spdlog::basic_logger_mt("file", "chemtui.log", true);
  if (argc < 2) {
    spdlog::error("usage: chemtui <file.xyz>");
    return 1;
  }
  std::string filename = argv[1];
  std::ifstream file(filename);
  Molecule mol;

  if (file.is_open()) {
    std::string line;
    std::getline(file, line); // Gets the number of atoms
    int num_atoms = std::stoi(line); // Converts string atom number to int atom number
    std::getline(file, line); // Skips comment line
    for (int atom = 0; atom < num_atoms; atom++) {
      Atom a;
      file >> a.element >> a.position.x >> a.position.y >> a.position.z;
      mol.atoms.push_back(a);
    }
  } else {
    spdlog::error("Could not open file: {}", filename);
    return 1;
  }
  std::ostringstream oss;
  oss << mol;
  spdlog::info("Molecule:\n{}", oss.str());

  mol.center();
  mol.detect_bonds();

  oss.str("");
  oss.clear();
  oss << mol;
  spdlog::info("Molecule after centering:\n{}", oss.str());
  auto screen = ScreenInteractive::Fullscreen();

  float angle_x = 0.0f;
  float angle_y = 0.0f;

  glm::quat orientation = glm::quat(1, 0, 0, 0);
  
  auto renderer = Renderer([&] {
      auto term = Terminal::Size();

      int char_w = term.dimx - 2;
      int char_h = term.dimy - 2;

      int width = char_w * 2;
      int height = char_h * 4;
      float max_extent = 0.1f;

      for (const auto& a : mol.atoms) {
        max_extent = std::max({max_extent,
                              std::abs(a.position.x),
                              std::abs(a.position.y),
                              std::abs(a.position.z),
                            });
      }

      float margin = 0.9f;
      float scale_x = (width / 2.0f) / max_extent;
      float scale_y = (width / 2.0f) / max_extent;
      float scale = std::min(scale_x, scale_y) * margin;

      // const glm::quat q = glm::quat(glm::vec3(angle_x, angle_y, 0.0f));

      auto c = Canvas(width, height);

      std::vector<glm::vec3> rotated;
      rotated.reserve(mol.atoms.size());
      for (const auto& atom : mol.atoms) {
        rotated.push_back(orientation * atom.position);
      }


      float z_min = std::numeric_limits<float>::infinity();
      float z_max = -std::numeric_limits<float>::infinity();

      for (const auto& p : rotated) {
        z_min = std::min(z_min, p.z);
        z_max = std::max(z_max, p.z);
      }
      float z_span = std::max(z_max - z_min, 1e-6f);

      auto fade = [&](float z) {
        float t = (z_max - z) / z_span;
        return 0.4f + 0.7f * (1.0f - t);
      };

      auto shade = [&](const glm::vec3& rgb, float f) {
        auto clamp8 = [](float v) {
          return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
        };
        return Color::RGB(clamp8(rgb.r * f), clamp8(rgb.g * f), clamp8(rgb.b * f));
      };

      std::vector<size_t> bond_order(mol.bonds.size());
      std::iota(bond_order.begin(), bond_order.end(), 0);
      std::sort(bond_order.begin(), bond_order.end(), [&](size_t a, size_t b) {
                  float za = (rotated[mol.bonds[a].a].z + rotated[mol.bonds[a].b].z) * 0.5f;
                  float zb = (rotated[mol.bonds[b].a].z + rotated[mol.bonds[b].b].z) * 0.5f;
                  return za < zb;
                });

      for (size_t idx : bond_order) {
        const auto& bond = mol.bonds[idx];
        const auto& pa = rotated[bond.a];
        const auto& pb = rotated[bond.b];
        float f = fade((pa.z + pb.z) * 0.5f);
        Color col = shade(glm::vec3(180, 180, 180), f);
        int ax = static_cast<int>(pa.x * scale) + width / 2;
        int ay = static_cast<int>(pa.y * scale) + height / 2;
        int bx = static_cast<int>(pb.x * scale) + width / 2;
        int by = static_cast<int>(pb.y * scale) + height / 2;
        c.DrawPointLine(ax, ay, bx, by, col);
      }

      std::vector<size_t> draw_order(mol.atoms.size());
      std::iota(draw_order.begin(), draw_order.end(), 0);
      std::sort(draw_order.begin(), draw_order.end(), [&](size_t a, size_t b) {
                  return rotated[a].z < rotated[b].z;
                });

      for (size_t idx : draw_order) {
        glm::vec3 p = rotated[idx];
        auto it = atom_rgb.find(mol.atoms[idx].element);
        glm::vec3 base = (it != atom_rgb.end()) ? it->second : glm::vec3(200, 200, 200);
        Color col = shade(base, fade(p.z));
        int px = static_cast<int>(p.x * scale) + width / 2;
        int py = static_cast<int>(p.y * scale) + height / 2;
        c.DrawPointCircleFilled(px, py, covalent_radii[mol.atoms[idx].element] * 10.0, col);
      }
      return canvas(std::move(c)) | border;
  });

  spdlog::set_default_logger(file_logger);
  auto with_events = CatchEvent(renderer, [&](Event event) {
    if (event == Event::Character('q')) { screen.ExitLoopClosure()(); return true; }

    const float step = 0.05f;
    // Camera axes: X = right, Y = up, Z = toward camera
    if (event == Event::ArrowUp) {
      auto delta = glm::angleAxis(step, glm::vec3(1, 0, 0));  // pitch around world X
      orientation = delta * orientation;
      return true;
    }
    if (event == Event::ArrowDown) {
      auto delta = glm::angleAxis(-step, glm::vec3(1, 0, 0));
      orientation = delta * orientation;
      return true;
    }
    if (event == Event::ArrowLeft) {
      auto delta = glm::angleAxis(-step, glm::vec3(0, 1, 0));  // yaw around world Y
      orientation = delta * orientation;
      return true;
    }
    if (event == Event::ArrowRight) {
      auto delta = glm::angleAxis(step, glm::vec3(0, 1, 0));
      orientation = delta * orientation;
      return true;
    }
    return false;
  });

  screen.Loop(with_events);
  return 0;
}
