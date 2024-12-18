#include "logger.h"
#define nvidia
// #define ramdisk_gpu_debug
#define CPU_NUM 4 // 0 for global cpu ulization logging
#define GPU_UTIL_FILE "/mnt/ramdisk/gpu_util"
/*
  Note[MS] : GPU monitoring period under 5 ms may not safe?
*/


// #define Experiment
SysMonitor::SysMonitor(){}

SysMonitor::SysMonitor(std::string& directory_, int period_){
  period = period_;
  directory = directory_;
  
  std::cout << "System utilization logging started" << "\n";
  CPU_daemon = std::thread(&SysMonitor::GetCPUGPUUtilization, this);
  // #ifdef nvidia
  //   GPU_daemon = std::thread(&SysMonitor::GetGPUUtilization, this);
  // #endif
  // CPU_daemon.detach();
  // #ifdef nvidia
  //   GPU_daemon.detach();
  // #endif
  CPU_daemon.join();
  log_File.close();
}

SysMonitor::~SysMonitor(){
  // must terminate CPU_daemon & GPU_daemon here.
  std::cout << "System monitoring terminated" << "\n";
}

struct cpuusage SysMonitor::GetCPUusageFromCpustat(struct cpustat s) {
  struct cpuusage r;
  strncpy(r.name, s.name, sizeof(r.name));
  r.name[sizeof(r.name) - 1] = '\0';
  r.idletime = s.idle + s.iowait;
  r.workingtime = s.user + s.nice + s.system + s.irq + s.softirq;
  return r;
}



long double SysMonitor::CpuUsageGetDiff(struct cpuusage now, struct cpuusage prev) {
  // the number of ticks that passed by since the last measurement.
  const unsigned long long workingtime = now.workingtime - prev.workingtime;
  const unsigned long long alltime = workingtime + (now.idletime - prev.idletime);
  // they are divided by themselves - so the unit does not matter.
  // printf("CPU Usage: %.0Lf%%\n", (long double)workingtime / alltime * 100.0L);
  return (long double)workingtime / alltime * 100.0L;
}

