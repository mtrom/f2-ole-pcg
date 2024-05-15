#include <gtest/gtest.h>

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  try {
    return RUN_ALL_TESTS();
  } catch(const std::exception& e) {
    std::cerr << "Unhandled exception caught: " << e.what() << std::endl;
  } catch(...) {
    std::cerr << "Unknown non-standard exception caught" << std::endl;
  }
  return -1; // Return an error code if an exception is caught
}
