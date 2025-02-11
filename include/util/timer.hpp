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
  Timer() { }
  Timer(std::string msg, std::string color = WHITE) {
    this->start(msg, color);
  }

  void start(std::string msg, std::string color = WHITE) {
    this->message = msg;
    this->color = color;
    this->start_ = std::chrono::high_resolution_clock::now();
  }

  void stop() {
    auto stop = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = stop - start_;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << color << message << "\t: ";
    std::cout << elapsed.count() << " s" << RESET << std::endl;
  }
private:
  std::string message;
  std::string color;
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};
