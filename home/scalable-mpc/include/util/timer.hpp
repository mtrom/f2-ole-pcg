#include <chrono>
#include <iomanip>
#include <string>

// for printing to the command line in colors
#define RED    "\033[0;31m"
#define GREEN  "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE   "\033[0;34m"
#define CYAN   "\033[0;36m"
#define WHITE  "\033[0;37m"
#define RESET  "\033[0m"

/**
 * class to unify time benchmarking
 */
class Timer {
public:
  Timer(std::string msg, std::string color = WHITE) : message(msg), color(color) {
    start = std::chrono::high_resolution_clock::now();
  }

  void stop() {
    auto stop = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = stop - start;
    if (using_laps) {
        laps.push_back(elapsed);
    } else {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << color << message << " (s)\t: ";
        std::cout << elapsed.count() << RESET << std::endl;
    }
  }

  void lap() {
    start = std::chrono::high_resolution_clock::now();
    using_laps = true;
  }

  void print() {
    float average = 0;
    float min = std::numeric_limits<float>::max();
    float max = 0;
    for (const auto& lap : laps) {
      average += lap.count();
      if (lap.count() < min) { min = lap.count(); }
      if (lap.count() > max) { max = lap.count(); }
    }
    average /= laps.size();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << color << message << " (s)\t: ";
    std::cout << average << " (AVG), ";
    std::cout << min << " (MIN), ";
    std::cout << max << " (MAX)" << RESET << std::endl;
  }
private:
  std::string message;
  std::string color;
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
  std::vector<std::chrono::duration<float>> laps;
  bool using_laps = false;
};
