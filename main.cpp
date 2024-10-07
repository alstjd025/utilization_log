#include "logger.h"

int main(int argv, char* argc[]){
  if(argv < 2){
    std::cout << "Not enough args, usage : pedriod(ms), log directory" << "\n";
    exit(-1);
  }
  int period;
  std::string dir;
  // duration = atoi(argc[1]);
  // cpu = atoi(argc[2]);
  // gpu = atoi(argc[3]);
  period = atoi(argc[1]);
  dir = argc[2];
  std::cout << "period " << period << " dir " << dir << "\n";
  SysMonitor monitor(dir, period);

  return 0;
}