// Simply parses /proc/stat.
void SysMonitor::GetCPUGPUUtilization() {
  struct cpuusage prev = {0};
  const int cpu_stat = open("/proc/stat", O_RDONLY);
  assert(cpu_stat != -1);
  fcntl(cpu_stat, F_SETFL, O_NONBLOCK);
  const int gpu_stat = open("/sys/devices/gpu.0/load", O_RDONLY);
  assert(gpu_stat != -1);
  fcntl(gpu_stat, F_SETFL, O_NONBLOCK);
  bool inital_cycle = true;
  // need timestamp
  struct timespec current_time;
  while (1) {
    log_File.open(directory, std::ios::app);
    // Read CPU utilizations
    // let's read everything in one call so it's nicely synced.
    int r_cpu = lseek(cpu_stat, SEEK_SET, 0);
    assert(r_cpu != -1);
    char buffer_cpu[10001];
    const ssize_t readed_cpu = read(cpu_stat, buffer_cpu, sizeof(buffer_cpu) - 1);
    assert(readed_cpu != -1);
    buffer_cpu[readed_cpu] = '\0';
    // Read the values from the readed buffer/
    FILE* cpu_file = fmemopen(buffer_cpu, readed_cpu, "r");
    struct cpustat c = {0};
    int cpu_idx = 1;
    while (fscanf(cpu_file, "%19s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", c.name, &c.user, &c.nice,
      &c.system, &c.idle, &c.iowait, &c.irq, &c.softirq, &c.steal, &c.guest,
      &c.guest_nice) == 11) {
      // Just an example for first cpu core.
      if (strcmp(c.name, "cpu") == 0) {
        if(CPU_NUM == 0){
          struct cpuusage now = GetCPUusageFromCpustat(c);
          cpu_util_ratio.push_back(CpuUsageGetDiff(now, prev));
          // std::cout << "CPU Usage: " << (int)cpu_util_ratio << "% \n";
          prev = now;
          c = {0};
        }
        continue;
      }
      struct cpuusage now = GetCPUusageFromCpustat(c);
      if(!inital_cycle){
        cpu_util_ratio.push_back(CpuUsageGetDiff(now, prev_cpu_usages.front()));
        // std::cout << "CPU Usage: " << (int)cpu_util_ratio[cpu_idx] << "% \n";
      }
      prev_cpu_usages.push(now);
      c = {0};
      if(!inital_cycle){
        // std::cout << "prev size " << prev_cpu_usages.size() << "\n";
        prev_cpu_usages.pop();
        // std::cout << "after size " << prev_cpu_usages.size() << "\n";
      }
      // std::cout << "cpu_idx " << cpu_idx << "\n";
      memset(buffer_cpu, 0, sizeof(buffer_cpu));
      if(cpu_idx == CPU_NUM){
        break;
      }
      cpu_idx++;
    }
    fclose(cpu_file);
    // // Read GPU utilizations
    int r = lseek(gpu_stat, SEEK_SET, 0);
    assert(r != -1);
    char buffer[8];
    const ssize_t readed = read(gpu_stat, buffer, sizeof(buffer) - 1);
    assert(readed != -1);
    buffer[readed] = '\0';
    // Read the values from the readed buffer/
    FILE* gpu_file = fmemopen(buffer, readed, "r");
    long long unsigned int percentage = 0;
    if(fscanf(gpu_file, "%llu", &percentage)){
      gpu_util_ratio = percentage / 10;
      // std::cout << "gpu: " << gpu_util_ratio << "\n";
    }
    fclose(gpu_file);

    // Get current time and log all.
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    int sec = (current_time.tv_sec % 1000);
    float milli = (current_time.tv_nsec / 1000000) / 1000.0;
    if(log_File.is_open() && !inital_cycle){
      // std::cout << "write" << "\n";
      std::cout << sec + milli << " ";
      log_File << sec + milli  << " ";
      for(auto& temp : cpu_util_ratio){
        log_File << temp << " ";
        std::cout << temp << " ";
      }
      log_File << gpu_util_ratio << "\n";
      std::cout << gpu_util_ratio << "\n";
    }
    log_File.close();
    inital_cycle = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(period));
    cpu_util_ratio.clear();
  }
}

void SysMonitor::GetGPUUtilization() {
  struct timespec now;
  #ifdef ramdisk_gpu_debug
    std::ofstream gpu_util_f;
    gpu_util_f.open(GPU_UTIL_FILE, std::ios::out | std::ios::trunc);
    if (!gpu_util_f.is_open()) {
      std::cerr << "Failed to open gpu_util(h)" << std::endl;
      return;
    }
  #endif
  #ifdef Experiment
  while(1){
    std::ifstream gpu_util;
    gpu_util.open(("gpu_util"));
    if (!gpu_util.is_open()) {
      std::cout << "GPU util file open error" << "\n";
      return;
    }
    int ratio = 0;
    gpu_util >> ratio;
    gpu_util_ratio = float(ratio);
    gpu_util.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(MONITORING_PERIOD_MS));
  }
  #endif
  #ifndef Experiment
  const int stat = open("/sys/devices/gpu.0/load", O_RDONLY);
  assert(stat != -1);
  fcntl(stat, F_SETFL, O_NONBLOCK);
  while (1) {
    // let's read everything in one call so it's nicely synced.
    int r = lseek(stat, SEEK_SET, 0);
    assert(r != -1);
    char buffer[8];
    clock_gettime(CLOCK_MONOTONIC, &now);
    const ssize_t readed = read(stat, buffer, sizeof(buffer) - 1);
    assert(readed != -1);
    buffer[readed] = '\0';
    // Read the values from the readed buffer/
    FILE* f = fmemopen(buffer, readed, "r");
    int percentage = 0;
    while (fscanf(f, "%llu", &percentage)) {
      gpu_util_ratio = percentage / 10;
      // std::cout << "GPU util: " << gpu_util_ratio << "% \n"; 
      break;
    }
    fclose(f);
    // gpu_util_f << gpu_util_ratio << " " << now.tv_sec << "." << now.tv_nsec /  1000000000.0 << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(period));
  }
  // gpu_util_f.close();
  #endif
}
